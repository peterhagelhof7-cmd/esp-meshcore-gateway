# Entscheidungslog

## E1 – Rolle: Repeater + Kanal-Senden (2026-10-13)
**Kontext:** In der offiziellen MeshCore-Firmware gibt es die CLI (`CommonCLI`) nur in Repeater,
Room Server und Sensor. Kanalnachrichten (Group-Text) sendet nur der Companion, und der hat keine
CLI, sondern das Binärprotokoll für die App.
**Entscheidung:** Als Basis dient der Repeater (`examples/simple_repeater`). Er wird um eine
Kanal-Tabelle (Name + PSK) und `sendChannelText()` ergänzt, gebaut aus `mesh::Mesh::createGroupDatagram()`
und `sendFlood()`.
**Begründung:** Die Webkonsole kann die offizielle CLI 1:1 nutzen, und die Sensormeter-Alarme können
in Kanäle gehen. Am MeshCore-Protokoll ändert sich nichts.
**Folge:** Room-Server-, Companion- und App-Anbindung (TCP 5000) sind nicht Teil des Scopes.
Sie können später als eigene Envs dazukommen.

## E2 – Upstream als Git-Submodul auf Release-Tag (2026-10-13)
**Entscheidung:** `meshcore-dev/MeshCore` kommt als Submodul unter `lib/` bzw. `external/` und wird auf
`repeater-v1.17.1` gepinnt. Eigener Code liegt nur in eigenen Dateien. Nötige Upstream-Änderungen
kommen als minimale Patches nach `patches/` und werden hier begründet.

## E3 – Einstellungen intern über die CLI (2026-10-13)
**Entscheidung:** Die Web-Einstellungsseite erzeugt `set …`-Befehle und liest Werte per `get …`.
Es gibt keinen eigenen Schreibzugriff auf `NodePrefs`.
**Begründung:** So gibt es nur eine Quelle der Wahrheit, und das Verhalten ist identisch zur offiziellen
Firmware, auch nach Upstream-Updates.

## E4 – Web ↔ Mesh nur über eine Queue (2026-10-13)
**Entscheidung:** ESPAsyncWebServer läuft im AsyncTCP-Task, MeshCore im loopTask. Alle Web-Aktionen
(CLI-Befehle, Kanal-Senden) gehen über eine FreeRTOS-Queue in den Mesh-Loop. Antworten kommen
über eine Rück-Queue bzw. den WebSocket zurück.
