#include "GatewayUITask.h"

#include <helpers/CommonCLI.h>
#include <time.h>

#include "target.h"
#include "GatewayStatus.h"
#include "NetworkManager.h"
#include "LogBuffer.h"

#define AUTO_OFF_MILLIS     60000   // Upstream: 20 s
#define BOOT_SCREEN_MILLIS  4000
#define PAGES               4

// 'meshcore', 128x13px - Logo aus examples/simple_repeater/UITask.cpp (MIT)
static const uint8_t meshcore_logo[] PROGMEM = {
    0x3c, 0x01, 0xe3, 0xff, 0xc7, 0xff, 0x8f, 0x03, 0x87, 0xfe, 0x1f, 0xfe, 0x1f, 0xfe, 0x1f, 0xfe,
    0x3c, 0x03, 0xe3, 0xff, 0xc7, 0xff, 0x8e, 0x03, 0x8f, 0xfe, 0x3f, 0xfe, 0x1f, 0xff, 0x1f, 0xfe,
    0x3e, 0x03, 0xc3, 0xff, 0x8f, 0xff, 0x0e, 0x07, 0x8f, 0xfe, 0x7f, 0xfe, 0x1f, 0xff, 0x1f, 0xfc,
    0x3e, 0x07, 0xc7, 0x80, 0x0e, 0x00, 0x0e, 0x07, 0x9e, 0x00, 0x78, 0x0e, 0x3c, 0x0f, 0x1c, 0x00,
    0x3e, 0x0f, 0xc7, 0x80, 0x1e, 0x00, 0x0e, 0x07, 0x1e, 0x00, 0x70, 0x0e, 0x38, 0x0f, 0x3c, 0x00,
    0x7f, 0x0f, 0xc7, 0xfe, 0x1f, 0xfc, 0x1f, 0xff, 0x1c, 0x00, 0x70, 0x0e, 0x38, 0x0e, 0x3f, 0xf8,
    0x7f, 0x1f, 0xc7, 0xfe, 0x0f, 0xff, 0x1f, 0xff, 0x1c, 0x00, 0xf0, 0x0e, 0x38, 0x0e, 0x3f, 0xf8,
    0x7f, 0x3f, 0xc7, 0xfe, 0x0f, 0xff, 0x1f, 0xff, 0x1c, 0x00, 0xf0, 0x1e, 0x3f, 0xfe, 0x3f, 0xf0,
    0x77, 0x3b, 0x87, 0x00, 0x00, 0x07, 0x1c, 0x0f, 0x3c, 0x00, 0xe0, 0x1c, 0x7f, 0xfc, 0x38, 0x00,
    0x77, 0xfb, 0x8f, 0x00, 0x00, 0x07, 0x1c, 0x0f, 0x3c, 0x00, 0xe0, 0x1c, 0x7f, 0xf8, 0x38, 0x00,
    0x73, 0xf3, 0x8f, 0xff, 0x0f, 0xff, 0x1c, 0x0e, 0x3f, 0xf8, 0xff, 0xfc, 0x70, 0x78, 0x7f, 0xf8,
    0xe3, 0xe3, 0x8f, 0xff, 0x1f, 0xfe, 0x3c, 0x0e, 0x3f, 0xf8, 0xff, 0xfc, 0x70, 0x3c, 0x7f, 0xf8,
    0xe3, 0xe3, 0x8f, 0xff, 0x1f, 0xfc, 0x3c, 0x0e, 0x1f, 0xf8, 0xff, 0xf8, 0x70, 0x3c, 0x7f, 0xf8,
};

// Zahl hinter "key": aus einem der fertigen stats-JSON-Strings lesen.
static bool jsonNumber(const char* json, const char* key, long& out) {
  char pat[32];
  snprintf(pat, sizeof(pat), "\"%s\":", key);
  const char* p = strstr(json, pat);
  if (!p) return false;
  out = strtol(p + strlen(pat), nullptr, 10);
  return true;
}

// UTF-8 -> Code Page 437 (der SSD1306-Treiber schaltet cp437 ein). Deutsche
// Umlaute, ß, ° und é gibt es dort, alles andere (Emojis ...) wird '?'.
static void toCp437(char* dst, size_t size, const char* src) {
  size_t j = 0;
  const unsigned char* s = (const unsigned char*)src;
  while (*s && j + 1 < size) {
    unsigned char c = *s;
    if (c < 0x80) {
      dst[j++] = (c < 0x20) ? ' ' : (char)c;
      s++;
      continue;
    }
    uint32_t cp = 0;
    int extra = 0;
    if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; extra = 1; }
    else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; extra = 2; }
    else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; extra = 3; }
    s++;
    for (int k = 0; k < extra && (*s & 0xC0) == 0x80; k++, s++) cp = (cp << 6) | (*s & 0x3F);
    while ((*s & 0xC0) == 0x80) s++;   // kaputte Folge überspringen
    char o;
    switch (cp) {
      case 0xE4: o = '\x84'; break;   // ä
      case 0xF6: o = '\x94'; break;   // ö
      case 0xFC: o = '\x81'; break;   // ü
      case 0xC4: o = '\x8E'; break;   // Ä
      case 0xD6: o = '\x99'; break;   // Ö
      case 0xDC: o = '\x9A'; break;   // Ü
      case 0xDF: o = '\xE1'; break;   // ß
      case 0xB0: o = '\xF8'; break;   // °
      case 0xE9: o = '\x82'; break;   // é
      default:   o = '?'; break;
    }
    dst[j++] = o;
  }
  dst[j] = 0;
}

