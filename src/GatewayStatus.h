#pragma once

#include <Arduino.h>

// Momentaufnahme des MeshCore-Zustands für den Webserver.
//
// MeshCore läuft im loopTask, ESPAsyncWebServer im AsyncTCP-Task. Der Web-Task
// darf MeshCore-Objekte nie direkt anfassen (docs/entscheidungen.md E4). Der
// loopTask schreibt deshalb regelmäßig diese Kopie (update()), der Web-Task
// liest sie unter Mutex (get()).
struct MeshSnapshot {
  char nodeName[32];
  char pubKeyHex[65];
  char firmwareVer[16];
  char buildDate[24];
  char role[16];
  char adminPassword[16];   // für den Web-Login (E7), wird nie ausgeliefert
  float freq;
  float bw;
  uint8_t sf;
  uint8_t cr;
  int8_t txPower;
  bool forwardingEnabled;
  double lat;
  double lon;
  // Fertige JSON-Objekte aus den offiziellen stats-Formatierern
  // (StatsFormatHelper): stats-core / stats-radio / stats-packets.
  char coreStatsJson[128];
  char radioStatsJson[160];
  char packetStatsJson[192];
  bool valid;
};

namespace gateway_status {
  void begin();
  // Nur aus dem loopTask aufrufen.
  void update();
  // Thread-sicher, aus beliebigem Task.
  MeshSnapshot get();
}
