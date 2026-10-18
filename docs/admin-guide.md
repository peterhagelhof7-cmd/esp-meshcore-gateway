# Admin-Handbuch: MeshCore-Gateway

Für alle, die ein Gateway aufsetzen und betreiben. Stand: Firmware 0.10.0, MeshCore 1.18.0.

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

**Feste IP-Adresse** (ab 0.9.0, sonst DHCP): Unter System → WLAN den Haken „feste IP-Adresse“ setzen. Die Felder
sind mit den aktuellen DHCP-Werten vorbelegt. Nötig sind IP-Adresse, Subnetzmaske und Gateway (Router); DNS 1 leer
heißt: der Router ist DNS-Server, DNS 2 ist optional.
- **„Adresse prüfen“** und beim Speichern prüft das Gateway per ARP und Ping, ob die Adresse schon belegt ist.
  Belegt (mit MAC des anderen Geräts) → wird nicht gespeichert. Liegt die Adresse in einem anderen Subnetz als das
  aktuelle WLAN, kann sie von dort nicht geprüft werden; nach Rückfrage wird trotzdem gespeichert.
- Nach dem Neustart meldet sich das Gateway **erst per DHCP** an, prüft die Adresse noch einmal, schaltet dann um
  und testet, ob der Router antwortet. Ist die Adresse belegt oder der Router nicht erreichbar, **bleibt es bei
  DHCP** und die feste IP wird verworfen; der Grund steht im Ereignis-Log und im WLAN-Bereich.
