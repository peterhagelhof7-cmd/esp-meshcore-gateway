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
<a href="#packets" data-v="packets">Pakete</a>
<a href="#settings" data-v="settings">Einstellungen</a>
<a href="#wifi" data-v="wifi">WLAN</a>
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
<dt>Signal</dt><dd id="w_rssi">–</dd>
<dt>Hostname</dt><dd id="w_host">–</dd>
<dt>MAC</dt><dd class="mono" id="w_mac">–</dd>
<dt>Freier Heap</dt><dd id="g_heap">–</dd>
</dl>
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
<label class="chk"><input type="checkbox" id="bkKey"> privaten Schlüssel einschließen</label>
<div class="row"><button type="button" class="sec" id="bkDown">Backup herunterladen</button></div>
<label for="bkFile">Backup-Datei einspielen</label>
<input type="file" id="bkFile" accept=".json,application/json">
<div class="row"><button type="button" id="bkRestore" disabled>Wiederherstellen</button><span class="msg" id="bkMsg" style="margin:0"></span></div>
<pre class="out" id="bkOut" hidden></pre>
<p class="hint">Das Backup ist eine Liste offizieller CLI-Befehle (Einstellungen + Regionen). Nicht enthalten: Admin-Passwort (per CLI nicht auslesbar), WLAN-Zugangsdaten. Mit Schlüssel ist die Datei geheim zu halten.</p>
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
<input id="hostname" name="hostname" maxlength="32">
<div class="row">
<button type="button" class="sec" id="scanBtn">Netze suchen</button>
<button type="submit">Speichern &amp; neu starten</button>
<button type="button" class="sec" id="rebootBtn">Nur neu starten</button>
</div>
<div class="msg" id="wifiMsg"></div>
</form>
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
  tick();
}
window.onhashchange=()=>show(location.hash.slice(1));

