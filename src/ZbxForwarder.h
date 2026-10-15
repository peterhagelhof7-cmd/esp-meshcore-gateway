#pragma once

#include <Arduino.h>

// Zabbix-Weiterleitung (E41): Dieses Gerät als "Gegenseite" eines oder
// mehrerer Gateways. Empfangene Kanalnachrichten mit Zabbix-Kennung
// ("... #Z1;Quelle;Ereignis;Wert;ACK") gehen per Zabbix-Sender-Protokoll
// (Trapper, TCP 10051) an Proxy/Server:
//   Host = Präfix + Gateway-Name (wie dessen WLAN-Hostname, z. B. de-by-ort01-gw)
//   Key  = meshcore.event, Wert = JSON (src, ev, state, value, ack, text, ...)
// Erst wenn Zabbix den Wert angenommen hat ("processed: 1"), sendet der
// Empfänger im selben Kanal "@[Gateway] ACK n" - das Gateway hört dann mit
// den Wiederholungen auf. Kein Zabbix -> kein ACK -> Gateway wiederholt.
//
// Eigener Task auf Core 0 (TCP blockiert), Übergabe aus dem loopTask per
// Queue, Antwort-ACK über mesh_actions (E4).
namespace zbx_fwd {

void begin();

// loopTask: empfangene Kanalnachricht ("Absender: Text")
void onChannelText(int channelIdx, const char* channelName, const char* text, float snr, uint8_t hops);

// Web (thread-sicher)
void configJson(String& out, bool maskNothing = true);
bool setFromJson(const char* json, size_t len, String& err);
void statusJson(String& out);
// Testwert (Lebenszeichen) sofort senden; Ergebnis erscheint im Status
void requestTest();

}  // namespace zbx_fwd
