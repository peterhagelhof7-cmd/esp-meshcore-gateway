#pragma once

#include <Arduino.h>
#include <MeshCore.h>
#include <helpers/ui/DisplayDriver.h>

class NodePrefs;

// OLED-Anzeige des Gateways (M5, docs/entscheidungen.md E20), ersetzt
// examples/simple_repeater/UITask:
//   - Startbild wie Upstream (Logo, Version)
//   - 3 Seiten, Taste kurz drücken = Anzeige an bzw. nächste Seite:
//       1 Netz:  Name, WLAN-Modus, IP (im Installer-AP: SSID + Passwort)
//       2 Funk:  Frequenz/SF/BW/CR, TX, Rauschpegel
//       3 Mesh:  Pakete RX/TX, Uhrzeit (NTP) bzw. Laufzeit
//   - Auto-Aus nach 60 s gegen Einbrennen, im Installer-AP bleibt sie an
//   - KEIN Abschalten per langem Tastendruck (Upstream: powerOff/Deep-Sleep;
//     ohne Akku wäre das Gateway dann nur per Stromtrennung zurückzuholen)
class GatewayUITask {
public:
  GatewayUITask(mesh::MainBoard& board, DisplayDriver& display) : _board(&board), _display(&display) {}
  void begin(NodePrefs* prefs, const char* buildDate, const char* firmwareVersion);
  void loop();

private:
  void render();
  void renderBoot();
  void renderNet();
  void renderRadio();
  void renderMesh();

  mesh::MainBoard* _board;
  DisplayDriver* _display;
  NodePrefs* _prefs = nullptr;
  char _versionInfo[40];
  unsigned long _startedAt = 0;
  unsigned long _autoOff = 0;
  unsigned long _nextRefresh = 0;
  uint8_t _page = 0;
};
