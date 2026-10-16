// Host-Test für src/RemoteFormat.cpp (tools/host-test.ps1), E42.
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

#include "RemoteFormat.h"

using namespace remote_fmt;

static int fails = 0;
static void check(const std::string& name, bool ok, const std::string& info = "") {
  printf("%s %s%s%s\n", ok ? "PASS" : "FAIL", name.c_str(), info.empty() ? "" : " -> ", info.c_str());
  if (!ok) fails++;
}
static bool near(float a, float b) { return std::fabs(a - b) < 0.001f; }
static std::string f(float v) { char b[32]; snprintf(b, sizeof(b), "%.3f", v); return b; }

// Wie struct RepeaterStats in MeshCore 1.18.0 (MyMesh.h)
struct RepeaterStats {
  uint16_t batt_milli_volts;
  uint16_t curr_tx_queue_len;
  int16_t noise_floor;
  int16_t last_rssi;
  uint32_t n_packets_recv;
  uint32_t n_packets_sent;
  uint32_t total_air_time_secs;
  uint32_t total_up_time_secs;
  uint32_t n_sent_flood, n_sent_direct;
  uint32_t n_recv_flood, n_recv_direct;
  uint16_t err_events;
  int16_t last_snr;
  uint16_t n_direct_dups, n_flood_dups;
  uint32_t total_rx_air_time_secs;
  uint32_t n_recv_errors;
};

