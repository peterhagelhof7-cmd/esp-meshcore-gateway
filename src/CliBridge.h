#pragma once

#include <Arduino.h>

// Web -> offizielle MeshCore-CLI.
//
// Der Web-Task legt Befehle als Job (1..MAX_BATCH Befehle) ab (submit). Der
// loopTask arbeitet pro loop()-Durchlauf genau einen Befehl ab und ruft dort
// the_mesh.handleCommand() auf - wie die serielle CLI in simple_repeater, also
// mit sender_timestamp = 0 und denselben Rechten inkl. "Serial Only"-Befehlen
// (docs/entscheidungen.md E4/E11). Jede Antwort wird im Job gespeichert und
// kann per id abgeholt werden (Einstellungsseite, E16).
namespace cli_bridge {
  static const size_t MAX_CMD_LEN = 159;   // wie command[160] in simple_repeater
  static const size_t MAX_REPLY_LEN = 191;
  static const size_t MAX_BATCH = 40;

  bool begin();

  // Thread-sicher. Rückgabe: Job-id (> 0) oder 0, wenn alle Job-Plätze belegt
  // sind. quiet = reine get-Abfragen nicht in die Webkonsole schreiben.
  uint32_t submit(const char* const* cmds, size_t count, bool quiet);
  uint32_t submitOne(const char* cmd);

  // Thread-sicher. Hängt {"done":..,"replies":[..]} an out an.
  // false = id unbekannt (zu alt oder nie vergeben).
  bool jobJson(uint32_t id, String& out);

  // Nur aus dem loopTask. true, wenn dabei ein Job fertig wurde.
  bool process();

  // Befehle mit Geheimnis im Argument oder in der Antwort (Passwörter,
  // privater Schlüssel, Bridge-Secret).
  bool isSecretCommand(const char* cmd);
  // "set prv.key abc" -> "set prv.key ********", "password x" -> "password ********".
  // Abfragen (get ...) bleiben unverändert. Ergebnis in einem statischen
  // Puffer, nur aus dem loopTask verwenden.
  const char* maskedCommand(const char* cmd);

  // Zugriffsliste (ACL) aus /s_contacts: Text "PP <pubkey-hex>" je Zeile,
  // wie "get acl" seriell. Datei wird von MeshCore verzögert geschrieben.
  struct AclEntry { uint8_t perm; char pubkeyHex[65]; };
  int readAcl(AclEntry* out, int max);
}
