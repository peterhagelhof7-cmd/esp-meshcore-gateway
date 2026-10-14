#include "EventLog.h"

#include <SPIFFS.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <stdarg.h>
#include <time.h>

namespace event_log {

static const char* CUR = "/gwlog.txt";
static const char* OLD = "/gwlog.1.txt";

static SemaphoreHandle_t s_lock = nullptr;
static char* s_buf = nullptr;
static size_t s_len = 0;
static uint32_t s_firstPendingMs = 0;
static uint32_t s_lastFlushMs = 0;
static bool s_everFlushed = false;
static uint32_t s_flushes = 0;
static uint32_t s_dropped = 0;
static uint32_t s_droppedReported = 0;

static size_t fileSize(const char* path) {
  if (!SPIFFS.exists(path)) return 0;
  File f = SPIFFS.open(path, FILE_READ);
  if (!f) return 0;
  size_t n = f.size();
  f.close();
  return n;
}

// Lock muss gehalten werden.
static void appendLocked(const char* line, size_t n) {
  if (s_len + n > BUF_BYTES) {
    s_dropped++;
    return;
  }
  if (s_len == 0) s_firstPendingMs = millis();
  memcpy(s_buf + s_len, line, n);
  s_len += n;
}

static void formatLine(char* out, size_t outLen, const char* tag, const char* text) {
  char ts[24];
  time_t now = time(nullptr);
  if (now > 1700000000) {   // Uhr gestellt (NTP/RTC)
    struct tm tm;
    localtime_r(&now, &tm);
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm);
  } else {
    snprintf(ts, sizeof(ts), "+%lus", (unsigned long)(millis() / 1000));
  }
  // Zeilenumbrüche im Text als " | " (eine Zeile pro Ereignis)
  size_t p = snprintf(out, outLen, "%s %-5s ", ts, tag);
  for (const char* s = text; *s && p + 4 < outLen; s++) {
    if (*s == '\r') continue;
    if (*s == '\n') {
      if (s[1]) { out[p++] = ' '; out[p++] = '|'; out[p++] = ' '; }
      continue;
    }
    out[p++] = *s;
  }
  out[p++] = '\n';
  out[p] = 0;
}

// Lock muss gehalten werden.
static void flushLocked() {
  if (s_dropped != s_droppedReported) {
    char line[96], msg[64];
    snprintf(msg, sizeof(msg), "%lu Zeile(n) verworfen (RAM-Puffer voll)",
             (unsigned long)(s_dropped - s_droppedReported));
    formatLine(line, sizeof(line), "LOG", msg);
    s_droppedReported = s_dropped;
    size_t n = strlen(line);
    if (s_len + n <= BUF_BYTES) { memcpy(s_buf + s_len, line, n); s_len += n; }
  }
  if (s_len == 0) return;
  if (fileSize(CUR) + s_len > FILE_MAX_BYTES) {
    SPIFFS.remove(OLD);
    SPIFFS.rename(CUR, OLD);
  }
  File f = SPIFFS.open(CUR, FILE_APPEND);
  if (!f) {
    // Ohne Dateisystem nicht endlos sammeln: Puffer verwerfen, Zähler merkt es.
    s_dropped++;
    s_droppedReported = s_dropped;
    s_len = 0;
    return;
  }
  f.write((const uint8_t*)s_buf, s_len);
  f.close();
  s_len = 0;
  s_lastFlushMs = millis();
  s_everFlushed = true;
  s_flushes++;
}

static void onShutdown() {
  // esp_restart() ruft das im Kontext des neustartenden Tasks auf. Nicht
  // blockieren, falls gerade ein anderer Task den Lock hält.
  if (s_lock && xSemaphoreTake(s_lock, pdMS_TO_TICKS(500)) == pdTRUE) {
    flushLocked();
    xSemaphoreGive(s_lock);
  }
}

