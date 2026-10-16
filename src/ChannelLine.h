#pragma once

#include <stddef.h>

// Zeile aus channel_log zerlegen (Kanal-API, E43). Ohne Arduino-Abhängigkeit,
// auf dem PC testbar (test/host/channel_line_test.cpp).
//
// Format (GatewayMesh): "<idx>\t<Kanalname>\t<SNR>\t<Hops>\t<Absender>: <Text>",
// SNR leer bei selbst gesendeten Nachrichten. Absender = Name, den der Sender
// vor die Nachricht stellt (MeshCore-Konvention "Name: Text"); fehlt ": ",
// bleibt der Absender leer und alles ist Text.
namespace chan_line {

struct Parsed {
  int idx;
  const char* channel;   // zeigen in den Arbeitspuffer
  bool hasSnr;
  float snr;
  int hops;
  const char* sender;
  const char* text;
};

// buf wird verändert (Tabs/Trenner durch Nullen ersetzt). false = kein gültiges Format.
bool parse(char* buf, Parsed& out);

}  // namespace chan_line
