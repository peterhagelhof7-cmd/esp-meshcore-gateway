#include "LogBuffer.h"

#include <esp_heap_caps.h>

LogBuffer console_log(150);
LogBuffer packet_log(300);
LogBuffer channel_log(200);
LogBuffer alert_log(100);

LogBuffer::LogBuffer(size_t capacity) : _cap(capacity) {}

bool LogBuffer::begin() {
  if (_lines) return true;
  _lock = xSemaphoreCreateMutex();
  size_t bytes = _cap * sizeof(Line);
  _lines = (Line*)heap_caps_calloc(1, bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!_lines) _lines = (Line*)calloc(1, bytes);
  return _lines != nullptr && _lock != nullptr;
}

void LogBuffer::push(char type, uint32_t epoch, const char* text) {
  if (!_lines) return;
  xSemaphoreTake(_lock, portMAX_DELAY);
  Line& l = _lines[_nextSeq % _cap];
  l.seq = _nextSeq++;
  l.uptimeMs = millis();
  l.epoch = epoch;
  l.type = type;
  strlcpy(l.text, text ? text : "", sizeof(l.text));
  xSemaphoreGive(_lock);
}

void LogBuffer::pushLines(char type, uint32_t epoch, const char* text) {
  if (!text) return;
  char part[LINE_LEN];
  const char* p = text;
  do {
    size_t n = 0;
    while (p[n] && p[n] != '\n' && n < LINE_LEN - 1) n++;
    memcpy(part, p, n);
    part[n] = 0;
    push(type, epoch, part);
    p += n;
    if (*p == '\n') p++;
  } while (*p);
}

static void appendEscaped(String& out, const char* s) {
  for (; *s; s++) {
    char c = *s;
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': break;
      case '\t': out += "\\t"; break;
      default:
        if ((uint8_t)c < 0x20) {
          char b[8];
          snprintf(b, sizeof(b), "\\u%04x", c);
          out += b;
        } else {
          out += c;
        }
    }
  }
}

uint32_t LogBuffer::appendJson(uint32_t since, size_t maxLines, String& out, bool& gap) {
  gap = false;
  if (!_lines) return since;
  xSemaphoreTake(_lock, portMAX_DELAY);
  uint32_t newest = _nextSeq - 1;
  uint32_t oldest = (_nextSeq > _cap) ? _nextSeq - _cap : 1;
  uint32_t from = since;
  if (from < oldest) {
    if (since > 0 && newest > 0) gap = true;
    from = oldest;
  }
  size_t n = 0;
  bool first = true;
  uint32_t seq = from;
  for (; seq <= newest && n < maxLines; seq++, n++) {
    const Line& l = _lines[seq % _cap];
    if (!first) out += ',';
    first = false;
    out += "{\"s\":";
    out += String(l.seq);
    out += ",\"u\":";
    out += String(l.uptimeMs);
    out += ",\"e\":";
    out += String(l.epoch);
    out += ",\"t\":\"";
    out += l.type;
    out += "\",\"x\":\"";
    appendEscaped(out, l.text);
    out += "\"}";
  }
  xSemaphoreGive(_lock);
  return seq;
}
