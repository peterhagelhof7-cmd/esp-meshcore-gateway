#include "RuleEngine.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

namespace rules {

static bool violated(const RuleParams& p, float v) {
  return p.op == Op::ABOVE ? v > p.threshold : v < p.threshold;
}

static bool recovered(const RuleParams& p, float v) {
  float h = p.hysteresis > 0 ? p.hysteresis : 0;
  return p.op == Op::ABOVE ? v <= p.threshold - h : v >= p.threshold + h;
}

Event evaluate(const RuleParams& p, RuleState& s, float value, bool valid, uint32_t nowMs) {
  if (!valid || isnan(value)) return Event::NONE;
  s.hasValue = true;
  s.lastValue = value;

  switch (s.state) {
    case State::NORMAL:
      if (violated(p, value)) {
        s.pendingSinceMs = nowMs;
        if (p.minDurationS == 0) {
          s.state = State::ALERT;
          s.lastAlertMs = nowMs;
          return Event::ALERT;
        }
        s.state = State::PENDING;
      }
      return Event::NONE;

    case State::PENDING:
      if (!violated(p, value)) {
        // Kurzer Ausreißer: zurück, ohne Meldung. (Hysterese gilt erst ab ALERT.)
        s.state = State::NORMAL;
        return Event::NONE;
      }
      if (nowMs - s.pendingSinceMs >= p.minDurationS * 1000UL) {
        s.state = State::ALERT;
        s.lastAlertMs = nowMs;
        return Event::ALERT;
      }
      return Event::NONE;

    case State::ALERT:
      if (recovered(p, value)) {
        s.state = State::NORMAL;
        return p.notifyRecovery ? Event::RECOVER : Event::NONE;
      }
      if (p.repeatMin > 0 && nowMs - s.lastAlertMs >= p.repeatMin * 60000UL) {
        s.lastAlertMs = nowMs;
        return Event::REMIND;
      }
      return Event::NONE;
  }
  return Event::NONE;
}

DevEvent deviceResult(DeviceState& d, bool ok, uint16_t offlineAfterFails, uint32_t nowMs) {
  if (ok) {
    d.fails = 0;
    if (d.offline) {
      d.offline = false;
      return DevEvent::ONLINE;
    }
    return DevEvent::NONE;
  }
  if (d.fails < 0xFFFF) d.fails++;
  if (!d.offline && offlineAfterFails > 0 && d.fails >= offlineAfterFails) {
    d.offline = true;
    d.offlineSinceMs = nowMs;
    return DevEvent::OFFLINE;
  }
  return DevEvent::NONE;
}

PingEvent pingResult(PingState& p, bool ok, uint32_t thresholdS, uint32_t nowMs) {
  if (!p.started) {
    p.started = true;
    p.lastOkMs = nowMs;   // Ausfallzeit zählt ab dem ersten Versuch
  }
  if (ok) {
    p.lastOkMs = nowMs;
    p.everOk = true;
    if (p.down) {
      p.down = false;
      return PingEvent::UP;
    }
    return PingEvent::NONE;
  }
  if (!p.down && nowMs - p.lastOkMs >= thresholdS * 1000UL) {
    p.down = true;
    return PingEvent::DOWN;
  }
  return PingEvent::NONE;
}

uint32_t pingDownSeconds(const PingState& p, uint32_t nowMs) {
  return p.started ? (nowMs - p.lastOkMs) / 1000UL : 0;
}

bool RateLimiter::allow(uint32_t nowMs) {
  if (_max <= 0) return false;
  int used = 0, freeSlot = -1;
  for (int i = 0; i < MAX_SLOTS; i++) {
    if (_used[i] && nowMs - _t[i] >= 3600000UL) _used[i] = false;   // aus dem Fenster gefallen
    if (_used[i]) used++;
    else if (freeSlot < 0) freeSlot = i;
  }
  if (used >= _max || freeSlot < 0) return false;
  _used[freeSlot] = true;
  _t[freeSlot] = nowMs;
  return true;
}

int RateLimiter::usedInWindow(uint32_t nowMs) {
  int used = 0;
  for (int i = 0; i < MAX_SLOTS; i++) {
    if (_used[i] && nowMs - _t[i] < 3600000UL) used++;
  }
  return used;
}

static void fmtNum(char* buf, size_t len, float v) {
  // Eine Nachkommastelle reicht für alle Sensormeter-Größen, ganze Zahlen ohne.
  if (fabsf(v - roundf(v)) < 0.05f && fabsf(v) < 1e6f) snprintf(buf, len, "%.0f", v);
  else snprintf(buf, len, "%.1f", v);
}

size_t renderTemplate(const char* tpl, const TemplateVars& v, char* out, size_t outLen) {
  if (!out || outLen == 0) return 0;
  size_t o = 0;
  auto put = [&](const char* s) {
    while (*s && o + 1 < outLen) out[o++] = *s++;
  };
  char num[24];
  for (const char* p = tpl ? tpl : ""; *p && o + 1 < outLen;) {
    if (*p == '{') {
      const char* e = strchr(p, '}');
      if (e && e - p < 16) {
        char key[16];
        size_t kl = e - p - 1;
        memcpy(key, p + 1, kl);
        key[kl] = 0;
        bool known = true;
        if (!strcmp(key, "device")) put(v.device ? v.device : "");
        else if (!strcmp(key, "metric")) put(v.metric ? v.metric : "");
        else if (!strcmp(key, "value")) { if (v.hasValue) { fmtNum(num, sizeof(num), v.value); put(num); } else put("?"); }
        else if (!strcmp(key, "unit")) put(v.unit ? v.unit : "");
        else if (!strcmp(key, "op")) put(v.op == Op::ABOVE ? ">" : "<");
        else if (!strcmp(key, "threshold")) { fmtNum(num, sizeof(num), v.threshold); put(num); }
        else if (!strcmp(key, "state")) put(v.state ? v.state : "");
        else if (!strcmp(key, "host")) put(v.host ? v.host : "");
        else known = false;
        if (known) { p = e + 1; continue; }
      }
    }
    out[o++] = *p++;
  }
  out[o] = 0;
  return o;
}

static const char* lastPart(const char* path) {
  const char* d = strrchr(path, '.');
  return d ? d + 1 : path;
}

const char* unitFor(const char* path) {
  const char* k = lastPart(path);
  if (!strcmp(k, "temperature")) return "\xC2\xB0" "C";   // °C (UTF-8)
  if (!strcmp(k, "humidity")) return "%";
  if (!strcmp(k, "pressureHpa")) return "hPa";
  if (!strcmp(k, "lux")) return "lx";
  if (!strcmp(k, "eco2Ppm")) return "ppm";
  return "";
}

const char* labelFor(const char* path) {
  const char* k = lastPart(path);
  if (!strcmp(k, "temperature")) return "Temperatur";
  if (!strcmp(k, "humidity")) return "Feuchte";
  if (!strcmp(k, "pressureHpa")) return "Luftdruck";
  if (!strcmp(k, "lux")) return "Helligkeit";
  if (!strcmp(k, "eco2Ppm")) return "eCO2";
  return k;
}

}  // namespace rules
