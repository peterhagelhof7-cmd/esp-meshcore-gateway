#pragma once

#include <Arduino.h>

// Sensormeter-Abfrage + Regelauswertung (M6, docs/entscheidungen.md E22).
//
// Eigener FreeRTOS-Task auf Core 0: HTTP-Abfragen blockieren bis zu einigen
// Sekunden und dürfen den Mesh-Loop (Core 1) nie aufhalten. Meldungen gehen
// über mesh_actions (Queue, E4) an den Mesh-Loop, vorher greift die
// Sendebegrenzung (max. N pro Stunde).
namespace sm_poller {
  void begin();

  // Thread-sicher, für die Weboberfläche.
  void statusJson(String& out);
  bool pollNow(int deviceIdx);
  // Testnachricht einer Regel mit aktuellem Wert (zählt nicht zum Limit).
  bool sendTest(int ruleIdx, String& err);
  // Chiptemperatur des ESP32-S3 in °C (alle 60 s gemessen), NAN = noch keine.
  float chipTempC();
}
