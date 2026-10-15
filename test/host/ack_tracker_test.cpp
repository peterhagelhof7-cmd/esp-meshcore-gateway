// Host-Test für src/AckTracker.cpp (tools/host-test.ps1), E39.
#include <cstdio>
#include <cstring>
#include <string>

#include "AckTracker.h"

using namespace ack;

static int fails = 0;
static void check(const std::string& name, bool ok, const std::string& info = "") {
  printf("%s %s%s%s\n", ok ? "PASS" : "FAIL", name.c_str(), info.empty() ? "" : " -> ", info.c_str());
  if (!ok) fails++;
}
static const uint32_t MIN = 60000UL;

int main() {
  setvbuf(stdout, nullptr, _IONBF, 0);

  // --- ACK-Texte ---
  {
    int id = -1;
    check("parse: ACK", Tracker::parseAck("ACK", id) && id == 0);
    check("parse: ack (klein)", Tracker::parseAck("ack", id) && id == 0);
    check("parse: ACK 3", Tracker::parseAck("ACK 3", id) && id == 3);
    check("parse: ACK #12", Tracker::parseAck("ACK #12", id) && id == 12);
    check("parse: Ack3!", Tracker::parseAck("Ack3!", id) && id == 3);
    check("parse: mit App-Erwähnung", Tracker::parseAck("@[DE-XX-ORT02-GW] ACK 7", id) && id == 7);
    check("parse: Leerzeichen drumherum", Tracker::parseAck("  ACK 4  ", id) && id == 4);
    check("parse: Satz mit ack -> nein", !Tracker::parseAck("ACK ich schau mal", id));
    check("parse: acknowledged -> nein", !Tracker::parseAck("acknowledged", id));
    check("parse: ACK 0 -> nein", !Tracker::parseAck("ACK 0", id));
    check("parse: ACK 100 -> nein", !Tracker::parseAck("ACK 100", id));
    check("parse: OK -> nein", !Tracker::parseAck("OK", id));
    check("parse: leer -> nein", !Tracker::parseAck("", id) && !Tracker::parseAck(nullptr, id));
  }

  // --- Wiederholungen nach 5, 15, 30 min ---
  {
    Tracker t;
    uint8_t id = t.open(Kind::RULE, 2, "haus-alarm", "ALARM Halle: Temperatur 31.0°C (>30.0°C)", 1000);
    check("open: Nummer 1", id == 1);
    check("open: offen für Regel 2", t.openIdOf(Kind::RULE, 2) == 1 && t.openIdOf(Kind::PING, 2) == 0);
    char buf[200];
    Tracker::format(t.at(0), false, buf, sizeof(buf));
    check("Text erste Meldung", std::string(buf) == "[ACK 1] ALARM Halle: Temperatur 31.0°C (>30.0°C)", buf);
    check("vor 5 min nichts fällig", t.due(1000 + 5 * MIN - 1) == -1);
    int i = t.due(1000 + 5 * MIN);
    check("nach 5 min fällig", i == 0);
    Tracker::format(t.at(i), true, buf, sizeof(buf));
    check("Text WDH 1/3", std::string(buf) == "[ACK 1] WDH 1/3: ALARM Halle: Temperatur 31.0°C (>30.0°C)", buf);
    t.repeated(i, 1000 + 5 * MIN);
    check("danach bis 15 min nichts", t.due(1000 + 15 * MIN - 1) == -1);
    i = t.due(1000 + 15 * MIN);
    check("nach 15 min fällig", i == 0);
    t.repeated(i, 1000 + 15 * MIN);
    t.markCleared(Kind::RULE, 2);
    i = t.due(1000 + 30 * MIN);
    check("nach 30 min fällig", i == 0);
    Tracker::format(t.at(i), true, buf, sizeof(buf));
    check("Text WDH 3/3 mit inzw. OK", std::string(buf).find("[ACK 1] WDH 3/3 (inzw. OK): ALARM") == 0, buf);
    t.repeated(i, 1000 + 30 * MIN);
    check("nach 3 Wiederholungen nichts mehr", t.due(1000 + 300 * MIN) == -1);
    check("aber noch bestätigbar", t.openIdOf(Kind::RULE, 2) == 1);
    check("nach 24 h vergessen", t.due(1000 + 24 * 60 * MIN) == -1 && t.openIdOf(Kind::RULE, 2) == 0);
  }

  // --- Sendepause: nicht im Block nachholen ---
  {
    Tracker t;
    t.open(Kind::RULE, 0, "a", "x", 0);
    int i = t.due(40 * MIN);   // erst nach 40 min wieder senden möglich
    check("Pause: eine fällig", i == 0);
    t.repeated(i, 40 * MIN);
    check("Pause: zweite nicht sofort", t.due(40 * MIN + 1000) == -1);
    check("Pause: zweite nach 5 min Abstand", t.due(45 * MIN) == 0);
    t.postpone(0, 45 * MIN);
    check("postpone: 5 min später erneut, Stufe bleibt", t.due(49 * MIN) == -1 && t.due(50 * MIN) == 0 && t.at(0).repeats == 1);
  }

  // --- Bestätigen ---
  {
    Tracker t;
    uint8_t a = t.open(Kind::RULE, 0, "haus-alarm", "A", 0);
    uint8_t b = t.open(Kind::PING, 1, "haus-alarm", "B", 0);
    uint8_t c = t.open(Kind::RULE, 3, "anderer", "C", 0);
    uint8_t ids[8];
    check("ACK in fremdem Text -> -1", t.acknowledge("haus-alarm", "hallo", ids, 8) == -1);
    check("ACK mit falscher Nummer -> 0", t.acknowledge("haus-alarm", "ACK 99", ids, 8) == 0);
    std::string ackB = "ACK " + std::to_string(b);
    int n = t.acknowledge("haus-alarm", ackB.c_str(), ids, 8);
    check("ACK <Nr> bestätigt genau einen", n == 1 && ids[0] == b && t.openIdOf(Kind::PING, 1) == 0 && t.openIdOf(Kind::RULE, 0) == a);
    n = t.acknowledge("haus-alarm", "ACK", ids, 8);
    check("ACK ohne Nr bestätigt alle im Kanal", n == 1 && ids[0] == a && t.openIdOf(Kind::RULE, 0) == 0);
    check("anderer Kanal bleibt offen", t.openIdOf(Kind::RULE, 3) == c);
    check("Bestätigtes wird nicht wiederholt", t.due(10 * MIN) != -1 && t.at(t.due(10 * MIN)).id == c);
    check("Web-Bestätigung", t.acknowledgeId(c) && t.openIdOf(Kind::RULE, 3) == 0 && !t.acknowledgeId(c));
  }

  // --- Neuer Alarm derselben Quelle ersetzt, Nummern, Verdrängen ---
  {
    Tracker t;
    uint8_t a = t.open(Kind::RULE, 0, "k", "A", 0);
    uint8_t a2 = t.open(Kind::RULE, 0, "k", "A2", 1000);
    check("gleiche Quelle: ersetzt mit neuer Nummer", a2 != a && t.openIdOf(Kind::RULE, 0) == a2);
    int used = 0;
    for (int i = 0; i < MAX_OPEN; i++) used += t.at(i).used;
    check("gleiche Quelle: nur ein Eintrag", used == 1);
    for (int s = 1; s < MAX_OPEN; s++) t.open(Kind::RULE, s, "k", "x", 2000 + s);
    t.open(Kind::PING, 0, "k", "neu", 9000);
    check("voll: ältester (Regel 0) verdrängt", t.openIdOf(Kind::RULE, 0) == 0 && t.openIdOf(Kind::PING, 0) != 0);
    Tracker u;
    for (int k = 0; k < 120; k++) u.open(Kind::RULE, 0, "k", "x", k);
    check("Nummern laufen 1..99 um", u.openIdOf(Kind::RULE, 0) == 21);
  }

  // --- Konfiguration geändert ---
  {
    Tracker t;
    t.open(Kind::RULE, 0, "k", "A", 0);
    t.open(Kind::RULE, 2, "k", "C", 0);
    t.open(Kind::PING, 0, "k", "P", 0);
    int8_t map[3] = {-1, 0, 1};   // Regel 0 gelöscht, 1->0, 2->1
    t.remap(Kind::RULE, map, 3);
    check("remap: gelöschte Regel weg", t.openIdOf(Kind::RULE, 0) == 0 || t.at(0).src != 0);
    check("remap: Regel 2 -> 1", t.openIdOf(Kind::RULE, 1) != 0 && t.openIdOf(Kind::RULE, 2) == 0);
    check("remap: Ping unberührt", t.openIdOf(Kind::PING, 0) != 0);
  }

  // --- Länge ---
  {
    Tracker t;
    std::string longText(300, 'x');
    t.open(Kind::RULE, 0, "k", longText.c_str(), 0);
    check("Text gekürzt auf TEXT_LEN-1", strlen(t.at(0).text) == TEXT_LEN - 1);
    char small[20];
    size_t n = Tracker::format(t.at(0), true, small, sizeof(small));
    check("format kürzt sicher", n == sizeof(small) - 1 && strlen(small) == sizeof(small) - 1);
  }

  printf(fails ? "%d FEHLER\n" : "ALLE TESTS OK\n", fails);
  return fails ? 1 : 0;
}
