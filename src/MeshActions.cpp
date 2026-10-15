#include "MeshActions.h"

#include "GatewayMesh.h"
#include "LogBuffer.h"

struct MeshAction {
  int8_t channel;
  mesh_actions::Origin origin;
  char text[mesh_actions::MAX_TEXT + 1];
};

static QueueHandle_t s_queue = nullptr;

bool mesh_actions::begin() {
  if (!s_queue) s_queue = xQueueCreate(8, sizeof(MeshAction));
  return s_queue != nullptr;
}

bool mesh_actions::sendChannel(int channelIdx, const char* text, Origin origin) {
  if (!s_queue) return false;
  MeshAction a;
  a.channel = (int8_t)channelIdx;
  a.origin = origin;
  strlcpy(a.text, text ? text : "", sizeof(a.text));
  return xQueueSend(s_queue, &a, 0) == pdTRUE;
}

void mesh_actions::process() {
  if (!s_queue) return;
  MeshAction a;
  if (xQueueReceive(s_queue, &a, 0) != pdTRUE) return;

  char err[96];
  err[0] = 0;
  if (!the_mesh.sendChannelText(a.channel, a.text, err, sizeof(err))) {
    char line[LogBuffer::LINE_LEN];
    snprintf(line, sizeof(line), "Senden fehlgeschlagen (%s): %s",
             a.origin == Origin::ALERT ? "Alarm" : (a.origin == Origin::ZBX ? "Zabbix-ACK" : "Web"), err);
    console_log.push('s', the_mesh.getRTCClock()->getCurrentTime(), line);
  }
}
