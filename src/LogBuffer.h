#pragma once

#include <Arduino.h>

// Thread-sicherer Ringpuffer für Textzeilen mit fortlaufender Sequenznummer.
// Schreiber: loopTask (CLI-Antworten, Paket-Hooks). Leser: Web-Task, der per
// "since"-Sequenz nur neue Zeilen abholt (docs/entscheidungen.md E10).
class LogBuffer {
public:
  static const size_t LINE_LEN = 256;

  struct Line {
    uint32_t seq;
    uint32_t uptimeMs;
    uint32_t epoch;     // RTC-Zeit des Knotens (vor NTP-Sync ggf. falsch)
    char type;          // c=Befehl, r=Antwort, s=System, R=RX, T=TX, F=TX-Fehler,
                        // M=Kanal empfangen, S=Kanal gesendet
    char text[LINE_LEN];
  };

  explicit LogBuffer(size_t capacity);
  bool begin();   // Speicher (bevorzugt PSRAM) holen

  void push(char type, uint32_t epoch, const char* text);
  // Wie push(), teilt aber an Zeilenumbrüchen und zu langen Zeilen in
  // mehrere Einträge.
  void pushLines(char type, uint32_t epoch, const char* text);

  // Hängt alle Zeilen mit seq >= since als JSON-Array-Elemente an 'out' an
  // (max. maxLines). Rückgabe: nächste abzufragende Sequenz.
  // 'gap' = true, wenn Zeilen zwischen since und der ältesten noch
  // vorhandenen Zeile schon überschrieben wurden.
  uint32_t appendJson(uint32_t since, size_t maxLines, String& out, bool& gap);

private:
  size_t _cap;
  Line* _lines = nullptr;
  uint32_t _nextSeq = 1;
  SemaphoreHandle_t _lock = nullptr;
};

extern LogBuffer console_log;   // CLI-Befehle, Antworten, Systemmeldungen
extern LogBuffer packet_log;    // RX/TX-Pakete
extern LogBuffer alert_log;     // Sensormeter-Meldungen: A=gesendet, X=unterdrückt/Fehler, S=System
extern LogBuffer channel_log;   // Kanalnachrichten: M=empfangen, S=gesendet
