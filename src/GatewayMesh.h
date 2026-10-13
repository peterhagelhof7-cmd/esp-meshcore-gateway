#pragma once

#include "MyMesh.h"

// Repeater aus MeshCore (examples/simple_repeater/MyMesh) plus Gateway-Hooks.
// Die Upstream-Klasse bleibt unverändert, hier wird nur abgeleitet.
//
// M2: Paket-Hooks (logRx/logTx/logTxFail) schreiben zusätzlich in den
// Live-Paket-Log der Weboberfläche. Das Upstream-Verhalten (Datei-Log bei
// "log start", Bridge) läuft über den Basisaufruf unverändert weiter.
//
// M4: Gruppenkanäle (ChannelStore). Empfangene Kanalnachrichten landen im
// Nachrichten-Log, Senden baut das Paket exakt wie
// BaseChatMesh::sendGroupMessage (Zeitstempel, TXT_TYPE_PLAIN, "Name: Text").
// Das Weiterleiten fremder Pakete bleibt unverändert Sache des Repeaters.
class GatewayMesh : public MyMesh {
public:
  using MyMesh::MyMesh;

  // Nur aus dem loopTask. err bekommt bei false den Grund.
  bool sendChannelText(int channelIdx, const char* text, char* err, size_t errLen);

protected:
  void logRx(mesh::Packet* pkt, int len, float score) override;
  void logTx(mesh::Packet* pkt, int len) override;
  void logTxFail(mesh::Packet* pkt, int len) override;

  int searchChannelsByHash(const uint8_t* hash, mesh::GroupChannel channels[], int max_matches) override;
  void onGroupDataRecv(mesh::Packet* packet, uint8_t type, const mesh::GroupChannel& channel,
                       uint8_t* data, size_t len) override;
};

extern GatewayMesh the_mesh;
