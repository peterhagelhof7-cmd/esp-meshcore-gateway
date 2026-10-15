# Admin-Handbuch: MeshCore-Gateway

Für alle, die ein Gateway aufsetzen und betreiben. Stand: Firmware 0.8.0, MeshCore 1.18.0.

**Inhalt**

1. [Was das Gateway ist](#1-was-das-gateway-ist)
2. [Hardware und Sicherheit](#2-hardware-und-sicherheit)
3. [Installieren mit dem Web-Flasher](#3-installieren-mit-dem-web-flasher)
4. [Erstinbetriebnahme Schritt für Schritt](#4-erstinbetriebnahme-schritt-für-schritt)
5. [Regionen richtig einrichten](#5-regionen-richtig-einrichten)
6. [Betrieb](#6-betrieb)
7. [Updates](#7-updates)
8. [Fehlerbehebung](#8-fehlerbehebung)
9. [Sicherheit im Netz](#9-sicherheit-im-netz)

---

## 1. Was das Gateway ist

Ein normaler **MeshCore-Repeater** (Firmware 1.18.0, unverändert) auf einem Heltec WiFi LoRa 32 V4, zusätzlich
mit WLAN:

- **Weboberfläche** für alle Repeater-Einstellungen, Regionen, Admin-Passwort, Backup und Firmware-Update
- **Webkonsole** mit den offiziellen MeshCore-CLI-Befehlen
- **Kanäle**: Nachrichten in Public, #Hashtag- und privaten Kanälen lesen und senden
- **Überwachung**: Sensormeter im LAN und Ping-Ziele abfragen, bei Grenzwerten oder Ausfall eine Meldung in einen
  Kanal senden; dazu die Chiptemperatur des Gateways selbst
- **Status**: Funk, Pakete, Nachbarn, WLAN; Ereignis-Log im Flash zum Herunterladen

Als Repeater leitet das Gateway weiter wie jeder andere MeshCore-Repeater. Das WLAN ist nur für die Verwaltung und
die Überwachung da, nicht für die Mesh-Pakete.

## 2. Hardware und Sicherheit

| Board | Chip | Firmware |
|---|---|---|
| Heltec WiFi LoRa 32 **V4** | ESP32-S3R2, 2 MB PSRAM | `MESHCORE-GW-HELTEC-V4` |
| Heltec WiFi LoRa 32 **V4-R8** | ESP32-S3R8, 8 MB PSRAM | `MESHCORE-GW-HELTEC-V4-R8` |

Beide haben unterschiedliche Pins. Die falsche Firmware lässt sich weder mit dem Web-Flasher noch per
Firmware-Update aufspielen: beide prüfen das Board.

> **⚠ Antenne zuerst.** Nie ohne angeschraubte 868-MHz-Antenne mit Strom versorgen: Der Repeater sendet sofort
> (Adverts) und der Sendeverstärker kann ohne Antenne Schaden nehmen.

- **Sendeleistung:** Voreingestellt `tx 17` (ca. 520 mW am Ausgang des V4). Erlaubt sind in Deutschland 500 mW ERP
  im Bereich 869,4–869,65 MHz. Mit einer Antenne über ca. 2 dBi die Leistung senken
  (ERP = Sendeleistung + Antennengewinn in dBd − Kabelverluste).
- **Stromversorgung:** USB-C, 5 V. Akku und Bluetooth nutzt das Gateway nicht.
- **Taste PRG:** schaltet das Display ein und blättert die Seiten. Ein langer Druck schaltet das Gateway **nicht**
  aus (ohne Akku wäre es sonst nur durch Stromtrennung zurückzuholen).
- **Ohne Display / mit TFT:** Läuft genauso, nur die Anzeige fehlt (TFT bleibt dunkel). Die IP steht dann im Router,
  die Einrichtung läuft wie unten über den WLAN-Hotspot.

## 3. Installieren mit dem Web-Flasher

**https://peterhagelhof7-cmd.github.io/esp-meshcore-gateway/**

Voraussetzungen: **Chrome oder Edge am Computer** (Windows, macOS, Linux). Firefox, Safari und Handys können
nicht auf USB zugreifen. Treiber sind nicht nötig, das V4 hat USB direkt im Chip.

1. Antenne anschrauben, Board mit einem **Datenkabel** (nicht nur Ladekabel) anschließen.
2. **„Verbinden & prüfen“** → Port auswählen (meist „USB JTAG/serial debug unit“).
3. Der Flasher liest Chip, Flash und PSRAM und zeigt das Ergebnis:
   - **„Erkannt: Heltec WiFi LoRa 32 V4 …“** (oder V4-R8) → passt, weiter mit Schritt 4.
   - **„Nicht geeignet“** → anderes Board, es wird nichts geschrieben.

   Bis hierhin wird **nichts** geschrieben. Ein laufendes Gateway startet beim Prüfen einmal neu, die Einstellungen
   bleiben.
4. **Version** wählen. Vorausgewählt ist die neueste **bewährte** Version, die schon länger im Echtbetrieb läuft.
   Eine Version **„neu, in Erprobung“** ist getestet, läuft aber noch nicht lange auf echten Gateways; für den
   Dauerbetrieb die bewährte nehmen.
5. **Flash vorher komplett löschen**: bei der Erstinstallation oder wenn vorher eine andere Firmware lief
   **angehakt lassen**. Das löscht WLAN, Kanäle, Einstellungen und die **Knoten-Identität** (das Gateway bekommt
   einen neuen Schlüssel und erscheint im Mesh als neuer Knoten).
6. **„Installieren“**: Die Dateien werden per Prüfsumme und Board-Merkmal kontrolliert, geschrieben und nach dem
   Schreiben verglichen. Dauer 1–2 Minuten, USB nicht trennen.
6. Nach „Fertig“ startet das Gateway. Der erste Start dauert ca. 30 s (Dateisystem wird angelegt).

**Findet der Browser kein Gerät:** Taste **PRG** gedrückt halten, kurz **RST** drücken, PRG loslassen und erneut
„Verbinden & prüfen“.

## 4. Erstinbetriebnahme Schritt für Schritt

Beim allerersten Start übernimmt das Gateway die **Empfehlungen für Deutschland nach hansemesh.de**
(Funkparameter EU/UK Narrow 869,618 MHz, Sendeleistung, Flood-Grenzen, Advert-Intervalle u. a.).
**Regionen legt es nicht an**, die hängen vom Standort ab (siehe [Abschnitt 5](#5-regionen-richtig-einrichten)).

### 4.1 WLAN einrichten

1. Mit dem Hotspot **`installer`** verbinden, Passwort **`installer`**. Das Display zeigt die Zugangsdaten.
2. Im Browser **http://192.168.4.1** öffnen, anmelden mit Benutzer **`admin`**, Passwort **`password`**.
3. **System → WLAN einrichten**: „Netze suchen“, Netz wählen, Passwort eingeben, **„Speichern & neu starten“**.
4. Das Gateway verbindet sich mit dem Heimnetz. Klappt das nicht innerhalb von 30 s, kommt der Hotspot zurück.
5. Die neue IP steht auf dem Display (Seite 1) und im Router. Der Hostname folgt dem Gerätenamen
   (Haken „wie Gerätename (LoRa)“), z. B. `de-by-ort01-gw`.

Fällt das Heim-WLAN später länger als 5 Minuten aus, öffnet das Gateway den Hotspot wieder und versucht parallel
weiter, ins Heimnetz zu kommen. Der Repeater läuft dabei ununterbrochen weiter.

### 4.2 Admin-Passwort ändern – sofort

**Einstellungen → Admin-Passwort.** Das Werkspasswort `password` kennt jeder. Es gilt für die Weboberfläche **und**
für die Fernverwaltung des Repeaters über die MeshCore-App. Maximal 15 Zeichen, keine Leerzeichen.

### 4.3 Name, Position, Kontakt

**Einstellungen → MeshCore-Einstellungen:**

- **Name**: nach dem Schema deiner Community, z. B. hansemesh `DE-<Land>-<Ort><Nr>-<Bezeichnung>`.
  Der Name erscheint in der App und im Mesh, der WLAN-Hostname zieht automatisch nach.
- **Position** (Breite/Länge): für die Karte in der App; weglassen oder ungenau angeben, wenn der Standort privat
  bleiben soll.
- **owner.info**: Kontakt für andere Betreiber (optional).
- **Gast-Passwort**: leer lassen = kein Gastzugang.

Funkparameter und Schlüssel werden erst nach **„Jetzt neu starten“** wirksam.

### 4.4 Regionen anpassen

Siehe [Abschnitt 5](#5-regionen-richtig-einrichten). **Dieser Schritt ist Pflicht.** Solange keine Region angelegt
ist, zeigt **Einstellungen → Regionen** die Warnung „Keine Region angelegt“.

### 4.5 Funk prüfen

- **Status → Funk**: Rauschpegel um −105 bis −115 dBm ist gut. Deutlich höher (z. B. −85 dBm) deutet auf Störer oder
  einen aktivierten Empfangsverstärker hin.
- **Einstellungen → Empfehlung**: zeigt Abweichungen von den hansemesh-Werten.
  Die Empfehlung ist auf einen **ländlichen Rand-Repeater** ausgelegt. Für einen Kern-Repeater in dichtem Gebiet
  `txdelay 2.0`, `direct.txdelay 0.9`, `flood.max.unscoped 2–3` mit der lokalen Community abstimmen.

### 4.6 Im Mesh ankommen

- **Status → Nachbarn**: 90 s nach jedem Start sucht das Gateway selbst nach Repeatern in direkter Reichweite,
  „Jetzt suchen“ wiederholt das. Mindestens ein Nachbar mit SNR über ca. −5 dB ist ein guter Start.
- In der **MeshCore-App** taucht der Repeater nach seinem nächsten Advert auf (spätestens nach 4 h). Sofort geht es
  über die Webkonsole mit `advert`.
- In der App lässt sich der Repeater mit dem Admin-Passwort fernverwalten.

### 4.7 Backup ziehen

**Einstellungen → Backup & Wiederherstellen → „Backup herunterladen“.** Mit dem Haken „Schlüssel und Passwörter
einschließen“ enthält die Datei auch den privaten Schlüssel (Knoten-Identität), Admin-, Gast- und WLAN-Passwort.
So lässt sich ein Gateway nach einem Defekt mit gleicher Identität wiederherstellen. **Diese Datei geheim
halten.**

## 5. Regionen richtig einrichten

### Warum das wichtig ist

MeshCore-Nachrichten können eine **Region (Scope)** tragen, z. B. `de-by` oder `rhein-main`. Ein Repeater
leitet solche Nachrichten **nur weiter, wenn er die Region kennt und Weiterleiten dort erlaubt ist** (`F` in der
Liste). Nachrichten ohne Region leitet er weiter, begrenzt durch `flood.max.unscoped` (Hops).

- **Fehlt eine Region**, die in deiner Gegend genutzt wird, schluckt dein Repeater diese Nachrichten. Das Netz hat
  an dieser Stelle ein Loch, ohne dass es jemand merkt.
- **Überflüssige Regionen** (z. B. `rhein-main` in Hamburg) lassen Verkehr durch, der dort nichts verloren hat,
  und kosten Sendezeit.

### Ausgangszustand

Ein frisch installiertes Gateway hat **keine Regionen** (nur `*`, den Platzhalter für Nachrichten ohne Region).
Es leitet dann nur Nachrichten ohne Region weiter. Die Weboberfläche warnt, bis mindestens eine Region angelegt
ist. Die Empfehlung (Einstellungen → Empfehlung) prüft Regionen bewusst nicht, weil es keine für alle Standorte
richtige Vorgabe gibt.

Beispiel Rhein-Main/Unterfranken: `de-he`, `de-by`, `rhein-main`.

### Vorgehen

1. **Bei der lokalen Community nachfragen**, welche Regionen genutzt werden (z. B. hansemesh.de für
   Norddeutschland, regionale Gruppen/Discord). Üblich sind Bundesland-Kürzel (`de-nw`, `de-ni`, `de-hh`, …) und
   Ballungsräume.
2. **Einstellungen → Regionen**: Die Liste zeigt den aktuellen Stand.
3. Je Region: Aktion **„anlegen (put)“**, Name (Kleinbuchstaben, Ziffern, `-`), optional Eltern-Region,
   **„Ausführen“**.
4. Optional die eigene Region **„als Home setzen“**.
5. **„Speichern (region save)“** – ohne diesen Schritt sind die Änderungen nach dem nächsten Neustart weg.
6. Später Überflüssiges: Aktion **„entfernen“** (Unterregionen zuerst), danach wieder speichern.

Gleiches geht in der Webkonsole: `region`, `region put de-nw`, `region remove de-nw`, `region save`.

Ein Backup enthält die Regionen; beim Wiederherstellen werden sie wieder angelegt und gespeichert.

> **Hinweise**
> - `region load` **nicht** benutzen: friert in MeshCore 1.17.x die Konsole ein.
> - `region denyf *` (keine Nachrichten ohne Region weiterleiten) nur in Absprache mit der Community: Nachrichten von
>   Geräten ohne gesetzte Region kämen dann nicht mehr durch. Sanfter ist ein niedriges `flood.max.unscoped`
>   (voreingestellt 5).
> - Private `$`-Regionen unterstützt das Gateway beim **Senden** in Kanäle nicht.

### Regionen bei Kanälen und Meldungen

Unter **Kanäle** lässt sich pro Kanal ein **Scope** setzen. Nachrichten des Gateways in diesen Kanal tragen dann die
Region und kommen **nur über Repeater, die diese Region kennen**. Für Alarmmeldungen deshalb:

- eine Region wählen, die **alle Repeater auf dem Weg zum Empfänger** kennen, oder
- **keinen Scope** setzen (Standard) – dann zählt nur `flood.max.unscoped`.

Nach dem Einrichten über **Überwachung → Regel → Test** prüfen, ob die Meldung in der App ankommt.

## 6. Betrieb

| Bereich | Wofür |
|---|---|
| **Status** | Knoten, Funk (Rauschpegel, letzte RSSI/SNR), Paketzähler, WLAN, Chiptemperatur, **Nachbarn** |
| **Konsole** | Offizielle MeshCore-CLI (`get …`, `set …`, `neighbors`, `advert`, `region …`). Kritische Befehle (z. B. `erase`) fragen nach. |
| **Kanäle** | Kanäle anlegen (Hashtag, privat mit Schlüssel aus der App, Public), Nachrichten lesen und senden. Max. 8 Kanäle. |
| **Überwachung** | Sensormeter im LAN, Regeln mit Grenzwert, Hysterese und Entwarnung; Ping-Ziele; Chiptemperatur als „Gateway (intern)“. Meldungen gehen in den gewählten Kanal. |
| **Pakete** | Live-Mitschnitt der empfangenen/gesendeten Pakete |
| **Einstellungen** | MeshCore-Einstellungen, Empfehlung, Regionen, Admin-Passwort, Backup |
| **System** | WLAN, Firmware-Update, Ereignis-Log (Download, max. 128 KB im Flash) |

**Display** (Taste PRG, Auto-Aus nach 60 s): 1 Netz (Name, WLAN, IP) · 2 Funk · 3 Mesh (Pakete, Uhrzeit) ·
4 neueste Kanalnachricht bzw. Alarm.

**Private Kanäle** aus der App übernehmen: In der App den Kanal teilen bzw. den Schlüssel anzeigen (32 Hex-Zeichen)
und unter Kanäle → „Privat“ mit gleichem Namen und Schlüssel anlegen.

## 7. Updates

- **Laufendes Gateway: System → Firmware-Update**, die `firmware.bin` für dein Board hochladen und
  „Hochladen & installieren“. Alle Einstellungen bleiben. Aktuelle Dateien:
  - bewährt 0.7.6: [V4](https://peterhagelhof7-cmd.github.io/esp-meshcore-gateway/fw/0.7.6/v4/firmware.bin) ·
    [V4-R8](https://peterhagelhof7-cmd.github.io/esp-meshcore-gateway/fw/0.7.6/r8/firmware.bin)
  - neu, in Erprobung 0.8.0 (MeshCore 1.18.0):
    [V4](https://peterhagelhof7-cmd.github.io/esp-meshcore-gateway/fw/0.8.0/v4/firmware.bin) ·
    [V4-R8](https://peterhagelhof7-cmd.github.io/esp-meshcore-gateway/fw/0.8.0/r8/firmware.bin)

  Welches Board du hast, zeigt der Web-Flasher bei „Verbinden & prüfen“.
  - Falsches Board und ältere Versionen werden abgelehnt („Downgrade erzwingen“ nur, wenn gewollt).
  - **Rollback:** Startet die neue Version nicht sauber (Absturz in der ersten Minute), kehrt das Gateway
    automatisch zur vorherigen zurück. Im Ereignis-Log steht dann eine `ROLLBACK`-Zeile.
- **Neuinstallation oder Rettung:** Web-Flasher (Abschnitt 3); ohne „komplett löschen“ bleiben die Einstellungen
  erhalten, sofern das Gateway vorher schon diese Firmware hatte.

## 8. Fehlerbehebung

| Problem | Lösung |
|---|---|
| Flasher findet kein Gerät | Datenkabel prüfen; PRG halten, RST drücken, PRG loslassen; anderen USB-Port |
| Flasher: „Nicht geeignet“ | Kein Heltec V4/V4-R8 (z. B. V3 oder anderes ESP32-Board) |
| Hotspot `installer` erscheint nicht | Display-Seite 1 prüfen; nach Neuinstallation ca. 30 s warten |
| Weboberfläche nicht erreichbar | IP auf Display-Seite 1 oder im Router; Hostname = Gerätename in Kleinbuchstaben |
| Passwort vergessen | Web-Flasher mit „komplett löschen“, neu einrichten (Werkspasswort `password`). Ein Backup mit Schlüsseln stellt danach die alte Identität und alle Einstellungen wieder her, ohne Backup ist der Knoten neu. |
| Keine Nachbarn | „Jetzt suchen“; Antenne/Standort prüfen; Liste ist nach jedem Neustart erst leer |
| Nachrichten mit Region kommen nicht durch | Regionen prüfen ([Abschnitt 5](#5-regionen-richtig-einrichten)), `region save` vergessen? |
| Rauschpegel sehr hoch (≈ −85 dBm) | Einstellungen → Empfehlung: `radio.rxgain`/`radio.fem.rxgain` = off |
| Meldungen kommen nicht an | Kanal in der App vorhanden? Scope passend? Überwachung → Regel → Test |

Das **Ereignis-Log** (System) hält Starts, Startgrund, WLAN-Wechsel, Befehle (Passwörter maskiert), Meldungen,
Updates und Rollbacks fest und überlebt Neustarts.

## 9. Sicherheit im Netz

- Die Weboberfläche nutzt **HTTP mit Basic-Auth** – im WLAN unverschlüsselt. Nur in vertrauenswürdigen Netzen
  betreiben, nicht ins Internet freigeben (keine Portweiterleitung).
- Admin-Passwort = Fernverwaltung über Funk: ein starkes, eigenes Passwort wählen.
- Backups mit Schlüsseln und Passwörtern wie Passwörter behandeln.

---

Quellcode, Entscheidungen und Testpläne: [README](../README.md), [`docs/entscheidungen.md`](entscheidungen.md),
[`docs/hw-testplan.md`](hw-testplan.md). Lizenz GPL-3.0.
