// Host-Test für src/ChannelLine.cpp (tools/host-test.ps1), E43.
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

#include "ChannelLine.h"

static int fails = 0;
static void check(const std::string& name, bool ok, const std::string& info = "") {
  printf("%s %s%s%s\n", ok ? "PASS" : "FAIL", name.c_str(), info.empty() ? "" : " -> ", info.c_str());
  if (!ok) fails++;
}

int main() {
  setvbuf(stdout, nullptr, _IONBF, 0);
  chan_line::Parsed p;
  {
    char b[] = "2\t#rhein-main\t-3.5\t4\tFunker1: Hallo: wie geht's?";
    bool ok = chan_line::parse(b, p);
    check("Empfangen: gültig", ok);
    check("Empfangen: Index + Kanal", p.idx == 2 && !strcmp(p.channel, "#rhein-main"), p.channel);
    check("Empfangen: SNR + Hops", p.hasSnr && std::fabs(p.snr + 3.5f) < 0.01f && p.hops == 4);
    check("Empfangen: Absender bis zum ersten \": \"", !strcmp(p.sender, "Funker1"), p.sender);
    check("Empfangen: Text behält weitere Doppelpunkte", !strcmp(p.text, "Hallo: wie geht's?"), p.text);
  }
  {
    char b[] = "0\thaus-alarm\t\t0\tDE-XX-ORT02-GW: [ACK 3] ALARM Heizraum";
    check("Gesendet: gültig, ohne SNR", chan_line::parse(b, p) && !p.hasSnr && p.hops == 0 && p.idx == 0);
    check("Gesendet: Absender = Gateway", !strcmp(p.sender, "DE-XX-ORT02-GW") && !strcmp(p.text, "[ACK 3] ALARM Heizraum"));
  }
  {
    char b[] = "1\tPublic\t7.25\t1\tohne Absender";
    check("Ohne \": \" -> Absender leer, alles Text", chan_line::parse(b, p) && !*p.sender && !strcmp(p.text, "ohne Absender"));
  }
  {
    char b[] = "1\tPublic\t7.25\t1\t";
    check("Leerer Text ok", chan_line::parse(b, p) && !*p.text);
  }
  {
    char a[] = "x\tPublic\t1\t1\ttext";
    check("Index keine Zahl -> ungültig", !chan_line::parse(a, p));
    char b[] = "1\tPublic\t1";
    check("Zu wenige Felder -> ungültig", !chan_line::parse(b, p));
    char c[] = "";
    check("Leer -> ungültig", !chan_line::parse(c, p));
  }
  {
    const char* a = "Warnung";
    check("utf8Cut: kurz bleibt", chan_line::utf8Cut(a, 20) == 7);
    check("utf8Cut: ASCII exakt", chan_line::utf8Cut(a, 4) == 4);
    const char* b = "Sturmb\xC3\xB6" "en";   // "Sturmböen", ö = 2 Byte ab Position 6
    check("utf8Cut: nicht im ö", chan_line::utf8Cut(b, 7) == 6, std::to_string(chan_line::utf8Cut(b, 7)));
    check("utf8Cut: hinter ö", chan_line::utf8Cut(b, 8) == 8);
    const char* c = "\xE2\x9A\xA0 Achtung";   // ⚠ = 3 Byte
    check("utf8Cut: 3-Byte-Zeichen ganz weg", chan_line::utf8Cut(c, 2) == 0);
    const char* d = "x\xF0\x9F\x8C\xA9";        // Emoji 4 Byte
    check("utf8Cut: 4-Byte-Emoji", chan_line::utf8Cut(d, 4) == 1 && chan_line::utf8Cut(d, 5) == 5);
    check("utf8Cut: nullptr", chan_line::utf8Cut(nullptr, 5) == 0);
  }
  printf(fails ? "%d FEHLER\n" : "ALLE OK\n", fails);
  return fails ? 1 : 0;
}
