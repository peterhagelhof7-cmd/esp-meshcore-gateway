// Host-Test für src/RuleEngine.cpp und src/SmJson.cpp (tools/host-test.ps1).
// Fixtures in test/host/fixtures: sm_live_*.json = echte Antworten zweier
// Sensormeter im LAN (2026-10-14), die übrigen nach den Firmware-Quellen.
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

#include "RuleEngine.h"
#include "SmJson.h"

using namespace rules;

static int fails = 0;
static void check(const std::string& name, bool ok, const std::string& info = "") {
  printf("%s %s%s%s\n", ok ? "PASS" : "FAIL", name.c_str(), info.empty() ? "" : " -> ", info.c_str());
  if (!ok) fails++;
}
static const char* ev(Event e) {
  switch (e) { case Event::ALERT: return "ALERT"; case Event::REMIND: return "REMIND"; case Event::RECOVER: return "RECOVER"; default: return "NONE"; }
}
static std::string readFile(const std::string& p) {
  std::ifstream f(p, std::ios::binary);
  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

int main(int argc, char** argv) {
  setvbuf(stdout, nullptr, _IONBF, 0);
  std::string fx = argc > 1 ? argv[1] : "test/host/fixtures";

  // --- Grenzwert über, sofort, mit Hysterese + Entwarnung ---
  {
    RuleParams p{Op::ABOVE, 30.0f, 1.0f, 0, 0, true};
    RuleState s;
    check("ABOVE: 29.9 -> nichts", evaluate(p, s, 29.9f, true, 0) == Event::NONE);
    check("ABOVE: 30.0 ist nicht über -> nichts", evaluate(p, s, 30.0f, true, 1000) == Event::NONE);
    check("ABOVE: 30.1 -> ALERT", evaluate(p, s, 30.1f, true, 2000) == Event::ALERT);
    check("ABOVE: 35 bleibt ALERT ohne Erinnerung", evaluate(p, s, 35.0f, true, 3000) == Event::NONE);
    check("ABOVE: 29.5 innerhalb Hysterese -> nichts", evaluate(p, s, 29.5f, true, 4000) == Event::NONE && s.state == State::ALERT);
    check("ABOVE: 29.0 = Grenze - Hysterese -> RECOVER", evaluate(p, s, 29.0f, true, 5000) == Event::RECOVER && s.state == State::NORMAL);
  }
  // --- Unter, ohne Entwarnung ---
  {
    RuleParams p{Op::BELOW, 5.0f, 0.5f, 0, 0, false};
    RuleState s;
    check("BELOW: 4.9 -> ALERT", evaluate(p, s, 4.9f, true, 0) == Event::ALERT);
    Event e = evaluate(p, s, 6.0f, true, 1000);
    check("BELOW: zurück ohne notifyRecovery -> NONE, Zustand NORMAL", e == Event::NONE && s.state == State::NORMAL, ev(e));
  }
  // --- Mindestdauer: Ausreißer ignorieren, Dauerzustand melden ---
  {
    RuleParams p{Op::ABOVE, 1000.0f, 50.0f, 120, 0, true};   // eCO2 > 1000 ppm für 2 min
    RuleState s;
    check("Dauer: Ausreißer t=0 -> PENDING", evaluate(p, s, 1200, true, 0) == Event::NONE && s.state == State::PENDING);
    check("Dauer: t=60 wieder normal -> NORMAL, keine Meldung", evaluate(p, s, 900, true, 60000) == Event::NONE && s.state == State::NORMAL);
    evaluate(p, s, 1100, true, 120000);
    check("Dauer: t=180 noch zu kurz", evaluate(p, s, 1100, true, 180000) == Event::NONE);
    check("Dauer: t=240 (120 s) -> ALERT", evaluate(p, s, 1150, true, 240000) == Event::ALERT);
  }
  // --- Erinnerung ---
  {
    RuleParams p{Op::ABOVE, 30.0f, 0, 0, 30, false};
    RuleState s;
    evaluate(p, s, 31, true, 0);
    check("Erinnerung: nach 29 min nichts", evaluate(p, s, 31, true, 29 * 60000) == Event::NONE);
    check("Erinnerung: nach 30 min REMIND", evaluate(p, s, 31, true, 30 * 60000) == Event::REMIND);
    check("Erinnerung: Zähler neu ab REMIND", evaluate(p, s, 31, true, 45 * 60000) == Event::NONE);
    check("Erinnerung: nach weiteren 30 min REMIND", evaluate(p, s, 31, true, 60 * 60000) == Event::REMIND);
  }
  // --- Ungültige Werte ---
  {
    RuleParams p{Op::ABOVE, 30.0f, 0, 0, 0, true};
    RuleState s;
    check("Ungültig: valid=false -> NONE", evaluate(p, s, 99, false, 0) == Event::NONE && s.state == State::NORMAL);
    check("Ungültig: NaN -> NONE", evaluate(p, s, NAN, true, 0) == Event::NONE && !s.hasValue);
  }
  // --- millis()-Überlauf ---
  {
    RuleParams p{Op::ABOVE, 30.0f, 0, 60, 0, true};
    RuleState s;
    evaluate(p, s, 31, true, 0xFFFF0000u);
    check("Überlauf: 30 s nach Wrap noch PENDING", evaluate(p, s, 31, true, 0xFFFF0000u + 30000u) == Event::NONE);
    check("Überlauf: 60 s nach Wrap ALERT", evaluate(p, s, 31, true, 0xFFFF0000u + 60000u) == Event::ALERT);
  }
  // --- Gerät offline/online ---
  {
    DeviceState d;
    check("Gerät: 1. Fehler", deviceResult(d, false, 3, 0) == DevEvent::NONE);
    check("Gerät: 2. Fehler", deviceResult(d, false, 3, 1) == DevEvent::NONE);
    check("Gerät: 3. Fehler -> OFFLINE", deviceResult(d, false, 3, 2) == DevEvent::OFFLINE);
    check("Gerät: weiterer Fehler kein 2. OFFLINE", deviceResult(d, false, 3, 3) == DevEvent::NONE);
    check("Gerät: OK -> ONLINE", deviceResult(d, true, 3, 4) == DevEvent::ONLINE);
    check("Gerät: OK ohne Meldung", deviceResult(d, true, 3, 5) == DevEvent::NONE);
    DeviceState d2;
    for (int i = 0; i < 10; i++) deviceResult(d2, false, 0, i);
    check("Gerät: Schwelle 0 = nie OFFLINE", !d2.offline);
  }
  // --- Ping-Check (E25): Schwellwert = max. Ausfallzeit in Sekunden ---
  {
    PingState p;
    check("Ping: OK -> nichts", pingResult(p, true, 60, 0) == PingEvent::NONE);
    check("Ping: Ausfall 50 s -> noch nichts", pingResult(p, false, 60, 50000) == PingEvent::NONE);
    check("Ping: Ausfall 60 s -> DOWN", pingResult(p, false, 60, 60000) == PingEvent::DOWN && pingDownSeconds(p, 60000) == 60);
    check("Ping: weiter aus -> kein zweites DOWN", pingResult(p, false, 60, 120000) == PingEvent::NONE);
    check("Ping: Antwort -> UP", pingResult(p, true, 60, 130000) == PingEvent::UP && pingDownSeconds(p, 130000) == 0);
    check("Ping: kurzer Aussetzer unter Schwelle -> nichts", pingResult(p, false, 60, 140000) == PingEvent::NONE &&
                                                             pingResult(p, true, 60, 150000) == PingEvent::NONE);
    PingState q;   // nie erreichbar seit Start: zählt ab erstem Versuch
    check("Ping: nie erreichbar, 1. Versuch", pingResult(q, false, 30, 1000) == PingEvent::NONE);
    check("Ping: nie erreichbar, nach 30 s DOWN", pingResult(q, false, 30, 31000) == PingEvent::DOWN && !q.everOk);
    PingState w;   // millis()-Überlauf
    pingResult(w, true, 20, 0xFFFFF000u);
    check("Ping: Überlauf", pingResult(w, false, 20, 0xFFFFF000u + 20000u) == PingEvent::DOWN);
    char out[161];
    TemplateVars tv{"Router", "", 75, true, "s", Op::ABOVE, 60, "PING-ALARM", "192.168.178.1"};
    renderTemplate("{state} {device} ({host}) seit {value} s nicht erreichbar", tv, out, sizeof(out));
    check("Ping: Vorlage mit {host}", std::string(out) == "PING-ALARM Router (192.168.178.1) seit 75 s nicht erreichbar", out);
    TemplateVars tv2{"X", "", 0, false, "", Op::ABOVE, 0, "OK"};   // host fehlt -> leer
    renderTemplate("[{host}]", tv2, out, sizeof(out));
    check("Ping: {host} ohne Wert = leer", std::string(out) == "[]", out);
  }
  // --- Sendebegrenzung ---
  {
    RateLimiter r;
    r.setMaxPerHour(6);
    int ok = 0;
    for (int i = 0; i < 10; i++) ok += r.allow(i * 1000);
    check("Limit: 6 von 10 in einer Minute", ok == 6, std::to_string(ok));
    check("Limit: nach 59 min weiter gesperrt", !r.allow(59 * 60000));
    check("Limit: nach 60 min wieder frei", r.allow(3600000 + 1000));
    RateLimiter z;
    z.setMaxPerHour(0);
    check("Limit: 0 = nie senden", !z.allow(0));
  }
  // --- Textvorlage ---
  {
    char out[161];
    TemplateVars v{"Serverraum", "Temperatur", 31.25f, true, "\xC2\xB0" "C", Op::ABOVE, 30.0f, "ALARM"};
    renderTemplate("{state} {device}: {metric} {value}{unit} ({op}{threshold}{unit})", v, out, sizeof(out));
    check("Vorlage: Standardtext", std::string(out) == "ALARM Serverraum: Temperatur 31.2\xC2\xB0" "C (>30\xC2\xB0" "C)", out);
    renderTemplate("{unbekannt} {value}", TemplateVars{"", "", 0, false, "", Op::BELOW, 5, ""}, out, sizeof(out));
    check("Vorlage: unbekannter Platzhalter bleibt, fehlender Wert = ?", std::string(out) == "{unbekannt} ?", out);
    char small[10];
    size_t n = renderTemplate("{device}{device}", v, small, sizeof(small));
    check("Vorlage: abgeschnitten, nullterminiert", n == 9 && strlen(small) == 9, small);
    check("Einheit/Label", !strcmp(unitFor("sensor2.lux"), "lx") && !strcmp(labelFor("sensor1.humidity"), "Feuchte") &&
                           !strcmp(labelFor("sensor.foo"), "foo"));
  }
  // --- Sensormeter-JSON ---
  {
    SmValue v[16];
    char err[64] = {0};
    for (const char* f : {"sm_live_108.json", "sm_live_148.json"}) {
      std::string j = readFile(fx + "/" + f);
      int n = smFlatten(j.data(), j.size(), v, 16, err, sizeof(err));
      bool ok = n == 2 && !strcmp(v[0].path, "sensor1.temperature") && !strcmp(v[1].path, "sensor1.humidity") &&
                !strcmp(v[0].sensorName, "Intern") && v[0].value > -40 && v[0].value < 85;
      check(std::string("JSON live ") + f, ok, std::to_string(n) + " Werte" + (n > 0 ? ", " + std::string(v[0].path) + "=" + std::to_string(v[0].value) : ""));
    }
    const char* poe = "{\"sensor1\":{\"name\":\"Intern\",\"valid\":true,\"temperature\":21.5,\"humidity\":40},"
                      "\"sensor2\":{\"name\":\"Lager\",\"valid\":true,\"temperature\":null,\"humidity\":null,"
                      "\"pressureHpa\":1013.2,\"lux\":350,\"eco2Ppm\":null}}";
    int n = smFlatten(poe, strlen(poe), v, 16, err, sizeof(err));
    check("JSON sm-poe mit null-Feldern", n == 4 && !strcmp(v[2].path, "sensor2.pressureHpa") && !strcmp(v[3].path, "sensor2.lux") &&
                                          !strcmp(v[3].sensorName, "Lager") && v[3].value == 350.0f, std::to_string(n));
    const char* wlan = "{\"sensor\":{\"name\":\"DHT22\",\"valid\":true,\"temperature\":19.0,\"humidity\":61.5}}";
    n = smFlatten(wlan, strlen(wlan), v, 16, err, sizeof(err));
    check("JSON sm-wlan", n == 2 && !strcmp(v[0].path, "sensor.temperature"), std::to_string(n));
    const char* inval = "{\"sensor1\":{\"name\":\"Intern\",\"valid\":false,\"temperature\":0,\"humidity\":0}}";
    check("JSON valid=false -> keine Werte", smFlatten(inval, strlen(inval), v, 16, err, sizeof(err)) == 0);
    const char* bad = "<html>Login</html>";
    // Ergebnis erst berechnen: die Auswertungsreihenfolge von Funktionsargumenten
    // ist unspezifiziert, err wäre sonst evtl. noch uninitialisiert.
    err[0] = 0;
    int badN = smFlatten(bad, strlen(bad), v, 16, err, sizeof(err));
    check("JSON kaputt -> -1", badN == -1 && strstr(err, "InvalidInput"), err);
    check("JSON max. Anzahl", smFlatten(poe, strlen(poe), v, 3, err, sizeof(err)) == 3);
  }

  printf(fails ? "%d FEHLER\n" : "ALLE TESTS OK\n", fails);
  return fails ? 1 : 0;
}
