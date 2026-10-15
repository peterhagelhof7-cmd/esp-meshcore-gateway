#include "ZbxFormat.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace zbx {

uint16_t crc16(const char* s) {
  uint16_t crc = 0xFFFF;
  for (; s && *s; s++) {
    crc ^= (uint16_t)((uint8_t)*s) << 8;
    for (int i = 0; i < 8; i++) crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
  }
  return crc;
}

void makeSrc(char kind, const char* key, char out[8]) {
  snprintf(out, 8, "%c%04x", kind, (unsigned)crc16(key));
}

static void fmtValue(float v, char* out, size_t n) {
  // kurz: ganze Zahlen ohne Nachkommastellen, sonst max. 2
  char buf[24];
  snprintf(buf, sizeof(buf), "%.2f", (double)v);
  char* dot = strchr(buf, '.');
  if (dot) {
    char* e = buf + strlen(buf) - 1;
    while (e > dot && *e == '0') *e-- = 0;
    if (e == dot) *e = 0;
  }
  if (!strcmp(buf, "-0")) strcpy(buf, "0");
  snprintf(out, n, "%s", buf);
}

size_t buildMarker(const Marker& m, char* out, size_t outLen) {
  char val[24] = "";
  if (m.hasValue) fmtValue(m.value, val, sizeof(val));
  char ack[4] = "";
  if (m.ack) snprintf(ack, sizeof(ack), "%u", (unsigned)m.ack);
  int n = snprintf(out, outLen, "%s%s;%c;%s;%s", MARK, m.src, m.ev, val, ack);
  if (n < 0) { if (outLen) out[0] = 0; return 0; }
  return (size_t)n < outLen ? (size_t)n : outLen - 1;
}

size_t appendMarker(const char* text, const Marker& m, char* out, size_t maxLen) {
  char mk[MARK_MAX + 8];
  size_t ml = buildMarker(m, mk, sizeof(mk));
  if (maxLen == 0) return 0;
  size_t room = maxLen > ml + 1 ? maxLen - 1 - ml : 0;
  size_t tl = text ? strlen(text) : 0;
  if (tl > room) {
    tl = room;
    // nicht mitten in einem UTF-8-Zeichen abschneiden
    while (tl > 0 && ((uint8_t)text[tl] & 0xC0) == 0x80) tl--;
  }
  if (tl) memcpy(out, text, tl);
  size_t mcopy = ml < maxLen - 1 - tl ? ml : maxLen - 1 - tl;
  memcpy(out + tl, mk, mcopy);
  out[tl + mcopy] = 0;
  return tl + mcopy;
}

bool parseMarker(const char* body, Marker& m, size_t& textLen) {
  memset(&m, 0, sizeof(m));
  if (!body) return false;
  const char* p = nullptr;
  for (const char* q = strstr(body, MARK); q; q = strstr(q + 1, MARK)) p = q;
  if (!p) return false;
  textLen = (size_t)(p - body);
  const char* f = p + strlen(MARK);
  // Feld 1: Quelle
  size_t i = 0;
  while (*f && *f != ';' && i < sizeof(m.src) - 1) m.src[i++] = *f++;
  m.src[i] = 0;
  if (*f != ';' || i < 1) return false;
  f++;
  // Feld 2: Ereignis (ein Buchstabe)
  if (!*f || !strchr("AEWOFNHT", *f) || f[1] != ';') return false;
  m.ev = *f;
  f += 2;
  // Feld 3: Wert
  if (*f && *f != ';') {
    char* end = nullptr;
    m.value = strtof(f, &end);
    if (end == f) return false;
    m.hasValue = true;
    f = end;
  }
  if (*f != ';') return false;
  f++;
  // Feld 4: ACK-Nr (optional), danach nur noch Leerraum
  if (isdigit((unsigned char)*f)) {
    int n = 0;
    while (isdigit((unsigned char)*f) && n < 1000) n = n * 10 + (*f++ - '0');
    if (n < 1 || n > 99) return false;
    m.ack = (uint8_t)n;
  }
  while (*f == ' ' || *f == '\r' || *f == '\n') f++;
  return *f == 0;
}

const char* stateOf(char ev) {
  switch (ev) {
    case 'A': case 'E': case 'W': case 'F': return "ALARM";
    case 'O': case 'N': return "OK";
    default: return "INFO";
  }
}

void writeHeader(uint8_t out[HEADER_LEN], uint32_t jsonLen) {
  memcpy(out, "ZBXD", 4);
  out[4] = 0x01;
  for (int i = 0; i < 8; i++) out[5 + i] = i < 4 ? (uint8_t)(jsonLen >> (8 * i)) : 0;
}

long readHeader(const uint8_t* in, size_t len) {
  if (len < HEADER_LEN || memcmp(in, "ZBXD", 4) != 0 || !(in[4] & 0x01)) return -1;
  uint32_t n = 0;
  for (int i = 0; i < 4; i++) n |= (uint32_t)in[5 + i] << (8 * i);
  if (in[9] || in[10] || in[11] || in[12]) return -1;   // > 4 GB: sicher kein Sender-Response
  return (long)n;
}

bool parseResponse(const char* json, int& processed, int& failed) {
  processed = failed = -1;
  if (!json || !strstr(json, "\"response\":\"success\"")) {
    // Zabbix setzt mitunter Leerzeichen
    if (!json || !strstr(json, "\"success\"")) return false;
  }
  const char* p = strstr(json, "processed:");
  const char* f = strstr(json, "failed:");
  if (!p || !f) return false;
  processed = atoi(p + 10);
  failed = atoi(f + 7);
  return true;
}

size_t jsonEscape(const char* in, char* out, size_t outLen) {
  size_t o = 0;
  if (!outLen) return 0;
  for (; in && *in && o + 7 < outLen; in++) {
    unsigned char c = (unsigned char)*in;
    if (c == '"' || c == '\\') { out[o++] = '\\'; out[o++] = (char)c; }
    else if (c == '\n') { out[o++] = '\\'; out[o++] = 'n'; }
    else if (c < 0x20) { o += snprintf(out + o, outLen - o, "\\u%04x", c); }
    else out[o++] = (char)c;
  }
  out[o] = 0;
  return o;
}

}  // namespace zbx
