#pragma once

#include <Arduino.h>

// Webkonsole -> offizielle MeshCore-CLI.
//
// Der Web-Task legt Befehle nur in eine FreeRTOS-Queue (submit). Der loopTask
// arbeitet sie in process() ab und ruft dort the_mesh.handleCommand() auf -
// genau wie die serielle CLI in simple_repeater/main.cpp, also mit
// sender_timestamp = 0. Damit gelten für die Webkonsole dieselben Rechte wie
// für die serielle Konsole, inkl. "Serial Only"-Befehle (erase, set freq,
// get prv.key, stats-*). Abgesichert ist das über den Web-Login
// (docs/entscheidungen.md E7/E11).
namespace cli_bridge {
  static const size_t MAX_CMD_LEN = 159;   // wie command[160] in simple_repeater

  bool begin();
  // Thread-sicher. false = Warteschlange voll.
  bool submit(const char* cmd);
  // Nur aus dem loopTask.
  void process();
}