void GatewayUITask::footer(int page) {
  char tmp[8];
  snprintf(tmp, sizeof(tmp), "%d/%d", page, PAGES);
  _display->setCursor(0, 54);
  _display->print(tmp);
}

void GatewayUITask::begin(NodePrefs* prefs, const char* buildDate, const char* firmwareVersion) {
  _prefs = prefs;
  _startedAt = millis();
  _autoOff = millis() + AUTO_OFF_MILLIS;
  _display->turnOn();
#if defined(PIN_USER_BTN)
  user_btn.begin();
#endif
  snprintf(_versionInfo, sizeof(_versionInfo), "%s GW %s", firmwareVersion, GATEWAY_VERSION);
}

void GatewayUITask::renderBoot() {
  _display->setColor(UIColor::corp_blue);
  _display->drawXbm((_display->width() - 128) / 2, 3, meshcore_logo, 128, 13);
  _display->setColor(UIColor::primary_txt);
  _display->setTextSize(1);
  _display->drawTextCentered(_display->width() / 2, 22, "< Repeater + WLAN >");
  _display->drawTextCentered(_display->width() / 2, 35, _versionInfo);
  _display->drawTextCentered(_display->width() / 2, 48, "Gateway");
}

void GatewayUITask::renderNet() {
  char tmp[48];
  _display->setCursor(0, 0);
  _display->print(_prefs->node_name);
  NetworkManager::Mode m = network.mode();
  if (m == NetworkManager::Mode::INSTALLER_AP) {
    _display->setCursor(0, 14);
    _display->print("Einrichtung: WLAN");
    _display->setCursor(0, 26);
    _display->print("installer / installer");
    _display->setCursor(0, 38);
    _display->print("http://192.168.4.1");
  } else if (m == NetworkManager::Mode::STATION) {
    _display->setCursor(0, 14);
    snprintf(tmp, sizeof(tmp), "WLAN %d dBm", network.rssi());
    _display->print(tmp);
    _display->setCursor(0, 26);
    _display->print(network.ssid().c_str());
    _display->setCursor(0, 38);
    snprintf(tmp, sizeof(tmp), "IP %s", network.ip().toString().c_str());
    _display->print(tmp);
  } else {
    _display->setCursor(0, 14);
    _display->print("WLAN verbinde ...");
    _display->setCursor(0, 26);
    _display->print(network.ssid().c_str());
  }
  footer(1);
}

void GatewayUITask::renderRadio() {
  char tmp[48];
  MeshSnapshot s = gateway_status::get();
  _display->setCursor(0, 0);
  _display->print("Funk");
  _display->setCursor(0, 14);
  snprintf(tmp, sizeof(tmp), "%.3f MHz SF%d", _prefs->freq, _prefs->sf);
  _display->print(tmp);
  _display->setCursor(0, 26);
  snprintf(tmp, sizeof(tmp), "BW %.1f CR %d TX %d", _prefs->bw, _prefs->cr, _prefs->tx_power_dbm);
  _display->print(tmp);
  long nf;
  if (jsonNumber(s.radioStatsJson, "noise_floor", nf)) {
    _display->setCursor(0, 38);
    snprintf(tmp, sizeof(tmp), "Rauschen %ld dBm", nf);
    _display->print(tmp);
  }
  footer(2);
}

void GatewayUITask::renderMesh() {
  char tmp[48];
  MeshSnapshot s = gateway_status::get();
  long rx = 0, tx = 0;
  jsonNumber(s.packetStatsJson, "recv", rx);
  jsonNumber(s.packetStatsJson, "sent", tx);
  _display->setCursor(0, 0);
  _display->print("Mesh");
  _display->setCursor(0, 14);
  snprintf(tmp, sizeof(tmp), "RX %ld  TX %ld", rx, tx);
  _display->print(tmp);
  _display->setCursor(0, 26);
  _display->print(_prefs->disable_fwd ? "Weiterleiten AUS" : "Weiterleiten an");
  _display->setCursor(0, 38);
  time_t now = time(nullptr);
  if (now > 1700000000) {
    struct tm lt;
    localtime_r(&now, &lt);
    strftime(tmp, sizeof(tmp), "%d.%m. %H:%M:%S", &lt);
  } else {
    unsigned long up = millis() / 1000;
    snprintf(tmp, sizeof(tmp), "Laufzeit %luh %02lum", up / 3600, (up / 60) % 60);
  }
  _display->print(tmp);
  footer(3);
}

