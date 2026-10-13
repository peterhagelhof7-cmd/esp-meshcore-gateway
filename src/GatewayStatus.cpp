#include "GatewayStatus.h"

#include <Mesh.h>
#include "MyMesh.h"

extern MyMesh the_mesh;

static SemaphoreHandle_t s_lock = nullptr;
static MeshSnapshot s_snap = {};

void gateway_status::begin() {
  if (!s_lock) s_lock = xSemaphoreCreateMutex();
}

void gateway_status::update() {
  MeshSnapshot n = {};
  NodePrefs* p = the_mesh.getNodePrefs();

  strlcpy(n.nodeName, p->node_name, sizeof(n.nodeName));
  mesh::Utils::toHex(n.pubKeyHex, the_mesh.self_id.pub_key, PUB_KEY_SIZE);
  strlcpy(n.firmwareVer, the_mesh.getFirmwareVer(), sizeof(n.firmwareVer));
  strlcpy(n.buildDate, the_mesh.getBuildDate(), sizeof(n.buildDate));
  strlcpy(n.role, the_mesh.getRole(), sizeof(n.role));
  strlcpy(n.adminPassword, p->password, sizeof(n.adminPassword));
  n.freq = p->freq;
  n.bw = p->bw;
  n.sf = p->sf;
  n.cr = p->cr;
  n.txPower = p->tx_power_dbm;
  n.forwardingEnabled = !p->disable_fwd;
  n.lat = p->node_lat;
  n.lon = p->node_lon;

  // Die Formatierer schreiben per sprintf ohne Längenangabe -> großzügige
  // Zwischenpuffer, erst danach begrenzt kopieren.
  char buf[256];
  buf[0] = 0; the_mesh.formatStatsReply(buf);
  strlcpy(n.coreStatsJson, buf, sizeof(n.coreStatsJson));
  buf[0] = 0; the_mesh.formatRadioStatsReply(buf);
  strlcpy(n.radioStatsJson, buf, sizeof(n.radioStatsJson));
  buf[0] = 0; the_mesh.formatPacketStatsReply(buf);
  strlcpy(n.packetStatsJson, buf, sizeof(n.packetStatsJson));
  n.valid = true;

  xSemaphoreTake(s_lock, portMAX_DELAY);
  s_snap = n;
  xSemaphoreGive(s_lock);
}

MeshSnapshot gateway_status::get() {
  MeshSnapshot copy;
  if (!s_lock) {
    memset(&copy, 0, sizeof(copy));
    return copy;
  }
  xSemaphoreTake(s_lock, portMAX_DELAY);
  copy = s_snap;
  xSemaphoreGive(s_lock);
  return copy;
}
