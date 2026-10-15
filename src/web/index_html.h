#pragma once

// Weboberfläche: Status, Konsole (offizielle MeshCore-CLI), Paket-Log, WLAN.
// Bewusst ohne externe Ressourcen, funktioniert auch im Installer-AP ohne
// Internet. Daten kommen per Polling (docs/entscheidungen.md E10).
static const char INDEX_HTML[] = R"HTML(<!doctype html>
<html lang="de">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>MeshCore Gateway</title>
<style>
:root{--bg:#f4f5f7;--card:#fff;--fg:#1d2330;--muted:#677084;--line:#e1e4ea;--acc:#0b6bcb;--ok:#1a8f4a;--warn:#c27a00;--bad:#c0392b;--term:#0f1218;--termfg:#d8dee9}
@media (prefers-color-scheme:dark){:root{--bg:#12151b;--card:#1b2029;--fg:#e6e9ef;--muted:#9aa3b5;--line:#2b3240;--acc:#5aa2f0;--ok:#43c47a;--warn:#e0a53a;--bad:#ef6a5b;--term:#0b0d12}}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--fg);font:15px/1.45 system-ui,-apple-system,Segoe UI,Roboto,sans-serif}
header{display:flex;align-items:center;gap:12px;padding:12px 16px;background:var(--card);border-bottom:1px solid var(--line);flex-wrap:wrap}
header h1{font-size:18px;margin:0;flex:1;min-width:0;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.pill{font-size:12px;padding:2px 8px;border-radius:99px;border:1px solid var(--line);color:var(--muted)}
nav{display:flex;gap:4px;padding:0 12px;background:var(--card);border-bottom:1px solid var(--line);overflow-x:auto}
nav a{padding:10px 12px;color:var(--muted);text-decoration:none;border-bottom:2px solid transparent;white-space:nowrap}
nav a.on{color:var(--fg);border-bottom-color:var(--acc)}
.view{display:none;max-width:1100px;margin:0 auto;padding:16px}
.view.on{display:block}
.grid{display:grid;gap:16px;grid-template-columns:repeat(auto-fit,minmax(300px,1fr))}
section{background:var(--card);border:1px solid var(--line);border-radius:10px;padding:14px 16px}
section h2{font-size:14px;text-transform:uppercase;letter-spacing:.04em;color:var(--muted);margin:0 0 10px}
dl{display:grid;grid-template-columns:auto 1fr;gap:4px 14px;margin:0}
dt{color:var(--muted)}dd{margin:0;font-variant-numeric:tabular-nums;overflow-wrap:anywhere}
.mono{font-family:ui-monospace,Consolas,monospace;font-size:13px}
label{display:block;margin:10px 0 4px;color:var(--muted);font-size:13px}
input,select{width:100%;padding:8px 10px;border:1px solid var(--line);border-radius:6px;background:var(--bg);color:var(--fg);font:inherit}
.row{display:flex;gap:8px;margin-top:12px;flex-wrap:wrap;align-items:center}
button{padding:8px 14px;border:1px solid var(--acc);background:var(--acc);color:#fff;border-radius:6px;font:inherit;cursor:pointer}
button.sec{background:transparent;color:var(--acc)}
[hidden]{display:none!important}
a.btn{display:inline-block;padding:8px 14px;border:1px solid var(--acc);border-radius:6px;text-decoration:none;color:var(--acc)}
button.chip{padding:4px 10px;font-size:13px;background:transparent;color:var(--fg);border-color:var(--line)}
button:disabled{opacity:.5;cursor:default}
.msg{margin-top:10px;font-size:13px;min-height:1em}
.ok{color:var(--ok)}.warn{color:var(--warn)}.bad{color:var(--bad)}
.term{background:var(--term);color:var(--termfg);border-radius:8px;padding:10px 12px;height:55vh;min-height:260px;overflow:auto;font:13px/1.5 ui-monospace,Consolas,monospace;white-space:pre-wrap;overflow-wrap:anywhere}
.term .c{color:#7fb8ff}.term .s{color:#8b93a5;font-style:italic}.term .R{color:#9fd49f}.term .T{color:#e8c46a}.term .F{color:#ff8a7a}
.term .ts{color:#5d6577;margin-right:8px}
.cli{display:flex;gap:8px;margin-top:10px}
.cli input{font-family:ui-monospace,Consolas,monospace}
.hint{font-size:13px;color:var(--muted);margin:8px 0 0}
.chk{display:inline-flex;gap:6px;align-items:center;font-size:13px;color:var(--muted);margin:0}
.chk input{width:auto}
.form{display:grid;grid-template-columns:minmax(150px,240px) 1fr;gap:8px 16px;align-items:start}
.form>label{margin:8px 0 0;color:var(--fg);font-size:14px}
.form .fld{display:flex;flex-direction:column;gap:3px}
.form .fld small{color:var(--muted);font-size:12px}
.form .fld.chg input,.form .fld.chg select{border-color:var(--warn);box-shadow:0 0 0 1px var(--warn)}
.form .fld.na{opacity:.55}
.radio4{display:grid;grid-template-columns:2fr 1.3fr 1fr 1fr;gap:6px}
.grp{grid-column:1/-1;margin:14px 0 2px;font-size:13px;text-transform:uppercase;letter-spacing:.04em;color:var(--muted);border-bottom:1px solid var(--line);padding-bottom:4px}
.grp:first-child{margin-top:0}
.banner{display:none;margin:0 0 16px;padding:10px 14px;border:1px solid var(--warn);border-radius:8px;color:var(--fg);background:color-mix(in srgb,var(--warn) 12%,transparent)}
.banner.on{display:flex;gap:12px;align-items:center;flex-wrap:wrap}
table{width:100%;border-collapse:collapse;font-size:13px}
th,td{text-align:left;padding:6px 8px;border-bottom:1px solid var(--line);vertical-align:top}
th{color:var(--muted);font-weight:500}
td.st{white-space:nowrap}
.tbl{overflow-x:auto}
pre.out{background:var(--bg);border:1px solid var(--line);border-radius:6px;padding:8px 10px;margin:8px 0 0;font:13px/1.45 ui-monospace,Consolas,monospace;white-space:pre-wrap;max-height:240px;overflow:auto}
@media (max-width:640px){.form{grid-template-columns:1fr}.form>label{margin-top:10px}.radio4{grid-template-columns:1fr 1fr}}
</style>
</head>
<body>
<header><h1 id="title">MeshCore Gateway</h1><span class="pill" id="wifiPill">…</span></header>
<nav>
<a href="#status" data-v="status">Status</a>
<a href="#console" data-v="console">Konsole</a>
<a href="#channels" data-v="channels">Kanäle</a>
<a href="#sm" data-v="sm">Überwachung</a>
<a href="#packets" data-v="packets">Pakete</a>
<a href="#settings" data-v="settings">Einstellungen</a>
<a href="#wifi" data-v="wifi">System</a>
</nav>

<div class="view" id="v_status"><div class="grid">
<section>
<h2>Knoten</h2>
<dl>
<dt>Name</dt><dd id="n_name">–</dd>
<dt>Rolle</dt><dd id="n_role">–</dd>
<dt>MeshCore</dt><dd id="n_fw">–</dd>
<dt>Gateway</dt><dd id="g_ver">–</dd>
<dt>Weiterleiten</dt><dd id="n_fwd">–</dd>
<dt>Position</dt><dd id="n_pos">–</dd>
<dt>Laufzeit</dt><dd id="c_up">–</dd>
<dt>Public Key</dt><dd class="mono" id="n_pub">–</dd>
</dl>
</section>
<section>
<h2>Funk</h2>
<dl>
<dt>Frequenz</dt><dd id="r_freq">–</dd>
<dt>Bandbreite</dt><dd id="r_bw">–</dd>
<dt>SF / CR</dt><dd id="r_sfcr">–</dd>
<dt>TX-Leistung</dt><dd id="r_tx">–</dd>
<dt>Rauschpegel</dt><dd id="r_nf">–</dd>
<dt>Letztes RSSI/SNR</dt><dd id="r_last">–</dd>
<dt>Sendezeit</dt><dd id="r_air">–</dd>
</dl>
</section>
<section>
<h2>Pakete</h2>
<dl>
<dt>Empfangen</dt><dd id="p_recv">–</dd>
<dt>Gesendet</dt><dd id="p_sent">–</dd>
<dt>Flood RX/TX</dt><dd id="p_flood">–</dd>
<dt>Direkt RX/TX</dt><dd id="p_direct">–</dd>
<dt>Empfangsfehler</dt><dd id="p_err">–</dd>
<dt>Sendewarteschlange</dt><dd id="c_queue">–</dd>
</dl>
</section>
<section>
<h2>WLAN</h2>
<dl>
<dt>Modus</dt><dd id="w_mode">–</dd>
<dt>SSID</dt><dd id="w_ssid">–</dd>
<dt>IP</dt><dd id="w_ip">–</dd>
<dt>Adressvergabe</dt><dd id="w_ipmode">–</dd>
<dt>Signal</dt><dd id="w_rssi">–</dd>
<dt>Hostname</dt><dd id="w_host">–</dd>
<dt>MAC</dt><dd class="mono" id="w_mac">–</dd>
<dt>Freier Heap</dt><dd id="g_heap">–</dd>
<dt>PSRAM</dt><dd id="g_psram">–</dd>
<dt>Chiptemperatur</dt><dd id="g_chip" title="Temperatur im ESP32-S3, nicht die Raumtemperatur">–</dd>
</dl>
</section>
<section>
<h2>Nachbarn</h2>
<div class="tbl"><table><thead><tr><th>Repeater</th><th>zuletzt gehört</th><th>SNR</th></tr></thead><tbody id="nbBody"><tr><td colspan="3">–</td></tr></tbody></table></div>
<div class="row"><button type="button" class="sec" id="nbScan">Jetzt suchen</button></div>
<div class="msg" id="nbMsg"></div>
<p class="hint">Repeater in direkter Funkreichweite (0 Hops). Die Liste steht nur im Arbeitsspeicher; nach jedem Start sucht das Gateway nach 90 s einmal selbst. Danach kommen Nachbarn mit ihren 0-Hop-Adverts dazu (meist alle 4 h). Kennung = Anfang des Public Keys wie in der App.</p>
</section>
</div></div>

<div class="view" id="v_console">
<section>
<h2>Konsole</h2>
<div class="term" id="conOut"></div>
<form class="cli" id="cliForm" autocomplete="off">
<input id="cliIn" list="cmdList" maxlength="159" placeholder="Befehl, z. B. ver, neighbors, get radio" spellcheck="false" autocapitalize="off">
<button type="submit">Senden</button>
</form>
<datalist id="cmdList"></datalist>
<div class="row" id="quick"></div>
<p class="hint">Offizielle MeshCore-Repeater-CLI mit den Rechten der seriellen Konsole. ↑/↓ blättert durch frühere Befehle. „log“ gibt den Datei-Log nur seriell aus. „poweroff“/„shutdown“ legt das Gateway schlafen, danach ist es nur per Reset/Stromtrennung wieder erreichbar.</p>
</section>
</div>

<div class="view" id="v_packets">
<section>
<h2>Paket-Log (live)</h2>
<div class="row" style="margin:0 0 10px">
<input id="pktFilter" placeholder="Filter, z. B. ADVERT oder GRP_TXT" style="flex:1;min-width:180px">
<label class="chk"><input type="checkbox" id="pktRx" checked> RX</label>
<label class="chk"><input type="checkbox" id="pktTx" checked> TX</label>
<button type="button" class="sec" id="pktPause">Pause</button>
<button type="button" class="sec" id="pktClear">Leeren</button>
</div>
<div class="term" id="pktOut"></div>
<p class="hint">Zeigt die letzten 300 Pakete seit dem Start. Unabhängig davon schreibt „log start“ wie im Original in die Datei auf dem Knoten.</p>
</section>
</div>

<div class="view" id="v_channels">
<section style="margin-bottom:16px">
<h2>Nachrichten</h2>
<div class="term" id="msgOut" style="height:40vh"></div>
<form class="cli" id="msgForm" autocomplete="off">
<select id="msgCh" style="width:auto;max-width:40%"></select>
<input id="msgText" maxlength="160" placeholder="Nachricht" spellcheck="true">
<button type="submit">Senden</button>
</form>
<p class="hint" id="msgHint">Kanalnachrichten sind Floods und kosten Airtime im ganzen Netz. Nicht zum Testen in Public schreiben.</p>
</section>

<section>
<h2>Kanäle</h2>
<div class="tbl"><table><thead><tr><th>Name</th><th>Art</th><th>Scope</th><th>Hash</th><th>Schlüssel</th><th></th></tr></thead><tbody id="chBody"></tbody></table></div>
<p class="hint" id="chEmpty" hidden>Noch keine Kanäle angelegt.</p>
<div class="form" style="margin-top:14px">
<div class="grp">Kanal hinzufügen</div>
<label for="chKind">Art</label>
<div class="fld"><select id="chKind"><option value="hashtag">Hashtag (#name, Schlüssel aus dem Namen)</option><option value="private">Privat (eigener Schlüssel)</option><option value="public">Public (öffentlicher Standardkanal)</option></select></div>
<label for="chName">Name</label>
<div class="fld"><input id="chName" maxlength="31" spellcheck="false" placeholder="#test"><small id="chNameHint">Hashtag-Kanäle: klein, ohne Umlaute und Leerzeichen. Groß-/Kleinschreibung ergibt sonst einen anderen Kanal.</small></div>
<label for="chKey" id="chKeyLab">Schlüssel</label>
<div class="fld" id="chKeyBox"><input id="chKey" maxlength="44" spellcheck="false" placeholder="leer = neuen Zufallsschlüssel erzeugen"><small>16 Byte als Base64 (wie in der App) oder 32 Hex-Zeichen</small></div>
<label for="chScope">Scope (Region)</label>
<div class="fld"><input id="chScope" maxlength="30" spellcheck="false" value="rhein-main" placeholder="leer = ohne Scope"><small>Vorbelegt mit der lokalen Region rhein-main (Betreiber-Vorgabe). Mit Scope leiten nur Repeater weiter, die diese Region kennen (bis flood.max Hops). Ohne Scope gilt flood.max.unscoped.</small></div>
</div>
<div class="row"><button type="button" id="chAdd">Hinzufügen</button><span class="msg" id="chMsg" style="margin:0"></span></div>
</section>
</div>

<div class="view" id="v_sm">
<section style="margin-bottom:16px">
<h2>Geräte</h2>
<div class="tbl"><table><thead><tr><th>Name</th><th>Adresse</th><th>Status</th><th>Messwerte</th><th></th></tr></thead><tbody id="smDevBody"></tbody></table></div>
<p class="hint" id="smDevEmpty" hidden>Noch kein Sensormeter eingetragen.</p>
<div class="form" style="margin-top:14px" id="smDevForm">
<div class="grp" id="smDevTitle">Gerät hinzufügen</div>
<label for="sdName">Name</label><div class="fld"><input id="sdName" maxlength="23" placeholder="z. B. Serverraum"></div>
<label for="sdHost">Adresse</label><div class="fld"><input id="sdHost" maxlength="63" placeholder="10.10.0.148" spellcheck="false"><small>IP oder Hostname im LAN, abgefragt wird http://Adresse/api/sensors</small></div>
<label for="sdPort">Port</label><div class="fld"><input id="sdPort" type="number" min="1" max="65535" value="80"></div>
<label for="sdInt">Abfrage alle (s)</label><div class="fld"><input id="sdInt" type="number" min="10" max="3600" value="60"></div>
<label for="sdUser">Login (optional)</label><div class="fld"><div class="radio4" style="grid-template-columns:1fr 1fr"><input id="sdUser" maxlength="15" placeholder="Benutzer" autocomplete="off"><input id="sdPass" type="password" maxlength="31" placeholder="Passwort" autocomplete="new-password"></div><small>Sensormeter liefern /api/sensors ohne Login</small></div>
<label>Optionen</label><div class="fld"><label class="chk"><input type="checkbox" id="sdOff" checked> Meldung, wenn nicht erreichbar</label><label class="chk"><input type="checkbox" id="sdEn" checked> aktiv</label></div>
</div>
<div class="row"><button type="button" id="sdSave">Gerät speichern</button><button type="button" class="sec" id="sdCancel" hidden>Abbrechen</button><span class="msg" id="sdMsg" style="margin:0"></span></div>
</section>

<section style="margin-bottom:16px" id="ackSec" hidden>
<h2>Offene Bestätigungen</h2>
<div class="tbl"><table><thead><tr><th>Nr</th><th>Meldung</th><th>Kanal</th><th>Seit</th><th>Wiederholt</th><th></th></tr></thead><tbody id="ackBody"></tbody></table></div>
<p class="hint">Bestätigen per Antwort im Kanal: <b>ACK</b> (alle offenen dort) oder <b>ACK Nr</b>. Hier bestätigen schickt ebenfalls eine kurze Rückmeldung in den Kanal.</p>
<span class="msg" id="ackMsg" style="margin:0"></span>
</section>

<section style="margin-bottom:16px">
<h2>Regeln</h2>
<div class="tbl"><table><thead><tr><th>Gerät / Messwert</th><th>Bedingung</th><th>Kanal</th><th>Zustand</th><th></th></tr></thead><tbody id="smRuleBody"></tbody></table></div>
<p class="hint" id="smRuleEmpty" hidden>Noch keine Regeln.</p>
<div class="form" style="margin-top:14px">
<div class="grp" id="smRuleTitle">Regel hinzufügen</div>
<label for="srDev">Gerät</label><div class="fld"><select id="srDev"></select></div>
<label for="srPath">Messwert</label><div class="fld"><select id="srPath"></select><small id="srPathHint">Liste aus der letzten Abfrage des Geräts</small></div>
<label for="srOp">Bedingung</label><div class="fld"><div class="radio4" style="grid-template-columns:1fr 1fr"><select id="srOp"><option value="0">über (&gt;)</option><option value="1">unter (&lt;)</option></select><input id="srThr" type="number" step="any" placeholder="Grenzwert"></div></div>
<label for="srHyst">Hysterese</label><div class="fld"><input id="srHyst" type="number" step="any" min="0" value="0.5"><small>Entwarnung erst, wenn der Wert um so viel zurück ist (verhindert Flattern)</small></div>
<label for="srMin">Mindestdauer (s)</label><div class="fld"><input id="srMin" type="number" min="0" max="86400" value="0"><small>So lange muss die Grenze überschritten sein, 0 = sofort</small></div>
<label for="srRep">Erinnern alle (min)</label><div class="fld"><input id="srRep" type="number" min="0" max="1440" value="0"><small>0 = nur einmal melden</small></div>
<label for="srCh">Kanal</label><div class="fld"><select id="srCh"></select></div>
<label for="srText">Text</label><div class="fld"><input id="srText" maxlength="99" spellcheck="false"><small>leer = Standardtext. Platzhalter: {state} {device} {metric} {value} {unit} {op} {threshold}</small></div>
<label>Optionen</label><div class="fld"><label class="chk"><input type="checkbox" id="srRec" checked> Entwarnung senden</label><label class="chk"><input type="checkbox" id="srAck"> aktiv bis bestätigt</label><label class="chk"><input type="checkbox" id="srEn" checked> aktiv</label><small>„aktiv bis bestätigt“: Meldung bekommt eine Nummer und wird nach 5, 15 und 30 min wiederholt, bis jemand im Kanal mit <b>ACK</b> (alle offenen) oder <b>ACK Nr</b> antwortet.</small></div>
</div>
<div class="row"><button type="button" id="srSave">Regel speichern</button><button type="button" class="sec" id="srCancel" hidden>Abbrechen</button><span class="msg" id="srMsg" style="margin:0"></span></div>
</section>

<section style="margin-bottom:16px">
<h2>Ping-Check</h2>
<div class="tbl"><table><thead><tr><th>Bezeichner</th><th>Ziel</th><th>Schwelle / Prüfabstand</th><th>Status</th><th></th></tr></thead><tbody id="pgBody"></tbody></table></div>
<p class="hint" id="pgEmpty" hidden>Noch keine Ping-Ziele (max. 5).</p>
<div class="form" style="margin-top:14px">
<div class="grp" id="pgTitle">Ping-Ziel hinzufügen</div>
<label for="pgName">Bezeichner</label><div class="fld"><input id="pgName" maxlength="23" placeholder="z. B. Router"><small>Pflicht, erscheint in der Meldung</small></div>
<label for="pgHost">Name oder IP</label><div class="fld"><input id="pgHost" maxlength="63" placeholder="10.10.0.1 oder nas.example.de" spellcheck="false"></div>
<label for="pgThr">Schwellwert (s)</label><div class="fld"><input id="pgThr" type="number" min="10" max="65535" value="60"><small>Antwortet das Ziel länger als so viele Sekunden nicht, geht eine Meldung in den Alarmkanal. Mindestens so groß wie der Prüfabstand.</small></div>
<label for="pgIv">Prüfabstand (s)</label><div class="fld"><input id="pgIv" type="number" min="10" max="3600" value="10"><small>10–3600 s, je Prüfung 3 Pings (eine Antwort genügt)</small></div>
<label>Optionen</label><div class="fld"><label class="chk"><input type="checkbox" id="pgAck"> aktiv bis bestätigt</label><label class="chk"><input type="checkbox" id="pgEn" checked> aktiv</label><small>Wiederholung nach 5, 15 und 30 min, bis im Alarmkanal jemand mit ACK antwortet.</small></div>
</div>
<div class="row"><button type="button" id="pgSave">Ping-Ziel speichern</button><button type="button" class="sec" id="pgCancel" hidden>Abbrechen</button><span class="msg" id="pgMsg" style="margin:0"></span></div>
</section>

<div class="grid">
<section>
<h2>Einstellungen</h2>
<label class="chk"><input type="checkbox" id="ssEn"> Abfrage und Meldungen aktiv</label>
<label for="ssCh">Alarmkanal (für Ping-Check, Offline-Meldungen und Regeln ohne eigenen Kanal)</label>
<select id="ssCh"></select>
<label for="ssMax">Höchstens Meldungen pro Stunde</label>
<input id="ssMax" type="number" min="0" max="60">
<label for="ssOff">Offline nach … Fehlversuchen in Folge (0 = nie)</label>
<input id="ssOff" type="number" min="0" max="100">
<label for="ssAlert">Text Alarm</label><input id="ssAlert" maxlength="99" spellcheck="false">
<label for="ssRec">Text Entwarnung</label><input id="ssRec" maxlength="99" spellcheck="false">
<label for="ssOffT">Text Offline</label><input id="ssOffT" maxlength="99" spellcheck="false">
<label for="ssOnT">Text wieder Online</label><input id="ssOnT" maxlength="99" spellcheck="false">
<label for="ssPgD">Text Ping-Alarm</label><input id="ssPgD" maxlength="99" spellcheck="false">
<label for="ssPgU">Text Ping wieder OK</label><input id="ssPgU" maxlength="99" spellcheck="false">
<label for="ssZbx">Zabbix-Kennung</label>
<select id="ssZbx"><option value="0">aus</option><option value="1">im Alarmtext anhängen</option><option value="2">zusätzlich in eigenem Zabbix-Kanal</option></select>
<div id="ssZbxChBox" hidden><label for="ssZbxCh">Zabbix-Kanal</label><select id="ssZbxCh"></select></div>
<div id="ssHbBox" hidden><label for="ssHb">Lebenszeichen alle … Stunden (0 = aus)</label><input id="ssHb" type="number" min="0" max="24"></div>
<p class="hint" id="ssZbxHint" hidden>Hängt an jede Meldung eine kurze Kennung wie <code>#Z1;R3fa2;A;31;3</code> (Quelle, Ereignis, Wert, ACK-Nr) für einen Zabbix-Empfänger an. Im eigenen Kanal bleibt der Alarmtext für Menschen sauber, dafür kostet jede Meldung doppelt Airtime (zählt beides zum Limit). Ein ACK im Zabbix-Kanal gilt wie eines im Alarmkanal.</p>
<div class="row"><button type="button" id="ssSave">Einstellungen speichern</button><span class="msg" id="ssMsg" style="margin:0"></span></div>
<p class="hint" id="ssLimit"></p>
</section>
<section>
<h2>Meldungen</h2>
<div class="term" id="smLog" style="height:420px"></div>
<p class="hint">Jede Meldung ist eine Kanalnachricht (Flood) und kostet Airtime. Der Knotenname wird automatisch vorangestellt.</p>
</section>
</div>

<section style="margin-top:16px">
<h2>Zabbix-Weiterleitung (Empfänger)</h2>
<p class="hint" style="margin-top:0">Für ein Gerät beim Zabbix-Proxy: empfangene Alarme anderer Gateways mit Zabbix-Kennung gehen per Trapper (Zabbix-Sender-Protokoll) an Zabbix, Host = Präfix + Gateway-Name (wie dessen WLAN-Hostname), Key <code>meshcore.event</code>. Erst wenn Zabbix den Wert angenommen hat, antwortet dieses Gerät im Kanal mit <code>@[Gateway] ACK n</code>.</p>
<div class="form">
<label>Aktiv</label><div class="fld"><label class="chk"><input type="checkbox" id="zbEn"> Zabbix-Weiterleitung einschalten</label></div>
<label for="zbSrv">Zabbix-Proxy/-Server</label><div class="fld"><div class="radio4" style="grid-template-columns:3fr 1fr"><input id="zbSrv" maxlength="63" placeholder="10.10.0.20" spellcheck="false"><input id="zbPort" type="number" min="1" max="65535" value="10051"></div><small>Bei Hosts, die ein Proxy überwacht, MUSS es dieser Proxy sein (Server lehnt sonst ab)</small></div>
<label for="zbPre">Host-Präfix</label><div class="fld"><input id="zbPre" maxlength="23" spellcheck="false" placeholder="leer"><small>z. B. <code>mesh-</code> → Host <code>mesh-de-by-ort01-gw</code></small></div>
<label for="zbCh">Kanäle</label><div class="fld"><input id="zbCh" maxlength="95" spellcheck="false" placeholder="leer = alle"><small>Kanalnamen mit Komma, z. B. <code>haus-alarm, zbx</code></small></div>
<label for="zbRx">Eigener Zabbix-Host</label><div class="fld"><input id="zbRx" maxlength="63" spellcheck="false"><small>für das eigene Lebenszeichen (<code>meshcore.rx.heartbeat</code>), leer = eigener Hostname</small></div>
<label for="zbHb">Lebenszeichen alle (min)</label><div class="fld"><input id="zbHb" type="number" min="0" max="120" value="5"></div>
<label>Optionen</label><div class="fld"><label class="chk"><input type="checkbox" id="zbAck" checked> ACK senden, sobald Zabbix angenommen hat</label><label class="chk"><input type="checkbox" id="zbUnm"> auch Nachrichten ohne Kennung weiterleiten (<code>meshcore.rx.unparsed</code> am eigenen Host)</label></div>
</div>
<div class="row"><button type="button" id="zbSave">Speichern</button><button type="button" class="sec" id="zbTest">Testwert senden</button><span class="msg" id="zbMsg" style="margin:0"></span></div>
<dl id="zbStat" style="margin-top:12px"></dl>
</section>
</div>

<div class="view" id="v_settings">
<div class="banner" id="rebootBanner"><span>Funkparameter oder Schlüssel geändert. Wirksam erst nach einem Neustart.</span><button type="button" id="bannerReboot">Jetzt neu starten</button></div>

<section style="margin-bottom:16px">
<h2>Empfehlung</h2>
<p class="hint" id="profName" style="margin-top:0">…</p>
<div class="tbl"><table><thead><tr><th>Einstellung</th><th>Ist</th><th>Soll</th><th></th><th>Warum</th></tr></thead><tbody id="profBody"></tbody></table></div>
<div class="row">
<button type="button" id="profApply" disabled>Abweichungen übernehmen</button>
<button type="button" class="sec" id="profReload">Neu prüfen</button>
<span class="msg" id="profMsg" style="margin:0"></span>
</div>
</section>

<section style="margin-bottom:16px">
<h2>MeshCore-Einstellungen</h2>
<form id="setForm" autocomplete="off"><div class="form" id="setFields"></div>
<div class="row">
<button type="submit" id="setSave" disabled>Änderungen speichern</button>
<button type="button" class="sec" id="setReload">Neu laden</button>
<span class="msg" id="setMsg" style="margin:0"></span>
</div>
</form>
<pre class="out" id="setOut" hidden></pre>
</section>

<div class="grid">
<section>
<h2>Regionen</h2>
<div class="msg warn" id="regWarn" hidden><b>Keine Region angelegt.</b> Nachrichten mit Region (Scope) leitet dieser Repeater nicht weiter. Welche Regionen in deiner Gegend genutzt werden, mit der lokalen Community abstimmen, dann anlegen und speichern (<a href="https://github.com/peterhagelhof7-cmd/esp-meshcore-gateway/blob/main/docs/admin-guide.md#5-regionen-richtig-einrichten" target="_blank" rel="noopener">Admin-Handbuch</a>).</div>
<pre class="out" id="regOut" style="margin-top:0">…</pre>
<div class="row">
<select id="regAct" style="flex:1;min-width:120px">
<option value="put">anlegen (put)</option>
<option value="remove">entfernen</option>
<option value="allowf">Flood erlauben</option>
<option value="denyf">Flood sperren</option>
<option value="home">als Home setzen</option>
</select>
<input id="regName" placeholder="Name, z. B. de-by" style="flex:1;min-width:120px">
<input id="regParent" placeholder="Eltern (optional)" style="flex:1;min-width:120px">
</div>
<div class="row">
<button type="button" class="sec" id="regRun">Ausführen</button>
<button type="button" id="regSave">Speichern (region save)</button>
<span class="msg" id="regMsg" style="margin:0"></span>
</div>
<p class="hint">Änderungen gelten sofort, bleiben aber nur nach „Speichern“ über einen Neustart erhalten. „F“ = Flood erlaubt, „^“ = Home-Region.</p>
</section>

<section>
<h2>Admin-Passwort</h2>
<label for="admPass">Neues Passwort (max. 15 Zeichen)</label>
<input id="admPass" type="password" maxlength="15" autocomplete="new-password">
<div class="row"><button type="button" id="admSave">Passwort ändern</button><span class="msg" id="admMsg" style="margin:0"></span></div>
<p class="hint">Gilt für Remote-Admin übers Mesh und für diese Weboberfläche (Benutzer „admin“). Nach der Änderung fragt der Browser neu nach dem Login.</p>
</section>

<section>
<h2>Backup &amp; Wiederherstellen</h2>
<label class="chk"><input type="checkbox" id="bkKey"> Schlüssel und Passwörter einschließen (vollständiges Backup)</label>
<div class="row"><button type="button" class="sec" id="bkDown">Backup herunterladen</button></div>
<label for="bkFile">Backup-Datei einspielen</label>
<input type="file" id="bkFile" accept=".json,application/json">
<div id="bkOpts" hidden>
<label class="chk" id="bkAdmL"><input type="checkbox" id="bkAdm" checked> Admin-Passwort übernehmen (danach neu anmelden)</label>
<label class="chk" id="bkWifiL"><input type="checkbox" id="bkWifi"> WLAN und Hostname übernehmen (Neustart)</label>
</div>
<div class="row"><button type="button" id="bkRestore" disabled>Wiederherstellen</button><span class="msg" id="bkMsg" style="margin:0"></span></div>
<pre class="out" id="bkOut" hidden></pre>
<p class="hint">Immer enthalten: alle MeshCore-Einstellungen und Regionen als offizielle CLI-Befehle, Zugriffsliste (ACL, <code>setperm</code>), Kanäle, Überwachung (Sensormeter, Regeln, Ping-Ziele), Hostname und WLAN-Name. <b>Nur mit Haken</b> zusätzlich: privater Schlüssel (Knoten-Identität), Admin- und Gast-Passwort, WLAN-Passwort, Sensormeter-Passwörter. Eine solche Datei ist geheim zu halten. Nicht enthalten: Nachbarn, Statistiken, Logs.</p>
</section>
</div>
</div>

<div class="view" id="v_wifi">
<section>
<h2>WLAN einrichten</h2>
<form id="wifiForm">
<label for="ssidSel">Gefundene Netze</label>
<select id="ssidSel"><option value="">– erst suchen –</option></select>
<label for="ssid">SSID</label>
<input id="ssid" name="ssid" maxlength="32" autocomplete="off" required>
<label for="pass">Passwort (leer lassen = gespeichertes behalten)</label>
<input id="pass" name="pass" type="password" maxlength="63" autocomplete="new-password">
<label for="hostname">Hostname</label>
<label class="chk"><input type="checkbox" id="hostAuto" checked> wie Gerätename (LoRa)</label>
<input id="hostname" name="hostname" maxlength="31" pattern="[A-Za-z0-9 _\-]*" title="Buchstaben, Ziffern, -" disabled>
<label>IP-Adresse</label>
<label class="chk"><input type="checkbox" id="ipStatic"> feste IP-Adresse (sonst automatisch per DHCP)</label>
<div id="ipBox" hidden>
<label for="ipAddr">IP-Adresse</label><input id="ipAddr" inputmode="decimal" spellcheck="false" placeholder="10.10.0.50">
<label for="ipMask">Subnetzmaske</label><input id="ipMask" inputmode="decimal" spellcheck="false" placeholder="255.255.255.0">
<label for="ipGw">Gateway (Router)</label><input id="ipGw" inputmode="decimal" spellcheck="false" placeholder="10.10.0.1">
<label for="ipDns1">DNS 1 (leer = Router)</label><input id="ipDns1" inputmode="decimal" spellcheck="false">
<label for="ipDns2">DNS 2 (optional)</label><input id="ipDns2" inputmode="decimal" spellcheck="false">
<div class="row"><button type="button" class="sec" id="ipCheckBtn">Adresse prüfen</button></div>
<p class="hint">Vor dem Speichern wird geprüft, ob die Adresse schon belegt ist (ARP und Ping im aktuellen WLAN). Nach dem Neustart meldet sich das Gateway erst per DHCP an, prüft erneut, schaltet dann um und testet den Router. Ist die Adresse belegt oder der Router nicht erreichbar, bleibt es bei DHCP.</p>
</div>
<p class="hint" id="ipNote" hidden></p>
<div class="row">
<button type="button" class="sec" id="scanBtn">Netze suchen</button>
<button type="submit">Speichern &amp; neu starten</button>
<button type="button" class="sec" id="rebootBtn">Nur neu starten</button>
</div>
<div class="msg" id="wifiMsg"></div>
</form>
</section>

<section style="margin-top:16px">
<h2>Firmware-Update</h2>
<dl>
<dt>Version</dt><dd id="fw_ver">–</dd>
<dt>Board</dt><dd id="fw_proj" class="mono">–</dd>
<dt>Partition</dt><dd id="fw_part">–</dd>
</dl>
<form id="otaForm">
<label class="chk" style="margin-top:12px"><input type="checkbox" id="otaForce"> Downgrade erzwingen (ältere Version zulassen)</label>
<label for="otaFile">Firmware-Datei (firmware.bin dieses Boards)</label>
<input type="file" id="otaFile" accept=".bin,application/octet-stream">
<div class="row">
<button type="submit" id="otaBtn" disabled>Hochladen &amp; installieren</button>
<progress id="otaProg" max="100" value="0" hidden style="flex:1;min-width:120px"></progress>
<span class="msg" id="otaMsg" style="margin:0"></span>
</div>
</form>
<p class="hint">Nur Dateien mit passendem Gateway-Merkmal werden angenommen (gleiches Board, keine ältere Version). Die neue Firmware muss 60 s stabil laufen, sonst startet automatisch wieder die vorherige.</p>
</section>

<section style="margin-top:16px">
<h2>Ereignis-Log</h2>
<dl>
<dt>Belegt</dt><dd id="lg_size">–</dd>
<dt>Noch im RAM</dt><dd id="lg_pend">–</dd>
<dt>Zuletzt geschrieben</dt><dd id="lg_last">–</dd>
</dl>
<div class="row"><a class="btn sec" id="lgDown" href="/api/log/download">Herunterladen</a><a class="btn sec" id="lgView" href="/api/log/download?inline=1" target="_blank" rel="noopener">Anzeigen</a><button type="button" class="sec" id="lgClear">Löschen</button><span class="msg" id="lgMsg" style="margin:0"></span></div>
<p class="hint">Dauerhaft im Flash, übersteht Neustarts: Starts mit Grund (auch Abstürze), WLAN-Ereignisse, Befehle aus Web und serieller Konsole (Geheimnisse ausgeblendet), Meldungen der Überwachung, gesendete Kanalnachrichten, Updates. Paketverkehr und empfangene Kanalnachrichten stehen nur in den Live-Logs. Flash-schonend: höchstens 128 KB (2 × 64 KB im Wechsel, ältestes fällt weg), geschrieben wird gesammelt ab 4 KB, spätestens alle 15 min und vor jedem Neustart. Bei Absturz oder Stromausfall fehlen höchstens die letzten 15 min.</p>
</section>
</div>

<script>
const $=id=>document.getElementById(id);
const set=(id,v)=>{$(id).textContent=(v===undefined||v===null||v==='')?'–':v};
function dur(s){const d=Math.floor(s/86400),h=Math.floor(s%86400/3600),m=Math.floor(s%3600/60);return(d?d+' d ':'')+h+' h '+m+' min'}
function store(k,v){try{if(v===undefined)return localStorage.getItem(k);localStorage.setItem(k,v)}catch(e){return null}}

/* ---------- Tabs ---------- */
let view='status';
function show(v){
  if(!$('v_'+v))v='status';
  view=v;
  document.querySelectorAll('.view').forEach(e=>e.classList.toggle('on',e.id==='v_'+v));
  document.querySelectorAll('nav a').forEach(a=>a.classList.toggle('on',a.dataset.v===v));
  if(v==='console')setTimeout(()=>$('cliIn').focus(),0);
  if(v==='settings')openSettings();
  if(v==='status'){lastNb=Date.now();loadNeighbors()}
  if(v==='channels')loadChannels();
  if(v==='sm'){smLoad();zbLoad()}
  if(v==='wifi')lgLoad();
  tick();
}
window.onhashchange=()=>show(location.hash.slice(1));

/* ---------- Status ---------- */
let hostTouched=false,ipTouched=false,lastWifi=null;
async function refresh(){
  try{
    const r=await fetch('/api/status',{cache:'no-store'});
    if(!r.ok)throw new Error(r.status);
    const j=await r.json();
    const n=j.node,ra=j.radio,c=j.stats_core||{},rs=j.stats_radio||{},p=j.stats_packets||{},w=j.wifi;
    document.title=n.name+' – MeshCore Gateway';
    set('title',n.name);
    if(nodeName!==n.name){nodeName=n.name;updMsgHint()}
    set('n_name',n.name);set('n_role',n.role);set('n_fw',n.firmware+' ('+n.build_date+')');
    set('g_ver',j.gateway.version+' · '+j.gateway.cpu_mhz+' MHz');
    set('n_fwd',n.forwarding?'an':'aus');
    set('n_pos',(n.lat||n.lon)?n.lat.toFixed(5)+', '+n.lon.toFixed(5):'nicht gesetzt');
    set('n_pub',n.pubkey);
    set('c_up',c.uptime_secs!==undefined?dur(c.uptime_secs):null);
    set('c_queue',c.queue_len);
    set('r_freq',ra.freq.toFixed(3)+' MHz');set('r_bw',ra.bw+' kHz');
    set('r_sfcr','SF'+ra.sf+' / 4:'+ra.cr);set('r_tx',ra.tx_power+' dBm (Chip)');
    set('r_nf',rs.noise_floor!==undefined?rs.noise_floor+' dBm':null);
    set('r_last',rs.last_rssi!==undefined?rs.last_rssi+' dBm / '+rs.last_snr+' dB':null);
    set('r_air',rs.tx_air_secs!==undefined?'TX '+rs.tx_air_secs+' s · RX '+rs.rx_air_secs+' s':null);
    set('p_recv',p.recv);set('p_sent',p.sent);
    set('p_flood',p.flood_rx!==undefined?p.flood_rx+' / '+p.flood_tx:null);
    set('p_direct',p.direct_rx!==undefined?p.direct_rx+' / '+p.direct_tx:null);
    set('p_err',p.recv_errors);
    const modes={station:'Verbunden',installer_ap:'Installer-AP',connecting:'Verbinde …'};
    set('w_mode',modes[w.mode]||w.mode);set('w_ssid',w.ssid);set('w_ip',w.ip);
    set('w_rssi',w.rssi?w.rssi+' dBm':null);set('w_host',w.hostname+(w.hostname_auto?' (wie Gerätename)':''));set('w_mac',w.mac);
    const gw=j.gateway,kb=b=>Math.round(b/1024);
    set('g_heap',kb(gw.free_heap)+' KB'+(gw.min_free_heap?' (Tiefststand '+kb(gw.min_free_heap)+' KB)':''));
    set('g_psram',gw.psram_total?kb(gw.psram_total-gw.psram_free)+' von '+kb(gw.psram_total)+' KB belegt ('+Math.round(100*(gw.psram_total-gw.psram_free)/gw.psram_total)+' %, frei min. '+kb(gw.psram_min_free)+' KB)':null);
    set('g_chip',typeof j.gateway.chip_temp_c==='number'?j.gateway.chip_temp_c.toFixed(1)+' °C':null);
    set('fw_ver',j.gateway.version+' (MeshCore '+n.firmware+')');set('fw_proj',j.gateway.project);
    set('fw_part',j.gateway.partition+' · '+j.gateway.image_state);
    $('wifiPill').textContent=(modes[w.mode]||w.mode)+' · '+w.ip;
    if(!hostTouched){$('hostAuto').checked=w.hostname_auto!==false;$('hostname').value=w.hostname;$('hostname').disabled=$('hostAuto').checked}
    lastWifi=w;
    set('w_ipmode',w.static?'fest':(w.cfg&&w.cfg.pending?'DHCP (feste IP '+w.cfg.ip+' wird geprüft)':'DHCP'));
    if(!ipTouched&&w.cfg){$('ipStatic').checked=!!w.cfg.ip;$('ipBox').hidden=!w.cfg.ip;
      $('ipAddr').value=w.cfg.ip;$('ipMask').value=w.cfg.mask;$('ipGw').value=w.cfg.gw;$('ipDns1').value=w.cfg.dns1;$('ipDns2').value=w.cfg.dns2}
    $('ipNote').hidden=!w.ip_note;$('ipNote').textContent=w.ip_note||'';
  }catch(e){$('wifiPill').textContent='keine Verbindung'}
}

/* ---------- Log-Ansichten (Konsole + Pakete) ---------- */
function fmtTime(l){
  if(l.e>1700000000){const d=new Date(l.e*1000);return d.toLocaleTimeString('de-DE')}
  return '+'+(l.u/1000).toFixed(1)+'s';
}
function makeFeed(url,out,maxLines,accept){
  const f={next:0,busy:false,paused:false};
  f.add=(cls,text,ts)=>{
    const atBottom=out.scrollHeight-out.scrollTop-out.clientHeight<30;
    const div=document.createElement('div');div.className=cls;
    if(ts){const s=document.createElement('span');s.className='ts';s.textContent=ts;div.appendChild(s)}
    div.appendChild(document.createTextNode(text));
    out.appendChild(div);
    while(out.childNodes.length>maxLines)out.removeChild(out.firstChild);
    if(atBottom)out.scrollTop=out.scrollHeight;
  };
  f.poll=async()=>{
    if(f.busy||f.paused)return;f.busy=true;
    try{
      for(let i=0;i<5;i++){
        const r=await fetch(url+'?since='+f.next,{cache:'no-store'});
        if(!r.ok)break;
        const j=await r.json();
        if(j.gap)f.add('s','… ältere Zeilen wurden inzwischen überschrieben');
        j.lines.forEach(l=>accept(l)&&f.add(l.t,(l.t==='c'?'> ':'')+l.x,fmtTime(l)));
        f.next=j.next;
        if(j.lines.length<60)break;
      }
    }catch(e){}
    f.busy=false;
  };
  return f;
}
const con=makeFeed('/api/console',$('conOut'),1000,()=>true);
let pktRows=[];
function pktOk(l){
  const dir=(l.t==='R')?$('pktRx').checked:$('pktTx').checked;
  const q=$('pktFilter').value.trim().toLowerCase();
  return dir&&(!q||l.x.toLowerCase().includes(q));
}
const pkt=makeFeed('/api/packets',$('pktOut'),500,l=>{pktRows.push(l);if(pktRows.length>500)pktRows.shift();return pktOk(l)});
function redrawPkts(){
  $('pktOut').innerHTML='';
  pktRows.forEach(l=>pktOk(l)&&pkt.add(l.t,({R:'RX ',T:'TX ',F:'TX FAIL '}[l.t]||'')+l.x,fmtTime(l)));
}
['pktFilter','pktRx','pktTx'].forEach(id=>$(id).oninput=$(id).onchange=redrawPkts);
$('pktPause').onclick=()=>{pkt.paused=!pkt.paused;$('pktPause').textContent=pkt.paused?'Weiter':'Pause'};
$('pktClear').onclick=()=>{pktRows=[];$('pktOut').innerHTML=''};
// RX/TX-Präfix auch bei Live-Zeilen
const pktAdd=pkt.add;pkt.add=(cls,text,ts)=>pktAdd(cls,(text.startsWith('RX ')||text.startsWith('TX '))?text:(({R:'RX ',T:'TX ',F:'TX FAIL '}[cls]||'')+text),ts);

/* ---------- Konsole ---------- */
const CMDS=['ver','board','clock','clock sync','time ','advert','advert.zerohop','neighbors','neighbor.remove ','discover.neighbors',
'stats-core','stats-radio','stats-packets','clear stats','log start','log stop','log erase','log','reboot','clkreboot',
'tempradio ','password ','setperm ','get acl','powersaving','powersaving on','powersaving off',
'get name','get radio','get freq','get tx','get repeat','get lat','get lon','get advert.interval','get flood.advert.interval',
'get flood.max','get flood.max.unscoped','get flood.max.advert','get af','get dutycycle','get rxdelay','get txdelay','get direct.txdelay',
'get int.thresh','get agc.reset.interval','get multi.acks','get allow.read.only','get guest.password','get owner.info','get path.hash.mode',
'get loop.detect','get cad','get radio.rxgain','get radio.fem.rxgain','get radio.fem.txgain','get adc.multiplier','get prv.key',
'get bridge.enabled','get bridge.delay','get bridge.source','get bridge.baud','get bridge.channel','get bridge.secret',
'set name ','set radio ','set freq ','set tx ','set repeat ','set lat ','set lon ','set advert.interval ','set flood.advert.interval ',
'set flood.max ','set flood.max.unscoped ','set flood.max.advert ','set af ','set dutycycle ','set rxdelay ','set txdelay ','set direct.txdelay ',
'set int.thresh ','set agc.reset.interval ','set multi.acks ','set allow.read.only ','set guest.password ','set owner.info ','set path.hash.mode ',
'set loop.detect ','set cad ','set radio.rxgain ','set radio.fem.rxgain ','set radio.fem.txgain ','set adc.multiplier ','set prv.key ',
'region','region list ','region get ','region put ','region remove ','region allowf ','region denyf ','region home','region home ',
'region default','region default ','region def ','region load','region save',
'gps','gps on','gps off','gps sync','gps setloc','gps advert','gps advert ','sensor get ','sensor set ','erase','poweroff','shutdown'];
$('cmdList').innerHTML=CMDS.map(c=>'<option value="'+c.trim()+'">').join('');
const QUICK=['ver','board','clock','neighbors','stats-core','stats-radio','stats-packets','get radio','get tx','advert'];
QUICK.forEach(c=>{const b=document.createElement('button');b.type='button';b.className='chip';b.textContent=c;b.onclick=()=>sendCmd(c);$('quick').appendChild(b)});
const DANGER={erase:'Löscht ALLE MeshCore-Daten (Identität, Einstellungen) auf dem Knoten!',
  poweroff:'Das Gateway geht in den Tiefschlaf und ist danach nur per Reset/Stromtrennung erreichbar.',
  shutdown:'Das Gateway geht in den Tiefschlaf und ist danach nur per Reset/Stromtrennung erreichbar.',
  reboot:'Das Gateway startet neu.',clkreboot:'Uhr zurücksetzen und neu starten.',
  'set prv.key':'Ersetzt den privaten Schlüssel = neue Knoten-Identität.'};
let hist=[];try{hist=JSON.parse(store('cliHist')||'[]')}catch(e){}
let hpos=hist.length;
async function sendCmd(cmd){
  const key=Object.keys(DANGER).find(k=>cmd===k||cmd.startsWith(k+' '));
  if(key&&!confirm(DANGER[key]+'\n\nBefehl wirklich ausführen?\n'+cmd))return;
  if(cmd&&hist[hist.length-1]!==cmd){hist.push(cmd);if(hist.length>50)hist.shift();store('cliHist',JSON.stringify(hist))}
  hpos=hist.length;
  try{
    const r=await fetch('/api/cli',{method:'POST',body:new URLSearchParams({cmd})});
    if(!r.ok){const j=await r.json().catch(()=>({}));con.add('F',j.error||('Fehler '+r.status))}
  }catch(e){con.add('F','Gateway nicht erreichbar')}
  // Antwort kommt über den Konsolen-Log, mehrmals kurz nachfragen
  for(const d of [150,400,900])setTimeout(con.poll,d);
}
$('cliForm').onsubmit=e=>{e.preventDefault();const c=$('cliIn').value;$('cliIn').value='';sendCmd(c.trim()===''?'':c.replace(/^\s+/,''))};
$('cliIn').onkeydown=e=>{
  if(e.key==='ArrowUp'&&hpos>0){hpos--;$('cliIn').value=hist[hpos];e.preventDefault()}
  else if(e.key==='ArrowDown'){if(hpos<hist.length-1){hpos++;$('cliIn').value=hist[hpos]}else{hpos=hist.length;$('cliIn').value=''}e.preventDefault()}
};

/* ---------- WLAN ---------- */
async function scan(){
  const b=$('scanBtn');b.disabled=true;$('wifiMsg').textContent='Suche läuft …';$('wifiMsg').className='msg';
  try{
    for(let i=0;i<20;i++){
      const j=await (await fetch('/api/wifi/scan',{cache:'no-store'})).json();
      if(!j.scanning){
        const seen=new Set(),sel=$('ssidSel');sel.innerHTML='<option value="">– auswählen –</option>';
        j.networks.sort((a,b)=>b.rssi-a.rssi).forEach(n=>{if(!n.ssid||seen.has(n.ssid))return;seen.add(n.ssid);
          const o=document.createElement('option');o.value=n.ssid;o.textContent=n.ssid+' ('+n.rssi+' dBm'+(n.secure?'':', offen')+')';sel.appendChild(o)});
        $('wifiMsg').textContent=seen.size+' Netze gefunden';break;
      }
      await new Promise(r=>setTimeout(r,1000));
    }
  }catch(e){$('wifiMsg').textContent='Suche fehlgeschlagen';$('wifiMsg').className='msg bad'}
  b.disabled=false;
}
$('ssidSel').onchange=e=>{if(e.target.value)$('ssid').value=e.target.value};
$('hostname').oninput=()=>hostTouched=true;
$('hostAuto').onchange=()=>{hostTouched=true;$('hostname').disabled=$('hostAuto').checked;if(!$('hostAuto').checked)$('hostname').focus()};
$('scanBtn').onclick=scan;
/* Feste IP (E40) */
// ipTouched, lastWifi: siehe hostTouched (Status-Refresh)
const ipRe=/^(25[0-5]|2[0-4]\d|1?\d?\d)(\.(25[0-5]|2[0-4]\d|1?\d?\d)){3}$/;
['ipAddr','ipMask','ipGw','ipDns1','ipDns2'].forEach(id=>$(id).addEventListener('input',()=>ipTouched=true));
$('ipStatic').onchange=()=>{
  ipTouched=true;const on=$('ipStatic').checked;$('ipBox').hidden=!on;
  if(on&&!$('ipAddr').value&&lastWifi&&lastWifi.mode==='station'){   // aktuelle DHCP-Werte als Vorschlag
    $('ipAddr').value=lastWifi.ip;$('ipMask').value=lastWifi.mask||'255.255.255.0';$('ipGw').value=lastWifi.gw||'';
    if(lastWifi.dns&&lastWifi.dns!==lastWifi.gw)$('ipDns1').value=lastWifi.dns;
  }
  if(on)$('ipAddr').focus();
};
function ipFields(){
  const f={ip:$('ipAddr').value.trim(),mask:$('ipMask').value.trim(),gw:$('ipGw').value.trim(),dns1:$('ipDns1').value.trim(),dns2:$('ipDns2').value.trim()};
  if(!ipRe.test(f.ip))return {err:'IP-Adresse ungültig'};
  if(!ipRe.test(f.mask))return {err:'Subnetzmaske ungültig'};
  if(!ipRe.test(f.gw))return {err:'Gateway (Router) ungültig'};
  if(f.dns1&&!ipRe.test(f.dns1))return {err:'DNS 1 ungültig'};
  if(f.dns2&&!ipRe.test(f.dns2))return {err:'DNS 2 ungültig'};
  return f;
}
// Belegung prüfen: {state,detail}; state present/absent/own/uncheckable
async function ipProbe(ip){
  const r=await fetch('/api/wifi/checkip',{method:'POST',body:new URLSearchParams({ip})});
  if(!r.ok){const j=await r.json().catch(()=>({}));throw new Error(j.error||('Fehler '+r.status))}
  for(let i=0;i<30;i++){
    await new Promise(res=>setTimeout(res,500));
    const j=await (await fetch('/api/wifi/checkip',{cache:'no-store'})).json();
    if(j.state!=='running')return j;
  }
  throw new Error('Prüfung dauert zu lange');
}
const ipStateText={present:'BELEGT',absent:'frei',own:'ist die aktuelle Adresse des Gateways',uncheckable:'nicht prüfbar'};
$('ipCheckBtn').onclick=async()=>{
  const f=ipFields();
  if(f.err){$('wifiMsg').textContent=f.err;$('wifiMsg').className='msg bad';return}
  $('ipCheckBtn').disabled=true;$('wifiMsg').textContent='Prüfe '+f.ip+' …';$('wifiMsg').className='msg';
  try{const j=await ipProbe(f.ip);
    $('wifiMsg').textContent=f.ip+': '+ipStateText[j.state]+(j.detail?' – '+j.detail:'');
    $('wifiMsg').className='msg '+(j.state==='present'?'bad':(j.state==='uncheckable'?'warn':'ok'))}
  catch(e){$('wifiMsg').textContent=e.message;$('wifiMsg').className='msg bad'}
  $('ipCheckBtn').disabled=false;
};
$('wifiForm').onsubmit=async e=>{
  e.preventDefault();
  const body=new URLSearchParams(new FormData(e.target));
  body.set('hostname',$('hostAuto').checked?'':$('hostname').value.trim());   // leer = automatisch
  let f={ip:'',mask:'',gw:'',dns1:'',dns2:''};
  if($('ipStatic').checked){
    f=ipFields();
    if(f.err){$('wifiMsg').textContent=f.err;$('wifiMsg').className='msg bad';return}
    const known=lastWifi&&lastWifi.cfg&&lastWifi.cfg.ip===f.ip&&lastWifi.static;
    if(!known){   // neue Adresse: vorher auf Belegung prüfen
      $('wifiMsg').textContent='Prüfe, ob '+f.ip+' frei ist …';$('wifiMsg').className='msg';
      let j;
      try{j=await ipProbe(f.ip)}catch(err){j={state:'uncheckable',detail:err.message}}
      if(j.state==='present'){$('wifiMsg').textContent=f.ip+' ist belegt ('+j.detail+'). Bitte eine freie Adresse wählen.';$('wifiMsg').className='msg bad';return}
      if(j.state==='uncheckable'&&!confirm(f.ip+' kann von hier nicht geprüft werden ('+j.detail+').\nDas Gateway prüft nach dem Neustart selbst und bleibt bei Belegung auf DHCP.\nTrotzdem speichern?'))return;
    }
  }
  Object.entries(f).forEach(([k,v])=>body.set(k,v));
  const r=await fetch('/api/wifi',{method:'POST',body});
  const j=await r.json().catch(()=>({}));
  if(r.ok){
    ipTouched=false;
    $('wifiMsg').textContent=f.ip
      ?'Gespeichert. Das Gateway startet neu, meldet sich erst per DHCP an, prüft '+f.ip+' und schaltet dann um. Danach erreichbar unter http://'+f.ip+'/ – ist die Adresse belegt oder der Router nicht erreichbar, bleibt es bei DHCP (Grund im Ereignis-Log).'
      :'Gespeichert. Das Gateway startet neu und verbindet sich mit dem WLAN. Klappt das nicht innerhalb von 30 s, kommt der Installer-AP zurück.';
    $('wifiMsg').className='msg ok'}
  else{$('wifiMsg').textContent=j.error||('Fehler '+r.status);$('wifiMsg').className='msg bad'}
};
$('rebootBtn').onclick=async()=>{if(!confirm('Gateway neu starten?'))return;await fetch('/api/reboot',{method:'POST'});$('wifiMsg').textContent='Neustart …';$('wifiMsg').className='msg warn'};

/* ---------- Firmware-Update ---------- */
$('otaFile').onchange=()=>{$('otaBtn').disabled=!$('otaFile').files.length;$('otaMsg').textContent=''};
$('otaForm').onsubmit=e=>{
  e.preventDefault();
  const f=$('otaFile').files[0];if(!f)return;
  if(!confirm('Firmware '+f.name+' ('+Math.round(f.size/1024)+' KB) installieren? Das Gateway startet danach neu.'))return;
  const fd=new FormData();
  if($('otaForce').checked)fd.append('force','1');   // VOR der Datei (Multipart-Reihenfolge)
  fd.append('firmware',f,f.name);
  const x=new XMLHttpRequest();
  $('otaBtn').disabled=true;$('otaProg').hidden=false;$('otaProg').value=0;
  $('otaMsg').textContent='Lade hoch …';$('otaMsg').className='msg';
  x.upload.onprogress=ev=>{if(ev.lengthComputable)$('otaProg').value=Math.round(ev.loaded*100/ev.total)};
  x.onload=()=>{
    let j={};try{j=JSON.parse(x.responseText)}catch(err){}
    if(x.status===200){$('otaProg').value=100;$('otaMsg').textContent='Installiert ('+j.version+'). Neustart … Seite lädt in 25 s neu.';$('otaMsg').className='msg ok';
      setTimeout(()=>location.reload(),25000)}
    else{$('otaMsg').textContent='Abgelehnt: '+(j.error||('Fehler '+x.status));$('otaMsg').className='msg bad';$('otaBtn').disabled=false;$('otaProg').hidden=true}
  };
  x.onerror=()=>{$('otaMsg').textContent='Verbindung abgebrochen';$('otaMsg').className='msg bad';$('otaBtn').disabled=false};
  x.open('POST','/api/ota');x.send(fd);
};

/* ---------- Kanäle ---------- */
let channels=[],nodeName='';
const KIND={public:'Public',hashtag:'Hashtag',private:'Privat'};
async function postForm(url,obj){
  const r=await fetch(url,{method:'POST',body:new URLSearchParams(obj)});
  const j=await r.json().catch(()=>({}));
  if(!r.ok)throw new Error(j.error||('Fehler '+r.status));
  return j;
}
async function loadChannels(){
  try{
    const j=await (await fetch('/api/channels',{cache:'no-store'})).json();
    channels=j.channels;
    const tb=$('chBody');tb.innerHTML='';
    channels.forEach(c=>{
      const tr=document.createElement('tr');
      const td=(t,cls)=>{const e=document.createElement('td');if(cls)e.className=cls;if(t!==undefined)e.textContent=t;tr.appendChild(e);return e};
      td(c.name);td(KIND[c.kind]);
      const sc=td();const si=document.createElement('input');si.value=c.scope;si.placeholder='–';si.style.minWidth='90px';
      si.onchange=async()=>{try{await postForm('/api/channels',{action:'scope',idx:c.idx,scope:si.value.trim()});$('chMsg').textContent='Scope gespeichert';$('chMsg').className='msg ok'}
        catch(e){$('chMsg').textContent=e.message;$('chMsg').className='msg bad';si.value=c.scope}};
      sc.appendChild(si);
      td(c.hash,'mono');
      const k=td('','mono');
      if(c.kind==='private'){const b=document.createElement('button');b.type='button';b.className='chip';b.textContent='anzeigen';
        // Hex wie in der MeshCore-App; Base64 (dieselben 16 Byte) nur klein darunter.
        b.onclick=()=>{k.textContent=c.key_hex;const s=document.createElement('small');s.style.display='block';
          s.textContent='Base64: '+c.key_b64;k.appendChild(s);k.title='Hex wie in der MeshCore-App'};k.appendChild(b)}
      else k.textContent=c.kind==='public'?'öffentlich':'aus Name';
      const d=td();const db=document.createElement('button');db.type='button';db.className='chip';db.textContent='löschen';
      db.onclick=async()=>{if(!confirm('Kanal '+c.name+' löschen? Regeln, die ihn nutzen, senden dann nicht mehr.'))return;
        try{await postForm('/api/channels',{action:'delete',idx:c.idx});loadChannels()}catch(e){$('chMsg').textContent=e.message;$('chMsg').className='msg bad'}};
      d.appendChild(db);tb.appendChild(tr);
    });
    $('chEmpty').hidden=channels.length>0;
    const sel=$('msgCh'),old=sel.value;sel.innerHTML='';
    channels.forEach(c=>{const o=document.createElement('option');o.value=c.idx;o.textContent=c.name;sel.appendChild(o)});
    if(channels.some(c=>String(c.idx)===old))sel.value=old;
    $('msgForm').querySelectorAll('input,button,select').forEach(e=>e.disabled=channels.length===0);
    updMsgHint();
    if(typeof fillRuleChannels==='function')fillRuleChannels();
  }catch(e){$('chMsg').textContent='Laden fehlgeschlagen';$('chMsg').className='msg bad'}
}
function updKind(){
  const k=$('chKind').value;
  $('chKeyBox').style.display=$('chKeyLab').style.display=k==='private'?'':'none';
  $('chName').placeholder=k==='hashtag'?'#test':(k==='public'?'Public':'Name');
  $('chNameHint').style.display=k==='hashtag'?'':'none';
}
$('chKind').onchange=updKind;updKind();
$('chAdd').onclick=async()=>{
  try{
    await postForm('/api/channels',{action:'add',kind:$('chKind').value,name:$('chName').value.trim(),key:$('chKey').value.trim(),scope:$('chScope').value.trim()});
    $('chName').value=$('chKey').value='';$('chScope').value='rhein-main';
    $('chMsg').textContent='Kanal angelegt';$('chMsg').className='msg ok';loadChannels();
  }catch(e){$('chMsg').textContent=e.message;$('chMsg').className='msg bad'}
};
function updMsgHint(){
  const max=160-(nodeName.length+2);
  $('msgText').maxLength=Math.max(1,max);
  const c=channels.find(x=>String(x.idx)===$('msgCh').value);
  $('msgHint').textContent='Max. '+max+' Zeichen (Absender „'+nodeName+': “ zählt mit). '+
    (c&&c.scope?'Scope: '+c.scope+'. ':'Ohne Scope. ')+'Kanalnachrichten sind Floods und kosten Airtime im ganzen Netz.';
}
$('msgCh').onchange=updMsgHint;
const msgs=makeFeed('/api/messages',$('msgOut'),500,()=>true);
const msgAdd=msgs.add;
msgs.add=(cls,text,ts)=>{
  if(cls!=='M'&&cls!=='S')return msgAdd(cls,text,ts);
  const p=text.split('\t');   // idx, Kanal, SNR, Hops, Text
  const extra=cls==='M'?'  ('+p[3]+' Hops, SNR '+p[2]+' dB)':'  (gesendet)';
  return msgAdd(cls==='M'?'R':'T',p[1]+'  '+p.slice(4).join(' ')+extra,ts);
};
$('msgForm').onsubmit=async e=>{
  e.preventDefault();
  const t=$('msgText').value.trim();if(!t)return;
  try{await postForm('/api/channels/send',{idx:$('msgCh').value,text:t});$('msgText').value='';
    for(const d of [300,900,2000])setTimeout(msgs.poll,d)}
  catch(err){msgs.add('F','Senden fehlgeschlagen: '+err.message)}
};

/* ---------- Sensormeter ---------- */
// Standardtexte wie in src/SmConfig.cpp (nur als Platzhalter angezeigt; leer = Standard)
const SM_DEF={alert:'{state} {device}: {metric} {value}{unit} ({op}{threshold}{unit})',
  recover:'OK {device}: {metric} {value}{unit} wieder normal',offline:'OFFLINE {device} nicht erreichbar',online:'ONLINE {device} wieder erreichbar',
  pingDown:'PING-ALARM {device} ({host}) seit {value} s ohne Antwort',pingUp:'PING-OK {device} ({host}) antwortet wieder'};
let smCfg=null,smSt=null,smEditDev=-1,smEditRule=-1;
// Pseudo-Gerät „Gateway (intern)“ (E30): Chiptemperatur, Regeln mit device=SM_INT.
const SM_INT=100,SM_INT_NAME='Gateway (intern)';
const smDevStatus=i=>(smSt&&(i===SM_INT?smSt.internal:smSt.devices.find(x=>x.idx===i)))||{};
const smLogFeed=makeFeed('/api/sm/log',$('smLog'),300,()=>true);
const smLogAdd=smLogFeed.add;
smLogFeed.add=(cls,text,ts)=>smLogAdd(cls==='A'?'T':(cls==='X'?'F':'s'),text,ts);
// Regeln verweisen per Kanal-NAME (nicht Platznummer) auf Kanäle, E24.
function chName(n){if(!n)return'Standard';return channels.some(x=>x.name===n)?n:n+' (fehlt!)'}
function fillChSelect(sel,withStd,cur){
  sel.innerHTML='';
  if(withStd){const o=document.createElement('option');o.value='';o.textContent=withStd;sel.appendChild(o)}
  const names=channels.map(c=>c.name);
  if(cur&&!names.includes(cur))names.push(cur);   // gelöschter Kanal bleibt sichtbar
  names.forEach(n=>{const o=document.createElement('option');o.value=n;o.textContent=channels.some(c=>c.name===n)?n:n+' (fehlt!)';sel.appendChild(o)});
  sel.value=cur||'';if(sel.selectedIndex<0)sel.value='';
}
function fillRuleChannels(){
  if(!smCfg)return;
  fillChSelect($('srCh'),'Alarmkanal (Standard)',$('srCh').value||'');
  fillChSelect($('ssCh'),'(keiner – Meldungen gehen verloren)',smSetDirty?$('ssCh').value:smCfg.settings.defaultChannel);
  fillChSelect($('ssZbxCh'),'– wählen –',smSetDirty?$('ssZbxCh').value:(smCfg.settings.zbxChannel||''));
}
function ago(s){if(s===null||s===undefined)return'–';if(s<90)return s+' s';if(s<5400)return Math.round(s/60)+' min';return Math.round(s/3600)+' h'}
function fmtVal(v){return (Math.round(v*10)/10).toString()}
async function smSave(cfg,msgEl,okText){
  const r=await fetch('/api/sm/save',{method:'POST',body:new URLSearchParams({json:JSON.stringify(cfg)})});
  const j=await r.json().catch(()=>({}));
  if(!r.ok){msgEl.textContent=j.error||('Fehler '+r.status);msgEl.className='msg bad';return false}
  await smLoad();   // erst neu laden, dann melden (Formular-Reset danach überschreibt sonst neue Eingaben)
  msgEl.textContent=okText;msgEl.className='msg ok';return true;
}
function smCfgForSave(){
  // Passwörter: "***" = unverändert; origIdx sagt dem Gateway, welches alte Passwort gemeint ist.
  const c=JSON.parse(JSON.stringify(smCfg));
  c.devices.forEach((d,i)=>{if(d.origIdx===undefined)d.origIdx=i});
  return c;
}
function smRender(){
  const st=smSt||{devices:[],rules:[],limit:{used:0,max:0}};
  // Geräte
  const tb=$('smDevBody');tb.innerHTML='';
  smCfg.devices.forEach((d,i)=>{
    const s=st.devices.find(x=>x.idx===i)||{};
    const tr=document.createElement('tr');
    const td=(t,cls)=>{const e=document.createElement('td');if(cls)e.className=cls;if(t!==undefined)e.textContent=t;tr.appendChild(e);return e};
    td(d.name+(d.enabled?'':' (aus)'));
    td(d.host+(d.port!==80?':'+d.port:''),'mono');
    const stc=td();
    if(!d.enabled)stc.textContent='deaktiviert';
    else if(!s.polled)stc.textContent='wartet …';
    else if(s.online){stc.textContent='✓ vor '+ago(s.lastOkAgoS);stc.className='ok'}
    else{stc.textContent=(s.offline?'OFFLINE: ':'✗ ')+(s.error||'Fehler');stc.className='bad'}
    td((s.values||[]).map(v=>(v.sensor&&st.devices.length?v.sensor+' ':'')+v.label+' '+fmtVal(v.value)+v.unit).join(' · ')||'–');
    const a=td();
    [['abfragen',async()=>{await postForm('/api/sm/poll',{idx:i});setTimeout(smLoad,2500)}],
     ['bearbeiten',()=>smDevEdit(i)],
     ['löschen',async()=>{const n=smCfg.rules.filter(r=>r.device===i).length;
       if(!confirm('Gerät '+d.name+' löschen?'+(n?' Dazu '+n+' Regel(n).':'')))return;
       const c=smCfgForSave();c.devices.splice(i,1);
       c.rules=c.rules.filter(r=>r.device!==i).map(r=>Object.assign(r,{device:r.device>i&&r.device!==SM_INT?r.device-1:r.device}));
       await smSave(c,$('sdMsg'),'Gerät gelöscht')}]].forEach(([t,f])=>{const b=document.createElement('button');b.type='button';b.className='chip';b.textContent=t;b.onclick=f;a.appendChild(b)});
    tb.appendChild(tr);
  });
  {  // feste Zeile: Gateway selbst (nicht lösch-/editierbar)
    const s=st.internal||{},tr=document.createElement('tr');
    const td=(t,cls)=>{const e=document.createElement('td');if(cls)e.className=cls;if(t!==undefined)e.textContent=t;tr.appendChild(e);return e};
    td(SM_INT_NAME);td('intern','mono');
    const stc=td();
    if(!s.polled)stc.textContent='wartet …';
    else if(s.online){stc.textContent='✓ vor '+ago(s.lastOkAgoS);stc.className='ok'}
    else{stc.textContent='✗ '+(s.error||'Fehler');stc.className='bad'}
    td((s.values||[]).map(v=>v.label+' '+fmtVal(v.value)+v.unit).join(' · ')||'–');
    td().innerHTML='<small>Chip, nicht Raum</small>';
    tb.appendChild(tr);
  }
  $('smDevEmpty').hidden=smCfg.devices.length>0;
  // Regeln
  const rb=$('smRuleBody');rb.innerHTML='';
  smCfg.rules.forEach((r,i)=>{
    const s=st.rules.find(x=>x.idx===i)||{};
    const dev=r.device===SM_INT?{name:SM_INT_NAME}:(smCfg.devices[r.device]||{name:'?'});
    const tr=document.createElement('tr');
    const td=(t,cls)=>{const e=document.createElement('td');if(cls)e.className=cls;if(t!==undefined)e.textContent=t;tr.appendChild(e);return e};
    td(dev.name+' · '+r.path+(r.enabled?'':' (aus)'));
    td((r.op?'< ':'> ')+r.threshold+(r.hysteresis?' (±'+r.hysteresis+')':'')+(r.minDurationS?', '+r.minDurationS+' s':'')+(r.repeatMin?', alle '+r.repeatMin+' min':'')+(r.ackRequired?', bis ACK':''));
    td(chName(r.channel));
    const z=td();
    const txt={alert:'ALARM',pending:'prüft …',normal:'normal'}[s.state]||'–';
    z.textContent=txt+(s.value!==null&&s.value!==undefined?' ('+fmtVal(s.value)+')':'')+(s.ack?' · ACK '+s.ack+' offen':'');
    z.className=s.state==='alert'||s.ack?'bad':(s.state==='pending'?'warn':'');
    const a=td();
    [['Test',async()=>{try{await postForm('/api/sm/test',{idx:i});$('srMsg').textContent='Testnachricht gesendet';$('srMsg').className='msg ok';setTimeout(smLogFeed.poll,600)}
        catch(e){$('srMsg').textContent=e.message;$('srMsg').className='msg bad'}}],
     ['bearbeiten',()=>smRuleEdit(i)],
     ['löschen',async()=>{if(!confirm('Regel löschen?'))return;const c=smCfgForSave();c.rules.splice(i,1);await smSave(c,$('srMsg'),'Regel gelöscht')}]]
      .forEach(([t,f])=>{const b=document.createElement('button');b.type='button';b.className='chip';b.textContent=t;b.onclick=f;a.appendChild(b)});
    rb.appendChild(tr);
  });
  $('smRuleEmpty').hidden=smCfg.rules.length>0;
  // Regel-Formular: Geräteliste + Messwerte
  const ds=$('srDev'),cur=ds.value;ds.innerHTML='';
  smCfg.devices.forEach((d,i)=>{const o=document.createElement('option');o.value=i;o.textContent=d.name;ds.appendChild(o)});
  {const o=document.createElement('option');o.value=SM_INT;o.textContent=SM_INT_NAME;ds.appendChild(o)}
  if(cur!==''&&(smCfg.devices[cur]||parseInt(cur)===SM_INT))ds.value=cur;
  if(!$('srPath').dataset.keep)fillPaths();
  $('ssLimit').textContent='Gesendet in der letzten Stunde: '+st.limit.used+' von '+st.limit.max+'.';
  pgRender(st);
  ackRender(st);
}
function ackRender(st){
  const acks=st.acks||[],tb=$('ackBody');tb.innerHTML='';
  $('ackSec').hidden=!acks.length;
  acks.slice().sort((a,b)=>b.ageS-a.ageS).forEach(a=>{
    const tr=document.createElement('tr');
    const td=(t,cls)=>{const e=document.createElement('td');if(cls)e.className=cls;if(t!==undefined)e.textContent=t;tr.appendChild(e);return e};
    td(String(a.id),'mono');td(a.text+(a.cleared?' (inzw. OK)':''));td(a.channel);td(ago(a.ageS));
    td(a.repeats+' von '+a.max+(a.repeats>=a.max?' (keine weitere)':''));
    const b=document.createElement('button');b.type='button';b.className='chip';b.textContent='bestätigen';
    b.onclick=async()=>{try{await postForm('/api/sm/ack',{id:a.id});$('ackMsg').textContent='ACK '+a.id+' bestätigt';$('ackMsg').className='msg ok';smLoad()}
      catch(e){$('ackMsg').textContent=e.message;$('ackMsg').className='msg bad'}};
    td().appendChild(b);
    tb.appendChild(tr);
  });
}
function fillPaths(want){
  const s=smDevStatus(parseInt($('srDev').value));
  const sel=$('srPath'),cur=want||sel.value;sel.innerHTML='';
  const vals=(s.values||[]).slice();
  if(cur&&!vals.some(v=>v.path===cur))vals.unshift({path:cur,label:cur,unit:'',sensor:''});
  vals.forEach(v=>{const o=document.createElement('option');o.value=v.path;o.textContent=v.path+(v.value!==undefined?'  ('+v.label+' '+fmtVal(v.value)+v.unit+')':'');sel.appendChild(o)});
  if(cur)sel.value=cur;
  $('srPathHint').textContent=vals.length?'Liste aus der letzten Abfrage des Geräts':'Noch keine Messwerte – Gerät erst abfragen lassen';
}
$('srDev').onchange=()=>fillPaths('');
async function smLoad(){
  try{
    const j=await (await fetch('/api/sm',{cache:'no-store'})).json();
    smCfg=j.config;smSt=j.status;
    if(!channels.length)await loadChannels();
    const s=smCfg.settings;
    if(!smSetDirty){   // bearbeitete, ungespeicherte Felder nicht überschreiben
      $('ssEn').checked=s.enabled;$('ssMax').value=s.maxPerHour;$('ssOff').value=s.offlineAfterFails;
      $('ssAlert').value=s.alertText;$('ssRec').value=s.recoverText;$('ssOffT').value=s.offlineText;$('ssOnT').value=s.onlineText;
      $('ssPgD').value=s.pingDownText||'';$('ssPgU').value=s.pingUpText||'';
      $('ssZbx').value=String(s.zbxMode||0);$('ssHb').value=s.heartbeatH||0;zbxModeUi();
    }
    fillRuleChannels();smRender();
  }catch(e){$('sdMsg').textContent='Laden fehlgeschlagen';$('sdMsg').className='msg bad'}
}
$('ssAlert').placeholder=SM_DEF.alert;$('ssRec').placeholder=SM_DEF.recover;$('ssOffT').placeholder=SM_DEF.offline;$('ssOnT').placeholder=SM_DEF.online;$('ssPgD').placeholder=SM_DEF.pingDown;$('ssPgU').placeholder=SM_DEF.pingUp;
$('srText').placeholder='Standard: '+SM_DEF.alert;
let smSetDirty=false;
function zbxModeUi(){const m=$('ssZbx').value;$('ssZbxChBox').hidden=m!=='2';$('ssHbBox').hidden=$('ssZbxHint').hidden=m==='0'}
$('ssZbx').addEventListener('change',zbxModeUi);
['ssEn','ssCh','ssMax','ssOff','ssAlert','ssRec','ssOffT','ssOnT','ssPgD','ssPgU','ssZbx','ssZbxCh','ssHb'].forEach(id=>$(id).addEventListener('input',()=>{smSetDirty=true;$('ssMsg').textContent='ungespeichert';$('ssMsg').className='msg warn'}));
$('ssSave').onclick=async()=>{
  const c=smCfgForSave();
  Object.assign(c.settings,{enabled:$('ssEn').checked,defaultChannel:$('ssCh').value,maxPerHour:parseInt($('ssMax').value)||0,
    offlineAfterFails:parseInt($('ssOff').value)||0,alertText:$('ssAlert').value.trim(),recoverText:$('ssRec').value.trim(),
    offlineText:$('ssOffT').value.trim(),onlineText:$('ssOnT').value.trim(),pingDownText:$('ssPgD').value.trim(),pingUpText:$('ssPgU').value.trim(),
    zbxMode:parseInt($('ssZbx').value)||0,zbxChannel:$('ssZbx').value==='2'?$('ssZbxCh').value:'',heartbeatH:parseInt($('ssHb').value)||0});
  smSetDirty=false;
  if(!await smSave(c,$('ssMsg'),'Gespeichert'))smSetDirty=true;
};
function smDevEdit(i){
  smEditDev=i;const d=smCfg.devices[i];
  $('sdName').value=d.name;$('sdHost').value=d.host;$('sdPort').value=d.port;$('sdInt').value=d.intervalS;
  $('sdUser').value=d.user;$('sdPass').value=d.pass;$('sdOff').checked=d.offlineAlert;$('sdEn').checked=d.enabled;
  $('smDevTitle').textContent='Gerät bearbeiten: '+d.name;$('sdCancel').hidden=false;$('sdName').focus();
}
function smDevReset(){
  smEditDev=-1;['sdName','sdHost','sdUser','sdPass'].forEach(id=>$(id).value='');$('sdPort').value=80;$('sdInt').value=60;
  $('sdOff').checked=$('sdEn').checked=true;$('smDevTitle').textContent='Gerät hinzufügen';$('sdCancel').hidden=true;
}
$('sdCancel').onclick=smDevReset;
$('sdSave').onclick=async()=>{
  const d={name:$('sdName').value.trim(),host:$('sdHost').value.trim(),port:parseInt($('sdPort').value)||80,intervalS:parseInt($('sdInt').value)||60,
    user:$('sdUser').value.trim(),pass:$('sdPass').value,offlineAlert:$('sdOff').checked,enabled:$('sdEn').checked};
  if(!d.name||!d.host){$('sdMsg').textContent='Name und Adresse angeben';$('sdMsg').className='msg bad';return}
  const c=smCfgForSave();
  if(smEditDev>=0){d.origIdx=smEditDev;c.devices[smEditDev]=d}else{if(c.devices.length>=8){$('sdMsg').textContent='Maximal 8 Geräte';$('sdMsg').className='msg bad';return}d.origIdx=-1;c.devices.push(d)}
  if(await smSave(c,$('sdMsg'),smEditDev>=0?'Gerät gespeichert':'Gerät angelegt, wird abgefragt …')){smDevReset();setTimeout(smLoad,3000)}
};
function smRuleEdit(i){
  smEditRule=i;const r=smCfg.rules[i];
  $('srDev').value=r.device;fillPaths(r.path);$('srOp').value=r.op;$('srThr').value=r.threshold;$('srHyst').value=r.hysteresis;
  $('srMin').value=r.minDurationS;$('srRep').value=r.repeatMin;fillChSelect($('srCh'),'Alarmkanal (Standard)',r.channel);$('srText').value=r.text;
  $('srRec').checked=r.notifyRecovery;$('srAck').checked=!!r.ackRequired;$('srEn').checked=r.enabled;$('srPath').dataset.keep='1';
  $('smRuleTitle').textContent='Regel bearbeiten';$('srCancel').hidden=false;
}
function smRuleReset(){
  smEditRule=-1;$('srThr').value='';$('srHyst').value='0.5';$('srMin').value=0;$('srRep').value=0;$('srCh').value='';$('srText').value='';
  $('srRec').checked=$('srEn').checked=true;$('srAck').checked=false;delete $('srPath').dataset.keep;$('smRuleTitle').textContent='Regel hinzufügen';$('srCancel').hidden=true;fillPaths('');
}
$('srCancel').onclick=smRuleReset;
$('srPath').onchange=()=>{$('srPath').dataset.keep='1'};
$('srSave').onclick=async()=>{
  const thr=parseFloat($('srThr').value);
  if(!$('srPath').value){$('srMsg').textContent='Messwert wählen (Gerät erst abfragen)';$('srMsg').className='msg bad';return}
  if(isNaN(thr)){$('srMsg').textContent='Grenzwert angeben';$('srMsg').className='msg bad';return}
  const r={device:parseInt($('srDev').value),path:$('srPath').value,op:parseInt($('srOp').value),threshold:thr,
    hysteresis:Math.abs(parseFloat($('srHyst').value)||0),minDurationS:parseInt($('srMin').value)||0,repeatMin:parseInt($('srRep').value)||0,
    channel:$('srCh').value,notifyRecovery:$('srRec').checked,text:$('srText').value.trim(),ackRequired:$('srAck').checked,enabled:$('srEn').checked};
  const c=smCfgForSave();
  if(smEditRule>=0)c.rules[smEditRule]=r;else{if(c.rules.length>=16){$('srMsg').textContent='Maximal 16 Regeln';$('srMsg').className='msg bad';return}c.rules.push(r)}
  if(await smSave(c,$('srMsg'),smEditRule>=0?'Regel gespeichert':'Regel angelegt'))smRuleReset();
};

/* ---------- Ping-Check ---------- */
let pgEdit=-1;
function pgRender(st){
  const tb=$('pgBody');tb.innerHTML='';
  const pings=smCfg.pings||[];
  pings.forEach((p,i)=>{
    const s=(st.pings||[]).find(x=>x.idx===i)||{};
    const tr=document.createElement('tr');
    const td=(t,cls)=>{const e=document.createElement('td');if(cls)e.className=cls;if(t!==undefined)e.textContent=t;tr.appendChild(e);return e};
    td(p.name+(p.enabled?'':' (aus)'));
    td(p.host+(s.ip&&s.ip!==p.host?' ('+s.ip+')':''),'mono');
    td(p.thresholdS+' s / alle '+(p.intervalS||10)+' s'+(p.ackRequired?', bis ACK':''));
    const z=td();
    if(!p.enabled)z.textContent='deaktiviert';
    else if(s.state==='up'){z.textContent='✓ '+s.rttMs+' ms';z.className='ok'}
    else if(s.state==='down'){z.textContent='ALARM: seit '+ago(s.noReplyS)+' keine Antwort';z.className='bad'}
    else if(s.state==='failing'){z.textContent='✗ '+(s.error||'keine Antwort')+' ('+ago(s.noReplyS)+')';z.className='warn'}
    else z.textContent='wartet …';
    if(s.ack){z.textContent+=' · ACK '+s.ack+' offen';z.className='bad'}
    const a=td();
    [['bearbeiten',()=>{pgEdit=i;$('pgName').value=p.name;$('pgHost').value=p.host;$('pgThr').value=p.thresholdS;$('pgIv').value=p.intervalS||10;$('pgAck').checked=!!p.ackRequired;$('pgEn').checked=p.enabled;
        $('pgTitle').textContent='Ping-Ziel bearbeiten: '+p.name;$('pgCancel').hidden=false}],
     ['löschen',async()=>{if(!confirm('Ping-Ziel '+p.name+' löschen?'))return;const c=smCfgForSave();c.pings.splice(i,1);await smSave(c,$('pgMsg'),'Ping-Ziel gelöscht')}]]
      .forEach(([t,f])=>{const b=document.createElement('button');b.type='button';b.className='chip';b.textContent=t;b.onclick=f;a.appendChild(b)});
    tb.appendChild(tr);
  });
  $('pgEmpty').hidden=pings.length>0;
}
function pgReset(){pgEdit=-1;$('pgName').value=$('pgHost').value='';$('pgThr').value=60;$('pgIv').value=10;$('pgAck').checked=false;$('pgEn').checked=true;$('pgTitle').textContent='Ping-Ziel hinzufügen';$('pgCancel').hidden=true}
$('pgCancel').onclick=pgReset;
$('pgSave').onclick=async()=>{
  const p={name:$('pgName').value.trim(),host:$('pgHost').value.trim(),thresholdS:parseInt($('pgThr').value)||0,intervalS:parseInt($('pgIv').value)||0,ackRequired:$('pgAck').checked,enabled:$('pgEn').checked};
  if(!p.name){$('pgMsg').textContent='Bezeichner angeben (Pflicht)';$('pgMsg').className='msg bad';return}
  if(!p.host){$('pgMsg').textContent='Name oder IP angeben';$('pgMsg').className='msg bad';return}
  const c=smCfgForSave();c.pings=c.pings||[];
  if(pgEdit>=0)c.pings[pgEdit]=p;else{if(c.pings.length>=5){$('pgMsg').textContent='Maximal 5 Ping-Ziele';$('pgMsg').className='msg bad';return}c.pings.push(p)}
  if(await smSave(c,$('pgMsg'),pgEdit>=0?'Ping-Ziel gespeichert':'Ping-Ziel angelegt'))pgReset();
};

/* ---------- Zabbix-Weiterleitung (E41) ---------- */
let zbDirty=false;
['zbEn','zbSrv','zbPort','zbPre','zbCh','zbRx','zbHb','zbAck','zbUnm'].forEach(id=>$(id).addEventListener('input',()=>{zbDirty=true;$('zbMsg').textContent='ungespeichert';$('zbMsg').className='msg warn'}));
async function zbLoad(){
  try{
    const j=await (await fetch('/api/zbx',{cache:'no-store'})).json();
    const c=j.config,s=j.status;
    if(!zbDirty){$('zbEn').checked=c.enabled;$('zbSrv').value=c.server;$('zbPort').value=c.port;$('zbPre').value=c.hostPrefix;
      $('zbCh').value=c.channels;$('zbRx').value=c.rxHost;$('zbRx').placeholder=s.rxHost;$('zbHb').value=c.heartbeatMin;$('zbAck').checked=c.autoAck;$('zbUnm').checked=c.forwardUnmarked}
    const rows=[['Empfangen',s.received],['An Zabbix übergeben',s.forwarded+(s.lastOkAgoS!==null?' (zuletzt vor '+ago(s.lastOkAgoS)+')':'')],
      ['ACK gesendet',s.acks],['Abgelehnt',s.rejected],['Wartend',s.pending],['Verbindungsfehler',s.errors],['Verworfen',s.dropped],
      ['Lebenszeichen',s.heartbeatAgoS!==null?'angenommen vor '+ago(s.heartbeatAgoS)+' ('+s.rxHost+')':'–'],
      ['Letzter Fehler',s.lastError||'–'],['Test',s.lastTest||'–']];
    $('zbStat').innerHTML='';
    rows.forEach(([k,v])=>{const dt=document.createElement('dt'),dd=document.createElement('dd');dt.textContent=k;dd.textContent=String(v);
      if(k==='Letzter Fehler'&&s.lastError)dd.className='bad';if(k==='Test'&&s.lastTest)dd.className=s.lastTest.startsWith('OK')?'ok':'bad';
      $('zbStat').append(dt,dd)});
    $('zbStat').hidden=!c.enabled;
  }catch(e){}
}
$('zbSave').onclick=async()=>{
  const c={enabled:$('zbEn').checked,server:$('zbSrv').value.trim(),port:parseInt($('zbPort').value)||10051,hostPrefix:$('zbPre').value.trim(),
    channels:$('zbCh').value.trim(),rxHost:$('zbRx').value.trim(),heartbeatMin:parseInt($('zbHb').value)||0,autoAck:$('zbAck').checked,forwardUnmarked:$('zbUnm').checked};
  const r=await fetch('/api/zbx/save',{method:'POST',body:new URLSearchParams({json:JSON.stringify(c)})});
  const j=await r.json().catch(()=>({}));
  if(!r.ok){$('zbMsg').textContent=j.error||('Fehler '+r.status);$('zbMsg').className='msg bad';return}
  zbDirty=false;$('zbMsg').textContent='Gespeichert';$('zbMsg').className='msg ok';zbLoad();
};
$('zbTest').onclick=async()=>{
  if(zbDirty){$('zbMsg').textContent='Erst speichern';$('zbMsg').className='msg warn';return}
  await fetch('/api/zbx/test',{method:'POST'});
  $('zbMsg').textContent='Testwert wird gesendet …';$('zbMsg').className='msg';
  setTimeout(zbLoad,2500);setTimeout(zbLoad,6000);
};

/* ---------- CLI-Aufträge (Batch) ---------- */
const sleep=ms=>new Promise(r=>setTimeout(r,ms));
async function runJob(cmds,quiet){
  const out=[];
  for(let i=0;i<cmds.length;i+=40){
    const part=cmds.slice(i,i+40);
    const body=new URLSearchParams();
    if(quiet)body.append('quiet','1');
    part.forEach(c=>body.append('cmd',c));
    let r,j;
    for(let t=0;t<20;t++){   // Warteschlange voll -> kurz warten
      r=await fetch('/api/cli/batch',{method:'POST',body});
      j=await r.json().catch(()=>({}));
      if(r.status!==503)break;
      await sleep(300);
    }
    if(!r.ok)throw new Error(j.error||('Fehler '+r.status));
    let res=null;
    for(let t=0;t<150;t++){
      await sleep(t<5?100:200);
      const q=await fetch('/api/cli/job?id='+j.id,{cache:'no-store'});
      if(!q.ok)throw new Error('Auftrag verloren');
      res=await q.json();
      if(res.done)break;
    }
    if(!res||!res.done)throw new Error('Zeitüberschreitung');
    out.push(...res.replies);
  }
  return out;
}
/* ---------- Nachbarn (E34) ---------- */
// Antwort der CLI "neighbors": "-none-" oder Zeilen "KENNUNG:sek_her:snr×4"
function parseNeighbors(r){
  const s=val(r);
  if(!s||s==='-none-')return [];
  return s.split(/\r?\n/).map(l=>l.trim().split(':')).filter(p=>p.length===3&&/^[0-9A-Fa-f]+$/.test(p[0]))
    .map(p=>({id:p[0].toUpperCase(),ago:parseInt(p[1],10),snr:parseInt(p[2],10)/4}));
}
function agoText(s){
  if(!(s>=0))return '?';
  if(s<60)return 'vor '+s+' s';
  if(s<3600)return 'vor '+Math.floor(s/60)+' min';
  if(s<86400)return 'vor '+Math.floor(s/3600)+' h '+Math.floor(s%3600/60)+' min';
  return 'vor '+Math.floor(s/86400)+' d';
}
let nbBusy=false;
async function loadNeighbors(){
  if(nbBusy)return;nbBusy=true;
  try{
    const [r]=await runJob(['neighbors'],true);
    const list=parseNeighbors(r&&r.reply);
    const tb=$('nbBody');tb.textContent='';
    if(!list.length){const tr=tb.insertRow();const td=tr.insertCell();td.colSpan=3;td.textContent='noch keine';}
    for(const n of list){
      const tr=tb.insertRow();
      const a=tr.insertCell();a.className='mono';a.textContent=n.id;
      tr.insertCell().textContent=agoText(n.ago);
      tr.insertCell().textContent=(n.snr>0?'+':'')+String(n.snr)+' dB';   // Viertel-dB, z. B. +12.25
    }
  }catch(e){}
  nbBusy=false;
}
$('nbScan').onclick=async()=>{
  const b=$('nbScan');b.disabled=true;
  try{
    const [r]=await runJob(['discover.neighbors'],false);
    $('nbMsg').className='msg';
    for(let s=60;s>0;s-=5){$('nbMsg').textContent='Suche gesendet ('+val(r&&r.reply)+'), Antworten bis zu 60 s … noch '+s+' s';await loadNeighbors();await sleep(5000);}
    await loadNeighbors();$('nbMsg').textContent='Suche abgeschlossen';$('nbMsg').className='msg ok';
  }catch(e){$('nbMsg').textContent=e.message;$('nbMsg').className='msg bad'}
  b.disabled=false;
};

const val=r=>String(r||'').replace(/^>\s?/,'').trim();
const isErr=r=>/^(error|err\b|\?\?|unknown)/i.test(val(r));
function same(a,b){
  a=val(a).replace(/%$/,'');b=val(b).replace(/%$/,'');
  if(a===b)return true;
  const pa=a.split(','),pb=b.split(',');
  if(pa.length!==pb.length)return false;
  return pa.every((x,i)=>{const fx=parseFloat(x),fy=parseFloat(pb[i]);
    return (!isNaN(fx)&&!isNaN(fy)&&/^[-\d.\s]+$/.test(x)&&/^[-\d.\s]+$/.test(pb[i]))?Math.abs(fx-fy)<=1e-6*Math.max(1,Math.abs(fy)):x.trim()===pb[i].trim()});
}
let needReboot=false;
function flagReboot(cmds){if(cmds.some(c=>/^set (radio|freq|prv\.key) /.test(c))){needReboot=true;$('rebootBanner').classList.add('on')}}
$('bannerReboot').onclick=async()=>{if(!confirm('Gateway jetzt neu starten?'))return;await fetch('/api/reboot',{method:'POST'});$('rebootBanner').innerHTML='<span>Neustart … Seite in ca. 20 s neu laden.</span>'};

/* ---------- Einstellungsfelder ---------- */
const OO=[['on','an'],['off','aus']];
const F=[
 {g:'Identität'},
 {k:'name',l:'Name',t:'text',max:31,h:'Standort und Kontakt im Namen erleichtern Absprachen. Nicht erlaubt: [ ] \\ : , ? *'},
 {k:'lat',l:'Breitengrad',t:'num',step:'any'},
 {k:'lon',l:'Längengrad',t:'num',step:'any'},
 {k:'owner.info',l:'Owner-Info',t:'text',max:119,h:'„|“ = Zeilenumbruch, nur mit Passwort lesbar'},
 {k:'guest.password',l:'Gast-Passwort',t:'text',max:15,h:'leer = kein Gastzugang (offen lassen lädt Bot-Spam ein)'},
 {g:'Funk'},
 {k:'radio',l:'Frequenz / BW / SF / CR',t:'radio',h:'Deutschland: 869.618 MHz, 62.5 kHz, SF8, CR8. Neustart nötig.'},
 {k:'tx',l:'TX-Leistung am Chip',t:'num',min:1,max:22,step:1,u:'dBm',h:'Heltec V4 mit PA: 16 ≈ 467 mW, 17 ≈ 520 mW, 22 ≈ 800 mW am Ausgang. Grenze: 27 dBm ERP inkl. Antenne.'},
 {k:'dutycycle',l:'Duty Cycle',t:'num',min:1,max:100,step:0.1,u:'%',h:'gesetzlich 10 % pro Stunde'},
 {k:'radio.rxgain',l:'RX Boosted Gain',t:'sel',o:OO,h:'1.17.1 auf V4: aus (sonst Noise Floor ca. -84 dBm)'},
 {k:'radio.fem.rxgain',l:'FEM-Empfangsverstärker',t:'sel',o:OO,h:'1.17.1 auf V4: aus'},
 {k:'radio.fem.txgain',l:'FEM-Sendeverstärker',t:'sel',o:OO,h:'nur bei Board-Revision mit KCT8103L'},
 {g:'Weiterleitung'},
 {k:'repeat',l:'Pakete weiterleiten',t:'sel',o:OO},
 {k:'flood.max',l:'Flood max. Hops (scoped)',t:'num',min:0,max:64,step:1},
 {k:'flood.max.unscoped',l:'Flood max. Hops (unscoped)',t:'num',min:0,max:64,step:1,h:'ländlich 5, dicht 2–3, nie 0'},
 {k:'flood.max.advert',l:'Flood max. Hops (Adverts)',t:'num',min:0,max:64,step:1},
 {k:'txdelay',l:'TX-Verzögerung (Flood)',t:'num',min:0,max:2,step:0.1,h:'Rand 0.7, Kern-Repeater 2.0'},
 {k:'direct.txdelay',l:'TX-Verzögerung (Direkt)',t:'num',min:0,max:2,step:0.1,h:'Rand 0.5, Kern-Repeater 0.9'},
 {k:'rxdelay',l:'RX-Delay (Filter)',t:'num',min:0,max:20,step:0.1,h:'0 = aus, dichte Netze bis 6'},
 {k:'path.hash.mode',l:'Pfad-Hash-Länge',t:'sel',o:[['0','1 Byte'],['1','2 Byte'],['2','3 Byte']]},
 {k:'loop.detect',l:'Loop-Erkennung',t:'sel',o:[['off','aus'],['minimal','minimal'],['moderate','moderat'],['strict','streng']]},
 {k:'cad',l:'Hardware-CAD',t:'sel',o:OO},
 {k:'int.thresh',l:'Interferenz-Schwelle',t:'num',min:0,max:255,step:1},
 {k:'agc.reset.interval',l:'AGC-Reset-Intervall',t:'num',min:0,max:1020,step:4,u:'s',h:'wird auf ein Vielfaches von 4 gerundet, 0 = aus'},
 {k:'multi.acks',l:'Multi-ACKs',t:'sel',o:[['0','aus'],['1','an']]},
 {k:'allow.read.only',l:'Nur-Lese-Zugang erlauben',t:'sel',o:OO},
 {g:'Adverts'},
 {k:'advert.interval',l:'Zero-Hop-Advert',t:'num',min:0,max:240,step:2,u:'min',h:'0 = aus, sonst 60–240'},
 {k:'flood.advert.interval',l:'Flood-Advert',t:'num',min:0,max:168,step:1,u:'h',h:'0 = aus, sonst 3–168 (nicht 168)'},
];
const FK=F.filter(f=>f.k);
const loaded={};
function fieldVal(f){
  if(f.t==='radio')return ['rf','rb','rs','rc'].map(s=>$('f_'+s).value.trim()).join(',');
  return $('f_'+f.k).value.trim();
}
function setCmd(f,v){return 'set '+f.k+' '+v}
// MeshCore gibt float32 ungerundet aus (txdelay 0.6999999, Frequenz 869.6179809). Unverändert in ein
// Zahlenfeld mit step=0.1 geschrieben, blockiert der Browser das Absenden (step mismatch) -> auf
// 4 Nachkommastellen runden. lat/lon nicht (step=any, 4 Stellen wären nur ~10 m genau).
function nf(f,v){return f.k!=='lat'&&f.k!=='lon'&&/^-?\d+\.\d{5,}$/.test(v)?String(+(+v).toFixed(4)):v}
function markChanges(){
  let n=0;
  FK.forEach(f=>{const box=$('fb_'+f.k);if(!box||box.classList.contains('na'))return;
    const c=!same(fieldVal(f),loaded[f.k]);box.classList.toggle('chg',c);if(c)n++});
  $('setSave').disabled=n===0;
  $('setMsg').textContent=n?n+' Änderung'+(n>1?'en':''):'';$('setMsg').className='msg warn';
}
function buildForm(){
  const w=$('setFields');w.innerHTML='';
  F.forEach(f=>{
    if(f.g){const d=document.createElement('div');d.className='grp';d.textContent=f.g;w.appendChild(d);return}
    const lab=document.createElement('label');lab.textContent=f.l+(f.u?' ('+f.u+')':'');lab.htmlFor='f_'+(f.t==='radio'?'rf':f.k);
    const box=document.createElement('div');box.className='fld';box.id='fb_'+f.k;
    let html='';
    if(f.t==='radio'){
      html='<div class="radio4"><input id="f_rf" type="number" step="0.001" min="150" max="2500" title="Frequenz MHz">'+
        '<select id="f_rb" title="Bandbreite kHz">'+['7.8','10.4','15.6','20.8','31.25','41.7','62.5','125','250','500'].map(b=>'<option value="'+b+'">'+b+' kHz</option>').join('')+'</select>'+
        '<select id="f_rs" title="Spreading Factor">'+[5,6,7,8,9,10,11,12].map(s=>'<option value="'+s+'">SF'+s+'</option>').join('')+'</select>'+
        '<select id="f_rc" title="Coding Rate">'+[5,6,7,8].map(s=>'<option value="'+s+'">CR'+s+'</option>').join('')+'</select></div>';
    }else if(f.t==='sel'){
      html='<select id="f_'+f.k+'">'+f.o.map(o=>'<option value="'+o[0]+'">'+o[1]+'</option>').join('')+'</select>';
    }else if(f.t==='num'){
      html='<input id="f_'+f.k+'" type="number"'+(f.min!==undefined?' min="'+f.min+'"':'')+(f.max!==undefined?' max="'+f.max+'"':'')+' step="'+(f.step||'any')+'">';
    }else{
      html='<input id="f_'+f.k+'" type="text" maxlength="'+f.max+'" spellcheck="false">';
    }
    box.innerHTML=html+'<small>'+(f.h||'')+'</small>';
    w.appendChild(lab);w.appendChild(box);
  });
  w.querySelectorAll('input,select').forEach(e=>e.oninput=e.onchange=markChanges);
}
function fill(f,reply){
  const box=$('fb_'+f.k);
  if(isErr(reply)){box.classList.add('na');box.querySelectorAll('input,select').forEach(e=>e.disabled=true);
    box.querySelector('small').textContent='Auf diesem Board nicht verfügbar ('+val(reply)+')';loaded[f.k]=null;return}
  box.classList.remove('na');box.querySelectorAll('input,select').forEach(e=>e.disabled=false);
  let v=nf(f,val(reply).replace(/%$/,''));
  loaded[f.k]=v;
  if(f.t==='radio'){const p=v.split(',');p[0]=nf(f,p[0]);
    $('f_rf').value=parseFloat(p[0]);
    const b=String(parseFloat(p[1]));const sel=$('f_rb');
    if(![...sel.options].some(o=>o.value===b)){const o=document.createElement('option');o.value=b;o.textContent=b+' kHz';sel.appendChild(o)}
    sel.value=b;$('f_rs').value=String(parseInt(p[2]));$('f_rc').value=String(parseInt(p[3]));
    loaded[f.k]=[parseFloat(p[0]),b,parseInt(p[2]),parseInt(p[3])].join(',');
  }else $('f_'+f.k).value=v;
}
async function loadSettings(){
  $('setMsg').textContent='Lade …';$('setMsg').className='msg';
  try{
    const rep=await runJob(FK.map(f=>'get '+f.k),true);
    FK.forEach((f,i)=>fill(f,rep[i]?rep[i].reply:''));
    markChanges();
    if(!$('setSave').disabled)return;
    $('setMsg').textContent='';
  }catch(e){$('setMsg').textContent='Laden fehlgeschlagen: '+e.message;$('setMsg').className='msg bad'}
}
function showResults(el,rep){
  el.hidden=false;
  el.textContent=rep.map(r=>(isErr(r.reply)?'✗ ':'✓ ')+r.cmd+(r.reply?'  →  '+r.reply:'')).join('\n');
}
$('setForm').onsubmit=async e=>{
  e.preventDefault();
  const cmds=[];
  FK.forEach(f=>{const box=$('fb_'+f.k);if(box.classList.contains('na'))return;
    const v=fieldVal(f);if(same(v,loaded[f.k]))return;
    if(f.k==='name'&&/[\[\]\\:,?*]/.test(v)){alert('Name enthält unerlaubte Zeichen');throw 0}
    cmds.push(setCmd(f,v))});
  if(!cmds.length)return;
  $('setSave').disabled=true;$('setMsg').textContent='Speichere …';$('setMsg').className='msg';
  try{
    const rep=await runJob(cmds,false);
    showResults($('setOut'),rep);
    const bad=rep.filter(r=>isErr(r.reply)).length;
    flagReboot(cmds);
    await loadSettings();
    $('setMsg').textContent=bad?bad+' Befehl(e) abgelehnt, siehe unten':'Gespeichert';$('setMsg').className='msg '+(bad?'bad':'ok');
    loadProfile();refresh();
  }catch(e){$('setMsg').textContent='Fehler: '+e.message;$('setMsg').className='msg bad'}
};
$('setReload').onclick=()=>{$('setOut').hidden=true;loadSettings()};

/* ---------- Empfehlungsprofil ---------- */
let prof=null,profDiff=[];
async function loadProfile(){
  $('profMsg').textContent='Prüfe …';$('profMsg').className='msg';
  try{
    if(!prof)prof=await (await fetch('/api/profile',{cache:'no-store'})).json();
    $('profName').textContent=prof.name;
    const withGet=prof.entries.filter(e=>e.get);
    const rep=await runJob(withGet.map(e=>e.get),true);
    const ist={};withGet.forEach((e,i)=>ist[e.cmd]=rep[i]?rep[i].reply:'');
    profDiff=[];
    const tb=$('profBody');tb.innerHTML='';
    prof.entries.forEach(e=>{
      const tr=document.createElement('tr');
      const ok=e.get?same(ist[e.cmd],e.expect):null;
      if(ok===false)profDiff.push(e.cmd);
      const cells=[e.cmd,e.get?val(ist[e.cmd]):'',e.expect||'',ok===null?'':(ok?'✓':'✗'),e.why];
      cells.forEach((c,i)=>{const td=document.createElement('td');td.textContent=c;if(i===0)td.className='mono';if(i===3){td.className='st '+(ok?'ok':'bad')}tr.appendChild(td)});
      tb.appendChild(tr);
    });
    $('profApply').disabled=profDiff.length===0;
    $('profMsg').textContent=profDiff.length?profDiff.length+' Abweichung(en)':'Alles wie empfohlen';
    $('profMsg').className='msg '+(profDiff.length?'warn':'ok');
  }catch(e){$('profMsg').textContent='Prüfung fehlgeschlagen: '+e.message;$('profMsg').className='msg bad'}
}
$('profApply').onclick=async()=>{
  const cmds=[...profDiff];
  if(cmds.some(c=>c.startsWith('region ')))cmds.push('region save');
  if(!confirm('Diese Befehle ausführen?\n\n'+cmds.join('\n')))return;
  $('profApply').disabled=true;$('profMsg').textContent='Übernehme …';$('profMsg').className='msg';
  try{
    const rep=await runJob(cmds,false);
    flagReboot(cmds);
    const bad=rep.filter(r=>isErr(r.reply));
    await loadProfile();await loadSettings();loadRegions();
    if(bad.length){$('profMsg').textContent='Abgelehnt: '+bad.map(r=>r.cmd+' → '+r.reply).join('; ');$('profMsg').className='msg bad'}
  }catch(e){$('profMsg').textContent='Fehler: '+e.message;$('profMsg').className='msg bad'}
};
$('profReload').onclick=loadProfile;

/* ---------- Regionen ---------- */
async function loadRegions(){
  try{const rep=await runJob(['region'],true);const t=(rep[0]&&rep[0].reply)||'';
    $('regOut').textContent=t||'(leer)';
    // Baum aus "region": je Zeile "name [F][^]", "*" = Wildcard (E36)
    const named=val(t).split(/\r?\n/).map(l=>l.trim().split(/\s+/)[0]).filter(n=>n&&n!=='*');
    $('regWarn').hidden=named.length>0}
  catch(e){$('regOut').textContent='Fehler: '+e.message}
}
async function regCmd(cmd){
  $('regMsg').textContent='…';$('regMsg').className='msg';
  try{const rep=await runJob([cmd],false);const r=rep[0]?rep[0].reply:'';
    $('regMsg').textContent=r||'OK';$('regMsg').className='msg '+(isErr(r)?'bad':'ok');await loadRegions();loadProfile()}
  catch(e){$('regMsg').textContent='Fehler: '+e.message;$('regMsg').className='msg bad'}
}
$('regRun').onclick=()=>{
  const a=$('regAct').value,n=$('regName').value.trim(),p=$('regParent').value.trim();
  if(!/^[#A-Za-z0-9._-]+$/.test(n)){$('regMsg').textContent='Name fehlt oder enthält ungültige Zeichen';$('regMsg').className='msg bad';return}
  regCmd('region '+a+' '+n+(a==='put'&&p?' '+p:''));
};
$('regSave').onclick=()=>regCmd('region save');

/* ---------- Admin-Passwort ---------- */
$('admSave').onclick=async()=>{
  const p=$('admPass').value;
  if(!p||p.length>15||/\s/.test(p)){$('admMsg').textContent='1–15 Zeichen, keine Leerzeichen';$('admMsg').className='msg bad';return}
  if(!confirm('Admin-Passwort ändern? Danach musst du dich mit admin / neuem Passwort neu anmelden.'))return;
  try{const rep=await runJob(['password '+p],true);const r=rep[0]?rep[0].reply:'';
    $('admPass').value='';$('admMsg').textContent=r||'OK';$('admMsg').className='msg '+(isErr(r)?'bad':'ok')}
  catch(e){$('admMsg').textContent='Fehler: '+e.message;$('admMsg').className='msg bad'}
};

/* ---------- Backup & Wiederherstellen ---------- */
function regionCmds(txt){
  // Baum aus "region": Einrückung = Ebene, "F" = Flood erlaubt, "^" = Home
  const cmds=[],stack=[];
  String(txt||'').split('\n').forEach(line=>{
    if(!line.trim())return;
    const depth=line.length-line.replace(/^ +/,'').length;
    const m=line.trim().match(/^(\S+?)(\^?)(\s+F)?$/);if(!m)return;
    const name=m[1];stack[depth]=name;stack.length=depth+1;
    if(name==='*')return;
    const parent=depth>1?stack[depth-1]:null;
    cmds.push('region put '+name+(parent?' '+parent:''));
    if(!m[3])cmds.push('region denyf '+name);
    if(m[2])cmds.push('region home '+name);
  });
  if(cmds.length)cmds.push('region save');
  return cmds;
}
// Backup v2 (E28): CLI-Befehle + ACL + Kanäle + Überwachung + Gateway (Hostname/WLAN),
// mit Haken zusätzlich Geheimnisse (prv.key, Admin-/Gast-/WLAN-/Sensormeter-Passwörter).
const SECRET_KEYS=['guest.password'];
$('bkDown').onclick=async()=>{
  $('bkMsg').textContent='Lese Einstellungen …';$('bkMsg').className='msg';
  try{
    const withKey=$('bkKey').checked;
    const fk=FK.filter(f=>withKey||!SECRET_KEYS.includes(f.k));
    const gets=fk.map(f=>'get '+f.k).concat(['region']);
    if(withKey)gets.push('get prv.key');
    const rep=await runJob(gets,true);
    const cmds=[];
    fk.forEach((f,i)=>{const r=rep[i].reply;if(isErr(r))return;let v=nf(f,val(r).replace(/%$/,''));
      if(f.t==='radio'){const p=v.split(',');v=[parseFloat(nf(f,p[0])),parseFloat(p[1]),parseInt(p[2]),parseInt(p[3])].join(',')}
      cmds.push(setCmd(f,v))});
    cmds.push(...regionCmds(rep[fk.length].reply));
    if(withKey){const k=val(rep[fk.length+1].reply);if(/^[0-9a-fA-F]{64,}$/.test(k))cmds.push('set prv.key '+k)}
    const ex=await (await fetch('/api/backup/extra?secrets='+(withKey?1:0),{cache:'no-store'})).json();
    ex.acl.forEach(a=>cmds.push('setperm '+a.pubkey+' '+a.perm));
    const st=await (await fetch('/api/status',{cache:'no-store'})).json();
    const chs=(await (await fetch('/api/channels',{cache:'no-store'})).json()).channels
      .map(c=>({kind:c.kind,name:c.name,key:c.kind==='private'?c.key_b64:'',scope:c.scope}));
    const smc=ex.sensormeter;
    smc.devices.forEach(d=>{if(d.pass==='***')d.pass=''});   // ohne Haken: Passwort leer lassen
    const file={format:'meshcore-gateway-backup',version:2,created:new Date().toISOString(),secrets:withKey,
      node:st.node.name,pubkey:st.node.pubkey,meshcore:st.node.firmware,gateway:st.gateway.version,commands:cmds,channels:chs,sensormeter:smc,
      gw:{hostname:ex.hostname,wifi:ex.wifi},zabbix:ex.zabbix};
    if(withKey)file.adminPassword=ex.adminPassword;
    const a=document.createElement('a');
    a.href=URL.createObjectURL(new Blob([JSON.stringify(file,null,2)],{type:'application/json'}));
    a.download='meshcore-gw-'+st.node.name.replace(/[^A-Za-z0-9_-]+/g,'_')+'-'+file.created.slice(0,10)+(withKey?'-VOLL':'')+'.json';
    document.body.appendChild(a);a.click();a.remove();
    $('bkMsg').textContent=cmds.length+' Befehle, '+chs.length+' Kanäle, '+ex.acl.length+' ACL-Einträge gesichert'+(withKey?' (mit Schlüssel und Passwörtern!)':'');$('bkMsg').className='msg ok';
  }catch(e){$('bkMsg').textContent='Backup fehlgeschlagen: '+e.message;$('bkMsg').className='msg bad'}
};
let bkCmds=null,bkChannels=[],bkSm=null,bkGw=null,bkAdm='',bkZbx=null;
$('bkFile').onchange=async e=>{
  bkCmds=null;$('bkRestore').disabled=true;$('bkOut').hidden=true;$('bkOpts').hidden=true;
  const f=e.target.files[0];if(!f)return;
  try{
    const j=JSON.parse(await f.text());
    if(j.format!=='meshcore-gateway-backup'||!Array.isArray(j.commands))throw new Error('kein Gateway-Backup');
    const allowed=/^(set [a-z.]+ |region (put|denyf|home|save)\b|setperm [0-9a-f]{64} \d{1,3}$)/;
    const bad=j.commands.filter(c=>typeof c!=='string'||!allowed.test(c));
    if(bad.length)throw new Error('unerwartete Befehle: '+bad.slice(0,3).join(', '));
    bkCmds=j.commands;bkChannels=Array.isArray(j.channels)?j.channels:[];
    bkSm=(j.sensormeter&&Array.isArray(j.sensormeter.devices)&&Array.isArray(j.sensormeter.rules))?j.sensormeter:null;
    bkGw=(j.gw&&j.gw.wifi&&j.gw.wifi.ssid)?j.gw:null;
    bkAdm=typeof j.adminPassword==='string'?j.adminPassword:'';
    bkZbx=(j.zabbix&&typeof j.zabbix==='object')?j.zabbix:null;   // ab 0.9.0
    // WLAN nur anbieten, wenn das Passwort drin ist oder die SSID gleich bleibt
    const st=await (await fetch('/api/status',{cache:'no-store'})).json().catch(()=>null);
    const wifiOk=bkGw&&(typeof bkGw.wifi.pass==='string'||(st&&st.wifi&&st.wifi.ssid===bkGw.wifi.ssid));
    $('bkWifiL').hidden=!bkGw;$('bkWifi').checked=false;$('bkWifi').disabled=!wifiOk;
    $('bkWifiL').title=wifiOk?'':'WLAN-Passwort nicht im Backup und anderes WLAN';
    $('bkAdmL').hidden=!bkAdm;$('bkAdm').checked=true;
    $('bkOpts').hidden=!bkGw&&!bkAdm;
    const mask=c=>c.startsWith('set prv.key ')?'set prv.key ••••••••':(c.startsWith('set guest.password ')?'set guest.password ••••':c);
    const acl=bkCmds.filter(c=>c.startsWith('setperm ')).length;
    $('bkOut').hidden=false;
    $('bkOut').textContent='Backup von '+j.node+' ('+j.created+')'+(j.secrets?' – mit Schlüssel und Passwörtern':'')+'\n\n'+bkCmds.map(mask).join('\n')+
      (bkChannels.length?'\n\nKanäle: '+bkChannels.map(c=>c.name+(c.scope?' ['+c.scope+']':'')).join(', '):'')+
      (bkSm?'\nÜberwachung: '+bkSm.devices.length+' Geräte, '+bkSm.rules.length+' Regeln, '+(bkSm.pings||[]).length+' Ping-Ziele':'')+
      (acl?'\nZugriffsliste: '+acl+' Einträge':'')+
      (bkGw?'\nGateway: Hostname '+(bkGw.hostname&&bkGw.hostname!=='meshcore-gw'?bkGw.hostname:'automatisch (wie Gerätename)')+', WLAN „'+bkGw.wifi.ssid+'“'+(typeof bkGw.wifi.pass==='string'?' (mit Passwort)':' (ohne Passwort)')+(bkGw.wifi.ip?', feste IP '+bkGw.wifi.ip:''):'')+
      (bkAdm?'\nAdmin-Passwort: enthalten':'');
    $('bkRestore').disabled=false;$('bkMsg').textContent=bkCmds.length+' Befehle bereit';$('bkMsg').className='msg';
  }catch(err){$('bkMsg').textContent='Datei ungültig: '+err.message;$('bkMsg').className='msg bad'}
};
$('bkRestore').onclick=async()=>{
  if(!bkCmds)return;
  const key=bkCmds.some(c=>c.startsWith('set prv.key '));
  const adm=bkAdm&&$('bkAdm').checked, wifi=bkGw&&$('bkWifi').checked&&!$('bkWifi').disabled;
  if(!confirm('Backup einspielen? Aktuelle Einstellungen werden überschrieben.'+(key?'\n\nACHTUNG: enthält einen privaten Schlüssel, die Knoten-Identität wird ersetzt.':'')+
    (adm?'\n\nDas Admin-Passwort wird übernommen, danach neu anmelden.':'')+(wifi?'\n\nWLAN und Hostname werden übernommen, das Gateway startet neu.':'')))return;
  $('bkRestore').disabled=true;$('bkMsg').textContent='Spiele ein …';$('bkMsg').className='msg';
  try{
    const rep=await runJob(bkCmds,false);
    showResults($('bkOut'),rep.map(r=>r.cmd.startsWith('set prv.key ')?{cmd:'set prv.key ••••••••',reply:r.reply.replace(/[0-9a-f]{64}/i,'…')}:r));
    flagReboot(bkCmds);
    let bad=rep.filter(r=>isErr(r.reply)).length;
    for(const c of bkChannels){   // vorhandene Kanäle (gleicher Schlüssel) werden übersprungen
      try{await postForm('/api/channels',{action:'add',kind:c.kind,name:c.name,key:c.key||'',scope:c.scope||''})}
      catch(e){if(!/existiert schon/.test(e.message))bad++}
    }
    if(bkSm){   // nach den Kanälen: Regeln verweisen per Name auf sie. Leeres Passwort = keins.
      const r=await fetch('/api/sm/save',{method:'POST',body:new URLSearchParams({json:JSON.stringify(bkSm)})});
      if(!r.ok)bad++;
    }
    if(bkZbx){   // Zabbix-Weiterleitung (E41)
      const r=await fetch('/api/zbx/save',{method:'POST',body:new URLSearchParams({json:JSON.stringify(bkZbx)})});
      if(!r.ok)bad++;
    }
    let tail='';
    if(adm||wifi){   // zuletzt: danach gilt der alte Login nicht mehr bzw. Neustart
      const p={};
      if(adm)p.adminPassword=bkAdm;
      if(wifi){p.ssid=bkGw.wifi.ssid;p.hostname=bkGw.hostname||'';if(typeof bkGw.wifi.pass==='string')p.pass=bkGw.wifi.pass;
        if(typeof bkGw.wifi.ip==='string')['ip','mask','gw','dns1','dns2'].forEach(k=>p[k]=bkGw.wifi[k]||'')}   // ab 0.9.0, leer = DHCP
      try{await postForm('/api/backup/apply',p);tail=(adm?' · Admin-Passwort übernommen, bitte neu anmelden':'')+(wifi?' · WLAN übernommen, Neustart':'')}
      catch(e){bad++;tail=' · '+e.message}
    }
    if(!adm&&!wifi){loadChannels();loadSettings();loadProfile();loadRegions()}
    $('bkMsg').textContent=(bad?bad+' Schritt(e) abgelehnt':'Eingespielt')+tail;$('bkMsg').className='msg '+(bad?'bad':'ok');
    // Erledigt: Auswahl zurücksetzen, sonst löst dieselbe Datei kein change mehr aus
    bkCmds=null;$('bkFile').value='';$('bkOpts').hidden=true;
    return;
  }catch(e){$('bkMsg').textContent='Fehler: '+e.message;$('bkMsg').className='msg bad'}
  $('bkRestore').disabled=false;
};

/* ---------- Ereignis-Log (E27) ---------- */
const kb=b=>(b/1024).toFixed(b<10240?1:0)+' KB';
async function lgLoad(){
  try{const j=await (await fetch('/api/log/info',{cache:'no-store'})).json();
    set('lg_size',kb(j.fileBytes)+' von '+kb(j.maxBytes)+(j.dropped?' · '+j.dropped+' Zeilen verworfen':''));
    set('lg_pend',kb(j.pendingBytes)+' (wird ab 4 KB oder nach '+Math.round(j.flushIntervalS/60)+' min geschrieben)');
    set('lg_last',j.lastFlushAgoS===null?'seit Start noch nicht':'vor '+ago(j.lastFlushAgoS)+' ('+j.flushes+'× seit Start)');
  }catch(e){}
}
$('lgClear').onclick=async()=>{
  if(!confirm('Ereignis-Log im Flash löschen?'))return;
  try{await postForm('/api/log/clear',{});$('lgMsg').textContent='Gelöscht';$('lgMsg').className='msg ok'}
  catch(e){$('lgMsg').textContent='Fehler: '+e.message;$('lgMsg').className='msg bad'}
  lgLoad();
};

buildForm();
let settingsLoaded=false;
async function openSettings(){
  if(settingsLoaded)return;settingsLoaded=true;
  await loadSettings();await loadProfile();await loadRegions();
}

/* ---------- Takt ---------- */
let lastStatus=0,lastSm=0,lastNb=0;
function tick(){
  if(document.hidden)return;
  const now=Date.now();
  if(now-lastStatus>=5000){lastStatus=now;refresh()}
  if(view==='status'&&now-lastNb>=30000){lastNb=now;loadNeighbors()}
  if(view==='console')con.poll();
  if(view==='packets')pkt.poll();
  if(view==='channels')msgs.poll();
  if(view==='sm'){smLogFeed.poll();if(now-lastSm>=3000){lastSm=now;smLoad();zbLoad()}}
}
setInterval(tick,1000);
show(location.hash.slice(1)||'status');
</script>
</body>
</html>
)HTML";
