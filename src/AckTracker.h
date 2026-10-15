#pragma once

#include <stddef.h>
#include <stdint.h>

// Alarme mit Bestätigungspflicht ("aktiv bis bestätigt", E39).
// Ein Alarm bekommt eine kurze Nummer (1..99) und wird nach 5, 15 und 30 min
// (ab der ersten Meldung) wiederholt, bis jemand im selben Kanal mit "ACK"
// bzw. "ACK <Nr>" antwortet. Danach keine Wiederholung mehr; bestätigen geht
// noch bis 24 h nach der ersten Meldung.
// Bewusst ohne Arduino-Abhängigkeit -> auf dem PC testbar
// (test/host/ack_tracker_test.cpp). Zeit kommt immer von außen (nowMs).

namespace ack {

static const int MAX_OPEN = 8;
static const int REPEATS = 3;
static const uint32_t REPEAT_AFTER_MIN[REPEATS] = {5, 15, 30};
static const uint32_t MIN_GAP_MS = 5UL * 60UL * 1000UL;   // nach Sendepause nicht im Block nachholen
static const uint32_t KEEP_MS = 24UL * 3600UL * 1000UL;
static const size_t TEXT_LEN = 140;
static const size_t CHANNEL_LEN = 32;

enum class Kind : uint8_t { RULE, PING };

struct Entry {
  bool used = false;
  uint8_t id = 0;
  Kind kind = Kind::RULE;
  int8_t src = -1;              // Regel- bzw. Ping-Index
  char channel[CHANNEL_LEN] = {0};
  char text[TEXT_LEN] = {0};    // Meldung ohne Präfix "[ACK n] "
  uint32_t sentMs = 0;          // erste Meldung
  uint32_t lastMs = 0;          // letzte Meldung (erste oder Wiederholung)
  uint8_t repeats = 0;          // gesendete Wiederholungen
  bool cleared = false;         // Zustand inzwischen wieder normal
};

class Tracker {
public:
  // Neuer Alarm. Ein offener Eintrag derselben Quelle wird ersetzt (neue
  // Nummer). Ist alles belegt, fällt der älteste Eintrag weg. Rückgabe: Nummer.
  uint8_t open(Kind kind, int src, const char* channel, const char* text, uint32_t nowMs);
  // Zustand der Quelle wieder normal: Eintrag bleibt offen, Wiederholungen
  // tragen den Zusatz "inzw. OK".
  void markCleared(Kind kind, int src);
  // Offene Nummer einer Quelle, 0 = keine.
  uint8_t openIdOf(Kind kind, int src) const;

  // Index des nächsten fälligen Eintrags oder -1.
  int due(uint32_t nowMs);
  // Wiederholung von Eintrag idx wurde gesendet.
  void repeated(int idx, uint32_t nowMs);
  // Wiederholung konnte nicht gesendet werden (Sendelimit, Kanal fehlt):
  // frühestens nach MIN_GAP_MS erneut versuchen, Stufe bleibt.
  void postpone(int idx, uint32_t nowMs) { if (idx >= 0 && idx < MAX_OPEN) _e[idx].lastMs = nowMs; }

  // Antworttext auswerten (ohne "Absender: "). Bestätigt passende offene
  // Einträge im Kanal. Rückgabe: Anzahl, Nummern in ids. -1 = kein ACK-Text.
  int acknowledge(const char* channel, const char* body, uint8_t* ids, int maxIds);
  // Bestätigen über die Weboberfläche. false = Nummer unbekannt.
  bool acknowledgeId(uint8_t id);

  // Konfiguration geändert: map[alter Index] = neuer Index oder -1 (weg).
  void remap(Kind kind, const int8_t* map, int n);

  const Entry& at(int i) const { return _e[i]; }

  // "ACK", "ack 3", "ACK #3", "@[Name] ACK 3" -> true, id (0 = alle)
  static bool parseAck(const char* body, int& id);
  // Meldungstext einer Wiederholung bzw. der ersten Meldung bauen.
  static size_t format(const Entry& e, bool repeat, char* out, size_t outLen);

private:
  void drop(int i) { _e[i] = Entry(); }
  uint8_t nextId();
  Entry _e[MAX_OPEN];
  uint8_t _lastId = 0;
};

}  // namespace ack
