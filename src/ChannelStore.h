#pragma once

#include <Arduino.h>

// Gruppenkanäle des Gateways (M4, docs/entscheidungen.md E18).
//
// Schlüssel wie in der offiziellen Companion-App (docs/companion_protocol.md):
//   - Public:   fester, öffentlich bekannter 16-Byte-Schlüssel
//   - #hashtag: erste 16 Byte von sha256("#name")
//   - privat:   zufälliger oder importierter 16-Byte-Schlüssel
// Kanal-Hash = erstes Byte von sha256(secret) - so wie BaseChatMesh::addChannel.
//
// Optional pro Kanal ein Scope (Region). Gesendet wird dann mit Transport-Code
// aus sha256("#region") (wie RegionMap::getTransportKeysFor für öffentliche
// Regionen), ohne Scope als normaler (unscoped) Flood.
//
// Persistenz in NVS (Namespace "gw"), unabhängig von MeshCore-"erase".
// Thread-sicher: Web-Task ändert, loopTask liest beim Empfangen/Senden.
struct GwChannel {
  char name[32];
  uint8_t secret[16];
  uint8_t hash;
  char scope[31];   // leer = unscoped
  bool used;
};

namespace channel_store {
  static const int MAX_CHANNELS = 8;

  enum class Kind : uint8_t { PUBLIC, HASHTAG, PRIVATE };

  void begin();

  // Kopie aller Plätze (used=false = frei). Thread-sicher.
  void list(GwChannel out[MAX_CHANNELS]);
  bool get(int idx, GwChannel& out);

  // Rückgabe: Index oder -1 (err enthält den Grund).
  // keyText: nur für PRIVATE - Base64 oder 32 Hex-Zeichen; leer = zufällig.
  int add(Kind kind, const char* name, const char* keyText, const char* scope, String& err);
  bool remove(int idx);
  bool setScope(int idx, const char* scope, String& err);

  // Für GatewayMesh::searchChannelsByHash (loopTask). Liefert Indizes.
  int findByHash(uint8_t hash, int idx[], int max);

  // Hilfen
  Kind kindOf(const GwChannel& c);
  String keyBase64(const GwChannel& c);
  String keyHex(const GwChannel& c);
  bool validScope(const char* s);
}
