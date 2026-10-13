#include "CliBridge.h"

#include "GatewayMesh.h"
#include "LogBuffer.h"

struct CliCommand {
  char text[cli_bridge::MAX_CMD_LEN + 1];
};

static QueueHandle_t s_queue = nullptr;

bool cli_bridge::begin() {
  if (!s_queue) s_queue = xQueueCreate(8, sizeof(CliCommand));
  return s_queue != nullptr;
}

bool cli_bridge::submit(const char* cmd) {
  if (!s_queue) return false;
  CliCommand c;
  strlcpy(c.text, cmd, sizeof(c.text));
  return xQueueSend(s_queue, &c, 0) == pdTRUE;
}

void cli_bridge::process() {
  if (!s_queue) return;
  CliCommand c;
  // Höchstens einen Befehl pro loop()-Durchlauf, damit der Mesh-Loop nicht
  // durch eine Befehlsserie aufgehalten wird.
  if (xQueueReceive(s_queue, &c, 0) != pdTRUE) return;

  uint32_t now = the_mesh.getRTCClock()->getCurrentTime();
  console_log.push('c', now, c.text);

  // Upstream-Antworten passen in 160 Byte (serieller Puffer in simple_repeater),
  // hier mit Reserve.
  char reply[512];
  reply[0] = 0;
  the_mesh.handleCommand(0, c.text, reply);   // sender_timestamp 0 = wie serielle CLI
  Serial.printf("[WEB-CLI] %s\n", c.text);
  if (reply[0]) {
    console_log.pushLines('r', now, reply);
    Serial.print("  -> "); Serial.println(reply);
  }
}