int main() {
  setvbuf(stdout, nullptr, _IONBF, 0);

  // --- Status ---
  {
    check("RepeaterStats ist 56 Byte (wie auf dem ESP32)", sizeof(RepeaterStats) == 56, std::to_string(sizeof(RepeaterStats)));
    RepeaterStats rs;
    memset(&rs, 0, sizeof(rs));
    rs.batt_milli_volts = 3987;
    rs.curr_tx_queue_len = 2;
    rs.noise_floor = -112;
    rs.total_up_time_secs = 93784;
    rs.err_events = 5;
    Status s = parseStatus((const uint8_t*)&rs, sizeof(rs));
    check("Status: gültig", s.ok);
    check("Status: Akku mV", s.battMv == 3987, std::to_string(s.battMv));
    check("Status: Warteschlange", s.txQueue == 2);
    check("Status: Rauschen negativ", s.noiseFloor == -112, std::to_string(s.noiseFloor));
    check("Status: Laufzeit", s.uptimeS == 93784, std::to_string(s.uptimeS));
    check("Status: Fehler-Flags", s.hasErrFlags && s.errFlags == 5);
    Status kurz = parseStatus((const uint8_t*)&rs, 24);
    check("Status: kurze (alte) Antwort mit Laufzeit gültig, ohne Flags", kurz.ok && kurz.uptimeS == 93784 && !kurz.hasErrFlags);
    check("Status: zu kurz -> ungültig", !parseStatus((const uint8_t*)&rs, 20).ok);
    check("Status: nullptr", !parseStatus(nullptr, 56).ok);
  }

  // --- Telemetrie (CayenneLPP, big endian) ---
  {
    // Kanal 1 Spannung 4,12 V (412 = 0x019C), Kanal 1 Temperatur -3,5 °C (-35 = 0xFFDD)
    const uint8_t a[] = {1, 116, 0x01, 0x9C, 1, 103, 0xFF, 0xDD, 0, 0, 0};
    Telemetry t = parseTelemetry(a, sizeof(a));
    check("Telemetrie: Spannung", t.hasVoltage && near(t.voltage, 4.12f), f(t.voltage));
    check("Telemetrie: Temperatur negativ", t.hasTemp && near(t.tempC, -3.5f), f(t.tempC));
    // Fremder Kanal 2 (externer Sensor) zuerst, dann Kanal 1; Feuchte (1 Byte) dazwischen
    const uint8_t b[] = {2, 103, 0x00, 0xC8, 2, 104, 80, 1, 116, 0x01, 0x4A, 1, 103, 0x01, 0x2C};
    t = parseTelemetry(b, sizeof(b));
    check("Telemetrie: nur Kanal 1 zählt (Spannung 3,30)", t.hasVoltage && near(t.voltage, 3.30f), f(t.voltage));
    check("Telemetrie: Kanal-1-Temperatur 30,0 statt Kanal 2", t.hasTemp && near(t.tempC, 30.0f), f(t.tempC));
    // Erster Wert je Größe gilt
    const uint8_t c[] = {1, 103, 0x00, 0x64, 1, 103, 0x01, 0x00};
    t = parseTelemetry(c, sizeof(c));
    check("Telemetrie: erste Temperatur gilt", near(t.tempC, 10.0f), f(t.tempC));
    // Unbekannter Typ bricht ab, Werte davor bleiben
    const uint8_t d[] = {1, 116, 0x01, 0x90, 1, 240, 9, 9, 9, 1, 103, 0, 10};
    t = parseTelemetry(d, sizeof(d));
    check("Telemetrie: unbekannter Typ (Polyline) -> Abbruch, Spannung bleibt",
          t.hasVoltage && near(t.voltage, 4.0f) && !t.hasTemp && t.unknownTypes == 1);
    // Abgeschnittener Wert
    const uint8_t e[] = {1, 116, 0x01};
    t = parseTelemetry(e, sizeof(e));
    check("Telemetrie: abgeschnitten -> nichts", !t.hasVoltage && !t.hasTemp);
    t = parseTelemetry(nullptr, 0);
    check("Telemetrie: leer", !t.hasVoltage && !t.hasTemp);
  }

  // --- Anmeldung ---
  {
    const uint8_t admin[] = {1, 2, 3, 4, RESP_LOGIN_OK, 0, 1, 3, 9, 9, 9, 9, 1};
    Login l = parseLogin(admin, sizeof(admin));
    check("Login: Admin", l.ok && l.admin && l.perms == 3);
    const uint8_t guest[] = {1, 2, 3, 4, RESP_LOGIN_OK, 0, 0, 1, 9, 9, 9, 9, 1};
    l = parseLogin(guest, sizeof(guest));
    check("Login: Gast", l.ok && !l.admin && l.perms == 1);
    const uint8_t other[] = {1, 2, 3, 4, 5, 0, 1, 3};
    check("Login: anderer Antworttyp -> kein Login", !parseLogin(other, sizeof(other)).ok);
    check("Login: zu kurz", !parseLogin(guest, 6).ok);
  }

  // --- Schlüssel ---
  {
    const char* hex = "A1B2C3D4c0ffee000102030405060708090a0b0c0d0e0f1011121314151617ff";
    uint8_t k[32];
    check("Schlüssel: Hex gemischt -> ok", keyFromHex(hex, k) && k[0] == 0xA1 && k[1] == 0xB2 && k[31] == 0xFF);
    char back[65];
    keyToHex(k, back);
    check("Schlüssel: zurück klein", !strcmp(back, "a1b2c3d4c0ffee000102030405060708090a0b0c0d0e0f1011121314151617ff"), back);
    check("Schlüssel: zu kurz", !keyFromHex("b0a2", k));
    check("Schlüssel: Nicht-Hex", !keyFromHex("x0a23599c0ffee000102030405060708090a0b0c0d0e0f1011121314151617ff", k));
    check("Schlüssel: nullptr", !keyFromHex(nullptr, k));
  }

  // --- Wartezeit ---
  {
    check("Timeout: Minimum 6 s", replyTimeoutMs(10, false, 0) == 6000, std::to_string(replyTimeoutMs(10, false, 0)));
    uint32_t fl = replyTimeoutMs(400, true, 0);
    check("Timeout: Flood 400 ms Airtime", fl == 2 * (500 + 6400) + 1000, std::to_string(fl));
    uint32_t d3 = replyTimeoutMs(400, false, 3);
    check("Timeout: Direkt 3 Hops", d3 == 2 * (500 + (2400 + 250) * 4) + 1000, std::to_string(d3));
    check("Timeout: Maximum 45 s", replyTimeoutMs(5000, true, 0) == 45000);
  }

  // --- Ablauf ---
  {
    PollFlow p;
    p.start(false);
    check("Ablauf: nicht angemeldet -> LOGIN", p.step == PollFlow::LOGIN);
    p.onReply();
    check("Ablauf: nach Login -> STATUS", p.step == PollFlow::STATUS && p.freshLogin);
    p.onReply();
    check("Ablauf: nach Status -> TELEMETRY", p.step == PollFlow::TELEMETRY && p.statusOk);
    p.onReply();
    check("Ablauf: alles da -> DONE", p.step == PollFlow::DONE && p.telemetryOk && p.finished());

    p.start(true);
    check("Ablauf: angemeldet -> gleich STATUS", p.step == PollFlow::STATUS && !p.freshLogin);
    bool reset = p.onTimeout();
    check("Ablauf: Status fehlt bei alter Anmeldung -> neu anmelden", reset && p.step == PollFlow::LOGIN && p.relogged);
    p.onReply();
    reset = p.onTimeout();
    check("Ablauf: Status fehlt auch nach Neuanmeldung -> FAILED", reset && p.step == PollFlow::FAILED && p.finished());

    p.start(false);
    reset = p.onTimeout();
    check("Ablauf: Login ohne Antwort -> FAILED", reset && p.step == PollFlow::FAILED);

    p.start(true);
    p.onReply();
    reset = p.onTimeout();
    check("Ablauf: Telemetrie fehlt -> DONE ohne Telemetrie, Sitzung bleibt",
          !reset && p.step == PollFlow::DONE && p.statusOk && !p.telemetryOk);
  }

  printf(fails ? "%d FEHLER\n" : "ALLE OK\n", fails);
  return fails ? 1 : 0;
}
