#pragma once

#include <Arduino.h>

// Empfohlene Repeater-Einstellungen nach hansemesh.de (Stand 2026-10-13),
// angepasst an diesen Standort (docs/entscheidungen.md E15).
//
// Jeder Eintrag ist ein offizieller CLI-Befehl. Zum Vergleich mit dem
// Ist-Zustand gibt es den passenden get-Befehl und den erwarteten Wert (so wie
// ihn die CLI ohne "> "-Präfix ausgibt). Einträge ohne get (z. B.
// "region save") werden beim Anwenden immer ausgeführt.
struct ProfileEntry {
  const char* cmd;
  const char* get;      // nullptr = nicht vergleichbar
  const char* expect;
  const char* why;
};

namespace profile {
  extern const char* NAME;
  extern const ProfileEntry ENTRIES[];
  extern const size_t COUNT;
}
