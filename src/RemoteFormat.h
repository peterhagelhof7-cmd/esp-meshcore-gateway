#pragma once

#include <stddef.h>
#include <stdint.h>

// Antworten entfernter Repeater auswerten (E42). Ohne Arduino-Abhängigkeit,
// auf dem PC testbar (test/host/remote_format_test.cpp).
//
// Formate wie MeshCore repeater-v1.18.0 (examples/simple_repeater/MyMesh.cpp):
// - Antwort auf REQ_TYPE_GET_STATUS: 4 Byte Tag, dann struct RepeaterStats
//   (little endian, natürliche Ausrichtung, 56 Byte; ältere Firmware kürzer).
// - Antwort auf REQ_TYPE_GET_TELEMETRY_DATA: 4 Byte Tag, dann CayenneLPP
//   (Kanal, Typ, Wert big endian). Kanal 1 = der Repeater selbst.
// - Antwort auf die Anmeldung: 4 Byte Zeit, RESP_SERVER_LOGIN_OK (0),
//   0, isAdmin, permissions, ...
namespace remote_fmt {

static const uint8_t REQ_GET_STATUS = 0x01;
static const uint8_t REQ_GET_TELEMETRY = 0x03;
static const uint8_t RESP_LOGIN_OK = 0;
static const uint8_t SELF_CHANNEL = 1;

struct Status {
  bool ok;
  uint16_t battMv;
  uint16_t txQueue;
  int16_t noiseFloor;
  uint32_t uptimeS;
  uint16_t errFlags;
  bool hasErrFlags;     // Feld erst ab 1.x vorhanden
};

// data/len = Antwort OHNE die 4 Byte Tag.
Status parseStatus(const uint8_t* data, size_t len);

struct Telemetry {
  bool hasVoltage;
  float voltage;        // V
  bool hasTemp;
  float tempC;
  int unknownTypes;     // übersprungen/abgebrochen wegen unbekanntem Typ
};

// data/len = Antwort OHNE die 4 Byte Tag. Nur Kanal 1 (selbst) wird
// übernommen, jeweils der erste Wert je Größe.
Telemetry parseTelemetry(const uint8_t* data, size_t len);

struct Login {
  bool ok;
  bool admin;
  uint8_t perms;
};
// data/len = komplette Antwort (inkl. 4 Byte Zeit).
Login parseLogin(const uint8_t* data, size_t len);

// 64 Hex-Zeichen <-> 32 Byte. Groß/klein egal, Leerzeichen nicht erlaubt.
bool keyFromHex(const char* hex, uint8_t key[32]);
void keyToHex(const uint8_t key[32], char out[65]);

// Wartezeit auf eine Antwort, ähnlich der MeshCore-App: Flood = 16 x Airtime,
// Direkt = (6 x Airtime + 250 ms) je Hop, beides hin und zurück plus die
// Antwortverzögerung des Repeaters (300 ms Anfrage, 600 ms CLI). Begrenzt auf
// 6..45 s.
uint32_t replyTimeoutMs(uint32_t airtimeMs, bool flood, uint8_t hops);

// Ablauf einer Überwachungsabfrage: ggf. anmelden, Status, Telemetrie.
// - Schon angemeldet -> direkt Status. Bleibt der aus, war die Anmeldung
//   vermutlich weg (Repeater neu gestartet: Gäste stehen nur im RAM) ->
//   einmal neu anmelden und Status wiederholen.
// - Anmeldung ohne Antwort -> fehlgeschlagen (Passwort falsch oder Repeater
//   nicht erreichbar; der Repeater antwortet auf falsche Passwörter nicht).
// - Telemetrie ohne Antwort -> trotzdem fertig, nur ohne Temperatur.
struct PollFlow {
  enum Step : uint8_t { LOGIN, STATUS, TELEMETRY, DONE, FAILED };
  Step step = LOGIN;
  bool freshLogin = false;   // Anmeldung in diesem Ablauf erfolgt
  bool relogged = false;     // Neuanmeldung nach ausbleibendem Status schon versucht
  bool statusOk = false;
  bool telemetryOk = false;

  void start(bool loggedIn) {
    *this = PollFlow();
    step = loggedIn ? STATUS : LOGIN;
  }
  void onReply() {
    if (step == LOGIN) { freshLogin = true; step = STATUS; }
    else if (step == STATUS) { statusOk = true; step = TELEMETRY; }
    else if (step == TELEMETRY) { telemetryOk = true; step = DONE; }
  }
  // true = Sitzung als abgemeldet behandeln (Pfad vergessen, neu anmelden)
  bool onTimeout() {
    if (step == LOGIN) { step = FAILED; return true; }
    if (step == STATUS) {
      if (!freshLogin && !relogged) { relogged = true; step = LOGIN; return true; }
      step = FAILED;
      return true;
    }
    if (step == TELEMETRY) { step = DONE; return false; }
    return false;
  }
  bool finished() const { return step == DONE || step == FAILED; }
};

}  // namespace remote_fmt
