#include "RemoteAdmin.h"

#include <esp_heap_caps.h>

#include <helpers/AdvertDataHelpers.h>
#include <helpers/TxtDataHelpers.h>

#include "CliBridge.h"
#include "GatewayMesh.h"
#include "LogBuffer.h"
#include "RemoteFormat.h"

using namespace remote;

namespace {

static const int MAX_SESSIONS = 12;
static const int MAX_JOBS = 8;
static const uint8_t PATH_UNKNOWN = 0xFF;   // OUT_PATH_UNKNOWN

struct Session {
  bool used;
  uint8_t key[32];
  uint8_t secret[32];
  uint8_t outPath[MAX_PATH_SIZE];
  uint8_t outPathLen;   // kodiert wie Packet::path_len, PATH_UNKNOWN = per Flood
  bool loggedIn;
  bool admin;
  bool console;         // aktuelles Ziel der Fernkonsole
  uint32_t lastUseMs;
};

struct Job {
  uint32_t id;          // 0 = Platz frei
  Kind kind;
  State state;
  uint8_t key[32];
  char pass[MAX_PASS + 1];
  char cmd[MAX_CMD + 1];
  int sess;
  remote_fmt::PollFlow flow;
  uint32_t tag;         // erwartete Antwort (REQ spiegelt den Zeitstempel)
  bool awaitLogin;
  uint32_t startMs, sentMs, deadlineMs;
  JobInfo info;
};

struct Store {
  Known known[MAX_KNOWN];
  Session sess[MAX_SESSIONS];
  Job jobs[MAX_JOBS];
  int active;           // Index des laufenden Auftrags, -1 = keiner
};

Store* S = nullptr;
SemaphoreHandle_t s_lock = nullptr;
uint32_t s_nextId = 1;

struct Lock {
  Lock() { xSemaphoreTake(s_lock, portMAX_DELAY); }
  ~Lock() { xSemaphoreGive(s_lock); }
};

int findSession(const uint8_t key[32]) {
  for (int i = 0; i < MAX_SESSIONS; i++) {
    if (S->sess[i].used && !memcmp(S->sess[i].key, key, 32)) return i;
  }
  return -1;
}

bool sessionBusy(int i) {
  for (int j = 0; j < MAX_JOBS; j++) {
    const Job& b = S->jobs[j];
    if (b.id && b.sess == i && (b.state == State::QUEUED || b.state == State::RUNNING)) return true;
  }
  return false;
}

// Nur loopTask (ECDH-Rechnung). Ältesten unbenutzten Platz wiederverwenden.
int openSession(const uint8_t key[32]) {
  int i = findSession(key);
  if (i >= 0) return i;
  int slot = -1;
  for (int k = 0; k < MAX_SESSIONS; k++) {
    Session& s = S->sess[k];
    if (!s.used) { slot = k; break; }
    if (s.console || sessionBusy(k)) continue;
    if (slot < 0 || (int32_t)(s.lastUseMs - S->sess[slot].lastUseMs) < 0) slot = k;
  }
  if (slot < 0) return -1;
  Session& s = S->sess[slot];
  memset(&s, 0, sizeof(s));
  s.used = true;
  memcpy(s.key, key, 32);
  the_mesh.self_id.calcSharedSecret(s.secret, key);
  s.outPathLen = PATH_UNKNOWN;
  s.lastUseMs = millis();
  return slot;
}

void displayName(const uint8_t key[32], char* out, size_t n) {
  for (int i = 0; i < MAX_KNOWN; i++) {
    if (S->known[i].heardMs && !memcmp(S->known[i].key, key, 32)) { strlcpy(out, S->known[i].name, n); return; }
  }
  snprintf(out, n, "%02X%02X%02X%02X", key[0], key[1], key[2], key[3]);
}

void conLog(char type, const char* text) {
  console_log.push(type, the_mesh.getRTCClock()->getCurrentTime(), text);
}

// Paket senden (Flood oder direkt). Aufrufer hält die Sperre.
void sendPacket(Job& j, mesh::Packet* pkt) {
  Session& s = S->sess[j.sess];
  uint32_t air = the_mesh.estAirtime(pkt->getRawLength());
  bool flood = s.outPathLen == PATH_UNKNOWN;
  uint8_t hops = flood ? 0 : (s.outPathLen & 63);
  if (flood) the_mesh.sendFlood(pkt, 0, the_mesh.pathHashSize());
  else the_mesh.sendDirect(pkt, s.outPath, s.outPathLen);
  j.info.flood = flood;
  j.info.hops = hops;
  j.sentMs = millis();
  j.deadlineMs = j.sentMs + remote_fmt::replyTimeoutMs(air, flood, hops);
  s.lastUseMs = j.sentMs;
}

bool sendLogin(Job& j) {
  Session& s = S->sess[j.sess];
  uint8_t temp[4 + MAX_PASS];
  uint32_t now = the_mesh.getRTCClock()->getCurrentTimeUnique();
  memcpy(temp, &now, 4);
  size_t len = strlen(j.pass);
  if (len > MAX_PASS) len = MAX_PASS;
  memcpy(&temp[4], j.pass, len);
  mesh::Identity dest(s.key);
  mesh::Packet* pkt = the_mesh.createAnonDatagram(PAYLOAD_TYPE_ANON_REQ, the_mesh.self_id, dest, s.secret, temp, 4 + len);
  if (!pkt) return false;
  j.awaitLogin = true;
  sendPacket(j, pkt);
  return true;
}

bool sendRequest(Job& j, uint8_t reqType) {
  Session& s = S->sess[j.sess];
  uint8_t temp[13];
  j.tag = the_mesh.getRTCClock()->getCurrentTimeUnique();
  memcpy(temp, &j.tag, 4);
  temp[4] = reqType;
  memset(&temp[5], 0, 4);   // Telemetrie: Byte 5 = inverse Rechtemaske, 0 = alles erlaubte
  the_mesh.getRNG()->random(&temp[9], 4);
  mesh::Identity dest(s.key);
  mesh::Packet* pkt = the_mesh.createDatagram(PAYLOAD_TYPE_REQ, dest, s.secret, temp, sizeof(temp));
  if (!pkt) return false;
  j.awaitLogin = false;
  sendPacket(j, pkt);
  return true;
}

bool sendCli(Job& j) {
  Session& s = S->sess[j.sess];
  uint8_t temp[5 + MAX_CMD + 1];
  uint32_t now = the_mesh.getRTCClock()->getCurrentTimeUnique();
  memcpy(temp, &now, 4);
  temp[4] = (TXT_TYPE_CLI_DATA << 2);
  size_t len = strlen(j.cmd);
  memcpy(&temp[5], j.cmd, len);
  mesh::Identity dest(s.key);
  mesh::Packet* pkt = the_mesh.createDatagram(PAYLOAD_TYPE_TXT_MSG, dest, s.secret, temp, 5 + len);
  if (!pkt) return false;
  j.awaitLogin = false;
  sendPacket(j, pkt);
  return true;
}

void finish(Job& j, State st, const char* error) {
  j.state = st;
  j.info.state = st;
  j.info.ms = millis() - j.startMs;
  if (error) strlcpy(j.info.error, error, sizeof(j.info.error));
  memset(j.pass, 0, sizeof(j.pass));   // Passwort nicht länger als nötig halten
  S->active = -1;
}

// Nächsten Schritt einer Überwachungsabfrage senden
void pollStep(Job& j) {
  using F = remote_fmt::PollFlow;
  bool ok = false;
  if (j.flow.step == F::LOGIN) ok = sendLogin(j);
  else if (j.flow.step == F::STATUS) ok = sendRequest(j, remote_fmt::REQ_GET_STATUS);
  else if (j.flow.step == F::TELEMETRY) ok = sendRequest(j, remote_fmt::REQ_GET_TELEMETRY);
  if (!ok && !j.flow.finished()) finish(j, State::FAILED, "kein freier Paketpuffer");
}

void pollFinished(Job& j) {
  if (j.flow.step == remote_fmt::PollFlow::DONE) {
    j.info.poll.hasTelemetry = j.flow.telemetryOk;
    finish(j, State::DONE, nullptr);
  } else {
    finish(j, State::FAILED, j.flow.freshLogin || j.flow.relogged ? "keine Antwort auf Statusabfrage"
                                                                  : "keine Antwort auf Anmeldung (Passwort falsch oder nicht erreichbar)");
  }
}

void start(Job& j) {
  j.state = State::RUNNING;
  j.info.state = State::RUNNING;
  j.startMs = millis();
  j.sess = openSession(j.key);
  if (j.sess < 0) { finish(j, State::FAILED, "zu viele Sitzungen"); return; }
  Session& s = S->sess[j.sess];
  char name[32];
  displayName(j.key, name, sizeof(name));
  if (j.kind == Kind::LOGIN) {
    char l[96];
    snprintf(l, sizeof(l), "[%s] Anmeldung …", name);
    conLog('s', l);
    if (!sendLogin(j)) finish(j, State::FAILED, "kein freier Paketpuffer");
  } else if (j.kind == Kind::CLI) {
    if (!s.loggedIn || !s.admin) { finish(j, State::FAILED, "nicht als Admin angemeldet"); return; }
    char l[LogBuffer::LINE_LEN];
    j.info.hidden = cli_bridge::isSecretCommand(j.cmd);
    snprintf(l, sizeof(l), "[%s] %s", name, cli_bridge::maskedCommand(j.cmd));
    conLog('c', l);
    if (!sendCli(j)) finish(j, State::FAILED, "kein freier Paketpuffer");
  } else {
    j.flow.start(s.loggedIn);
    pollStep(j);
  }
}

void forgetRoute(Session& s) {
  s.loggedIn = false;
  s.outPathLen = PATH_UNKNOWN;
}

void timeout(Job& j) {
  Session& s = S->sess[j.sess];
  if (j.kind == Kind::POLL) {
    if (j.flow.onTimeout()) forgetRoute(s);
    if (j.flow.finished()) pollFinished(j);
    else pollStep(j);
    return;
  }
  char name[32], l[LogBuffer::LINE_LEN];
  displayName(j.key, name, sizeof(name));
  if (j.kind == Kind::LOGIN) {
    forgetRoute(s);
    finish(j, State::FAILED, "keine Antwort (Passwort falsch oder Repeater nicht erreichbar)");
  } else {
    // Pfad könnte nicht mehr stimmen -> nächster Befehl wieder per Flood
    s.outPathLen = PATH_UNKNOWN;
    finish(j, State::FAILED, "keine Antwort (nochmal senden)");
  }
  snprintf(l, sizeof(l), "[%s] %s", name, j.info.error);
  conLog('s', l);
}

int runningJobFor(int sess) {
  if (S->active < 0) return -1;
  Job& j = S->jobs[S->active];
  return (j.state == State::RUNNING && j.sess == sess) ? S->active : -1;
}

void onResponse(Job& j, const uint8_t* data, size_t len) {
  Session& s = S->sess[j.sess];
  if (j.awaitLogin) {
    remote_fmt::Login lg = remote_fmt::parseLogin(data, len);
    if (!lg.ok) return;
    s.loggedIn = true;
    s.admin = lg.admin;
    if (j.kind == Kind::LOGIN) {
      for (int i = 0; i < MAX_SESSIONS; i++) S->sess[i].console = false;
      s.console = true;
      j.info.admin = lg.admin;
      finish(j, State::DONE, nullptr);
      char name[32], l[LogBuffer::LINE_LEN];
      displayName(j.key, name, sizeof(name));
      snprintf(l, sizeof(l), "[%s] angemeldet als %s (%s, %u ms)%s", name, lg.admin ? "Admin" : "Gast",
               j.info.flood ? "Flood" : "direkt", (unsigned)j.info.ms,
               lg.admin ? "" : " - CLI braucht das Admin-Passwort");
      conLog('s', l);
    } else {
      j.flow.onReply();
      pollStep(j);
    }
    return;
  }
  if (len < 4 || memcmp(data, &j.tag, 4) != 0) return;   // alte/fremde Antwort
  if (j.kind != Kind::POLL) return;
  using F = remote_fmt::PollFlow;
  if (j.flow.step == F::STATUS) {
    remote_fmt::Status st = remote_fmt::parseStatus(data + 4, len - 4);
    if (!st.ok) return;
    PollData& p = j.info.poll;
    p.hasStatus = true;
    p.battMv = st.battMv;
    p.noiseFloor = st.noiseFloor;
    p.uptimeS = st.uptimeS;
    p.hasErr = st.hasErrFlags;
    p.errFlags = st.errFlags;
  } else if (j.flow.step == F::TELEMETRY) {
    remote_fmt::Telemetry t = remote_fmt::parseTelemetry(data + 4, len - 4);
    PollData& p = j.info.poll;
    p.hasVoltage = t.hasVoltage;
    p.voltage = t.voltage;
    p.hasTemp = t.hasTemp;
    p.tempC = t.tempC;
  } else {
    return;
  }
  j.flow.onReply();
  if (j.flow.finished()) pollFinished(j);
  else pollStep(j);
}

uint32_t submit(Kind kind, const uint8_t key[32], const char* pass, const char* cmd, String& err) {
  if (!S) { err = "nicht bereit"; return 0; }
  Lock l;
  int slot = -1;
  for (int i = 0; i < MAX_JOBS; i++) {
    Job& j = S->jobs[i];
    if (!j.id) { slot = i; break; }
    if ((j.state == State::DONE || j.state == State::FAILED) && (slot < 0 || j.id < S->jobs[slot].id)) slot = i;
  }
  if (slot < 0) { err = "Warteschlange voll, gleich nochmal"; return 0; }
  Job& j = S->jobs[slot];
  memset(&j, 0, sizeof(j));
  j.id = s_nextId++;
  j.kind = kind;
  j.state = State::QUEUED;
  j.info.state = State::QUEUED;
  j.info.kind = kind;
  j.sess = -1;
  memcpy(j.key, key, 32);
  if (pass) strlcpy(j.pass, pass, sizeof(j.pass));
  if (cmd) strlcpy(j.cmd, cmd, sizeof(j.cmd));
  return j.id;
}

}  // namespace

