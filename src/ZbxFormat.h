#pragma once

#include <stddef.h>
#include <stdint.h>

// Zabbix-Anbindung (E41): maschinenlesbare Kennung in Alarmtexten und das
// Zabbix-Sender-Protokoll (Trapper). Ohne Arduino-Abhängigkeit -> auf dem PC
// testbar (test/host/zbx_format_test.cpp).
//
// Kennung am Textende, Version Z1:
//   "<Text> #Z1;<Quelle>;<Ereignis>;<Wert>;<ACK-Nr>"
//   Quelle   R/P/D/G + 4 Hex (Regel, Ping-Ziel, Gerät offline, Gateway selbst),
//            stabil über einen Hash, damit Zabbix je Quelle ein Item anlegen kann
//   Ereignis A Alarm, E Erinnerung, W Wiederholung, O Entwarnung/OK,
//            F offline, N online, H Lebenszeichen, T Test
//   Wert     Zahl oder leer, ACK-Nr 1..99 oder leer
namespace zbx {

static const char* const MARK = " #Z1;";
static const size_t MARK_MAX = 40;   // längste Kennung inkl. Leerzeichen

struct Marker {
  char src[8];       // z. B. "R3fa2"
  char ev;           // A E W O F N H T
  bool hasValue;
  float value;
  uint8_t ack;       // 0 = keine
};

// CRC-16/CCITT, für die Quellkennung
uint16_t crc16(const char* s);
// Quellkennung aus Typbuchstabe und Bestandteilen (z. B. Gerätename|Pfad|op)
void makeSrc(char kind, const char* key, char out[8]);

// Kennung bauen (mit führendem Leerzeichen). Rückgabe Länge.
size_t buildMarker(const Marker& m, char* out, size_t outLen);
// Text + Kennung so zusammensetzen, dass das Ganze in maxLen passt: der Text
// wird nötigenfalls gekürzt, die Kennung nie.
size_t appendMarker(const char* text, const Marker& m, char* out, size_t maxLen);

// Kennung im Text suchen (letztes Vorkommen). textLen = Länge des Textes ohne
// Kennung. false = keine (gültige) Kennung.
bool parseMarker(const char* body, Marker& m, size_t& textLen);

// "ALARM" / "OK" / "INFO" für Zabbix-Trigger
const char* stateOf(char ev);

// --- Zabbix-Sender-Protokoll ---
// Kopf: "ZBXD" 0x01 + Länge (uint64 little endian) vor dem JSON
static const size_t HEADER_LEN = 13;
void writeHeader(uint8_t out[HEADER_LEN], uint32_t jsonLen);
// Länge aus dem Antwortkopf, -1 = kein gültiger Kopf
long readHeader(const uint8_t* in, size_t len);
// Antwort-JSON auswerten: {"response":"success","info":"processed: 1; failed: 0; ..."}
bool parseResponse(const char* json, int& processed, int& failed);

// JSON-String escapen (für Werte im Sender-Request)
size_t jsonEscape(const char* in, char* out, size_t outLen);

}  // namespace zbx
