#include "BatteryLogic.h"

namespace batt {

int percentFromMv(uint16_t mv) {
  // Stützpunkte Ruhespannung -> Ladezustand (typische 1S-LiPo-Kurve)
  static const uint16_t MV[] = {4200, 4150, 4110, 4080, 4020, 3980, 3950, 3910, 3870, 3850, 3840,
                                3820, 3800, 3790, 3770, 3750, 3730, 3710, 3690, 3610, 3270};
  static const uint8_t PCT[] = {100, 95, 90, 85, 80, 75, 70, 65, 60, 55, 50,
                                45, 40, 35, 30, 25, 20, 15, 10, 5, 0};
  static const int N = sizeof(MV) / sizeof(MV[0]);
  if (mv >= MV[0]) return 100;
  if (mv <= MV[N - 1]) return 0;
  for (int i = 1; i < N; i++) {
    if (mv >= MV[i]) {
      // linear zwischen den Stützpunkten i-1 (höher) und i
      int span = MV[i - 1] - MV[i];
      int p = PCT[i] + (int)(PCT[i - 1] - PCT[i]) * (mv - MV[i]) / span;
      return p;
    }
  }
  return 0;
}

uint16_t Monitor::avgMv() const {
  if (!_n) return 0;
  uint32_t sum = 0;
  for (int i = 0; i < _n; i++) sum += _buf[i];
  return (uint16_t)(sum / _n);
}

uint16_t Monitor::spreadMv() const {
  if (!_n) return 0;
  uint16_t lo = _buf[0], hi = _buf[0];
  for (int i = 1; i < _n; i++) {
    if (_buf[i] < lo) lo = _buf[i];
    if (_buf[i] > hi) hi = _buf[i];
  }
  return hi - lo;
}

void Monitor::evaluate(State& s, Reason& r) const {
  uint16_t avg = avgMv();
  if (avg < LOW_MV) { s = State::Absent; r = Reason::TooLow; return; }
  if (avg > HIGH_MV) { s = State::Absent; r = Reason::TooHigh; return; }
  if (spreadMv() > SPREAD_MV) { s = State::Absent; r = Reason::Unstable; return; }
  s = State::Present;
  r = Reason::None;
}

void Monitor::add(uint16_t mv) {
  _last = mv;
  _buf[_pos] = mv;
  _pos = (_pos + 1) % WINDOW;
  if (_n < WINDOW) _n++;
  if (_n < MIN_SAMPLES) return;

  State s;
  Reason r;
  evaluate(s, r);
  if (_state == State::Checking) {   // erste Entscheidung sofort
    _state = s;
    _reason = r;
    _cand = s;
    _candCount = 0;
    return;
  }
  if (s == _state) {
    _reason = r;          // Begründung darf sich innerhalb "kein AKKU" ändern
    _candCount = 0;
    return;
  }
  if (s != _cand) {
    _cand = s;
    _candReason = r;
    _candCount = 0;
  }
  _candReason = r;
  if (++_candCount >= DEBOUNCE) {
    _state = _cand;
    _reason = _candReason;
    _candCount = 0;
  }
}

const char* stateText(State s) {
  switch (s) {
    case State::Present: return "AKKU vorhanden";
    case State::Absent: return "kein AKKU";
    default: return "wird gepr\xC3\xBC" "ft";   // ü (UTF-8)
  }
}

const char* stateKey(State s) {
  switch (s) {
    case State::Present: return "present";
    case State::Absent: return "absent";
    default: return "checking";
  }
}

const char* reasonText(Reason r) {
  switch (r) {
    case Reason::TooLow: return "Spannung zu niedrig";
    case Reason::TooHigh: return "Spannung zu hoch (nur Ladeausgang)";
    case Reason::Unstable: return "Spannung schwankt (Lader ohne Akku)";
    default: return "";
  }
}

}  // namespace batt
