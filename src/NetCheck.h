#pragma once

#include <Arduino.h>
#include <IPAddress.h>

// Netz-Prüfungen für die feste IP (E40) und den Ping-Check (E25).
//
// probe(): Antwortet ein Gerät unter dieser Adresse? Erst ARP (findet auch
// Geräte mit Firewall, die Ping verwerfen), dann Ping. Läuft in einem eigenen
// kurzlebigen Task (blockiert ca. 3 s); Ergebnis per result() abholen.
// Nur EIN Auftrag gleichzeitig (Web-Prüfung und Boot-Prüfung teilen sich das).
namespace net_check {

enum class State : uint8_t {
  IDLE,
  RUNNING,
  PRESENT,       // Adresse antwortet (ARP oder Ping) -> belegt bzw. erreichbar
  ABSENT,        // keine Antwort
  OWN,           // eigene aktuelle Adresse
  UNCHECKABLE,   // nicht prüfbar (kein WLAN-Client, anderes Subnetz)
};

// false = schon ein Auftrag aktiv
bool probe(IPAddress ip);
// Zustand; detail z. B. MAC-Adresse oder Grund
State result(char* detail, size_t detailLen);
const char* stateName(State s);

// Ping (3 Echos, blockiert bis ca. 1,5 s). true = mindestens eine Antwort.
bool ping(const char* host, uint32_t& rttMs, char* ipOut, size_t ipLen, char* err, size_t errLen);

// Gültige Netzmaske (zusammenhängende Einsen, /8../30)
bool validMask(IPAddress m);

}  // namespace net_check
