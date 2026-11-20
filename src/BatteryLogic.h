#pragma once

#include <stdint.h>

// Akku-Erkennung (docs/entscheidungen.md E46), ohne Arduino-Abhängigkeiten,
// damit sie auf dem Host getestet werden kann (test/host/battery_test.cpp).
//
// Der V4 misst VBAT über einen Spannungsteiler (GPIO1, Freigabe GPIO37). Ohne
// Akku hängt an diesem Knoten nur der Ausgang des Laderegler-ICs: Er liefert
// dann entweder (fast) nichts, eine Spannung über der Ladeschlussspannung eines
// LiPo oder springt, weil der Lader ohne Last ständig neu startet. Ein
// angeschlossener Akku hält die Spannung dagegen ruhig zwischen etwa 3,0 und
// 4,25 V. Genau das prüft die Erkennung über ein gleitendes Fenster.
namespace batt {

enum class State : uint8_t { Checking, Present, Absent };
enum class Reason : uint8_t { None, TooLow, TooHigh, Unstable };

// Grenzwerte: aus dem Verhalten eines 1S-LiPo abgeleitet, am echten Gerät noch
// zu bestätigen (docs/hw-testplan.md Abschnitt 15).
static const int WINDOW = 12;            // Messungen im Fenster (alle 5 s = 1 min)
static const int MIN_SAMPLES = 3;        // vorher "wird geprüft"
static const int DEBOUNCE = 3;           // so oft hintereinander, bevor der Zustand wechselt
static const uint16_t LOW_MV = 2500;     // darunter: kein Akku (leer gelaufener LiPo schaltet bei ~2,5-3,0 V ab)
static const uint16_t HIGH_MV = 4350;    // darüber: kein Akku (LiPo lädt bis 4,20 V, Toleranz der Messung)
static const uint16_t SPREAD_MV = 250;   // größere Schwankung im Fenster: kein Akku

// Grobe Ladezustandsschätzung eines 1S-LiPo aus der Ruhespannung (0..100).
int percentFromMv(uint16_t mv);

class Monitor {
 public:
  void add(uint16_t mv);
  State state() const { return _state; }
  Reason reason() const { return _reason; }
  uint16_t avgMv() const;
  uint16_t lastMv() const { return _last; }
  uint16_t spreadMv() const;
  int count() const { return _n; }

 private:
  void evaluate(State& s, Reason& r) const;
  uint16_t _buf[WINDOW] = {};
  int _n = 0, _pos = 0;
  uint16_t _last = 0;
  State _state = State::Checking;
  Reason _reason = Reason::None;
  State _cand = State::Checking;
  Reason _candReason = Reason::None;
  int _candCount = 0;
};

const char* stateText(State s);    // "AKKU vorhanden" / "kein AKKU" / "wird geprüft"
const char* stateKey(State s);     // "present" / "absent" / "checking" (API)
const char* reasonText(Reason r);  // Begründung für "kein AKKU"

}  // namespace batt
