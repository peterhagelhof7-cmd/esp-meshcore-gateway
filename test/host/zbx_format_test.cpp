// Host-Test für src/ZbxFormat.cpp (tools/host-test.ps1), E41.
#include <cstdio>
#include <cstring>
#include <string>

#include "ZbxFormat.h"

using namespace zbx;

static int fails = 0;
static void check(const std::string& name, bool ok, const std::string& info = "") {
  printf("%s %s%s%s\n", ok ? "PASS" : "FAIL", name.c_str(), info.empty() ? "" : " -> ", info.c_str());
  if (!ok) fails++;
}

int main() {
  setvbuf(stdout, nullptr, _IONBF, 0);

  // --- Quelle ---
  {
    char a[8], b[8], c[8];
    makeSrc('R', "Heizraum|sensor1.temperature|0", a);
    makeSrc('R', "Heizraum|sensor1.temperature|0", b);
    makeSrc('R', "Heizraum|sensor1.humidity|0", c);
    check("Quelle: R + 4 Hex", strlen(a) == 5 && a[0] == 'R', a);
    check("Quelle: stabil", !strcmp(a, b));
    check("Quelle: anderer Messwert anders", strcmp(a, c) != 0);
    check("CRC-16/CCITT-FALSE Referenz 123456789 = 29b1", crc16("123456789") == 0x29b1);
  }

  // --- Kennung bauen/lesen ---
  {
    Marker m{};
    strcpy(m.src, "R3fa2"); m.ev = 'A'; m.hasValue = true; m.value = 31.0f; m.ack = 3;
    char buf[64];
    buildMarker(m, buf, sizeof(buf));
    check("bauen: ganze Zahl ohne Komma", std::string(buf) == " #Z1;R3fa2;A;31;3", buf);
    m.value = 21.456f; m.ack = 0;
    buildMarker(m, buf, sizeof(buf));
    check("bauen: 2 Nachkommastellen, ohne ACK", std::string(buf) == " #Z1;R3fa2;A;21.46;", buf);
    m.value = -3.5f;
    buildMarker(m, buf, sizeof(buf));
    check("bauen: negativ", std::string(buf) == " #Z1;R3fa2;A;-3.5;", buf);
    m.hasValue = false; m.ev = 'H';
    buildMarker(m, buf, sizeof(buf));
    check("bauen: ohne Wert", std::string(buf) == " #Z1;R3fa2;H;;", buf);

    Marker r; size_t tl;
    const char* t1 = "[ACK 3] ALARM Heizraum: Temperatur 31°C (>30°C) #Z1;R3fa2;A;31;3";
    check("lesen: vollständig", parseMarker(t1, r, tl) && !strcmp(r.src, "R3fa2") && r.ev == 'A' && r.hasValue && r.value == 31.0f && r.ack == 3);
    check("lesen: Textlänge ohne Kennung", std::string(t1, tl) == "[ACK 3] ALARM Heizraum: Temperatur 31°C (>30°C)");
    check("lesen: ohne Wert/ACK", parseMarker("x #Z1;Pab12;O;;", r, tl) && r.ev == 'O' && !r.hasValue && r.ack == 0);
    check("lesen: letztes Vorkommen zählt", parseMarker("a #Z1;R0000;A;1; b #Z1;R1111;O;2;", r, tl) && !strcmp(r.src, "R1111"));
    check("lesen: keine Kennung", !parseMarker("ALARM Heizraum", r, tl));
    check("lesen: falsches Ereignis", !parseMarker("x #Z1;R1;X;1;", r, tl));
    check("lesen: Müll dahinter", !parseMarker("x #Z1;R1;A;1;2 und mehr", r, tl));
    check("lesen: ACK 100", !parseMarker("x #Z1;R1;A;1;100", r, tl));
    check("lesen: Wert kein Zahl", !parseMarker("x #Z1;R1;A;abc;", r, tl));
  }

  // --- Text + Kennung in 160 Byte ---
  {
    Marker m{};
    strcpy(m.src, "P1234"); m.ev = 'W'; m.ack = 12;
    std::string longText(200, 'x');
    char out[161];
    size_t n = appendMarker(longText.c_str(), m, out, 120);
    check("anhängen: passt in maxLen", n == 119 && strlen(out) == 119, std::to_string(n));
    Marker r; size_t tl;
    check("anhängen: Kennung bleibt vollständig", parseMarker(out, r, tl) && r.ack == 12 && r.ev == 'W');
    std::string umlaut = std::string(100, 'a') + "\xC3\xA4\xC3\xA4\xC3\xA4";   // äää
    n = appendMarker(umlaut.c_str(), m, out, 101 + 17);
    bool clean = true;
    // vor der Kennung darf kein halbes UTF-8-Zeichen stehen
    size_t mk = std::string(out).find(" #Z1;");
    if (mk > 0 && ((unsigned char)out[mk - 1] == 0xC3)) clean = false;
    check("anhängen: UTF-8 nicht zerschnitten", clean && mk != std::string::npos, out);
    n = appendMarker("kurz", m, out, sizeof(out));
    check("anhängen: kurzer Text unverändert", std::string(out) == "kurz #Z1;P1234;W;;12", out);
  }

  // --- Zustand ---
  check("Zustand A/E/W/F = ALARM", !strcmp(stateOf('A'), "ALARM") && !strcmp(stateOf('W'), "ALARM") && !strcmp(stateOf('F'), "ALARM"));
  check("Zustand O/N = OK", !strcmp(stateOf('O'), "OK") && !strcmp(stateOf('N'), "OK"));
  check("Zustand H/T = INFO", !strcmp(stateOf('H'), "INFO") && !strcmp(stateOf('T'), "INFO"));

  // --- Sender-Protokoll ---
  {
    uint8_t h[HEADER_LEN];
    writeHeader(h, 0x0102);
    check("Kopf: ZBXD 01 + LE-Länge", !memcmp(h, "ZBXD\x01\x02\x01\x00\x00\x00\x00\x00\x00", 13));
    check("Kopf lesen", readHeader(h, sizeof(h)) == 0x0102);
    check("Kopf lesen: Müll", readHeader((const uint8_t*)"HTTP/1.1 400 ", 13) == -1);
    int p, f;
    check("Antwort: processed 1", parseResponse("{\"response\":\"success\",\"info\":\"processed: 1; failed: 0; total: 1; seconds spent: 0.000055\"}", p, f) && p == 1 && f == 0);
    check("Antwort: failed 1 (Host unbekannt)", parseResponse("{\"response\":\"success\",\"info\":\"processed: 0; failed: 1; total: 1; seconds spent: 0.000021\"}", p, f) && p == 0 && f == 1);
    check("Antwort: failed", !parseResponse("{\"response\":\"failed\",\"info\":\"x\"}", p, f));
    char e[64];
    jsonEscape("a\"b\\c\nd\x01", e, sizeof(e));
    check("JSON-Escape", std::string(e) == "a\\\"b\\\\c\\nd\\u0001", e);
  }

  printf(fails ? "%d FEHLER\n" : "ALLE TESTS OK\n", fails);
  return fails ? 1 : 0;
}