// Seite 4: neueste Zeile des Nachrichten-Logs (empfangen oder gesendet, also
// auch Alarme des Gateways). Format der Zeile siehe GatewayMesh:
// "idx \t Kanal \t SNR \t Hops \t Text", SNR leer = gesendet.
void GatewayUITask::renderMessage() {
  static const int COLS = 21;   // 128 px / 6 px Standardschrift
  static const int ROWS = 4;
  LogBuffer::Line l;
  _display->setCursor(0, 0);
  if (!channel_log.latest(l)) {
    _display->print("Nachricht");
    _display->setCursor(0, 22);
    _display->print("noch keine");
    footer(4);
    return;
  }

  const char* f[5] = {"", "", "", "", ""};
  char* p = l.text;
  for (int i = 0; i < 5; i++) {
    f[i] = p;
    char* t = (i < 4) ? strchr(p, '\t') : nullptr;
    if (!t) break;
    *t = 0;
    p = t + 1;
  }
  bool sent = (l.type == 'S');

  // Kopf: Kanal links, Uhrzeit bzw. Alter rechts
  char head[24], when[12], tmp[48];
  toCp437(head, sizeof(head), f[1]);
  time_t ep = l.epoch;
  if (ep > 1700000000) {
    struct tm lt;
    localtime_r(&ep, &lt);
    strftime(when, sizeof(when), "%H:%M", &lt);
  } else {
    unsigned long age = (millis() - l.uptimeMs) / 60000;
    if (age < 60) snprintf(when, sizeof(when), "-%lum", age);
    else snprintf(when, sizeof(when), "-%luh", age / 60);
  }
  int maxHead = COLS - 1 - (int)strlen(when);
  if ((int)strlen(head) > maxHead) head[maxHead] = 0;
  _display->print(head);
  _display->drawTextRightAlign(_display->width(), 0, when);

  // Text mit Wortumbruch, Zeichen bei COLS fest (Schrift ist nicht proportional)
  char body[LogBuffer::LINE_LEN];
  toCp437(body, sizeof(body), f[4]);
  const char* b = body;
  for (int row = 0; row < ROWS && *b; row++) {
    while (*b == ' ') b++;
    int n = strlen(b);
    if (n > COLS) {
      n = COLS;
      if (row < ROWS - 1) {
        int sp = COLS;
        while (sp > 0 && b[sp] != ' ') sp--;
        if (sp > 0) n = sp;   // hart trennen nur Wörter > 1 Zeile
      } else {
        n = COLS - 3;
      }
    }
    memcpy(tmp, b, n);
    tmp[n] = 0;
    if (row == ROWS - 1 && (int)strlen(b) > COLS) strcat(tmp, "...");
    _display->setCursor(0, 12 + row * 10);
    _display->print(tmp);
    b += n;
  }

  footer(4);
  if (sent) {
    strcpy(tmp, "gesendet");
  } else {
    snprintf(tmp, sizeof(tmp), "SNR %s %s hop", f[2], f[3]);
  }
  _display->drawTextRightAlign(_display->width(), 54, tmp);
}

void GatewayUITask::render() {
  _display->setTextSize(1);
  _display->setColor(UIColor::primary_txt);
  if (millis() < _startedAt + BOOT_SCREEN_MILLIS) {
    renderBoot();
    return;
  }
  switch (_page) {
    case 1: renderRadio(); break;
    case 2: renderMesh(); break;
    case 3: renderMessage(); break;
    default: renderNet(); break;
  }
}

void GatewayUITask::loop() {
#if defined(PIN_USER_BTN)
  int ev = user_btn.check();
  if (ev == BUTTON_EVENT_CLICK || ev == BUTTON_EVENT_LONG_PRESS) {
    // Langer Druck bewusst wie Klick (kein powerOff, siehe Header)
    if (_display->isOn()) {
      _page = (_page + 1) % PAGES;
    } else {
      _display->turnOn();
    }
    _autoOff = millis() + AUTO_OFF_MILLIS;
    _nextRefresh = 0;
  }
#endif

  // Im Installer-AP bleibt die Anzeige an: sie zeigt, wie man ins Setup kommt.
  if (network.mode() == NetworkManager::Mode::INSTALLER_AP) {
    if (!_display->isOn()) _display->turnOn();
    _autoOff = millis() + AUTO_OFF_MILLIS;
    if (millis() >= _startedAt + BOOT_SCREEN_MILLIS) _page = 0;
  }

  if (_display->isOn()) {
    if (millis() >= _nextRefresh) {
      _display->startFrame();
      render();
      _display->endFrame();
      _nextRefresh = millis() + 1000;
    }
    if (millis() > _autoOff) {
      _display->turnOff();
      _page = 0;
    }
  }
}
