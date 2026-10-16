#include "RemoteFormat.h"

#include <string.h>

namespace remote_fmt {

static uint16_t le16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t le32(const uint8_t* p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

// Versätze in struct RepeaterStats (MyMesh.h)
static const size_t OFS_BATT = 0, OFS_TXQ = 2, OFS_NOISE = 4, OFS_UPTIME = 20, OFS_ERR = 40;

Status parseStatus(const uint8_t* data, size_t len) {
  Status s;
  memset(&s, 0, sizeof(s));
  if (!data || len < OFS_UPTIME + 4) return s;   // ohne Laufzeit unbrauchbar
  s.ok = true;
  s.battMv = le16(data + OFS_BATT);
  s.txQueue = le16(data + OFS_TXQ);
  s.noiseFloor = (int16_t)le16(data + OFS_NOISE);
  s.uptimeS = le32(data + OFS_UPTIME);
  if (len >= OFS_ERR + 2) {
    s.errFlags = le16(data + OFS_ERR);
    s.hasErrFlags = true;
  }
  return s;
}

// Größe des Werts je CayenneLPP-Typ (CayenneLPP.h), 0 = unbekannt
static int lppSize(uint8_t type) {
  switch (type) {
    case 0: case 1: case 102: case 104: case 120: case 142: return 1;   // digital in/out, presence, humidity, percentage, switch
    case 2: case 3: case 101: case 103: case 115: case 116: case 117:   // analog, lux, temp, pressure, voltage, current
    case 121: case 125: case 128: case 132: return 2;                   // altitude, concentration, power, direction
    case 135: return 3;                                                 // colour
    case 100: case 118: case 130: case 131: case 133: return 4;         // generic, frequency, distance, energy, unixtime
    case 113: case 134: return 6;                                       // accelerometer, gyrometer
    case 136: return 9;                                                 // gps
    default: return 0;
  }
}

Telemetry parseTelemetry(const uint8_t* data, size_t len) {
  Telemetry t;
  memset(&t, 0, sizeof(t));
  if (!data) return t;
  size_t i = 0;
  while (i + 2 <= len) {
    uint8_t ch = data[i], type = data[i + 1];
    if (ch == 0 && type == 0) break;   // Auffüllung mit Nullen am Ende
    int n = lppSize(type);
    if (n == 0) { t.unknownTypes++; break; }   // Länge unbekannt -> Rest nicht lesbar
    if (i + 2 + n > len) break;
    const uint8_t* v = data + i + 2;
    if (ch == SELF_CHANNEL) {
      if (type == 116 && !t.hasVoltage) {            // 0,01 V, ohne Vorzeichen
        t.voltage = ((v[0] << 8) | v[1]) / 100.0f;
        t.hasVoltage = true;
      } else if (type == 103 && !t.hasTemp) {        // 0,1 °C, mit Vorzeichen
        t.tempC = (int16_t)((v[0] << 8) | v[1]) / 10.0f;
        t.hasTemp = true;
      }
    }
    i += 2 + n;
  }
  return t;
}

Login parseLogin(const uint8_t* data, size_t len) {
  Login l;
  memset(&l, 0, sizeof(l));
  if (!data || len < 8 || data[4] != RESP_LOGIN_OK) return l;
  l.ok = true;
  l.admin = data[6] != 0;
  l.perms = data[7];
  return l;
}

static int hexVal(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

bool keyFromHex(const char* hex, uint8_t key[32]) {
  if (!hex || strlen(hex) != 64) return false;
  for (int i = 0; i < 32; i++) {
    int h = hexVal(hex[2 * i]), l = hexVal(hex[2 * i + 1]);
    if (h < 0 || l < 0) return false;
    key[i] = (uint8_t)((h << 4) | l);
  }
  return true;
}

void keyToHex(const uint8_t key[32], char out[65]) {
  static const char* d = "0123456789abcdef";
  for (int i = 0; i < 32; i++) {
    out[2 * i] = d[key[i] >> 4];
    out[2 * i + 1] = d[key[i] & 15];
  }
  out[64] = 0;
}

uint32_t replyTimeoutMs(uint32_t airtimeMs, bool flood, uint8_t hops) {
  uint32_t t;
  if (flood) t = 2 * (500 + 16 * airtimeMs);
  else t = 2 * (500 + (6 * airtimeMs + 250) * (hops + 1));
  t += 1000;   // Antwortverzögerung des Repeaters + Reserve
  if (t < 6000) t = 6000;
  if (t > 45000) t = 45000;
  return t;
}

}  // namespace remote_fmt
