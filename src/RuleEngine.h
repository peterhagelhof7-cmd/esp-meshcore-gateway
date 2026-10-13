#pragma once

#include <stddef.h>
#include <stdint.h>

// Grenzwert-Logik für Sensormeter-Alarme (M6, docs/entscheidungen.md E22).
// Bewusst ohne Arduino-Abhängigkeit -> auf dem PC testbar
// (test/host/rule_engine_test.cpp). Zeit kommt immer von außen (nowMs).

namespace rules {

enum class Op : uint8_t { ABOVE, BELOW };   // '>' bzw. '<'

struct RuleParams {
  Op op;
  float threshold;
  float hysteresis;      // >= 0; Rückkehr erst bei threshold -/+ hysteresis
  uint32_t minDurationS; // so lange muss die Verletzung anstehen
  uint32_t repeatMin;    // 0 = keine Erinnerung, sonst erneut melden solange verletzt
  bool notifyRecovery;
};

enum class State : uint8_t { NORMAL, PENDING, ALERT };

struct RuleState {
  State state = State::NORMAL;
  uint32_t pendingSinceMs = 0;
  uint32_t lastAlertMs = 0;
  bool hasValue = false;
  float lastValue = 0;
};

enum class Event : uint8_t { NONE, ALERT, REMIND, RECOVER };

// Neuen Messwert auswerten. valid=false (Wert fehlt/ungültig): Zustand bleibt.
Event evaluate(const RuleParams& p, RuleState& s, float value, bool valid, uint32_t nowMs);

// Gerät erreichbar/nicht erreichbar mit Schwelle (Anzahl Fehlversuche in Folge).
struct DeviceState {
  uint16_t fails = 0;
  bool offline = false;
  uint32_t offlineSinceMs = 0;
};
enum class DevEvent : uint8_t { NONE, OFFLINE, ONLINE };
DevEvent deviceResult(DeviceState& d, bool ok, uint16_t offlineAfterFails, uint32_t nowMs);

// Sendebegrenzung: höchstens maxPerHour Meldungen in einem gleitenden
// 60-Minuten-Fenster (Funkkanal schonen, Duty Cycle 10 %).
class RateLimiter {
public:
  static const int MAX_SLOTS = 60;
  void setMaxPerHour(int n) { _max = n < 0 ? 0 : (n > MAX_SLOTS ? MAX_SLOTS : n); }
  bool allow(uint32_t nowMs);   // true = senden erlaubt (und wird gezählt)
  int usedInWindow(uint32_t nowMs);
private:
  uint32_t _t[MAX_SLOTS] = {0};
  bool _used[MAX_SLOTS] = {false};
  int _max = 6;
};

// Textvorlage füllen: {device} {metric} {value} {unit} {op} {threshold} {state}
struct TemplateVars {
  const char* device;
  const char* metric;
  float value;
  bool hasValue;
  const char* unit;
  Op op;
  float threshold;
  const char* state;   // z. B. "ALARM", "OK", "ERINNERUNG"
};
size_t renderTemplate(const char* tpl, const TemplateVars& v, char* out, size_t outLen);

// Einheit zur Messgröße (letzter Pfadteil: temperature -> "°C" ...).
const char* unitFor(const char* path);
// Anzeigename der Messgröße (deutsch, kurz) - Pfad ohne Sensorpräfix als Fallback.
const char* labelFor(const char* path);

}  // namespace rules