- Status → WLAN zeigt unter „Adressvergabe“, ob DHCP oder die feste IP aktiv ist. Die Einstellung ist im Backup.
- Die Adresse am besten außerhalb des DHCP-Bereichs des Routers wählen (Fritzbox: Heimnetz → Netzwerk →
  IPv4-Einstellungen), sonst kann der Router sie später an ein anderes Gerät vergeben.

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
| **Status** | Knoten, Funk (Rauschpegel, letzte RSSI/SNR), Paketzähler, WLAN, Chiptemperatur, Heap und PSRAM (mit Tiefststand), **Nachbarn** |
| **Konsole** | [Offizielle MeshCore-CLI](https://docs.meshcore.io/cli_commands/) (`get …`, `set …`, `neighbors`, `advert`, `region …`), auch für **andere Repeater per Funk** (Fernkonsole, siehe unten). Kritische Befehle (z. B. `erase`) fragen nach. |
| **Kanäle** | Kanäle anlegen (Hashtag, privat mit Schlüssel aus der App, Public), Nachrichten lesen und senden. Max. 8 Kanäle. |
| **Überwachung** | Sensormeter im LAN, Regeln mit Grenzwert, Hysterese und Entwarnung; Ping-Ziele; Chiptemperatur als „Gateway (intern)“. Meldungen gehen in den gewählten Kanal, auf Wunsch „aktiv bis bestätigt“ (siehe unten). |
| **Pakete** | Live-Mitschnitt der empfangenen/gesendeten Pakete |
| **Einstellungen** | MeshCore-Einstellungen, Empfehlung, Regionen, Admin-Passwort, Backup |
| **System** | WLAN, Firmware-Update, Ereignis-Log (Download, max. 128 KB im Flash), Kanal-API-Schlüssel |

**Display** (Taste PRG, Auto-Aus nach 60 s): 1 Netz (Name, WLAN, IP) · 2 Funk · 3 Mesh (Pakete, Uhrzeit) ·
4 neueste Kanalnachricht bzw. Alarm.

**Alarme „aktiv bis bestätigt“** (ab 0.9.0, je Regel und je Ping-Ziel ein Haken): Kanalnachrichten haben keine
Empfangsbestätigung und werden nicht gespeichert, wer gerade nicht im Mesh ist, verpasst sie. Mit dem Haken bekommt
der Alarm eine Nummer, z. B. `[ACK 3] ALARM Heizraum: Temperatur 31°C (>30°C)`, und wird nach **5, 15 und 30
Minuten** wiederholt (`[ACK 3] WDH 1/3: …`), bis jemand **im selben Kanal** antwortet:
- `ACK` bestätigt alle offenen Alarme in diesem Kanal, `ACK 3` nur Nummer 3 (Groß-/Kleinschreibung egal, eine
  Antwort per „Antworten“ in der App mit `@[Gateway-Name]` davor geht auch). Ganze Sätze wie „ack, schau ich mir
  an“ zählen absichtlich nicht.
- Das Gateway antwortet mit `ACK 3 bestätigt (Name)`, damit alle im Kanal es sehen.
- Unter Überwachung → **Offene Bestätigungen** stehen alle offenen Alarme; „bestätigen“ dort wirkt wie ein ACK im
  Kanal (Rückmeldung „bestätigt (Web)“).
- Geht der Wert vorher wieder in Ordnung, laufen die Wiederholungen weiter, mit Zusatz „(inzw. OK)“.
- Nach drei Wiederholungen ist Schluss; bestätigen geht noch 24 Stunden. Wiederholungen zählen zum Sendelimit
  pro Stunde; ist es erreicht, wird die Wiederholung 5 Minuten später nachgeholt.
- Die offenen Alarme stehen nur im RAM: ein Neustart des Gateways vergisst sie (steht der Alarm dann noch an,
  wird er neu gemeldet).
- Antwort per **Direktnachricht** an das Gateway geht nicht: ein Repeater nimmt Direktnachrichten nur von
  angemeldeten Admins/Gästen an. Deshalb im Kanal antworten.

**Zabbix-Anbindung** (ab 0.9.0): Ein zweites Gateway mit WLAN zum Zabbix-Proxy holt die Alarme aus dem Mesh und
liefert sie per Trapper (Zabbix-Sender-Protokoll, TCP 10051) ein. Beide Geräte haben dieselbe Firmware.

*Am meldenden Gateway* (Überwachung → Einstellungen → **Zabbix-Kennung**):
- **im Alarmtext anhängen**: jede Meldung bekommt am Ende eine Kennung, z. B.
  `[ACK 3] ALARM Heizraum: Temperatur 31°C (>30°C) #Z1;R3fa2;A;31;3` (Quelle, Ereignis, Wert, ACK-Nr). Der Text
  wird dafür nötigenfalls gekürzt, die Kennung nie.
- **zusätzlich in eigenem Zabbix-Kanal**: der Alarmkanal bleibt sauber, die Kopie mit Kennung geht in einen
  eigenen (privaten) Kanal. Kostet doppelt Airtime, beides zählt zum Sendelimit.
- Optional **Lebenszeichen** alle N Stunden (nur mit Kennung), damit Zabbix den Ausfall eines Gateways bemerkt.
- Ereignisse: A Alarm, E Erinnerung, W Wiederholung, O Entwarnung, F offline, N wieder online, H Lebenszeichen,
  T Test. Die Quelle (`R…` Regel, `P…` Ping-Ziel, `D…` Gerät, `G…` Gateway) ist ein Hash aus Gerät/Messwert/Richtung
  bzw. Name und bleibt gleich, wenn sich nur Grenzwert oder Reihenfolge ändern.

*Am Empfänger* (Überwachung → **Zabbix-Weiterleitung**): einschalten, Proxy/Server und Port eintragen, optional
Host-Präfix und Kanalfilter. „Testwert senden“ prüft die Verbindung (`meshcore.rx.heartbeat` am eigenen Host).
- Der Zabbix-Host einer Meldung = Präfix + Name des meldenden Gateways wie dessen WLAN-Hostname
  (`DE-BY-ORT01-GW` → `de-by-ort01-gw`), Key `meshcore.event`, Wert = JSON.
- **ACK**: Erst wenn Zabbix den Wert angenommen hat (`processed: 1`), antwortet der Empfänger im selben Kanal mit
  `@[DE-BY-ORT01-GW] ACK 3`. Damit hört das Gateway mit den Wiederholungen auf – „bestätigt“ heißt dann „liegt in
  Zabbix“, um die Benachrichtigung kümmert sich Zabbix. Ist Zabbix nicht erreichbar, gibt es kein ACK: der
  Empfänger versucht es alle 30 s (bis 1 h), das Gateway wiederholt im Mesh.
- Lehnt Zabbix ab (Host fehlt, Item kein Trapper, Allowed hosts), gibt es ebenfalls kein ACK; der Grund steht im
  Zabbix-Bereich und im Ereignis-Log.
- **Wird der Host von einem Proxy überwacht, muss der Empfänger an diesen Proxy senden** (der Server lehnt sonst ab).
- Ein ACK mit `@[Name]` gilt nur für das Gateway dieses Namens; ohne `@[…]` für alle Gateways im Kanal.
- Ist die Weiterleitung auf einem Gateway an, das selbst Alarme meldet, gehen auch dessen **eigene** Alarme an
  Zabbix (Host = eigener Name); das ACK bucht es dann direkt, ohne Funk.

*In Zabbix*: [`docs/zabbix-template-meshcore.yaml`](zabbix-template-meshcore.yaml) importieren (Format 7.0).
Template **MeshCore Gateway Alarme** je meldendem Gateway (Host ohne Schnittstelle, Name wie oben), Template
**MeshCore Zabbix-Empfaenger** für den Empfänger. Ein Problem je Alarm, die Entwarnung derselben Quelle schließt es.
Den Trigger „kein Lebenszeichen“ nur aktivieren, wenn am Gateway Lebenszeichen eingestellt sind.

**Fernkonsole** (ab 0.10.0): Andere Repeater im Mesh lassen sich vom Gateway aus per CLI verwalten, so wie mit der
MeshCore-App.
- Konsole → oben das Ziel wählen. Die Liste füllt sich aus empfangenen Adverts (Name, Schlüsselanfang, Hops, zuletzt
  gehört). Fehlt ein Repeater, „Schlüssel eingeben …“ wählen und seinen öffentlichen Schlüssel (64 Hex-Zeichen) eintragen.
- Mit dem **Admin-Passwort** des Repeaters anmelden. Das Passwort wird nicht gespeichert. Mit dem Gast-Passwort
  klappt die Anmeldung, CLI-Befehle beantwortet ein Repeater aber nur Admins.
- Befehle gehen dann per Funk an den Repeater, im Verlauf mit `[Name]` davor. Jede Antwort dauert einige Sekunden;
  kommt keine („keine Antwort (nochmal senden)“), einfach erneut senden. Es ist immer nur ein Befehl unterwegs.
- Bei `reboot`, `erase`, `set prv.key` usw. nennt die Rückfrage den Ziel-Repeater.
- „Abmelden“ oder 15 min ohne Befehl → die Konsole spricht wieder mit dem Gateway selbst.
- Keine Antwort auf die Anmeldung heißt: Passwort falsch **oder** Repeater nicht erreichbar (ein Repeater antwortet
  auf ein falsches Passwort gar nicht).

**Entfernte Repeater überwachen** (ab 0.10.0): Überwachung → Gerät hinzufügen → Typ **Repeater (per Funk)**.
- Repeater aus der Liste wählen (oder Schlüssel eintragen), Name, Passwort. Das **Gast-Passwort genügt**; hat der
  Repeater keins, leer lassen.
- Abfrage alle 3600 s (erlaubt 15 min bis 12 h). Je Abfrage gehen 2–3 kurze Pakete über Funk. Nach einem Fehlversuch
  fragt das Gateway nach 10 min erneut; nach „Offline nach … Fehlversuchen“ kommt die Meldung „nicht erreichbar“.
- Messwerte für Regeln: **Akku** (V), **Temperatur** (°C, meist die Chiptemperatur des Repeaters), **Laufzeit** (h),
  **Rauschen** (dBm). Beispiele: Akku unter 3,5 V; Temperatur über 60 °C; **Laufzeit unter 1 h = Repeater ist neu
  gestartet** (Absturz, Stromausfall).
- Regeln, Kanäle, „aktiv bis bestätigt“ und Zabbix-Kennung funktionieren wie bei Sensormetern. Die Abfrage läuft per
  Funk, also auch ohne WLAN am Gateway.
- Höchstens 8 Geräte insgesamt (Sensormeter und Repeater zusammen).

**Kanal-API** (ab 0.10.0): Andere Systeme im LAN können die Kanäle und Nachrichten des Gateways lesen, z. B.
home-central, das Nachrichten eines Kanals an Telegram weiterleitet, und – wenn freigegeben – in einen Kanal
senden. Der Zugriff läuft über einen eigenen API-Schlüssel, nicht über das Admin-Passwort.
- System → **Kanal-API** → „Schlüssel erzeugen“. Den Schlüssel (32 Hex-Zeichen) im anderen System eintragen.
  „Neu erzeugen“ macht den alten sofort ungültig, „Ausschalten“ sperrt die API. Ohne Schlüssel ist die API aus.
- **Senden** (z. B. Unwetter- und NINA/KATWARN-Warnungen von home-central): zusätzlich **„Senden erlauben in“**
  einen Kanal wählen und „Übernehmen“. Nur in diesen einen Kanal darf über die API gesendet werden, höchstens
  **10 Nachrichten pro Stunde**; zu lange Texte kürzt das Gateway.
- Endpunkte, Felder, Fehlercodes, Beispiele und der Ablauf zum Abonnieren: **[Kanal-API – Referenz](api.md)**.
  Kurz: `GET /api/v1/channels`, `GET /api/v1/messages?since=<next>`, `POST /api/v1/send`, jeweils mit Header
  `X-API-Key: <Schlüssel>`.
- Die Übertragung ist unverschlüsselt (HTTP), wie die ganze Weboberfläche: nur im eigenen Netz verwenden. Der
  Schlüssel ist nicht im Backup; nach einer Wiederherstellung neu erzeugen.

**Private Kanäle** aus der App übernehmen: In der App den Kanal teilen bzw. den Schlüssel anzeigen (32 Hex-Zeichen)
und unter Kanäle → „Privat“ mit gleichem Namen und Schlüssel anlegen.

## 7. Updates

- **Laufendes Gateway: System → Firmware-Update**, die `firmware.bin` für dein Board hochladen und
  „Hochladen & installieren“. Alle Einstellungen bleiben. Aktuelle Dateien:
  - bewährt 0.10.0 (MeshCore 1.18.0):
    [V4](https://peterhagelhof7-cmd.github.io/esp-meshcore-gateway/fw/0.10.0/v4/firmware.bin) ·
    [V4-R8](https://peterhagelhof7-cmd.github.io/esp-meshcore-gateway/fw/0.10.0/r8/firmware.bin)
  - ältere bewährte Version 0.9.0 (MeshCore 1.18.0):
    [V4](https://peterhagelhof7-cmd.github.io/esp-meshcore-gateway/fw/0.9.0/v4/firmware.bin) ·
    [V4-R8](https://peterhagelhof7-cmd.github.io/esp-meshcore-gateway/fw/0.9.0/r8/firmware.bin)
  - ältere bewährte Version 0.8.0 (MeshCore 1.18.0):
    [V4](https://peterhagelhof7-cmd.github.io/esp-meshcore-gateway/fw/0.8.0/v4/firmware.bin) ·
    [V4-R8](https://peterhagelhof7-cmd.github.io/esp-meshcore-gateway/fw/0.8.0/r8/firmware.bin)
  - ältere Version 0.7.6 (MeshCore 1.17.1):
    [V4](https://peterhagelhof7-cmd.github.io/esp-meshcore-gateway/fw/0.7.6/v4/firmware.bin) ·
    [V4-R8](https://peterhagelhof7-cmd.github.io/esp-meshcore-gateway/fw/0.7.6/r8/firmware.bin)

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
