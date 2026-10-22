#include "Profile.h"

// Quellen: hansemesh.de/anleitungen/repeater-checkliste/,
//          hansemesh.de/anleitungen/einstellungen-deutschland/,
//          hansemesh.de/anleitungen/heltec-v3-v4/
// Befehlsnamen gegen MeshCore 1.18.0 (CommonCLI.cpp, CommonRadioPrefs.cpp,
// HeltecV4Board.cpp) geprüft. hansemesh nennt
// teils andere Namen (advert.interval.zero/.flood), siehe E15.
//
// Regionen gehören bewusst NICHT ins Profil (E36): sie hängen vom Standort ab
// und werden mit der lokalen Community abgestimmt. Die Weboberfläche warnt,
// solange keine Region angelegt ist.

const char* profile::NAME = "HanseMesh-Empfehlung Repeater Heltec V4 (ländlich)";

const ProfileEntry profile::ENTRIES[] = {
  {"set radio 869.618,62.5,8,8", "get radio", "869.618,62.5,8,8", "EU/UK Narrow, Standard in Deutschland (Neustart nötig)"},
  {"set tx 17", "get tx", "17", "Heltec V4 mit PA: ca. 520 mW am Ausgang, ERP mit Antenne prüfen"},
  {"set dutycycle 10", "get dutycycle", "10.0%", "gesetzliche 10 % im 60-Minuten-Fenster"},
  {"set flood.max 10", "get flood.max", "10", "Scoped Floods enden nach 10 Hops"},
  {"set flood.max.unscoped 5", "get flood.max.unscoped", "5", "ländlich 5 (dicht 2-3)"},
  {"set flood.max.advert 5", "get flood.max.advert", "5", "fremde Flood-Adverts nach wenigen Hops stoppen"},
  {"set advert.interval 240", "get advert.interval", "240", "Zero-Hop-Advert alle 4 h (Maximum)"},
  {"set flood.advert.interval 48", "get flood.advert.interval", "48", "Flood-Advert alle 48 h (nie 168)"},
  {"set txdelay 0.7", "get txdelay", "0.7", "Rand-Repeater 0.7, Kern-Repeater 2.0"},
  {"set direct.txdelay 0.5", "get direct.txdelay", "0.5", "Rand-Repeater 0.5, Kern-Repeater 0.9"},
  {"set rxdelay 0", "get rxdelay", "0", "kein Filter für schwache Floods"},
  {"set path.hash.mode 2", "get path.hash.mode", "2", "3-Byte-Pfade gegen ID-Kollisionen"},
  {"set loop.detect minimal", "get loop.detect", "minimal", "reicht mit 3-Byte-Pfaden"},
  {"set cad off", "get cad", "off", "Hardware-CAD blockiert bis zu 4 s"},
  {"set int.thresh 0", "get int.thresh", "0", "bringt in 1.16/1.17 nichts"},
  {"set agc.reset.interval 60", "get agc.reset.interval", "60", "Empfänger-Regelung regelmäßig zurücksetzen"},
  {"set multi.acks 1", "get multi.acks", "1", "zuverlässigere Zustellung"},
  {"set radio.rxgain off", "get radio.rxgain", "off", "1.17.1 auf V4: Vorverstärker hebt Noise Floor auf ca. -84 dBm"},
  {"set radio.fem.rxgain off", "get radio.fem.rxgain", "off", "nur V4.3: FEM-LNA hebt Signal und Rauschen gleich, kein SNR-Gewinn (V4.2: entfällt)"},
};

const size_t profile::COUNT = sizeof(profile::ENTRIES) / sizeof(profile::ENTRIES[0]);