bool remote::begin() {
  if (S) return true;
  s_lock = xSemaphoreCreateMutex();
  S = (Store*)heap_caps_calloc(1, sizeof(Store), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!S) S = (Store*)calloc(1, sizeof(Store));
  if (S) S->active = -1;
  return S && s_lock;
}

int remote::known(Known* out, int max) {
  if (!S) return 0;
  Lock l;
  int n = 0;
  for (int i = 0; i < MAX_KNOWN && n < max; i++) {
    if (S->known[i].heardMs) out[n++] = S->known[i];
  }
  // zuletzt gehörte zuerst (Einfügesortierung, n <= 32)
  for (int i = 1; i < n; i++) {
    Known k = out[i];
    int j = i - 1;
    while (j >= 0 && (int32_t)(out[j].heardMs - k.heardMs) < 0) { out[j + 1] = out[j]; j--; }
    out[j + 1] = k;
  }
  return n;
}

bool remote::nameOf(const uint8_t key[32], char* out, size_t n) {
  if (!S) return false;
  Lock l;
  for (int i = 0; i < MAX_KNOWN; i++) {
    if (S->known[i].heardMs && !memcmp(S->known[i].key, key, 32)) { strlcpy(out, S->known[i].name, n); return true; }
  }
  return false;
}

uint32_t remote::submitLogin(const uint8_t key[32], const char* pass, String& err) {
  if (!pass) pass = "";
  if (strlen(pass) > MAX_PASS) { err = "Passwort max. 15 Zeichen (Grenze von MeshCore)"; return 0; }
  return submit(Kind::LOGIN, key, pass, nullptr, err);
}

