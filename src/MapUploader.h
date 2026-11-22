#pragma once

#include <Arduino.h>

// Karten-Uploader (E47): trägt das Gateway und die von ihm gehörten Repeater
// in die öffentliche MeshCore-Karte map.meshcore.io ein.
//
// Hintergrund: Die Karte nimmt jedes *knotensignierte* Advert an, egal mit
// welchem Schlüssel der Hochladende es überträgt – verlangt wird nur, dass die
// Upload-Signatur zum mitgesendeten publicKey passt (geprüft am echten Gerät,
// 2026-11-22). Das Gateway hat sein eigenes Advert und die gehörten Adverts
// ohnehin als Rohpaket, kann also ohne Companion-App direkt hochladen.
//
// Ablauf (POST https://map.meshcore.io/api/v1/uploader/node):
//   data      = {"params":{"freq":MHz,"cr":..,"sf":..,"bw":kHz},
//                "links":["meshcore://<rohes Advert hex>"]}
//   signature = Ed25519( SHA-256(data) )  mit dem Upload-Schlüssel
//   publicKey = öffentlicher Upload-Schlüssel (hex)
//
// Der Upload-Schlüssel ist NICHT der Knotenschlüssel: ein eigens erzeugtes
// Ed25519-Paar im NVS (Namespace "mapup"). "Neu erzeugen" würfelt es neu; der
// bisherige Schlüssel kann dann den von ihm angelegten Karteneintrag nicht mehr
// aktualisieren (die Karte merkt sich updated_by). Geht der Schlüssel bei einem
// NVS-Erase verloren, ist das unkritisch: der nächste Upload legt den Eintrag
// mit dem neuen Schlüssel erneut an (Zuordnung erfolgt über den Knotenschlüssel
// im Advert, nicht über den Upload-Schlüssel).
//
// Nichts läuft automatisch: Hochgeladen wird nur auf ausdrückliche Aktion in der
// Weboberfläche (eigenen Eintrag listen / bekannte Repeater listen). Die
// HTTPS-Arbeit erledigt ein eigener Core-0-Task, damit weder loopTask (MeshCore)
// noch der Web-Task blockiert.

class GatewayMesh;

namespace map_uploader {

// Einmalig in setup() nach the_mesh.begin(): Schlüssel laden, Core-0-Task starten.
void begin();

// Aus dem loopTask (MeshCore-Kontext) regelmäßig aufrufen: baut bei Bedarf ein
// frisches eigenes Advert (mit aktuellem Zeitstempel) für einen angeforderten
// Eigen-Upload. Billig, solange nichts angefordert ist.
void loop(GatewayMesh& mesh);

// Ein gehörtes Advert merken (nur Repeater/Room). Aus dem MeshCore-Kontext
// (GatewayMesh::onAdvertRecv). raw/len = Rohpaket wie empfangen.
void captureHeard(const uint8_t* raw, uint8_t len, const uint8_t* pub,
                  const char* name, uint8_t type, bool has_loc, uint32_t adv_timestamp);

// --- Weboberfläche (beliebiger Task, thread-sicher) ---

// Status als JSON: Upload-Schlüssel (pubkey), eigener Eintrag, Liste der
// bekannten Repeater, letztes Ergebnis.
String statusJson();

// Neues zufälliges Upload-Schlüsselpaar erzeugen und speichern.
bool generateKey(String& err);

// Upload anstoßen. Kehrt sofort zurück; der Task arbeitet im Hintergrund.
void requestSelfUpload();
void requestKnownUpload();

// Automatisches Melden: Intervall in Stunden (0 = aus, nur manuell). Standard 6.
// Gemeldet wird kurz nach dem Boot und danach alle <Stunden> Stunden – jeweils
// der eigene Eintrag und die bekannten Repeater. Wird im NVS gespeichert.
void setAutoHours(uint16_t hours);
uint16_t getAutoHours();

}  // namespace map_uploader
