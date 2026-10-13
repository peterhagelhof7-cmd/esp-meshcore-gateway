#include "GatewayMesh.h"

#include "LogBuffer.h"

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
