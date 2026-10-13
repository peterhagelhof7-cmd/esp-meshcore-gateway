// Host-Test für src/FwMarker.cpp (ohne Hardware, mit g++ gebaut):
//   tools\host-test.ps1   bzw.
//   g++ -std=c++17 -O1 -I src test/host/fw_marker_test.cpp src/FwMarker.cpp -o fw_marker_test
//   fw_marker_test <firmware.bin> <erwartete-projekt-id> <erwartete-version>
//
// Prüft den Scanner gegen die ECHTE firmware.bin mit vielen Chunk-Größen
// (Lehre aus der Sensormeter-Familie: NUL-Bytes, große HTTP-Chunks, Marker
// über Chunkgrenzen), dazu Negativ- und Grenzfälle und den Versionsvergleich.
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "FwMarker.h"

static int fails = 0;
static void check(const char* name, bool ok, const std::string& info = "") {
  printf("%s %s%s%s\n", ok ? "PASS" : "FAIL", name, info.empty() ? "" : " -> ", info.c_str());
  if (!ok) fails++;
}

static bool scan(const std::vector<uint8_t>& data, size_t chunk, std::string& id, std::string& ver) {
  FwMarkerScanner s;
  s.reset();
  for (size_t off = 0; off < data.size(); off += chunk) {
    size_t n = std::min(chunk, data.size() - off);
    s.feed(data.data() + off, n);
  }
  id = s.projectId();
  ver = s.version();
  return s.found();
}

static std::vector<uint8_t> bytes(const std::string& s) { return std::vector<uint8_t>(s.begin(), s.end()); }

int main(int argc, char** argv) {
  if (argc < 4) {
    fprintf(stderr, "Aufruf: %s <firmware.bin> <projekt-id> <version>\n", argv[0]);
    return 2;
  }
  FILE* f = fopen(argv[1], "rb");
  if (!f) { perror(argv[1]); return 2; }
  std::vector<uint8_t> fw;
  uint8_t buf[65536];
  size_t n;
  while ((n = fread(buf, 1, sizeof(buf), f)) > 0) fw.insert(fw.end(), buf, buf + n);
  fclose(f);
  printf("firmware.bin: %zu Bytes\n", fw.size());

  // 1) Echte Firmware, viele Chunk-Größen (1 Byte bis ganze Datei)
  const size_t chunks[] = {1, 2, 3, 7, 13, 64, 511, 512, 1024, 1436, 2048, 4096, 5744, 8192, 65536, fw.size()};
  for (size_t c : chunks) {
    std::string id, ver;
    bool ok = scan(fw, c, id, ver) && id == argv[2] && ver == argv[3];
    check(("echte Firmware, Chunk " + std::to_string(c)).c_str(), ok, id + " / " + ver);
  }

  // 2) Marker genau über jede mögliche Chunkgrenze legen
  {
    std::string m = std::string("MCGW-FW-ID:") + argv[2] + ":" + argv[3] + ":MCGW-FW-END";
    std::vector<uint8_t> d(300, 0);
    d.insert(d.begin() + 100, m.begin(), m.end());
    bool all = true;
    for (size_t split = 90; split < 100 + m.size() + 5; split++) {
      FwMarkerScanner s;
      s.reset();
      s.feed(d.data(), split);
      s.feed(d.data() + split, d.size() - split);
      if (!s.found() || std::string(s.version()) != argv[3]) { all = false; break; }
    }
    check("Marker über jede Chunkgrenze", all);
  }

  // 3) Negativfälle
  {
    std::string id, ver;
    std::vector<uint8_t> noise(200000);
    for (size_t i = 0; i < noise.size(); i++) noise[i] = (uint8_t)((i * 2654435761u) >> 13);
    check("Zufallsdaten: kein Marker", !scan(noise, 1436, id, ver));
    check("Textdatei: kein Marker", !scan(bytes("Hallo, das ist keine Firmware\n"), 7, id, ver));
    check("Nur Prefix + NUL (Köder wie im Binary)", !scan(bytes(std::string("MCGW-FW-ID:\0:MCGW-FW-END", 24)), 1, id, ver));
    check("Abgeschnittener Marker", !scan(bytes("MCGW-FW-ID:MESHCORE-GW-HELTEC-V4:0.4.0:MCGW-FW-EN"), 5, id, ver));
    check("Marker ohne Version", !scan(bytes("MCGW-FW-ID:MESHCORE-GW-HELTEC-V4:MCGW-FW-END"), 5, id, ver));
    check("Ungültiges Zeichen im Feld", !scan(bytes("MCGW-FW-ID:MESH CORE:1.0.0:MCGW-FW-END"), 5, id, ver));
    // Köder direkt vor echtem Marker
    bool ok = scan(bytes(std::string("xxMCGW-FW-ID:\0yyMCGW-FW-ID:MESHCORE-GW-HELTEC-V4-R8:1.2.3:MCGW-FW-END", 70)), 3, id, ver);
    check("Köder + echter Marker (R8)", ok && id == "MESHCORE-GW-HELTEC-V4-R8" && ver == "1.2.3", id + " / " + ver);
    // Doppeltes M vor Prefix
    ok = scan(bytes("MMCGW-FW-ID:A:1.0.0:MCGW-FW-END"), 1, id, ver);
    check("MM-Prefix", ok && id == "A" && ver == "1.0.0", id + " / " + ver);
  }

  // 4) Versionsvergleich
  struct { const char *a, *b; int sign; } v[] = {
    {"0.4.0", "0.3.0", 1}, {"0.3.0", "0.4.0", -1}, {"0.4.0", "0.4.0", 0},
    {"1.0.0", "0.9.9", 1}, {"0.10.0", "0.9.0", 1}, {"0.4.0", "0.4.0-rc3", 1},
    {"0.4.0-rc2", "0.4.0-rc10", -1}, {"0.4.1-rc1", "0.4.0", 1}, {"v0.4.0", "0.4.0", 0},
    {"murks", "0.0.1", -1},
  };
  for (auto& t : v) {
    int r = fwVersionCompare(t.a, t.b);
    int s = (r > 0) - (r < 0);
    check((std::string("Version ") + t.a + " vs " + t.b).c_str(), s == t.sign, std::to_string(r));
  }

  printf(fails ? "%d FEHLER\n" : "ALLE TESTS OK\n", fails);
  return fails ? 1 : 0;
}
