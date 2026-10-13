#pragma once

#include <stddef.h>
#include <stdint.h>

// Firmware-Erkennungsmerkmal für OTA (docs/entscheidungen.md E19).
//
// Jede Firmware enthält den Text
//   "MCGW-FW-ID:<PROJEKT-ID>:<VERSION>:MCGW-FW-END"
// Beim OTA-Upload wird der Datenstrom danach durchsucht: falsches Projekt
// (z. B. R8-Firmware auf V4) oder ältere Version -> Abbruch.
//
// Lehren aus der Sensormeter-Familie (Memory "OTA marker-scan"):
//   - nie String/strstr auf Binärdaten (bricht an der ersten NUL ab)
//   - Chunks nie in einen gedeckelten Puffer kopieren (große HTTP-Chunks)
// Deshalb: Byte-für-Byte-Zustandsautomat, chunkgrenzen- und NUL-sicher,
// keine Kopie der Daten. Bewusst ohne Arduino-Abhängigkeit -> auf dem PC
// testbar (test/host/fw_marker_test.cpp).
//
// Kein kryptografischer Schutz, nur gegen versehentlich falsche Dateien. Die
// Zugangskontrolle ist der Web-Login.
class FwMarkerScanner {
public:
  static const size_t MAX_FIELD = 48;

  void reset();
  void feed(const uint8_t* data, size_t len);
  bool found() const { return _found; }
  const char* projectId() const { return _id; }
  const char* version() const { return _ver; }

private:
  void restartAt(uint8_t c);
  bool finishBody();

  int _prefixPos = 0;          // wie viele Prefix-Zeichen passen
  bool _inBody = false;
  char _body[2 * MAX_FIELD + 16];
  size_t _bodyLen = 0;
  bool _found = false;
  char _id[MAX_FIELD + 1] = {0};
  char _ver[MAX_FIELD + 1] = {0};
};

// Vergleich "a.b.c" bzw. "a.b.c-rcN". Ergebnis <0, 0, >0 wie strcmp.
// Eine Version ohne -rc ist neuer als jede -rc derselben a.b.c.
// Nicht parsebare Versionen gelten als 0.0.0.
int fwVersionCompare(const char* a, const char* b);
