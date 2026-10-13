#pragma once

// Statusseite + WLAN-Einrichtung (M1). Bewusst ohne externe Ressourcen,
// funktioniert auch im Installer-AP ohne Internet.
static const char INDEX_HTML[] = R"HTML(<!doctype html>
<html lang="de">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>MeshCore Gateway</title>
<style>
:root{--bg:#f4f5f7;--card:#fff;--fg:#1d2330;--muted:#677084;--line:#e1e4ea;--acc:#0b6bcb;--ok:#1a8f4a;--warn:#c27a00;--bad:#c0392b}
@media (prefers-color-scheme:dark){:root{--bg:#12151b;--card:#1b2029;--fg:#e6e9ef;--muted:#9aa3b5;--line:#2b3240;--acc:#5aa2f0;--ok:#43c47a;--warn:#e0a53a;--bad:#ef6a5b}}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--fg);font:15px/1.45 system-ui,-apple-system,Segoe UI,Roboto,sans-serif}
header{display:flex;align-items:center;gap:12px;padding:14px 16px;background:var(--card);border-bottom:1px solid var(--line)}
header h1{font-size:18px;margin:0;flex:1}
.pill{font-size:12px;padding:2px 8px;border-radius:99px;border:1px solid var(--line);color:var(--muted)}
main{max-width:1100px;margin:0 auto;padding:16px;display:grid;gap:16px;grid-template-columns:repeat(auto-fit,minmax(300px,1fr))}
section{background:var(--card);border:1px solid var(--line);border-radius:10px;padding:14px 16px}
section h2{font-size:14px;text-transform:uppercase;letter-spacing:.04em;color:var(--muted);margin:0 0 10px}
dl{display:grid;grid-template-columns:auto 1fr;gap:4px 14px;margin:0}
dt{color:var(--muted)}dd{margin:0;font-variant-numeric:tabular-nums;overflow-wrap:anywhere}
.mono{font-family:ui-monospace,Consolas,monospace;font-size:13px}
label{display:block;margin:10px 0 4px;color:var(--muted);font-size:13px}
input,select{width:100%;padding:8px 10px;border:1px solid var(--line);border-radius:6px;background:var(--bg);color:var(--fg);font:inherit}
.row{display:flex;gap:8px;margin-top:12px;flex-wrap:wrap}
button{padding:8px 14px;border:1px solid var(--acc);background:var(--acc);color:#fff;border-radius:6px;font:inherit;cursor:pointer}
button.sec{background:transparent;color:var(--acc)}
button:disabled{opacity:.5;cursor:default}
.msg{margin-top:10px;font-size:13px;min-height:1em}
.ok{color:var(--ok)}.warn{color:var(--warn)}.bad{color:var(--bad)}
.full{grid-column:1/-1}
</style>
</head>
<body>
<header><h1 id="title">MeshCore Gateway</h1><span class="pill" id="wifiPill">…</span></header>
<main>
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
<dt>Akku</dt><dd id="c_bat">–</dd>
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
<section class="full">
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
</main>
<script>
const $=id=>document.getElementById(id);
const set=(id,v)=>{$(id).textContent=(v===undefined||v===null||v==='')?'–':v};
function dur(s){const d=Math.floor(s/86400),h=Math.floor(s%86400/3600),m=Math.floor(s%3600/60);return(d?d+' d ':'')+h+' h '+m+' min'}
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
    set('c_bat',c.battery_mv?(c.battery_mv/1000).toFixed(2)+' V':'–');
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
$('rebootBtn').onclick=async()=>{await fetch('/api/reboot',{method:'POST'});$('wifiMsg').textContent='Neustart …';$('wifiMsg').className='msg warn'};
refresh();setInterval(refresh,5000);
</script>
</body>
</html>
)HTML";
