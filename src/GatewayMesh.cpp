#include "GatewayMesh.h"

#include "LogBuffer.h"
#include "ChannelStore.h"
#include "SmPoller.h"
#include "ZbxForwarder.h"

#include <helpers/TxtDataHelpers.h>
#include <helpers/BaseChatMesh.h>

static const char* payloadTypeName(uint8_t t) {
  switch (t) {
    case PAYLOAD_TYPE_REQ: return "REQ";
    case PAYLOAD_TYPE_RESPONSE: return "RESPONSE";
    case PAYLOAD_TYPE_TXT_MSG: return "TXT_MSG";
    case PAYLOAD_TYPE_ACK: return "ACK";
    case PAYLOAD_TYPE_ADVERT: return "ADVERT";
    case PAYLOAD_TYPE_GRP_TXT: return "GRP_TXT";
    case PAYLOAD_TYPE_GRP_DATA: return "GRP_DATA";
    case PAYLOAD_TYPE_ANON_REQ: return "ANON_REQ";
    case PAYLOAD_TYPE_PATH: return "PATH";
    case PAYLOAD_TYPE_TRACE: return "TRACE";
    case PAYLOAD_TYPE_MULTIPART: return "MULTIPART";
    case PAYLOAD_TYPE_CONTROL: return "CONTROL";
    case PAYLOAD_TYPE_RAW_CUSTOM: return "RAW_CUSTOM";
    default: return "?";
  }
}

// Gleicher Inhalt wie die Upstream-Zeilen im Datei-Log (MyMesh::logRx/logTx),
// plus Klartext-Typname und Hop-Anzahl.
static int formatPacket(char* buf, size_t size, mesh::Packet* pkt, int len) {
  uint8_t type = pkt->getPayloadType();
  int n = snprintf(buf, size, "len=%d %s(%d) route=%s hops=%d payload=%d", len,
                   payloadTypeName(type), type, pkt->isRouteDirect() ? "D" : "F",
                   pkt->getPathHashCount(), pkt->payload_len);
  if (n > 0 && (size_t)n < size &&
      (type == PAYLOAD_TYPE_PATH || type == PAYLOAD_TYPE_REQ ||
       type == PAYLOAD_TYPE_RESPONSE || type == PAYLOAD_TYPE_TXT_MSG)) {
    n += snprintf(buf + n, size - n, " [%02X -> %02X]", (uint32_t)pkt->payload[1], (uint32_t)pkt->payload[0]);
  }
  return n;
}

void GatewayMesh::logRx(mesh::Packet* pkt, int len, float score) {
  MyMesh::logRx(pkt, len, score);

  char buf[LogBuffer::LINE_LEN];
  int n = formatPacket(buf, sizeof(buf), pkt, len);
  if (n > 0 && (size_t)n < sizeof(buf)) {
    snprintf(buf + n, sizeof(buf) - n, " SNR=%.1f RSSI=%d score=%d",
             _radio->getLastSNR(), (int)_radio->getLastRSSI(), (int)(score * 1000));
  }
  packet_log.push('R', getRTCClock()->getCurrentTime(), buf);
}

void GatewayMesh::logTx(mesh::Packet* pkt, int len) {
  MyMesh::logTx(pkt, len);

  char buf[LogBuffer::LINE_LEN];
  formatPacket(buf, sizeof(buf), pkt, len);
  packet_log.push('T', getRTCClock()->getCurrentTime(), buf);
}

void GatewayMesh::logTxFail(mesh::Packet* pkt, int len) {
  MyMesh::logTxFail(pkt, len);

  char buf[LogBuffer::LINE_LEN];
  formatPacket(buf, sizeof(buf), pkt, len);
  packet_log.push('F', getRTCClock()->getCurrentTime(), buf);
}

// ---------------------------------------------------------------------------
// M4: Gruppenkanäle
// ---------------------------------------------------------------------------

static void toGroupChannel(const GwChannel& c, mesh::GroupChannel& out) {
  // Wie BaseChatMesh::addChannel: 32-Byte-Secret-Feld, 16-Byte-Schlüssel,
  // Rest Nullen (geht so auch in den HMAC ein).
  memset(&out, 0, sizeof(out));
  memcpy(out.secret, c.secret, sizeof(c.secret));
  out.hash[0] = c.hash;
}

int GatewayMesh::searchChannelsByHash(const uint8_t* hash, mesh::GroupChannel channels[], int max_matches) {
  int idx[channel_store::MAX_CHANNELS];
  int n = channel_store::findByHash(hash[0], idx, max_matches < channel_store::MAX_CHANNELS ? max_matches : channel_store::MAX_CHANNELS);
  int out = 0;
  for (int i = 0; i < n; i++) {
    GwChannel c;
    if (channel_store::get(idx[i], c)) toGroupChannel(c, channels[out++]);
  }
  return out;
}

