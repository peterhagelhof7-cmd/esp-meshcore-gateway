#pragma once

#include <Arduino.h>

#include "BatteryLogic.h"

// Akku am internen Stecker (E46): Messung im loopTask (alle 5 s mit dem
// Status-Snapshot), Lesen thread-sicher aus Web-, UI- und Poller-Task.
namespace battery {

struct Info {
  batt::State state;
  batt::Reason reason;
  uint16_t mv;        // Mittel über das Fenster
  uint16_t lastMv;    // letzte Einzelmessung
  uint16_t spreadMv;
  int8_t percent;     // -1 = kein Akku / unbekannt
};

// Nur aus dem loopTask: eine Messung (board.getBattMilliVolts()) übernehmen.
void sample(uint16_t mv);
Info get();

}  // namespace battery