uint32_t remote::submitCli(const char* cmd, String& err) {
  if (!S) { err = "nicht bereit"; return 0; }
  if (!cmd || !*cmd) { err = "leerer Befehl"; return 0; }
  if (strlen(cmd) > MAX_CMD) { err = "Befehl zu lang (max. 159 Zeichen)"; return 0; }
  uint8_t key[32];
  {
    Lock l;
    int c = -1;
    for (int i = 0; i < MAX_SESSIONS; i++) if (S->sess[i].used && S->sess[i].console) c = i;
    if (c < 0 || !S->sess[c].loggedIn) { err = "nicht angemeldet"; return 0; }
    if (!S->sess[c].admin) { err = "nur als Gast angemeldet - CLI braucht das Admin-Passwort"; return 0; }
    for (int i = 0; i < MAX_JOBS; i++) {
      const Job& j = S->jobs[i];
      if (j.id && j.kind == Kind::CLI && (j.state == State::QUEUED || j.state == State::RUNNING)) {
        err = "vorheriger Befehl läuft noch";
        return 0;
      }
    }
    memcpy(key, S->sess[c].key, 32);
    S->sess[c].lastUseMs = millis();
  }
  return submit(Kind::CLI, key, nullptr, cmd, err);
}

uint32_t remote::submitPoll(const uint8_t key[32], const char* pass, String& err) {
  return submit(Kind::POLL, key, pass ? pass : "", nullptr, err);
}