void GatewayMesh::onGroupDataRecv(mesh::Packet* packet, uint8_t type, const mesh::GroupChannel& channel,
                                  uint8_t* data, size_t len) {
  if (type != PAYLOAD_TYPE_GRP_TXT || len < 5) return;     // GRP_DATA zeigt das Gateway nicht an
  if ((data[4] >> 2) != TXT_TYPE_PLAIN) return;            // wie BaseChatMesh::onGroupDataRecv

  uint32_t timestamp;
  memcpy(&timestamp, data, 4);

  // Text ist nicht nullterminiert und kann Padding-Nullen enthalten.
  char text[MAX_PACKET_PAYLOAD + 1];
  size_t tlen = len - 5;
  if (tlen > MAX_PACKET_PAYLOAD) tlen = MAX_PACKET_PAYLOAD;
  memcpy(text, &data[5], tlen);
  text[tlen] = 0;
  for (char* p = text; *p; p++) {
    if (*p == '\t' || *p == '\n' || *p == '\r') *p = ' ';   // Trennzeichen im Log-Format
  }

  // Kanal anhand des Schlüssels bestimmen (Hash ist nur 1 Byte).
  int idx = -1;
  GwChannel c;
  for (int i = 0; i < channel_store::MAX_CHANNELS; i++) {
    if (channel_store::get(i, c) && memcmp(c.secret, channel.secret, sizeof(c.secret)) == 0) { idx = i; break; }
  }
  if (idx < 0) return;

  char line[LogBuffer::LINE_LEN];
  snprintf(line, sizeof(line), "%d\t%s\t%.1f\t%u\t%s", idx, c.name, packet->getSNR(),
           (unsigned)packet->getPathHashCount(), text);
  channel_log.push('M', timestamp, line);
  sm_poller::onChannelText(c.name, text);   // Antwort "ACK [Nr]" auf Alarme (E39)
  zbx_fwd::onChannelText(idx, c.name, text, packet->getSNR(), (uint8_t)packet->getPathHashCount());   // E41
}

bool GatewayMesh::sendChannelText(int channelIdx, const char* text, char* err, size_t errLen) {
  GwChannel c;
  if (!channel_store::get(channelIdx, c)) {
    snprintf(err, errLen, "Kanal %d unbekannt", channelIdx);
    return false;
  }
  if (!text || !*text) {
    snprintf(err, errLen, "leerer Text");
    return false;
  }

  // Aufbau wie BaseChatMesh::sendGroupMessage
  uint8_t temp[5 + MAX_TEXT_LEN + 32];
  uint32_t timestamp = getRTCClock()->getCurrentTimeUnique();
  memcpy(temp, &timestamp, 4);
  temp[4] = TXT_TYPE_PLAIN;
  int prefix_len = snprintf((char*)&temp[5], MAX_TEXT_LEN, "%s: ", getNodeName());
  int text_len = strlen(text);
  if (text_len + prefix_len > MAX_TEXT_LEN) text_len = MAX_TEXT_LEN - prefix_len;
  if (text_len <= 0) {
    snprintf(err, errLen, "Knotenname zu lang");
    return false;
  }
  memcpy(&temp[5 + prefix_len], text, text_len);
  temp[5 + prefix_len + text_len] = 0;

  mesh::GroupChannel gc;
  toGroupChannel(c, gc);
  mesh::Packet* pkt = createGroupDatagram(PAYLOAD_TYPE_GRP_TXT, gc, temp, 5 + prefix_len + text_len);
  if (!pkt) {
    snprintf(err, errLen, "kein freier Paketpuffer");
    return false;
  }

  // Scope (Region) wie RegionMap::getTransportKeysFor für öffentliche
  // Regionen: Schlüssel = sha256("#name"), die ersten 16 Byte.
  TransportKey scope;
  memset(&scope, 0, sizeof(scope));
  if (c.scope[0]) {
    char tag[sizeof(c.scope) + 1];
    snprintf(tag, sizeof(tag), "%s%s", c.scope[0] == '#' ? "" : "#", c.scope);
    uint8_t h[32];
    mesh::Utils::sha256(h, sizeof(h), (const uint8_t*)tag, strlen(tag));
    memcpy(scope.key, h, sizeof(scope.key));
  }
  uint8_t path_hash_size = getNodePrefs()->path_hash_mode + 1;
  sendFloodScoped(scope, pkt, 0, path_hash_size);

  char line[LogBuffer::LINE_LEN];
  char shown[MAX_TEXT_LEN + 1];
  memcpy(shown, text, text_len);
  shown[text_len] = 0;
  for (char* p = shown; *p; p++) if (*p == '\t' || *p == '\n' || *p == '\r') *p = ' ';
  snprintf(line, sizeof(line), "%d\t%s\t\t0\t%s: %s", channelIdx, c.name, getNodeName(), shown);
  channel_log.push('S', timestamp, line);
  // Eigene Nachrichten hört der Knoten nicht über Funk. Damit ein Gerät, das
  // selbst die Zabbix-Weiterleitung macht, auch seine eigenen Alarme meldet,
  // bekommt sie die Weiterleitung hier direkt (E41).
  char own[MAX_TEXT_LEN + 40];
  snprintf(own, sizeof(own), "%s: %s", getNodeName(), shown);
  zbx_fwd::onChannelText(channelIdx, c.name, own, 0, 0);
  return true;
}
