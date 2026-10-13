#pragma once

#include <Arduino.h>

class AsyncWebServerRequest;

// Firmware-Update per Web-Upload (M5, docs/entscheidungen.md E19).
//
// - Upload streamt direkt in die freie OTA-Partition (Update.h)
// - der Datenstrom wird nach dem Firmware-Merkmal durchsucht (FwMarker):
//   gleiche Projekt-ID (Board-Variante!) Pflicht, ältere Version nur mit
//   "Downgrade erzwingen"
// - Rollback: neue Firmware gilt erst nach OTA_VALID_AFTER_MS stabiler
//   Laufzeit als gültig (verifyRollbackLater + mark_valid). Stürzt sie vorher
//   ab, startet der Bootloader wieder die alte.
namespace ota_update {
  static const uint32_t OTA_VALID_AFTER_MS = 60000;

  void begin();
  // ESPAsyncWebServer-Upload-Callback (läuft im AsyncTCP-Task)
  void onUpload(AsyncWebServerRequest* req, const String& filename, size_t index,
                uint8_t* data, size_t len, bool final);
  // Request-Abschluss: Ergebnis senden. true = erfolgreich, Neustart nötig.
  bool onRequestDone(AsyncWebServerRequest* req);
  // loopTask: Firmware nach stabiler Laufzeit als gültig markieren.
  void loop();

  const char* projectId();
  const char* runningPartition();
  const char* imageState();   // "valid", "pending-verify", ...
}
