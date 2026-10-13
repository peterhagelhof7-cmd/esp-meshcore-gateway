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
</style>
</head>
<body>
<header><h1 id="title">MeshCore Gateway</h1><span class="pill" id="wifiPill">…</span></header>
<nav>
<a href="#status" data-v="status">Status</a>
<a href="#console" data-v="console">Konsole</a>
<a href="#packets" data-v="packets">Pakete</a>
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