static const char* resetReason() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON: return "Einschalten";
    case ESP_RST_EXT: return "Reset-Pin";
    case ESP_RST_SW: return "Software-Neustart";
    case ESP_RST_PANIC: return "ABSTURZ (Panic)";
    case ESP_RST_INT_WDT: return "Interrupt-Watchdog";
    case ESP_RST_TASK_WDT: return "Task-Watchdog";
    case ESP_RST_WDT: return "Watchdog";
    case ESP_RST_DEEPSLEEP: return "Deep-Sleep";
    case ESP_RST_BROWNOUT: return "Unterspannung (Brownout)";
    case ESP_RST_SDIO: return "SDIO";
    default: return "unbekannt";
  }
}

void begin() {
  if (s_lock) return;
  s_lock = xSemaphoreCreateMutex();
  s_buf = (char*)heap_caps_malloc(BUF_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!s_buf) s_buf = (char*)malloc(BUF_BYTES);
  esp_register_shutdown_handler(onShutdown);
  addf("SYS", "=== Start (%s), Gateway " GATEWAY_VERSION, resetReason());
}

void add(const char* tag, const char* text) {
  if (!s_lock || !s_buf) return;
  char line[300];
  formatLine(line, sizeof(line), tag, text ? text : "");
  xSemaphoreTake(s_lock, portMAX_DELAY);
  appendLocked(line, strlen(line));
  xSemaphoreGive(s_lock);
}

void addf(const char* tag, const char* fmt, ...) {
  char text[256];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(text, sizeof(text), fmt, ap);
  va_end(ap);
  add(tag, text);
}

void loop() {
  if (!s_lock || !s_buf) return;
  xSemaphoreTake(s_lock, portMAX_DELAY);
  bool due = s_len >= FLUSH_BYTES ||
             (s_len > 0 && millis() - s_firstPendingMs >= FLUSH_INTERVAL_MS) ||
             s_dropped != s_droppedReported;
  if (due) flushLocked();
  xSemaphoreGive(s_lock);
}

void flush() {
  if (!s_lock || !s_buf) return;
  xSemaphoreTake(s_lock, portMAX_DELAY);
  flushLocked();
  xSemaphoreGive(s_lock);
}

void clear() {
  if (!s_lock || !s_buf) return;
  xSemaphoreTake(s_lock, portMAX_DELAY);
  SPIFFS.remove(OLD);
  SPIFFS.remove(CUR);
  s_len = 0;
  s_dropped = s_droppedReported = 0;
  xSemaphoreGive(s_lock);
  add("LOG", "Log gelöscht");
}

Info info() {
  Info i = {};
  if (!s_lock) return i;
  xSemaphoreTake(s_lock, portMAX_DELAY);
  i.fileBytes = fileSize(CUR) + fileSize(OLD);
  i.pendingBytes = s_len;
  i.lastFlushAgoS = s_everFlushed ? (millis() - s_lastFlushMs) / 1000 : 0xFFFFFFFFUL;
  i.flushes = s_flushes;
  i.dropped = s_dropped;
  xSemaphoreGive(s_lock);
  return i;
}

static size_t readInto(const char* path, char* dst, size_t room) {
  if (!SPIFFS.exists(path)) return 0;
  File f = SPIFFS.open(path, FILE_READ);
  if (!f) return 0;
  size_t n = f.read((uint8_t*)dst, room);
  f.close();
  return n;
}

char* snapshot(size_t& len) {
  len = 0;
  if (!s_lock || !s_buf) return nullptr;
  xSemaphoreTake(s_lock, portMAX_DELAY);
  size_t cap = fileSize(OLD) + fileSize(CUR) + s_len + 1;
  char* out = (char*)heap_caps_malloc(cap, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!out) out = (char*)malloc(cap);
  if (out) {
    len += readInto(OLD, out + len, cap - 1 - len);
    len += readInto(CUR, out + len, cap - 1 - len);
    memcpy(out + len, s_buf, s_len);
    len += s_len;
    out[len] = 0;
  }
  xSemaphoreGive(s_lock);
  return out;
}

}  // namespace event_log
