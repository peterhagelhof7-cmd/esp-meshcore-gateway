#include "FwMarker.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Die Konstanten stehen selbst im Binary ("MCGW-FW-ID:" gefolgt von NUL).
// Das ist kein Problem: der Automat verlangt danach gültige Feldzeichen und
// das Endmerkmal, eine NUL bricht den Kandidaten ab.
static const char PREFIX[] = "MCGW-FW-ID:";
static const char SUFFIX[] = ":MCGW-FW-END";
static const size_t PREFIX_LEN = sizeof(PREFIX) - 1;
static const size_t SUFFIX_LEN = sizeof(SUFFIX) - 1;

static bool isFieldChar(uint8_t c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
         c == '.' || c == '-' || c == '_' || c == '+' || c == ':';
}

void FwMarkerScanner::reset() {
  _prefixPos = 0;
  _inBody = false;
  _bodyLen = 0;
  _found = false;
  _id[0] = 0;
  _ver[0] = 0;
}

void FwMarkerScanner::restartAt(uint8_t c) {
  // 'M' kommt im Prefix nur an Position 0 vor -> einfacher Neustart genügt.
  _inBody = false;
  _bodyLen = 0;
  _prefixPos = (c == (uint8_t)PREFIX[0]) ? 1 : 0;
}

bool FwMarkerScanner::finishBody() {
  // _body = "<id>:<version>" (Suffix schon abgeschnitten)
  _body[_bodyLen] = 0;
  char* sep = strchr(_body, ':');
  if (!sep) return false;
  *sep = 0;
  const char* id = _body;
  const char* ver = sep + 1;
  if (!*id || !*ver || strchr(ver, ':')) return false;
  if (strlen(id) > MAX_FIELD || strlen(ver) > MAX_FIELD) return false;
  strcpy(_id, id);
  strcpy(_ver, ver);
  return true;
}

void FwMarkerScanner::feed(const uint8_t* data, size_t len) {
  if (_found) return;   // erstes gültiges Merkmal zählt
  for (size_t i = 0; i < len; i++) {
    uint8_t c = data[i];
    if (!_inBody) {
      if (c == (uint8_t)PREFIX[_prefixPos]) {
        if (++_prefixPos == (int)PREFIX_LEN) {
          _inBody = true;
          _bodyLen = 0;
        }
      } else {
        restartAt(c);
      }
      continue;
    }

    if (!isFieldChar(c) && !(c >= 'A' && c <= 'Z')) {
      restartAt(c);
      continue;
    }
    if (_bodyLen >= sizeof(_body) - 1) {
      restartAt(c);
      continue;
    }
    _body[_bodyLen++] = (char)c;
    if (_bodyLen > SUFFIX_LEN && memcmp(_body + _bodyLen - SUFFIX_LEN, SUFFIX, SUFFIX_LEN) == 0) {
      _bodyLen -= SUFFIX_LEN;
      if (finishBody()) {
        _found = true;
        return;
      }
      restartAt(c);
    }
  }
}

// ---------------------------------------------------------------------------

struct Ver {
  long major, minor, patch;
  long rc;   // -1 = keine RC (Release)
};

static Ver parseVer(const char* s) {
  Ver v = {0, 0, 0, -1};
  if (!s) return v;
  if (*s == 'v' || *s == 'V') s++;
  char* end;
  v.major = strtol(s, &end, 10);
  if (*end != '.') return Ver{0, 0, 0, -1};
  v.minor = strtol(end + 1, &end, 10);
  if (*end != '.') return Ver{0, 0, 0, -1};
  v.patch = strtol(end + 1, &end, 10);
  if (*end == '-') {
    const char* r = end + 1;
    if (strncmp(r, "rc", 2) == 0) v.rc = strtol(r + 2, nullptr, 10);
    else v.rc = 0;   // andere Vorabversion wie rc0 behandeln
  }
  return v;
}

int fwVersionCompare(const char* a, const char* b) {
  Ver x = parseVer(a), y = parseVer(b);
  if (x.major != y.major) return x.major < y.major ? -1 : 1;
  if (x.minor != y.minor) return x.minor < y.minor ? -1 : 1;
  if (x.patch != y.patch) return x.patch < y.patch ? -1 : 1;
  if (x.rc == y.rc) return 0;
  if (x.rc == -1) return 1;    // Release > RC
  if (y.rc == -1) return -1;
  return x.rc < y.rc ? -1 : 1;
}