bool remote::job(uint32_t id, JobInfo& out) {
  if (!S || !id) return false;
  Lock l;
  for (int i = 0; i < MAX_JOBS; i++) {
    const Job& j = S->jobs[i];
    if (j.id != id) continue;
    out = j.info;
    if (j.state == State::RUNNING) out.ms = millis() - j.startMs;
    return true;
  }
  return false;
}

void remote::logout() {
  if (!S) return;
  Lock l;
  for (int i = 0; i < MAX_SESSIONS; i++) {
    if (!S->sess[i].console) continue;
    S->sess[i].console = false;
    char name[32], t[64];
    displayName(S->sess[i].key, name, sizeof(name));
    snprintf(t, sizeof(t), "[%s] Fernkonsole beendet", name);
    conLog('s', t);
  }
}

void remote::consoleInfo(ConsoleInfo& out) {
  memset(&out, 0, sizeof(out));
  if (!S) return;
  Lock l;
  for (int i = 0; i < MAX_SESSIONS; i++) {
    const Session& s = S->sess[i];
    if (!s.used || !s.console) continue;
    out.active = s.loggedIn;
    memcpy(out.key, s.key, 32);
    displayName(s.key, out.name, sizeof(out.name));
    out.admin = s.admin;
    out.flood = s.outPathLen == PATH_UNKNOWN;
    out.hops = out.flood ? 0 : (s.outPathLen & 63);
    out.idleS = (millis() - s.lastUseMs) / 1000;
    for (int j = 0; j < MAX_JOBS; j++) {
      const Job& b = S->jobs[j];
      if (b.id && b.kind == Kind::CLI && (b.state == State::QUEUED || b.state == State::RUNNING)) out.busy = true;
    }
  }
}

