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

  // Für RemoteAdmin (E42)
  uint32_t estAirtime(int len) { return _radio->getEstAirtimeFor(len); }
  uint8_t pathHashSize() { return getNodePrefs()->path_hash_mode + 1; }

protected:
  void logRx(mesh::Packet* pkt, int len, float score) override;
  void logTx(mesh::Packet* pkt, int len) override;
  void logTxFail(mesh::Packet* pkt, int len) override;

  int searchChannelsByHash(const uint8_t* hash, mesh::GroupChannel channels[], int max_matches) override;
  void onGroupDataRecv(mesh::Packet* packet, uint8_t type, const mesh::GroupChannel& channel,
                       uint8_t* data, size_t len) override;

  // E42: Fernzugriff auf andere Repeater. Sitzungs-Ziele (RemoteAdmin) kommen
  // bei der Peer-Suche VOR den eigenen Clients (ACL), Indizes dahinter werden
  // an MyMesh weitergereicht (minus Anzahl der Sitzungs-Treffer).
  void onAdvertRecv(mesh::Packet* packet, const mesh::Identity& id, uint32_t timestamp,
                    const uint8_t* app_data, size_t app_data_len) override;
  void onControlDataRecv(mesh::Packet* packet) override;
  int searchPeersByHash(const uint8_t* hash) override;
  void getPeerSharedSecret(uint8_t* dest_secret, int peer_idx) override;
  void onPeerDataRecv(mesh::Packet* packet, uint8_t type, int sender_idx, const uint8_t* secret,
                      uint8_t* data, size_t len) override;
  bool onPeerPathRecv(mesh::Packet* packet, int sender_idx, const uint8_t* secret, uint8_t* path,
                      uint8_t path_len, uint8_t extra_type, uint8_t* extra, uint8_t extra_len) override;

private:
  static const int MAX_REMOTE_MATCH = 4;
  int _remoteN = 0;                       // Sitzungs-Treffer der letzten Peer-Suche
  int _remoteIdx[MAX_REMOTE_MATCH] = {0};
  int _baseN = 0;                         // Treffer von MyMesh (ACL)
  int baseIndexFor(const uint8_t* secret);
};

extern GatewayMesh the_mesh;