/* ---------- Status ---------- */
let hostTouched=false;
async function refresh(){
  try{
    const r=await fetch('/api/status',{cache:'no-store'});
    if(!r.ok)throw new Error(r.status);
    const j=await r.json();
    const n=j.node,ra=j.radio,c=j.stats_core||{},rs=j.stats_radio||{},p=j.stats_packets||{},w=j.wifi;
    document.title=n.name+' – MeshCore Gateway';
    set('title',n.name);
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
    set('w_rssi',w.rssi?w.rssi+' dBm':null);set('w_host',w.hostname);set('w_mac',w.mac);
    set('g_heap',Math.round(j.gateway.free_heap/1024)+' KB');
    $('wifiPill').textContent=(modes[w.mode]||w.mode)+' · '+w.ip;
    if(!hostTouched&&!$('hostname').value)$('hostname').value=w.hostname;
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
$('scanBtn').onclick=scan;
$('wifiForm').onsubmit=async e=>{
  e.preventDefault();
  const body=new URLSearchParams(new FormData(e.target));
  const r=await fetch('/api/wifi',{method:'POST',body});
  const j=await r.json().catch(()=>({}));
  if(r.ok){$('wifiMsg').textContent='Gespeichert. Das Gateway startet neu und verbindet sich mit dem WLAN. Klappt das nicht innerhalb von 30 s, kommt der Installer-AP zurück.';$('wifiMsg').className='msg ok'}
  else{$('wifiMsg').textContent=j.error||('Fehler '+r.status);$('wifiMsg').className='msg bad'}
};
$('rebootBtn').onclick=async()=>{if(!confirm('Gateway neu starten?'))return;await fetch('/api/reboot',{method:'POST'});$('wifiMsg').textContent='Neustart …';$('wifiMsg').className='msg warn'};

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
const val=r=>String(r||'').replace(/^>\s?/,'').trim();
const isErr=r=>/^(error|err\b|\?\?|unknown)/i.test(val(r));
function same(a,b){
  a=val(a).replace(/%$/,'');b=val(b).replace(/%$/,'');
  if(a===b)return true;
  const pa=a.split(','),pb=b.split(',');
  if(pa.length!==pb.length)return false;
  return pa.every((x,i)=>{const fx=parseFloat(x),fy=parseFloat(pb[i]);
    return (!isNaN(fx)&&!isNaN(fy)&&/^[-\d.\s]+$/.test(x)&&/^[-\d.\s]+$/.test(pb[i]))?Math.abs(fx-fy)<1e-6:x.trim()===pb[i].trim()});
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
  let v=val(reply).replace(/%$/,'');
  loaded[f.k]=v;
  if(f.t==='radio'){const p=v.split(',');
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
  try{const rep=await runJob(['region'],true);$('regOut').textContent=(rep[0]&&rep[0].reply)||'(leer)'}
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
$('bkDown').onclick=async()=>{
  $('bkMsg').textContent='Lese Einstellungen …';$('bkMsg').className='msg';
  try{
    const gets=FK.map(f=>'get '+f.k).concat(['region']);
    const withKey=$('bkKey').checked;
    if(withKey)gets.push('get prv.key');
    const rep=await runJob(gets,true);
    const cmds=[];
    FK.forEach((f,i)=>{const r=rep[i].reply;if(isErr(r))return;let v=val(r).replace(/%$/,'');
      if(f.t==='radio'){const p=v.split(',');v=[parseFloat(p[0]),parseFloat(p[1]),parseInt(p[2]),parseInt(p[3])].join(',')}
      cmds.push(setCmd(f,v))});
    cmds.push(...regionCmds(rep[FK.length].reply));
    if(withKey){const k=val(rep[FK.length+1].reply);if(/^[0-9a-fA-F]{64,}$/.test(k))cmds.push('set prv.key '+k)}
    const st=await (await fetch('/api/status',{cache:'no-store'})).json();
    const file={format:'meshcore-gateway-backup',version:1,created:new Date().toISOString(),
      node:st.node.name,pubkey:st.node.pubkey,meshcore:st.node.firmware,gateway:st.gateway.version,commands:cmds};
    const a=document.createElement('a');
    a.href=URL.createObjectURL(new Blob([JSON.stringify(file,null,2)],{type:'application/json'}));
    a.download='meshcore-gw-'+st.node.name.replace(/[^A-Za-z0-9_-]+/g,'_')+'-'+file.created.slice(0,10)+'.json';
    document.body.appendChild(a);a.click();a.remove();
    $('bkMsg').textContent=cmds.length+' Befehle gesichert'+(withKey?' (mit Schlüssel!)':'');$('bkMsg').className='msg ok';
  }catch(e){$('bkMsg').textContent='Backup fehlgeschlagen: '+e.message;$('bkMsg').className='msg bad'}
};
let bkCmds=null;
$('bkFile').onchange=async e=>{
  bkCmds=null;$('bkRestore').disabled=true;$('bkOut').hidden=true;
  const f=e.target.files[0];if(!f)return;
  try{
    const j=JSON.parse(await f.text());
    if(j.format!=='meshcore-gateway-backup'||!Array.isArray(j.commands))throw new Error('kein Gateway-Backup');
    const allowed=/^(set [a-z.]+ |region (put|denyf|home|save)\b)/;
    const bad=j.commands.filter(c=>typeof c!=='string'||!allowed.test(c));
    if(bad.length)throw new Error('unerwartete Befehle: '+bad.slice(0,3).join(', '));
    bkCmds=j.commands;
    $('bkOut').hidden=false;
    $('bkOut').textContent='Backup von '+j.node+' ('+j.created+')\n\n'+bkCmds.map(c=>c.startsWith('set prv.key ')?'set prv.key ••••••••':c).join('\n');
    $('bkRestore').disabled=false;$('bkMsg').textContent=bkCmds.length+' Befehle bereit';$('bkMsg').className='msg';
  }catch(err){$('bkMsg').textContent='Datei ungültig: '+err.message;$('bkMsg').className='msg bad'}
};
$('bkRestore').onclick=async()=>{
  if(!bkCmds)return;
  const key=bkCmds.some(c=>c.startsWith('set prv.key '));
  if(!confirm('Backup einspielen? Aktuelle Einstellungen werden überschrieben.'+(key?'\n\nACHTUNG: enthält einen privaten Schlüssel, die Knoten-Identität wird ersetzt.':'')))return;
  $('bkRestore').disabled=true;$('bkMsg').textContent='Spiele ein …';$('bkMsg').className='msg';
  try{
    const rep=await runJob(bkCmds,false);
    showResults($('bkOut'),rep.map(r=>r.cmd.startsWith('set prv.key ')?{cmd:'set prv.key ••••••••',reply:r.reply.replace(/[0-9a-f]{64}/i,'…')}:r));
    flagReboot(bkCmds);
    const bad=rep.filter(r=>isErr(r.reply)).length;
    $('bkMsg').textContent=bad?bad+' Befehl(e) abgelehnt':'Eingespielt';$('bkMsg').className='msg '+(bad?'bad':'ok');
    loadSettings();loadProfile();loadRegions();
  }catch(e){$('bkMsg').textContent='Fehler: '+e.message;$('bkMsg').className='msg bad'}
  $('bkRestore').disabled=false;
};

buildForm();
let settingsLoaded=false;
async function openSettings(){
  if(settingsLoaded)return;settingsLoaded=true;
  await loadSettings();await loadProfile();await loadRegions();
}

/* ---------- Takt ---------- */
let lastStatus=0;
function tick(){
  if(document.hidden)return;
  const now=Date.now();
  if(now-lastStatus>=5000){lastStatus=now;refresh()}
  if(view==='console')con.poll();
  if(view==='packets')pkt.poll();
}
setInterval(tick,1000);
show(location.hash.slice(1)||'status');
</script>
</body>
</html>
)HTML";
