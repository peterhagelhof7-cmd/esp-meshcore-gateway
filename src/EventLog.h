#pragma once

#include <Arduino.h>

// Dauerhaftes Ereignis-Log auf SPIFFS (docs/entscheidungen.md E27).
//
// Flash-schonend: Zeilen sammeln sich in einem RAM-Puffer und werden nur
// gebündelt geschrieben (ab FLUSH_BYTES, spätestens nach FLUSH_INTERVAL_MS,
// vor jedem Neustart über einen Shutdown-Handler). Zwei Dateien zu je
// FILE_MAX_BYTES im Wechsel: Ist die aktuelle voll, wird sie zur alten und
// die bisherige alte gelöscht. Insgesamt also höchstens 2 x FILE_MAX_BYTES.
//
// Was hineinkommt: Systemmeldungen, Web-/serielle CLI-Befehle (Geheimnisse
// maskiert) mit Antwort, Sensormeter-/Ping-Meldungen, vom Gateway gesendete
// Kanalnachrichten, WLAN-Ereignisse. NICHT: Paketverkehr und empfangene
// Kanalnachrichten (zu viel Volumen für den Flash, bleiben im RAM-Log).
namespace event_log {
  static const size_t FILE_MAX_BYTES = 64 * 1024;
  static const size_t BUF_BYTES = 8 * 1024;        // RAM-Puffer (PSRAM)
  static const size_t FLUSH_BYTES = 4 * 1024;
  static const uint32_t FLUSH_INTERVAL_MS = 15UL * 60UL * 1000UL;

  // Nach SPIFFS.begin() aufrufen.
  void begin();
  // Thread-sicher. tag = kurze Kategorie (SYS, CMD, ...).
  void add(const char* tag, const char* text);
  void addf(const char* tag, const char* fmt, ...) __attribute__((format(printf, 2, 3)));
  // Nur aus dem loopTask: schreibt bei Bedarf (Größe/Zeit).
  void loop();
  // Sofort schreiben (z. B. vor einem Neustart).
  void flush();
  // Thread-sicher: beide Dateien + Puffer löschen.
  void clear();

  struct Info {
    uint32_t fileBytes;     // aktuelle + alte Datei
    uint32_t pendingBytes;  // noch im RAM
    uint32_t lastFlushAgoS; // 0xFFFFFFFF = noch nie
    uint32_t flushes;       // Schreibvorgänge seit Start
    uint32_t dropped;       // verworfene Zeilen (Puffer voll)
  };
  Info info();

  // Thread-sicher: alte Datei + aktuelle Datei + Puffer am Stück in einen
  // frisch geholten Speicher (bevorzugt PSRAM). Freigabe mit free().
  // nullptr = kein Speicher.
  char* snapshot(size_t& len);
}