void remote::process() {
  if (!S) return;
  Lock l;
  uint32_t now = millis();
  // Fernkonsole nach 15 min ohne Befehl beenden
  for (int i = 0; i < MAX_SESSIONS; i++) {
    Session& s = S->sess[i];
    if (s.used && s.console && !sessionBusy(i) && now - s.lastUseMs > CONSOLE_IDLE_MS) {
      s.console = false;
      char name[32], t[80];
      displayName(s.key, name, sizeof(name));
      snprintf(t, sizeof(t), "[%s] Fernkonsole nach 15 min ohne Befehl beendet", name);
      conLog('s', t);
    }
  }
  if (S->active >= 0) {
    Job& j = S->jobs[S->active];
    if (j.state == State::RUNNING && (int32_t)(now - j.deadlineMs) >= 0) timeout(j);
    return;
  }
  int next = -1;
  for (int i = 0; i < MAX_JOBS; i++) {
    const Job& j = S->jobs[i];
    if (j.id && j.state == State::QUEUED && (next < 0 || j.id < S->jobs[next].id)) next = i;
  }
  if (next < 0) return;
  S->active = next;
  start(S->jobs[next]);
}

void remote::onAdvert(const uint8_t* key, const char* name, uint8_t type, uint8_t hops, float snr) {
  if (!S || (type != ADV_TYPE_REPEATER && type != ADV_TYPE_ROOM)) return;
  if (!memcmp(key, the_mesh.self_id.pub_key, 32)) return;
  Lock l;
  int slot = -1;
  for (int i = 0; i < MAX_KNOWN; i++) {
    if (S->known[i].heardMs && !memcmp(S->known[i].key, key, 32)) { slot = i; break; }
  }
  if (slot < 0) {
    for (int i = 0; i < MAX_KNOWN; i++) {
      if (!S->known[i].heardMs) { slot = i; break; }
      if (slot < 0 || (int32_t)(S->known[i].heardMs - S->known[slot].heardMs) < 0) slot = i;
    }
  }
  Known& k = S->known[slot];
  memcpy(k.key, key, 32);
  strlcpy(k.name, name && *name ? name : "?", sizeof(k.name));
  k.type = type;
  k.hops = hops;
  k.snr = snr;
  k.heardMs = millis() | 1;   // 0 = Platz frei
}

