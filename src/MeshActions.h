#pragma once

#include <Arduino.h>

// Aufträge an den Mesh-Loop aus anderen Tasks (Web, Sensormeter-Poller).
// Wie bei der CLI (E4): nur Queue, MeshCore wird ausschließlich im loopTask
// angefasst.
namespace mesh_actions {
  enum class Origin : uint8_t { WEB, ALERT, ZBX };

  static const size_t MAX_TEXT = 160;   // MAX_TEXT_LEN in BaseChatMesh, inkl. "Name: "

  bool begin();
  // Thread-sicher. false = Warteschlange voll.
  bool sendChannel(int channelIdx, const char* text, Origin origin);
  // Nur loopTask: höchstens einen Auftrag pro Durchlauf ausführen.
  void process();
}
