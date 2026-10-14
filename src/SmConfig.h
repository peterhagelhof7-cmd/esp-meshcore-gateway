#pragma once

#include <Arduino.h>

// Konfiguration der Sensormeter-Anbindung (M6, docs/entscheidungen.md E22).
// Persistenz als JSON-Blob in NVS (Namespace "gw"), unabhängig von MeshCore.
// Thread-sicher: Web-Task schreibt (setFromJson), Poller-Task liest (copy).

namespace sm {

static const int MAX_DEVICES = 8;
static const int MAX_RULES = 16;
static const int MAX_PINGS = 5;
static const uint16_t PING_INTERVAL_MIN_S = 10;    // Prüfabstand je Ziel: 10..3600 s
static const uint16_t PING_INTERVAL_MAX_S = 3600;
static const uint16_t PING_INTERVAL_DEF_S = 10;

struct Device {
  bool used;
  bool enabled;
  char name[24];
  char host[64];        // IP oder Hostname
  uint16_t port;
  char user[16];        // optional Basic-Auth (Sensormeter braucht für /api/sensors keine)
  char pass[32];
  uint16_t intervalS;   // Abfrageintervall
  bool offlineAlert;    // Meldung, wenn nicht erreichbar
};

struct Rule {
  bool used;
  bool enabled;
  int8_t device;        // Index in devices
  char path[32];        // z. B. "sensor1.temperature"
  uint8_t op;           // 0 = über (>), 1 = unter (<)
  float threshold;
  float hysteresis;
  uint32_t minDurationS;
  uint16_t repeatMin;   // 0 = keine Erinnerung
  char channel[32];     // Kanalname, leer = Standardkanal (Name statt Index, E24)
  bool notifyRecovery;
  char text[100];       // leer = Standardvorlage
};

// Ping-Check (E25): Ziel antwortet länger als thresholdS nicht -> Meldung in
// den Alarmkanal (= Standardkanal), bei Rückkehr Entwarnung.
struct PingTarget {
  bool used;
  bool enabled;
  char name[24];        // Bezeichner (Pflicht)
  char host[64];        // IP oder Hostname
  uint16_t thresholdS;  // max. Ausfallzeit, intervalS..65535 s
  uint16_t intervalS;   // Prüfabstand, PING_INTERVAL_MIN_S..PING_INTERVAL_MAX_S
};

struct Settings {
  bool enabled;
  char defaultChannel[32];    // Kanalname, leer = keiner
  uint8_t maxPerHour;         // Sendebegrenzung, 0 = nichts senden
  uint8_t offlineAfterFails;  // 0 = nie offline melden
  char alertText[100];
  char recoverText[100];
  char offlineText[100];
  char onlineText[100];
  char pingDownText[100];
  char pingUpText[100];
};

struct Config {
  Settings settings;
  Device devices[MAX_DEVICES];
  Rule rules[MAX_RULES];
  PingTarget pings[MAX_PINGS];
};

void begin();
// Kopie (für den Poller). Rückgabe: Generation (ändert sich bei jeder Änderung).
uint32_t copy(Config& out);
uint32_t generation();

// Komplette Konfiguration aus JSON übernehmen (Web). Passwort "***" = altes
// behalten. false = ungültig, err enthält den Grund.
bool setFromJson(const char* json, size_t len, String& err);
// Als JSON. maskPass = Passwörter als "***" (Standard). Unmaskiert nur fürs
// Backup mit Geheimnissen.
void toJson(String& out, bool maskPass = true);

// Standardvorlagen
extern const char* DEFAULT_ALERT;
extern const char* DEFAULT_RECOVER;
extern const char* DEFAULT_OFFLINE;
extern const char* DEFAULT_ONLINE;
extern const char* DEFAULT_PING_DOWN;
extern const char* DEFAULT_PING_UP;

}  // namespace sm