int remote::matchHash(uint8_t hash, int* sessOut, int max) {
  if (!S) return 0;
  Lock l;
  int n = 0;
  for (int i = 0; i < MAX_SESSIONS && n < max; i++) {
    if (S->sess[i].used && S->sess[i].key[0] == hash) sessOut[n++] = i;
  }
  return n;
}

void remote::secretOf(int sess, uint8_t* out) {
  Lock l;
  memcpy(out, S->sess[sess].secret, 32);
}

bool remote::onData(int sess, bool flood, uint8_t type, const uint8_t* data, size_t len) {
  (void)flood;
  if (!S) return false;
  Lock l;
  int ji = runningJobFor(sess);
  if (type == PAYLOAD_TYPE_RESPONSE) {
    if (ji >= 0) onResponse(S->jobs[ji], data, len);
    return true;   // Antworten an das Gateway als Repeater gibt es sonst nicht
  }
  if (type == PAYLOAD_TYPE_TXT_MSG && len > 5 && (data[4] >> 2) == TXT_TYPE_CLI_DATA && ji >= 0 &&
      S->jobs[ji].kind == Kind::CLI) {
    Job& j = S->jobs[ji];
    size_t n = len - 5;
    if (n > MAX_REPLY) n = MAX_REPLY;
    memcpy(j.info.reply, data + 5, n);
    j.info.reply[n] = 0;   // Rest kann mit Nullen aufgefüllt sein
    finish(j, State::DONE, nullptr);
    char name[32], line[LogBuffer::LINE_LEN];
    displayName(j.key, name, sizeof(name));
    snprintf(line, sizeof(line), "[%s] %s", name, j.info.hidden ? "(Antwort enthält ein Geheimnis, nur in der Konsole angezeigt)" : j.info.reply);
    conLog(j.info.hidden ? 's' : 'r', line);
    return true;
  }
  return false;   // z. B. CLI-Befehl desselben Knotens an dieses Gateway -> Repeater-Code
}

void remote::onPath(int sess, const uint8_t* path, uint8_t pathLen, uint8_t extraType, const uint8_t* extra, size_t extraLen) {
  if (!S) return;
  {
    Lock l;
    Session& s = S->sess[sess];
    s.outPathLen = mesh::Packet::copyPath(s.outPath, path, pathLen);
  }
  if (extraType == PAYLOAD_TYPE_RESPONSE && extraLen > 0) onData(sess, true, PAYLOAD_TYPE_RESPONSE, extra, extraLen);
}
