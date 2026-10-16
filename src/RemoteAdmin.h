#pragma once

#include <Arduino.h>

// Fernzugriff auf andere Repeater (E42): Fernkonsole und Fernüberwachung.
//
// Das Gateway verhält sich dabei wie die MeshCore-App: Anmeldung per
// ANON_REQ mit Passwort, danach verschlüsselte Anfragen (REQ: Status,
// Telemetrie) und CLI-Befehle (TXT_MSG, TXT_TYPE_CLI_DATA). Der Repeater
// antwortet zuerst per Flood (mit Pfad), danach direkt.
//
// Ablauf über Aufträge (Jobs): Web-Task und Poller-Task legen Aufträge ab,
// der loopTask arbeitet sie nacheinander ab (immer nur einer unterwegs, schont
// die Funkzeit) - wie cli_bridge. Alle Zustände liegen im PSRAM, nur im RAM:
// nach einem Neustart meldet sich das Gateway bei Bedarf neu an.
//
// Empfang: GatewayMesh leitet Pakete von Sitzungs-Zielen hierher
// (searchPeersByHash & Co.); was nicht zu einem laufenden Auftrag passt, geht
// an den Repeater-Code von MeshCore weiter.
namespace remote {

static const int MAX_KNOWN = 32;
static const size_t MAX_CMD = 159;
static const size_t MAX_REPLY = 191;
static const size_t MAX_PASS = 15;          // MeshCore nimmt höchstens 15 Zeichen
static const uint32_t CONSOLE_IDLE_MS = 15UL * 60 * 1000;

// Aus Adverts bekannte Repeater/Room-Server (für die Auswahl in der Oberfläche)
struct Known {
  uint8_t key[32];
  char name[32];
  uint8_t type;      // ADV_TYPE_REPEATER = 2, ADV_TYPE_ROOM = 3
  uint8_t hops;
  float snr;
  uint32_t heardMs;  // millis() des letzten Adverts
};

enum class Kind : uint8_t { LOGIN, CLI, POLL };
enum class State : uint8_t { QUEUED, RUNNING, DONE, FAILED };

struct PollData {
  bool hasStatus;
  uint16_t battMv;
  int16_t noiseFloor;
  uint32_t uptimeS;
  bool hasErr;
  uint16_t errFlags;
  bool hasTelemetry;
  bool hasVoltage;
  float voltage;
  bool hasTemp;
  float tempC;
};

struct JobInfo {
  State state;
  Kind kind;
  bool flood;          // letzte Anfrage per Flood (sonst direkt)
  uint8_t hops;        // bei direkt: Länge des Pfads
  uint32_t ms;         // Dauer bis zur Antwort bzw. bis jetzt
  bool admin;          // LOGIN: als Admin angemeldet
  bool hidden;         // CLI: Antwort enthält ein Geheimnis (nicht ins Log)
  char error[72];
  char reply[MAX_REPLY + 1];
  PollData poll;
};

struct ConsoleInfo {
  bool active;         // Ziel gewählt und angemeldet
  uint8_t key[32];
  char name[32];
  bool admin;
  bool flood;
  uint8_t hops;
  uint32_t idleS;
  bool busy;           // Befehl unterwegs
};

bool begin();

// --- Thread-sicher (Web, Poller) ---
int known(Known* out, int max);   // zuletzt gehörte zuerst
bool nameOf(const uint8_t key[32], char* out, size_t n);

// Rückgabe: Auftrags-id (> 0) oder 0 mit Grund in err.
uint32_t submitLogin(const uint8_t key[32], const char* pass, String& err);   // Fernkonsole
uint32_t submitCli(const char* cmd, String& err);                             // an das Konsolen-Ziel
uint32_t submitPoll(const uint8_t key[32], const char* pass, String& err);    // Überwachung
bool job(uint32_t id, JobInfo& out);   // false = unbekannt (zu alt)
void logout();
void consoleInfo(ConsoleInfo& out);

// --- Nur aus dem loopTask ---
void process();

// Hooks aus GatewayMesh (loopTask)
void onAdvert(const uint8_t* key, const char* name, uint8_t type, uint8_t hops, float snr);
int matchHash(uint8_t hash, int* sessOut, int max);
void secretOf(int sess, uint8_t* out);
// true = Paket gehörte zu einem laufenden Auftrag (verbraucht)
bool onData(int sess, bool flood, uint8_t type, const uint8_t* data, size_t len);
void onPath(int sess, const uint8_t* path, uint8_t pathLen, uint8_t extraType, const uint8_t* extra, size_t extraLen);

}  // namespace remote
