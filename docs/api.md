# Kanal-API – Referenz

Ab Firmware **0.10.0**. Über diese API lesen andere Systeme im LAN die Kanäle und Kanalnachrichten eines
Gateways und können, wenn freigegeben, in **einen** Kanal senden. Typisches Beispiel ist
[home-central](https://github.com/peterhagelhof7-cmd/home-central): Nachrichten eines Kanals gehen an Telegram,
Unwetter- und NINA-Warnungen gehen ins Mesh.

Bedienung im Gateway und Hintergründe: [Admin-Guide §6](admin-guide.md#6-betrieb), Entscheidungen
[E43](entscheidungen.md) (Lesen) und [E44](entscheidungen.md) (Senden).

## Überblick

| Methode | Pfad | Wozu |
|---|---|---|
| `GET` | `/api/v1/channels` | Kanäle des Gateways |
| `GET` | `/api/v1/messages` | Kanalnachrichten ab einem Zeiger („abonnieren“) |
| `POST` | `/api/v1/send` | Nachricht in den freigegebenen Kanal senden |

- Basis-URL: `http://<IP oder Hostname des Gateways>` (Port 80). Es gibt **nur HTTP**, kein HTTPS.
- Antworten sind immer JSON in UTF-8 (`Content-Type: application/json`), auch Fehler.
- Die API ist versioniert (`/v1/`). Felder kommen höchstens hinzu, bestehende ändern ihre Bedeutung nicht.
  Clients sollen unbekannte Felder ignorieren.
- Es werden keine CORS-Header gesendet, die API ist für Server gedacht, nicht für Webseiten fremder Herkunft.

## Einrichten und Anmelden

1. Im Gateway: **System → Kanal-API → „Schlüssel erzeugen“**. Der Schlüssel besteht aus 32 Hex-Zeichen
   (128 Bit Zufall, auf dem Gerät erzeugt).
2. Zum Senden zusätzlich **„Senden erlauben in“** auf genau einen Kanal stellen und „Übernehmen“. Ohne diese
   Freigabe erlaubt der Schlüssel nur Lesen.

Jede Anfrage trägt den Schlüssel in einem Header:

```
X-API-Key: 3f9c0a1b2c3d4e5f60718293a4b5c6d7
```

oder gleichwertig:

```
Authorization: Bearer 3f9c0a1b2c3d4e5f60718293a4b5c6d7
```

Der Schlüssel gehört **nicht** in die URL: Sie landet sonst in Logs von Proxys und Servern.

- „Neu erzeugen“ macht den alten Schlüssel sofort ungültig, „Ausschalten“ löscht ihn (API aus).
- Der Schlüssel ist nicht im Backup des Gateways. Nach einer Wiederherstellung neu erzeugen und beim Client
  eintragen.
- Das Admin-Passwort der Weboberfläche gilt für diese Endpunkte **nicht** und wird nicht gebraucht.

## Fehler

Fehler kommen immer als `{"error": "<Text auf Deutsch>"}` mit passendem HTTP-Status. Der Text ist für Menschen
gedacht und kann sich ändern; Clients werten den **Status** aus.

| Status | Bedeutung | Endpunkte |
|---|---|---|
| `400` | Anfrage unvollständig, z. B. leerer Text | send |
| `401` | Schlüssel fehlt oder falsch | alle |
| `403` | API ausgeschaltet (kein Schlüssel erzeugt) | alle |
| `403` | Senden nicht freigegeben, oder anderer als der freigegebene Kanal | send |
| `404` | Der freigegebene Kanal existiert im Gateway nicht mehr | send |
| `429` | Sendelimit erreicht (10 Nachrichten pro Stunde) | send |
| `503` | Sende-Warteschlange gerade voll, gleich nochmal versuchen | send |

Ein unbekannter Pfad liefert `404` mit Klartext `Nicht gefunden`.

## GET /api/v1/channels

Alle im Gateway angelegten Kanäle, **ohne** Kanalschlüssel.

```
curl -H "X-API-Key: 3f9c…" http://10.10.0.151/api/v1/channels
```

```json
{
  "boot": "a088dea8",
  "gateway": "DE-XX-ORT02-GW",
  "channels": [
    {"idx": 0, "name": "Public",      "kind": "public",  "scope": ""},
    {"idx": 1, "name": "#rhein-main", "kind": "hashtag", "scope": "rhein-main"},
    {"idx": 2, "name": "haus-alarm",   "kind": "private", "scope": ""}
  ]
}
```

| Feld | Typ | Bedeutung |
|---|---|---|
| `boot` | String, 8 Hex | Kennung dieses Starts des Gateways, neu bei jedem Neustart (siehe unten) |
| `gateway` | String | Gerätename (MeshCore-Knotenname) |
| `channels[].idx` | Zahl 0–7 | Platz des Kanals im Gateway |
| `channels[].name` | String | Kanalname, genau so wie in `messages` und `send` zu verwenden |
| `channels[].kind` | `public` / `hashtag` / `private` | Art des Kanals |
| `channels[].scope` | String | Region, in die das Gateway sendet; leer = ohne Region |

## GET /api/v1/messages

Empfangene **und** vom Gateway gesendete Kanalnachrichten, ältere zuerst.

| Parameter | Pflicht | Bedeutung |
|---|---|---|
| `since` | nein (Standard `0`) | Zeiger: `next` aus der vorigen Antwort. `0` = ab der ältesten noch vorhandenen Nachricht |
| `channel` | nein | nur dieser Kanal (exakter Name). `#` in der URL als `%23` schreiben |

```
curl -H "X-API-Key: 3f9c…" "http://10.10.0.151/api/v1/messages?since=0&channel=haus-alarm"
```

```json
{
  "boot": "a088dea8",
  "messages": [
    {"seq": 41, "time": 1792190013, "dir": "rx", "channelIdx": 2, "channel": "haus-alarm",
     "sender": "Anna", "text": "ACK 3", "snr": 7.25, "hops": 2},
    {"seq": 42, "time": 1792190020, "dir": "tx", "channelIdx": 2, "channel": "haus-alarm",
     "sender": "DE-XX-ORT02-GW", "text": "ACK 3 bestätigt (Anna)", "snr": null, "hops": 0}
  ],
  "next": 43,
  "gap": false
}
```

| Feld | Typ | Bedeutung |
|---|---|---|
| `boot` | String | wie bei `channels` |
| `next` | Zahl | Zeiger für die nächste Anfrage (`since=next`) |
| `gap` | bool | `true` = zwischen `since` und der ältesten noch vorhandenen Nachricht fehlen Nachrichten (zu lange nicht gefragt) |
| `messages[].seq` | Zahl | fortlaufende Nummer, eindeutig innerhalb eines `boot` |
| `messages[].time` | Zahl | Unix-Zeit (s). Bei `rx` die Uhr des **Absenders**, bei `tx` die des Gateways. Uhren im Mesh können falsch gehen; vor der NTP-Zeit des Gateways auch `tx` |
| `messages[].dir` | `rx` / `tx` | `rx` empfangen, `tx` vom Gateway selbst gesendet (Alarme, Web, API) |
| `messages[].channelIdx` | Zahl | Platz des Kanals zum Zeitpunkt der Nachricht |
| `messages[].channel` | String | Kanalname |
| `messages[].sender` | String | Name vor dem ersten `": "` im Nachrichtentext (MeshCore-Konvention „Name: Text“); leer, wenn es keinen gibt |
| `messages[].text` | String | Text nach dem Absendernamen. Zeilenumbrüche/Tabs sind durch Leerzeichen ersetzt |
| `messages[].snr` | Zahl / `null` | Empfangsqualität in dB, bei `tx` `null` |
| `messages[].hops` | Zahl | Anzahl Weiterleitungen, bei `tx` `0` |

### Zeiger, Seiten und Grenzen

- Eine Antwort deckt **höchstens 50 Log-Zeilen** ab. Mit Kanalfilter können es weniger Nachrichten sein, `next`
  rückt trotzdem über die übersprungenen Zeilen hinweg.
- Solange `next` größer als das gesendete `since` ist, kann es weitere geben: sofort mit `since=next` nachfragen.
  Ist `next == since`, ist nichts Neues da.
- Das Gateway hält die **letzten 200 Kanal-Zeilen im RAM**. Liegt `since` davor, kommt `gap: true` und die Antwort
  beginnt bei der ältesten noch vorhandenen Zeile.
- Nach einem **Neustart des Gateways** ist der Puffer leer und die Zählung beginnt neu bei 1; `boot` hat dann einen
  anderen Wert. Ein Client, der sich `boot` merkt, setzt dann `since` auf `0`. Alles ab dort ist neu.
  (Ohne diese Prüfung bliebe ein alter, großer Zeiger einfach stehen, bis die Zählung ihn wieder erreicht.)
- Zeilen gehen bei einem Neustart verloren; die API ist **keine** Zustellgarantie.

### Abonnieren: Client-Ablauf

```
gespeichert: boot_alt, zeiger          # z. B. in einer Datei, damit ein Neustart des Clients nichts verliert

alle 10–30 s:
    ch = GET /api/v1/channels
    wenn nichts gespeichert:              # erster Start: alten Verlauf nicht nachholen
        zeiger = Ende ermitteln (Seiten durchlaufen, nichts verarbeiten)
    sonst wenn ch.boot != boot_alt:       # Gateway neu gestartet
        zeiger = 0
    wiederholen:
        r = GET /api/v1/messages?since=zeiger
        wenn r.boot != ch.boot: abbrechen  # genau jetzt neu gestartet -> nächste Runde
        r.messages verarbeiten (bei r.gap: Hinweis „verpasst“)
        wenn r.next == zeiger: fertig
        zeiger = r.next
    boot_alt = ch.boot; zeiger speichern
```

Fragt der Client öfter als alle 10 s, bringt das nichts (Funk ist langsam) und belastet den kleinen Webserver.

## POST /api/v1/send

Sendet eine Nachricht in den **freigegebenen** Kanal. Das Gateway stellt wie immer seinen Gerätenamen voran
(`DE-XX-ORT02-GW: <Text>`).

Body als **Formular** (`application/x-www-form-urlencoded`, UTF-8). JSON im Body wird nicht ausgewertet.

| Feld | Pflicht | Bedeutung |
|---|---|---|
| `channel` | ja | Kanalname, muss genau dem freigegebenen Kanal entsprechen |
| `text` | ja | Nachricht. Zeilenumbrüche und Tabs werden zu Leerzeichen, Leerzeichen am Rand entfernt |

```
curl -H "X-API-Key: 3f9c…" --data-urlencode "channel=warnungen" \
     --data-urlencode "text=⚠ Wetterwarnung: Amtliche WARNUNG vor STURMBÖEN" \
     http://10.10.0.151/api/v1/send
```

```json
{"ok": true, "truncated": false, "text": "⚠ Wetterwarnung: Amtliche WARNUNG vor STURMBÖEN"}
```

| Feld | Bedeutung |
|---|---|
| `ok` | `true` = in die Sende-Warteschlange gestellt (das Gateway sendet sie im nächsten Augenblick per Funk) |
| `truncated` | `true` = Text war zu lang und wurde gekürzt |
| `text` | der tatsächlich gesendete Text (nach Bereinigen und Kürzen) |

- **Länge:** Eine MeshCore-Kanalnachricht fasst 160 Byte inklusive „Gerätename: “. Längere Texte werden auf den
  Rest **gekürzt**, ohne ein UTF-8-Zeichen (Umlaut, Emoji) zu zerschneiden. Faustregel: etwa 140 Byte; Umlaute
  zählen 2, die meisten Emojis 4 Byte.
- **Sendelimit:** höchstens **10 Nachrichten pro Stunde** (gleitendes Fenster), danach `429`. Gezählt werden nur
  angenommene Nachrichten (`200`); abgelehnte Anfragen (`4xx`, `503`) verbrauchen nichts. Das Limit gilt für die
  API allein, unabhängig von den Meldungen der Überwachung.
- `ok: true` heißt angenommen, nicht zugestellt: Kanalnachrichten sind Floods ohne Empfangsbestätigung.
- Die gesendete Nachricht erscheint anschließend in `messages` mit `dir: "tx"`. Wer selbst sendet und gleichzeitig
  abonniert, sollte sich den zurückgemeldeten `text` merken und solche `tx`-Nachrichten überspringen (sonst Echo).
- Jedes Senden steht im Ereignis-Log des Gateways (`API  Senden in <Kanal> von <IP>`).

## Sicherheit

- Nur HTTP: Schlüssel und Inhalte gehen unverschlüsselt durchs Netz. Nur im eigenen, vertrauenswürdigen LAN
  verwenden, nie ins Internet freigeben.
- Lesen sieht alle Kanäle des Gateways, auch private. Wer den Schlüssel hat, liest also mit.
- Senden ist auf **einen** Kanal und 10 Nachrichten pro Stunde begrenzt; private Kanäle und der Alarmkanal bleiben
  geschützt, solange sie nicht freigegeben sind.
- Bei Verdacht: im Gateway „Neu erzeugen“ – der alte Schlüssel gilt sofort nicht mehr.

## Beispiel in Python

```python
import requests

GW = "http://10.10.0.151"
H = {"X-API-Key": "3f9c0a1b2c3d4e5f60718293a4b5c6d7"}

boot, since = None, 0
ch = requests.get(f"{GW}/api/v1/channels", headers=H, timeout=10).json()
if ch["boot"] != boot:
    boot, since = ch["boot"], 0
while True:
    r = requests.get(f"{GW}/api/v1/messages", params={"since": since, "channel": "haus-alarm"},
                     headers=H, timeout=10).json()
    for m in r["messages"]:
        print(m["dir"], m["sender"], m["text"])
    if r["next"] == since:
        break
    since = r["next"]

r = requests.post(f"{GW}/api/v1/send", headers=H, timeout=10,
                  data={"channel": "warnungen", "text": "Test von außen"})
print(r.status_code, r.json())
```
