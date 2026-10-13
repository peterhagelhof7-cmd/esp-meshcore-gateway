#include "CliBridge.h"

#include <esp_heap_caps.h>

#include "GatewayMesh.h"
#include "LogBuffer.h"

using namespace cli_bridge;

struct CliJob {
  uint32_t id;          // 0 = Platz frei
  uint8_t count;
  uint8_t next;         // nächster abzuarbeitender Befehl
  bool quiet;
  bool done;
  char cmds[MAX_BATCH][MAX_CMD_LEN + 1];
  char replies[MAX_BATCH][MAX_REPLY_LEN + 1];
};

// 4 Plätze à ca. 14 KB, im PSRAM. Fertige Jobs bleiben abrufbar, bis ihr Platz
// für einen neuen Job gebraucht wird (ältester fertiger zuerst).
static const size_t SLOTS = 4;
static CliJob* s_jobs = nullptr;
static uint32_t s_nextId = 1;
static SemaphoreHandle_t s_lock = nullptr;

bool cli_bridge::begin() {
  if (s_jobs) return true;
  s_lock = xSemaphoreCreateMutex();
  size_t bytes = SLOTS * sizeof(CliJob);
  s_jobs = (CliJob*)heap_caps_calloc(1, bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!s_jobs) s_jobs = (CliJob*)calloc(1, bytes);
  return s_jobs && s_lock;
}

uint32_t cli_bridge::submit(const char* const* cmds, size_t count, bool quiet) {
  if (!s_jobs || count == 0 || count > MAX_BATCH) return 0;
  xSemaphoreTake(s_lock, portMAX_DELAY);
  CliJob* slot = nullptr;
  for (size_t i = 0; i < SLOTS; i++) {
    CliJob& j = s_jobs[i];
    if (j.id == 0) { slot = &j; break; }
    if (j.done && (!slot || j.id < slot->id)) slot = &j;
  }
  uint32_t id = 0;
  if (slot) {
    id = s_nextId++;
    slot->id = id;
    slot->count = count;
    slot->next = 0;
    slot->quiet = quiet;
    slot->done = false;
    for (size_t i = 0; i < count; i++) {
      strlcpy(slot->cmds[i], cmds[i] ? cmds[i] : "", sizeof(slot->cmds[i]));
      slot->replies[i][0] = 0;
    }
  }
  xSemaphoreGive(s_lock);
  return id;
}

uint32_t cli_bridge::submitOne(const char* cmd) {
  return submit(&cmd, 1, false);
}

static void appendEscaped(String& out, const char* s) {
  for (; *s; s++) {
    char c = *s;
    if (c == '"') out += "\\\"";
    else if (c == '\\') out += "\\\\";
    else if (c == '\n') out += "\\n";
    else if ((uint8_t)c < 0x20) out += ' ';
    else out += c;
  }
}

bool cli_bridge::jobJson(uint32_t id, String& out) {
  if (!s_jobs || id == 0) return false;
  bool found = false;
  xSemaphoreTake(s_lock, portMAX_DELAY);
  for (size_t i = 0; i < SLOTS; i++) {
    CliJob& j = s_jobs[i];
    if (j.id != id) continue;
    found = true;
    out += "{\"id\":";
    out += String(j.id);
    out += ",\"done\":";
    out += j.done ? "true" : "false";
    out += ",\"replies\":[";
    for (size_t k = 0; k < j.next; k++) {
      if (k) out += ',';
      out += "{\"cmd\":\"";
      appendEscaped(out, j.cmds[k]);
      out += "\",\"reply\":\"";
      appendEscaped(out, j.replies[k]);
      out += "\"}";
    }
    out += "]}";
    break;
  }
  xSemaphoreGive(s_lock);
  return found;
}

bool cli_bridge::process() {
  if (!s_jobs) return false;

  // Ältesten offenen Job suchen und den nächsten Befehl herauskopieren -
  // handleCommand() läuft danach ohne gehaltenen Mutex.
  char cmd[MAX_CMD_LEN + 1];
  uint32_t id = 0;
  uint8_t idx = 0;
  bool quiet = false;
  xSemaphoreTake(s_lock, portMAX_DELAY);
  CliJob* job = nullptr;
  for (size_t i = 0; i < SLOTS; i++) {
    CliJob& j = s_jobs[i];
    if (j.id != 0 && !j.done && (!job || j.id < job->id)) job = &j;
  }
  if (job) {
    id = job->id;
    idx = job->next;
    quiet = job->quiet;
    strlcpy(cmd, job->cmds[idx], sizeof(cmd));
  }
  xSemaphoreGive(s_lock);
  if (!id) return false;

  uint32_t now = the_mesh.getRTCClock()->getCurrentTime();
  // quiet: reine Abfragen (get/region get) nicht in die Konsole, Änderungen schon.
  bool isQuery = memcmp(cmd, "get ", 4) == 0 || memcmp(cmd, "region get ", 11) == 0 ||
                 strcmp(cmd, "region") == 0 || strcmp(cmd, "ver") == 0;
  bool log = !(quiet && isQuery);
  // Geheimnisse nicht im Konsolen-Log/seriell ablegen: "password <neu>" (die
  // CLI wiederholt es sogar in der Antwort) und "set prv.key <hex>".
  bool isPassword = memcmp(cmd, "password ", 9) == 0;
  bool isKeySet = memcmp(cmd, "set prv.key ", 12) == 0;
  const char* logCmd = isPassword ? "password ********" : (isKeySet ? "set prv.key ********" : cmd);
  if (log) console_log.push('c', now, logCmd);

  // handleCommand() verändert den Puffer (Tokenizer) -> Arbeitskopie.
  char work[MAX_CMD_LEN + 1];
  strlcpy(work, cmd, sizeof(work));
  char reply[512];
  reply[0] = 0;
  the_mesh.handleCommand(0, work, reply);   // sender_timestamp 0 = wie serielle CLI
  if (isPassword && memcmp(reply, "password now: ", 14) == 0) {
    strcpy(reply, "password now: ********");
  }

  if (log) {
    Serial.printf("[WEB-CLI] %s\n", logCmd);
    if (reply[0]) {
      console_log.pushLines('r', now, reply);
      Serial.print("  -> "); Serial.println(reply);
    }
  }

  bool finished = false;
  xSemaphoreTake(s_lock, portMAX_DELAY);
  // Platz kann inzwischen nicht neu vergeben sein (nur fertige Jobs werden
  // überschrieben), id-Prüfung trotzdem als Absicherung.
  if (job->id == id && job->next == idx) {
    strlcpy(job->replies[idx], reply, sizeof(job->replies[idx]));
    if (isPassword || isKeySet) strlcpy(job->cmds[idx], logCmd, sizeof(job->cmds[idx]));
    job->next++;
    if (job->next >= job->count) {
      job->done = true;
      finished = true;
    }
  }
  xSemaphoreGive(s_lock);
  return finished;
}
