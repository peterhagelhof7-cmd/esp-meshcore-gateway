// Host-Test für src/BatteryLogic.cpp (tools/host-test.ps1), E46.
#include <cstdio>
#include <string>

#include "BatteryLogic.h"

using namespace batt;

static int fails = 0;
static void check(const std::string& name, bool ok, const std::string& info = "") {
  printf("%s %s%s%s\n", ok ? "PASS" : "FAIL", name.c_str(), info.empty() ? "" : " -> ", info.c_str());
  if (!ok) fails++;
}
static std::string st(const Monitor& m) { return stateKey(m.state()); }

int main() {
  setvbuf(stdout, nullptr, _IONBF, 0);

  // Ladezustand
  check("Prozent: 4,20 V = 100", percentFromMv(4200) == 100);
  check("Prozent: über 4,20 V = 100", percentFromMv(4300) == 100);
  check("Prozent: 3,84 V = 50", percentFromMv(3840) == 50);
  check("Prozent: 3,27 V = 0", percentFromMv(3270) == 0 && percentFromMv(3000) == 0);
  check("Prozent: zwischen Stützpunkten linear", percentFromMv(3925) > 65 && percentFromMv(3925) < 70,
        std::to_string(percentFromMv(3925)));
  {
    bool mono = true;
    for (int mv = 3200; mv < 4250; mv += 5) if (percentFromMv(mv) > percentFromMv(mv + 5)) mono = false;
    check("Prozent: monoton steigend", mono);
  }

  // Ruhiger Akku -> vorhanden, erst ab MIN_SAMPLES
  {
    Monitor m;
    m.add(3900);
    m.add(3905);
    check("vor MIN_SAMPLES: wird geprüft", m.state() == State::Checking, st(m));
    m.add(3895);
    check("Akku ruhig 3,9 V: vorhanden", m.state() == State::Present, st(m));
    check("Mittelwert", m.avgMv() == 3900, std::to_string(m.avgMv()));
    for (int i = 0; i < 30; i++) m.add(i % 2 ? 4180 : 4120);   // Laden an USB, leichte Welligkeit
    check("Akku lädt an USB (4,12-4,18 V): bleibt vorhanden", m.state() == State::Present, st(m));
  }

  // Ohne Akku: Lader springt
  {
    Monitor m;
    const uint16_t seq[] = {4200, 3600, 4400, 2900, 4300, 4100, 3200};
    for (uint16_t v : seq) m.add(v);
    check("springende Spannung: kein AKKU", m.state() == State::Absent, st(m));
    check("Grund: schwankt", m.reason() == Reason::Unstable);
  }
  // Ohne Akku: Ladeausgang über Ladeschluss
  {
    Monitor m;
    for (int i = 0; i < 5; i++) m.add(4480);
    check("4,48 V ruhig: kein AKKU (zu hoch)", m.state() == State::Absent && m.reason() == Reason::TooHigh, st(m));
  }
  // Ohne Akku: nichts am Teiler
  {
    Monitor m;
    for (int i = 0; i < 5; i++) m.add(120);
    check("0,12 V: kein AKKU (zu niedrig)", m.state() == State::Absent && m.reason() == Reason::TooLow, st(m));
  }

  // Entprellen: ein einzelner Ausreißer kippt den Zustand nicht, Abziehen schon
  {
    Monitor m;
    for (int i = 0; i < 12; i++) m.add(3950);
    m.add(4300);   // Spread 350 -> Kandidat "kein AKKU", erst 1×
    check("Ausreißer 1×: bleibt vorhanden", m.state() == State::Present, st(m));
    m.add(3950);   // Ausreißer noch im Fenster -> 2×
    check("Ausreißer 2×: bleibt vorhanden", m.state() == State::Present, st(m));
    m.add(3950);   // 3× -> Wechsel
    check("Ausreißer 3× im Fenster: kein AKKU", m.state() == State::Absent, st(m));
    for (int i = 0; i < 12; i++) m.add(3950);   // Ausreißer aus dem Fenster
    check("danach wieder ruhig: AKKU vorhanden", m.state() == State::Present, st(m));
  }
  {
    Monitor m;
    for (int i = 0; i < 12; i++) m.add(4480);
    check("Start ohne Akku", m.state() == State::Absent, st(m));
    for (int i = 0; i < 20; i++) m.add(3980);   // Akku angesteckt
    check("Akku angesteckt: nach Fensterwechsel vorhanden", m.state() == State::Present, st(m));
  }

  // Texte
  check("Text vorhanden", std::string(stateText(State::Present)) == "AKKU vorhanden");
  check("Text kein Akku", std::string(stateText(State::Absent)) == "kein AKKU");

  printf("%s (%d Fehler)\n", fails ? "FEHLER" : "OK", fails);
  return fails ? 1 : 0;
}
