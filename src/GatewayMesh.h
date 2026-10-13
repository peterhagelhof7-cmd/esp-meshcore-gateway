#pragma once

#include "MyMesh.h"

// Repeater aus MeshCore (examples/simple_repeater/MyMesh) plus Gateway-Hooks.
// Die Upstream-Klasse bleibt unverändert, hier wird nur abgeleitet.
//
// M2: Paket-Hooks (logRx/logTx/logTxFail) schreiben zusätzlich in den
// Live-Paket-Log der Weboberfläche. Das Upstream-Verhalten (Datei-Log bei
// "log start", Bridge) läuft über den Basisaufruf unverändert weiter.
class GatewayMesh : public MyMesh {
public:
  using MyMesh::MyMesh;

protected:
  void logRx(mesh::Packet* pkt, int len, float score) override;
  void logTx(mesh::Packet* pkt, int len) override;
  void logTxFail(mesh::Packet* pkt, int len) override;
};

extern GatewayMesh the_mesh;
