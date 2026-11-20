#include "Battery.h"

#include "EventLog.h"

static batt::Monitor s_mon;
static battery::Info s_info = {batt::State::Checking, batt::Reason::None, 0, 0, 0, -1};
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

void battery::sample(uint16_t mv) {
  batt::State before = s_mon.state();
  s_mon.add(mv);

  Info n;
  n.state = s_mon.state();
  n.reason = s_mon.reason();
  n.mv = s_mon.avgMv();
  n.lastMv = s_mon.lastMv();
  n.spreadMv = s_mon.spreadMv();
  n.percent = n.state == batt::State::Present ? (int8_t)batt::percentFromMv(n.mv) : -1;

  portENTER_CRITICAL(&s_mux);
  s_info = n;
  portEXIT_CRITICAL(&s_mux);

  if (n.state != before) {
    if (n.state == batt::State::Present) {
      event_log::addf("SYS", "AKKU vorhanden (%.2f V, ca. %d %%)", n.mv / 1000.0f, n.percent);
    } else if (n.state == batt::State::Absent) {
      event_log::addf("SYS", "kein AKKU (%s, %.2f V)", batt::reasonText(n.reason), n.mv / 1000.0f);
    }
  }
}

battery::Info battery::get() {
  portENTER_CRITICAL(&s_mux);
  Info c = s_info;
  portEXIT_CRITICAL(&s_mux);
  return c;
}
