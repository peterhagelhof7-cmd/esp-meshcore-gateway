#pragma once

#include <stddef.h>

// Sensormeter-Antwort von GET /api/sensors in eine flache Werteliste
// umwandeln (M6, docs/entscheidungen.md E22). Ohne Arduino-Abhängigkeit
// (nur ArduinoJson, header-only) -> auf dem PC mit echten Antworten testbar.
//
// Bekannte Formate (geprüft gegen die Firmware-Quellen und live im LAN):
//   sensormeter / -poe:  {"sensor1":{"name":"Intern","valid":true,"temperature":23.8,"humidity":39},
//                         "sensor2":{"name":"...","valid":true,"temperature":..,"humidity":..,
//                                    "pressureHpa":..,"lux":..,"eco2Ppm":..}}   (fehlende Werte = null)
//   sensormeter-wlan:    {"sensor":{"name":"DHT22","valid":true,"temperature":..,"humidity":..}}
// Regeln: jedes Objekt der obersten Ebene ist ein Sensor; valid=false -> alle
// seine Werte überspringen; null/NaN/Nicht-Zahlen überspringen; Pfad =
// "<sensor>.<feld>" (z. B. "sensor2.lux").
struct SmValue {
  char path[32];
  char sensorName[24];
  float value;
};

// Rückgabe: Anzahl Werte, -1 bei JSON-Fehler (err gefüllt).
int smFlatten(const char* json, size_t len, SmValue out[], int maxOut, char* err, size_t errLen);
