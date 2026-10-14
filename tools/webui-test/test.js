// UI-Smoke-Test der Gateway-Weboberfläche gegen mock_server.py
const puppeteer = require('puppeteer-core');
const out = process.argv[2] || '.';
const sleep = ms => new Promise(r => setTimeout(r, ms));
let fails = 0;
const check = (name, ok, info = '') => { console.log((ok ? 'PASS ' : 'FAIL ') + name + (info ? ' -> ' + info : '')); if (!ok) fails++; };

// CLI-Befehl direkt über die API (wie die Konsole), wartet auf den Job
async function runCli(page, cmd) {
  return page.evaluate(async c => {
    const r = await fetch('/api/cli', { method: 'POST', body: new URLSearchParams({ cmd: c }) });
    const j = await r.json();
    for (let i = 0; i < 50; i++) {
      const q = await (await fetch('/api/cli/job?id=' + j.id)).json();
      if (q.done) return q.replies[0].reply;
      await new Promise(res => setTimeout(res, 100));
    }
  }, cmd);
}

(async () => {
  const browser = await puppeteer.launch({ executablePath: 'C:/Program Files/Google/Chrome/Application/chrome.exe', headless: true });
  const page = await browser.newPage();
  const errors = [];
  page.on('pageerror', e => errors.push(e.message));
  // Bestätigungen annehmen, nur "erase" ablehnen (wird unten geprüft)
  page.on('dialog', d => {
    const m = d.message();
    console.log('DIALOG: ' + m.split('\n')[0]);
    if (m.includes('Löscht ALLE')) d.dismiss(); else d.accept();
  });
  await page.setViewport({ width: 1200, height: 900 });
  await page.goto('http://127.0.0.1:' + (process.env.MOCK_PORT || 8765) + '/', { waitUntil: 'networkidle0' });
  await sleep(1500);

  check('Status: Name gesetzt', (await page.$eval('#n_name', e => e.textContent)) === 'Heltec Repeater');
  check('Status: Laufzeit formatiert', (await page.$eval('#c_up', e => e.textContent)) === '1 h 2 min');
  check('Status: keine Akku-Zeile', !(await page.$('#c_bat')));
  const nbWait = async (fn, ms = 15000) => { const t = Date.now(); while (Date.now() - t < ms) { if (await fn()) return true; await sleep(250); } return false; };
  // Nachbarn (E34): Kennung, Alter, SNR in dB (Rohwert /4)
  const nbRows = () => page.$$eval('#nbBody tr', r => r.map(t => [...t.children].map(c => c.textContent)));
  check('Nachbarn: Tabelle aus "neighbors"', await nbWait(async () => JSON.stringify(await nbRows()) ===
        JSON.stringify([['AB12CD34', 'vor 2 min', '+12.25 dB'], ['EF56AB78', 'vor 1 h 6 min', '-3 dB']])), JSON.stringify(await nbRows()));
  const conBefore = await page.evaluate(async () => (await (await fetch('/api/console?since=0')).json()).lines.map(l => l.x).join('|'));
  check('Nachbarn: Abfrage nicht in der Konsole (quiet)', !conBefore.includes('neighbors'), conBefore.slice(-200));
  await page.click('#nbScan');
  check('Nachbarn: Suche gesendet, Countdown', await nbWait(async () => /Suche gesendet \(OK - Discover sent\).*noch \d+ s/.test(await page.$eval('#nbMsg', e => e.textContent))),
        await page.$eval('#nbMsg', e => e.textContent));
  check('Nachbarn: neuer Nachbar erscheint', await nbWait(async () => (await nbRows()).some(r => r[0] === '1F2E3D4C' && r[1] === 'vor 3 s' && r[2] === '+5 dB')), JSON.stringify(await nbRows()));
  check('Nachbarn: Button während der Suche gesperrt', await page.$eval('#nbScan', e => e.disabled));
  await page.screenshot({ path: out + '/01_status.png', fullPage: true });

  await page.click('nav a[data-v=console]');
  await sleep(1500);
  check('Konsole: Startmeldung sichtbar', (await page.$eval('#conOut', e => e.textContent)).includes('Gateway gestartet'));
  await page.type('#cliIn', 'neighbors');
  await page.keyboard.press('Enter');
  await sleep(1500);
  const con = await page.$eval('#conOut', e => e.innerText);
  check('Konsole: Befehl-Echo', con.includes('> neighbors'));
  check('Konsole: mehrzeilige Antwort', con.includes('AB12CD34') && con.includes('EF56AB78'));
  await page.click('#quick button');   // "ver"
  await sleep(1500);
  check('Konsole: Schnellbutton ver', (await page.$eval('#conOut', e => e.innerText)).includes('v1.17.1 (Build'));
  // Verlauf: Pfeil hoch holt letzten Befehl
  await page.focus('#cliIn');
  await page.keyboard.press('ArrowUp');
  check('Konsole: Verlauf ↑', (await page.$eval('#cliIn', e => e.value)) === 'ver', await page.$eval('#cliIn', e => e.value));
  await page.$eval('#cliIn', e => e.value = '');
  // Gefährlicher Befehl -> Bestätigungsdialog (abgelehnt), darf nicht gesendet werden
  await page.type('#cliIn', 'erase');
  await page.keyboard.press('Enter');
  await sleep(1200);
  check('Konsole: erase nach Ablehnung nicht gesendet', !(await page.$eval('#conOut', e => e.innerText)).includes('> erase'));
  await page.screenshot({ path: out + '/02_console.png' });

  await page.click('nav a[data-v=packets]');
  await sleep(2500);
  let n = await page.$$eval('#pktOut > div', d => d.length);
  check('Pakete: Zeilen vorhanden', n > 10, n + ' Zeilen');
  await page.type('#pktFilter', 'ADVERT');
  await sleep(300);
  const txt = await page.$$eval('#pktOut > div', d => d.map(x => x.innerText));
  check('Pakete: Filter ADVERT', txt.length > 0 && txt.every(t => t.includes('ADVERT')), txt.length + ' Zeilen');
  check('Pakete: RX/TX-Präfix', txt.some(t => t.includes('RX len=')) && txt.some(t => t.includes('TX len=')));
  await page.click('#pktRx');
  await sleep(300);
  const txOnly = await page.$$eval('#pktOut > div', d => d.map(x => x.innerText));
  check('Pakete: nur TX', txOnly.length > 0 && txOnly.every(t => t.includes('TX ')));
  await page.screenshot({ path: out + '/03_packets.png' });

  // ---------- Kanäle ----------
  {
    const t = async sel => page.$eval(sel, e => e.textContent);
    const until = async (fn, ms = 6000) => { const s = Date.now(); while (Date.now() - s < ms) { if (await fn()) return true; await sleep(150); } return false; };
    await page.click('nav a[data-v=channels]');
    check('Kanäle: leer + Senden gesperrt', await until(async () => !(await page.$eval('#chEmpty', e => e.hidden)) && await page.$eval('#msgText', e => e.disabled)));
    check('Kanäle: Scope mit rhein-main vorbelegt', (await page.$eval('#chScope', e => e.value)) === 'rhein-main');
    // Hashtag mit Großbuchstaben -> wird klein + # vorangestellt (ohne Scope)
    await page.$eval('#chScope', e => e.value = '');
    await page.select('#chKind', 'hashtag');
    await page.type('#chName', 'Test');
    await page.click('#chAdd');
    check('Kanäle: #test angelegt', await until(async () => (await page.$$eval('#chBody tr td:first-child', d => d.map(x => x.textContent))).includes('#test')));
    const testHash = await page.$$eval('#chBody tr', rows => rows.map(r => [...r.children].map(c => c.textContent)).find(r => r[0] === '#test')[3]);
    check('Kanäle: #test Hash d9 (= sha256-Ableitung)', testHash === 'd9', testHash);
    // Duplikat abgelehnt
    await page.type('#chName', '#test');
    await page.click('#chAdd');
    check('Kanäle: Duplikat abgelehnt', await until(async () => (await t('#chMsg')).includes('existiert schon')), await t('#chMsg'));
    await page.$eval('#chName', e => e.value = '');
    // Ungültiger Hashtag
    await page.type('#chName', '#Grüße');
    await page.click('#chAdd');
    check('Kanäle: Umlaut im Hashtag abgelehnt', await until(async () => (await t('#chMsg')).includes('nur a-z')), await t('#chMsg'));
    await page.$eval('#chName', e => e.value = '');
    // Privat mit Hex-Schlüssel + Scope
    await page.select('#chKind', 'private');
    check('Kanäle: Schlüsselfeld nur bei privat', await page.$eval('#chKeyBox', e => e.style.display !== 'none'));
    await page.type('#chName', 'sensormeter');
    await page.type('#chKey', '00112233445566778899aabbccddeeff');
    check('Kanäle: Scope nach Anlegen wieder vorbelegt', (await page.$eval('#chScope', e => e.value)) === 'rhein-main');
    await page.click('#chAdd');
    check('Kanäle: privat angelegt', await until(async () => (await page.$$eval('#chBody tr', r => r.length)) === 2));
    // Public
    await page.select('#chKind', 'public');
    await page.click('#chAdd');
    await until(async () => (await page.$$eval('#chBody tr', r => r.length)) === 3);
    const pubHash = await page.$$eval('#chBody tr', rows => rows.map(r => [...r.children].map(c => c.textContent)).find(r => r[0] === 'Public')[3]);
    check('Kanäle: Public Hash 11', pubHash === '11', pubHash);
    // Schlüssel des privaten Kanals anzeigen
    await page.$$eval('#chBody tr', rows => rows.find(r => r.children[0].textContent === 'sensormeter').querySelector('button.chip').click());
    const keyShown = await page.$$eval('#chBody tr', rows => rows.find(r => r.children[0].textContent === 'sensormeter').children[4].textContent);
    // Hex wie in der MeshCore-App, Base64 nur als Zusatz (Nutzer hielt Base64 für einen anderen Schlüssel)
    check('Kanäle: Schlüssel als Hex (wie App) + Base64', keyShown === '00112233445566778899aabbccddeeffBase64: ABEiM0RVZneImaq7zN3u/w==', keyShown);
    // Nachricht senden + Empfang
    await page.select('#msgCh', '1');
    check('Kanäle: Hinweis nennt Scope', (await t('#msgHint')).includes('rhein-main'), await t('#msgHint'));
    await page.type('#msgText', 'Hallo vom Gateway');
    await page.keyboard.press('Enter');
    check('Kanäle: gesendete Nachricht sichtbar', await until(async () => (await t('#msgOut')).includes('sensormeter  Heltec Repeater: Hallo vom Gateway')), (await t('#msgOut')).slice(-200));
    check('Kanäle: empfangene Nachricht mit SNR/Hops', await until(async () => /#test  Funker\d: Hallo Nr\. \d+  \(2 Hops, SNR 6\.5 dB\)/.test(await t('#msgOut'))));
    await page.screenshot({ path: out + '/08_channels.png', fullPage: true });
  }

  // ---------- Sensormeter ----------
  {
    const t = async sel => page.$eval(sel, e => e.textContent);
    const until = async (fn, ms = 8000) => { const s = Date.now(); while (Date.now() - s < ms) { if (await fn()) return true; await sleep(200); } return false; };
    const rows = sel => page.$$eval(sel + ' tr', r => r.map(x => [...x.children].map(c => c.textContent)));
    const setVal = (sel, v) => page.$eval(sel, (e, v) => { e.value = v; e.dispatchEvent(new Event('input')); e.dispatchEvent(new Event('change')); }, v);
    await page.click('nav a[data-v=sm]');
    check('SM: leer', await until(async () => !(await page.$eval('#smDevEmpty', e => e.hidden))));
    // Echtes Sensormeter im LAN (live, 2026-10-14 erreichbar)
    await setVal('#sdName', 'Büro');
    await setVal('#sdHost', '192.168.178.148');
    await page.click('#sdSave');
    check('SM: echtes Gerät online mit Temperatur', await until(async () => {
      const r = await rows('#smDevBody'); return r.length === 2 && r[0][2].startsWith('✓') && /Temperatur \d+(\.\d)?°C/.test(r[0][3]);
    }, 15000), JSON.stringify(await rows('#smDevBody')));
    // Simuliertes heißes Gerät mit Passwort
    await setVal('#sdName', 'Heizraum');
    await setVal('#sdHost', 'sim-hot');
    await setVal('#sdUser', 'admin');
    await setVal('#sdPass', 'geheim');
    await page.click('#sdSave');
    check('SM: zweites Gerät', await until(async () => (await rows('#smDevBody')).length === 3));
    // Ungültige Adresse
    await setVal('#sdName', 'Kaputt');
    await setVal('#sdHost', 'bad host');
    await page.click('#sdSave');
    check('SM: ungültige Adresse abgelehnt', await until(async () => (await t('#sdMsg')).includes('Adresse ungültig')), await t('#sdMsg'));
    await page.click('#sdCancel').catch(() => {});
    await setVal('#sdName', ''); await setVal('#sdHost', '');
    // Passwort maskiert + bleibt beim Bearbeiten erhalten
    const cfg1 = await page.evaluate(async () => (await (await fetch('/api/sm')).json()).config);
    check('SM: Passwort maskiert', cfg1.devices[1].pass === '***');
    // Regel: Heizraum Temperatur > 30 -> ALARM in Kanal "sensormeter" (idx 1)
    await page.select('#srDev', '1');
    await until(async () => (await page.$$eval('#srPath option', o => o.map(x => x.value))).includes('sensor1.temperature'));
    await page.select('#srPath', 'sensor1.temperature');
    await setVal('#srThr', '30');
    await page.select('#srCh', 'sensormeter');
    await page.click('#srSave');
    check('SM: Regel ALARM', await until(async () => { const r = await rows('#smRuleBody'); return r.length === 1 && r[0][3].startsWith('ALARM'); }), JSON.stringify(await rows('#smRuleBody')));
    check('SM: Meldung im Log', await until(async () => (await t('#smLog')).includes('sensormeter -> ALARM Heizraum: Temperatur 35°C (>30°C)')), (await t('#smLog')).slice(-160));
    // Regel 2: Büro > 50 -> normal, dann Test-Button
    await page.select('#srDev', '0');
    await until(async () => (await page.$$eval('#srPath option', o => o.length)) > 0);
    await page.select('#srPath', 'sensor1.temperature');
    await setVal('#srThr', '50');
    await setVal('#srText', 'Büro zu warm: {value}{unit}');
    await page.click('#srSave');
    check('SM: Regel 2 normal', await until(async () => { const r = await rows('#smRuleBody'); return r.length === 2 && r[1][3].startsWith('normal'); }), JSON.stringify(await rows('#smRuleBody')));
    await page.$$eval('#smRuleBody tr', r => r[1].querySelector('button').click());   // Test
    check('SM: Test-Button ohne Kanal -> Fehler', await until(async () => (await t('#srMsg')).includes('kein gültiger Kanal')), await t('#srMsg'));
    // Standardkanal setzen -> Test geht
    await page.select('#ssCh', '#test');
    check('SM: Einstellung als ungespeichert markiert', (await t('#ssMsg')) === 'ungespeichert');
    await sleep(3500);   // ein Aktualisierungszyklus darf die Auswahl nicht zurücksetzen
    check('SM: ungespeicherte Auswahl bleibt', (await page.$eval('#ssCh', e => e.value)) === '#test');
    await page.click('#ssSave');
    await until(async () => (await t('#ssMsg')) === 'Gespeichert');
    await until(async () => (await rows('#smRuleBody')).length === 2);
    await page.$$eval('#smRuleBody tr', r => r[1].querySelector('button').click());
    const alarmCount = s => (s.match(/ALARM Heizraum/g) || []).length;
    check('SM: Speichern sendet anstehenden Alarm NICHT erneut', alarmCount(await t('#smLog')) === 1, String(alarmCount(await t('#smLog'))));
    check('SM: Log nennt behaltene Zustände', (await t('#smLog')).includes('Regeln mit Zustand behalten'));
    check('SM: Test mit eigenem Text -> TEST-Präfix', await until(async () => /#test -> TEST Büro zu warm: \d+(\.\d)?°C/.test(await t('#smLog'))), (await t('#smLog')).slice(-160));
    // Gerät bearbeiten ohne Passwortänderung -> Passwort bleibt
    await page.$$eval('#smDevBody tr', r => r[1].querySelectorAll('button')[1].click());
    check('SM: Bearbeiten-Formular', (await page.$eval('#sdPass', e => e.value)) === '***');
    await setVal('#sdInt', '30');
    await page.click('#sdSave');
    await until(async () => (await t('#sdMsg')) === 'Gerät gespeichert');
    const kept = await page.evaluate(async () => { const r = await fetch('/api/sm'); return (await r.json()).config.devices[1]; });
    check('SM: Intervall geändert, Passwort weiter gesetzt', kept.intervalS === 30 && kept.pass === '***', JSON.stringify(kept));
    // Pseudo-Gerät „Gateway (intern)“ (E30): feste letzte Zeile, Regel auf Chiptemperatur
    const devRows = await rows('#smDevBody');
    check('SM: Gateway-intern-Zeile mit Chiptemperatur', devRows.length === 3 && devRows[2][0] === 'Gateway (intern)' &&
          /Chiptemperatur 52\.3°C/.test(devRows[2][3]), JSON.stringify(devRows[2]));
    check('Status: Chiptemperatur angezeigt', (await t('#g_chip')) === '52.3 °C', await t('#g_chip'));
    await page.select('#srDev', '100');
    await until(async () => (await page.$$eval('#srPath option', o => o.map(x => x.value))).includes('gateway.chipTemp'));
    await page.select('#srPath', 'gateway.chipTemp');
    await setVal('#srThr', '40');
    await setVal('#srText', '');
    await page.select('#srCh', 'sensormeter');
    await page.click('#srSave');
    check('SM: Regel auf Gateway intern -> ALARM', await until(async () => { const r = await rows('#smRuleBody'); return r.length === 3 && r[2][0].startsWith('Gateway (intern)') && r[2][3].startsWith('ALARM'); }),
          JSON.stringify(await rows('#smRuleBody')));
    check('SM: Meldung Chiptemperatur im Log', await until(async () => (await t('#smLog')).includes('sensormeter -> ALARM Gateway: Chiptemperatur 52.3°C (>40°C)')), (await t('#smLog')).slice(-160));
    await page.screenshot({ path: out + '/10_sensormeter.png', fullPage: true });
    // Gerät 0 löschen -> Regel 2 (Büro) weg, Regel 1 zeigt jetzt auf Gerät 0
    await page.$$eval('#smDevBody tr', r => r[0].querySelectorAll('button')[2].click());
    check('SM: Gerät gelöscht + Regel umnummeriert', await until(async () => {
      const c = await page.evaluate(async () => (await (await fetch('/api/sm')).json()).config);
      return c.devices.length === 1 && c.rules.length === 2 && c.rules[0].device === 0 && c.rules[1].device === 100 && c.devices[0].name === 'Heizraum';
    }));

    // Kanal-Zuordnung per Name (E24): Kanal löschen -> Regel zeigt "fehlt",
    // Kanal (auch an anderer Platznummer) neu anlegen -> Regel funktioniert wieder.
    await page.click('nav a[data-v=channels]');
    await until(async () => (await page.$$eval('#chBody tr', r => r.length)) === 3);
    await page.$$eval('#chBody tr', rows => rows.find(r => r.children[0].textContent === 'sensormeter').querySelectorAll('button.chip')[1].click());
    await until(async () => (await page.$$eval('#chBody tr', r => r.length)) === 2);
    await page.click('nav a[data-v=sm]');
    check('SM: gelöschter Kanal als "fehlt" markiert', await until(async () => (await rows('#smRuleBody'))[0][2] === 'sensormeter (fehlt!)'), JSON.stringify(await rows('#smRuleBody')));
    await page.$$eval('#smRuleBody tr', r => r[0].querySelector('button').click());   // Test
    check('SM: Test ohne Kanal -> Fehler', await until(async () => (await t('#srMsg')).includes('kein gültiger Kanal')), await t('#srMsg'));
    await page.click('nav a[data-v=channels]');
    await page.select('#chKind', 'hashtag');
    await page.$eval('#chName', e => e.value = 'platzhalter');   // belegt den alten Platz
    await page.click('#chAdd');
    await until(async () => (await page.$$eval('#chBody tr', r => r.length)) === 3);
    await page.select('#chKind', 'private');
    await page.$eval('#chName', e => e.value = 'sensormeter');
    await page.$eval('#chKey', e => e.value = '00112233445566778899aabbccddeeff');
    await page.click('#chAdd');
    await until(async () => (await page.$$eval('#chBody tr', r => r.length)) === 4);
    await page.click('nav a[data-v=sm]');
    check('SM: Regel findet neu angelegten Kanal per Name', await until(async () => (await rows('#smRuleBody'))[0][2] === 'sensormeter'), JSON.stringify(await rows('#smRuleBody')));
    await page.$$eval('#smRuleBody tr', r => r[0].querySelector('button').click());
    check('SM: Test geht wieder in "sensormeter" (nicht in den Platzhalter)', await until(async () => {
      const log = await t('#smLog'); return log.includes('sensormeter -> TEST Heizraum') && !log.includes('#platzhalter ->');
    }), (await t('#smLog')).slice(-160));
    // Platzhalter wieder entfernen (Backup-Test erwartet 3 Kanäle)
    await page.click('nav a[data-v=channels]');
    await page.$$eval('#chBody tr', rows => rows.find(r => r.children[0].textContent === '#platzhalter').querySelectorAll('button.chip')[0].click());
    await until(async () => (await page.$$eval('#chBody tr', r => r.length)) === 3);

    // ---------- Ping-Check (E25), Alarmkanal = #test ----------
    await page.click('nav a[data-v=sm]');
    check('Tab heißt Überwachung', (await page.$eval('nav a[data-v=sm]', e => e.textContent)) === 'Überwachung');
    check('Ping: leer', await until(async () => !(await page.$eval('#pgEmpty', e => e.hidden))));
    await setVal('#pgHost', '192.168.178.148');
    await page.click('#pgSave');
    check('Ping: Bezeichner Pflicht', (await t('#pgMsg')).includes('Bezeichner'), await t('#pgMsg'));
    await setVal('#pgName', 'Sensormeter Büro');
    await setVal('#pgThr', '5');
    await page.click('#pgSave');
    check('Ping: Schwellwert < 10 s abgelehnt', await until(async () => (await t('#pgMsg')).includes('Schwellwert')), await t('#pgMsg'));
    await setVal('#pgThr', '60');
    await setVal('#pgIv', '5');
    await page.$eval('#pgMsg', e => e.textContent = '');
    await page.click('#pgSave');
    check('Ping: Prüfabstand < 10 s abgelehnt', await until(async () => (await t('#pgMsg')).includes('Prüfabstand')), await t('#pgMsg'));
    await setVal('#pgIv', '90');
    await page.$eval('#pgMsg', e => e.textContent = '');
    await page.click('#pgSave');
    check('Ping: Schwellwert < Prüfabstand abgelehnt', await until(async () => (await t('#pgMsg')).includes('mind. Prüfabstand')), await t('#pgMsg'));
    await setVal('#pgIv', '30');
    await page.click('#pgSave');
    check('Ping: Prüfabstand in Tabelle', await until(async () => ((await rows('#pgBody'))[0]?.[2] || '') === '60 s / alle 30 s'), JSON.stringify(await rows('#pgBody')));
    check('Ping: Formular nach Speichern zurück auf 10 s', (await page.$eval('#pgIv', e => e.value)) === '10');
    check('Ping: erreichbares Ziel ✓ mit ms', await until(async () => /✓ \d+ ms/.test((await rows('#pgBody'))[0]?.[3] || ''), 15000), JSON.stringify(await rows('#pgBody')));
    await page.$$eval('#pgBody tr', r => r[0].querySelectorAll('button')[0].click());
    check('Ping: Bearbeiten lädt Prüfabstand', (await page.$eval('#pgIv', e => e.value)) === '30');
    await page.click('#pgCancel');
    await setVal('#pgName', 'Testziel');
    await setVal('#pgHost', '192.0.2.1');   // TEST-NET-1, antwortet garantiert nie
    await setVal('#pgThr', '10');
    await page.click('#pgSave');
    check('Ping: unerreichbares Ziel -> ALARM nach Schwellwert', await until(async () => ((await rows('#pgBody'))[1]?.[3] || '').startsWith('ALARM'), 35000), JSON.stringify(await rows('#pgBody')));
    check('Ping: Meldung im Alarmkanal', await until(async () => /#test -> PING-ALARM Testziel \(192\.0\.2\.1\) seit 1\d s ohne Antwort/.test(await t('#smLog'))), (await t('#smLog')).slice(-200));
    for (const n of ['Z3', 'Z4', 'Z5']) { await setVal('#pgName', n); await setVal('#pgHost', '192.0.2.' + n[1]); await setVal('#pgThr', '600'); await page.$eval('#pgMsg', e => e.textContent = ''); await page.click('#pgSave'); await until(async () => (await t('#pgMsg')) === 'Ping-Ziel angelegt'); }
    await setVal('#pgName', 'Z6'); await setVal('#pgHost', '192.0.2.6');
    await page.click('#pgSave');
    check('Ping: max. 5 Ziele', (await t('#pgMsg')) === 'Maximal 5 Ping-Ziele', (await t('#pgMsg')) + ' | ' + JSON.stringify((await rows('#pgBody')).map(r => r[0])));
    await page.click('#pgCancel').catch(() => {});
    for (let k = 0; k < 3; k++) {   // Z3..Z5 wieder löschen
      const before = (await rows('#pgBody')).length;
      await page.$$eval('#pgBody tr', r => r[r.length - 1].querySelectorAll('button')[1].click());
      await until(async () => (await rows('#pgBody')).length === before - 1);
    }
    check('Ping: zurück auf 2 Ziele', (await rows('#pgBody')).length === 2);
    await page.screenshot({ path: out + '/11_ping.png', fullPage: true });
  }

  // ---------- Einstellungen ----------
  const fs = require('fs'), path = require('path');
  const dl = path.resolve(out, 'downloads');
  fs.mkdirSync(dl, { recursive: true });
  fs.readdirSync(dl).forEach(f => fs.unlinkSync(path.join(dl, f)));
  const cdp = await page.createCDPSession();
  await cdp.send('Page.setDownloadBehavior', { behavior: 'allow', downloadPath: dl });
  const waitFor = async (fn, ms = 8000) => { const t = Date.now(); while (Date.now() - t < ms) { if (await fn()) return true; await sleep(150); } return false; };
  const text = sel => page.$eval(sel, e => e.textContent);

  await page.click('nav a[data-v=settings]');
  check('Einst.: Profil geladen', await waitFor(async () => (await text('#profMsg')).includes('Abweichung')), await text('#profMsg'));
  const rows = await page.$$eval('#profBody tr', r => r.length);
  check('Einst.: Profil-Tabelle ohne Regionen', rows === 19 && !(await text('#profBody')).includes('region'), rows + ' Zeilen');
  const crBefore = await page.$eval('#f_rc', e => e.value);
  check('Einst.: CR vorher 5 (Upstream-Default)', crBefore === '5', crBefore);
  check('Einst.: fem.txgain als nicht verfügbar markiert', await page.$eval('#fb_radio\\.fem\\.txgain', e => e.classList.contains('na')));
  check('Einst.: Regionen leer', await waitFor(async () => (await text('#regOut')).trim() === '* F'), await text('#regOut'));
  check('Einst.: Warnung „Keine Region angelegt“', await waitFor(async () => !(await page.$eval('#regWarn', e => e.hidden))));
  await page.screenshot({ path: out + '/05_settings_before.png', fullPage: true });

  await page.click('#profApply');
  check('Einst.: Profil übernommen', await waitFor(async () => (await text('#profMsg')) === 'Alles wie empfohlen'), await text('#profMsg'));
  await waitFor(async () => (await page.$eval('#f_rc', e => e.value)) === '8');
  check('Einst.: CR jetzt 8', (await page.$eval('#f_rc', e => e.value)) === '8');
  check('Einst.: dutycycle 10', (await page.$eval('#f_dutycycle', e => e.value)) === '10.0', await page.$eval('#f_dutycycle', e => e.value));
  check('Einst.: Neustart-Banner sichtbar', await page.$eval('#rebootBanner', e => e.classList.contains('on')));
  // Profil legt keine Regionen an (E36), Warnung bleibt bis eigene Regionen da sind
  await sleep(500);
  check('Einst.: Profil legt keine Regionen an', (await text('#regOut')).trim() === '* F' && !(await page.$eval('#regWarn', e => e.hidden)), await text('#regOut'));
  for (const n of ['de-by', 'rhein-main']) {
    await page.select('#regAct', 'put');
    await page.$eval('#regName', (e, v) => { e.value = v; }, n);
    await page.click('#regRun');
    await waitFor(async () => (await text('#regOut')).includes(n + ' F'));
  }
  await page.click('#regSave');
  await waitFor(async () => (await text('#regMsg')).length > 1 && (await text('#regMsg')) !== '…');
  const reg = await text('#regOut');
  check('Einst.: Regionen von Hand angelegt', ['de-by F', 'rhein-main F'].every(r => reg.includes(r)), JSON.stringify(reg));
  check('Einst.: Warnung weg', await waitFor(async () => await page.$eval('#regWarn', e => e.hidden)));
  check('Einst.: Profil weiter „Alles wie empfohlen“ (Regionen zählen nicht)', await waitFor(async () => (await text('#profMsg')) === 'Alles wie empfohlen'), await text('#profMsg'));

  // Feld ändern -> nur dieser set-Befehl, danach 1 Abweichung im Profil
  await page.$eval('#f_txdelay', e => { e.value = '1.5'; e.dispatchEvent(new Event('input')); });
  check('Einst.: Änderung markiert', await page.$eval('#fb_txdelay', e => e.classList.contains('chg')));
  await page.click('#setSave');
  check('Einst.: gespeichert', await waitFor(async () => (await text('#setMsg')) === 'Gespeichert'), await text('#setMsg'));
  check('Einst.: nur ein set-Befehl', (await text('#setOut')).trim().split('\n').length === 1, await text('#setOut'));
  check('Einst.: Profil meldet Abweichung', await waitFor(async () => (await text('#profMsg')) === '1 Abweichung(en)'), await text('#profMsg'));
  // Ungültiger Wert -> CLI lehnt ab, UI zeigt Fehler
  await page.$eval('#f_advert\\.interval', e => { e.value = '30'; e.dispatchEvent(new Event('input')); });
  await page.click('#setSave');
  check('Einst.: Fehler sichtbar', await waitFor(async () => (await text('#setMsg')).includes('abgelehnt')), await text('#setMsg'));
  check('Einst.: Wert nicht übernommen', await waitFor(async () => (await page.$eval('#f_advert\\.interval', e => e.value)) === '240'));

  // Backup mit Schlüssel
  await page.click('#bkKey');
  await page.click('#bkDown');
  check('Backup: Datei geschrieben', await waitFor(async () => fs.readdirSync(dl).some(f => f.endsWith('.json'))));
  const bkFile = path.join(dl, fs.readdirSync(dl).find(f => f.endsWith('.json')));
  const bk = JSON.parse(fs.readFileSync(bkFile, 'utf8'));
  check('Backup: Inhalt', bk.commands.includes('set tx 17') && bk.commands.includes('set radio 869.618,62.5,8,8') &&
        bk.commands.includes('region put rhein-main') && bk.commands.some(c => c.startsWith('set prv.key ')), bk.commands.length + ' Befehle');
  check('Backup: unsupported fem.txgain ausgelassen', !bk.commands.some(c => c.includes('fem.txgain')));
  check('Backup: Sensormeter-Konfig enthalten (per Kanalname, mit Passwort)', !!bk.sensormeter && bk.sensormeter.devices.length === 1 &&
        bk.sensormeter.rules[0].channel === 'sensormeter' && bk.sensormeter.settings.defaultChannel === '#test' && bk.sensormeter.devices[0].pass === 'geheim',
        JSON.stringify(bk.sensormeter && bk.sensormeter.devices));
  check('Backup (voll): Admin-, Gast-, WLAN-Passwort', bk.version === 2 && bk.secrets === true && bk.adminPassword === 'password' &&
        bk.commands.some(c => c.startsWith('set guest.password')) && bk.gw && bk.gw.wifi.ssid === 'TestNetz' && bk.gw.wifi.pass === 'geheim123' &&
        bk.gw.hostname === '', JSON.stringify(bk.gw));
  check('Backup: ACL als setperm', bk.commands.includes('setperm c0ffee' + '11'.repeat(29) + ' 3'), bk.commands.filter(c => c.startsWith('setperm')).join());
  // Backup ohne Haken: keine Geheimnisse
  fs.renameSync(path.join(dl, fs.readdirSync(dl).find(f => f.endsWith('.json'))), path.join(dl, 'voll.json.bak'));
  await page.click('#bkKey');
  await page.click('#bkDown');
  check('Backup (ohne Geheimnisse): Datei', await waitFor(async () => fs.readdirSync(dl).some(f => f.endsWith('.json'))));
  const bkLite = JSON.parse(fs.readFileSync(path.join(dl, fs.readdirSync(dl).find(f => f.endsWith('.json'))), 'utf8'));
  check('Backup (ohne Geheimnisse): nichts Geheimes', bkLite.secrets === false && !('adminPassword' in bkLite) && !('pass' in bkLite.gw.wifi) &&
        !bkLite.commands.some(c => c.startsWith('set prv.key') || c.startsWith('set guest.password')) && bkLite.sensormeter.devices[0].pass === '' &&
        bkLite.commands.some(c => c.startsWith('setperm ')) && bkLite.gw.wifi.ssid === 'TestNetz', JSON.stringify(bkLite.gw));
  fs.unlinkSync(path.join(dl, fs.readdirSync(dl).find(f => f.endsWith('.json'))));
  fs.renameSync(path.join(dl, 'voll.json.bak'), bkFile);
  check('Backup: Ping-Ziele enthalten', !!bk.sensormeter && Array.isArray(bk.sensormeter.pings) && bk.sensormeter.pings.length === 2 &&
        bk.sensormeter.pings[1].host === '192.0.2.1' && bk.sensormeter.pings[1].thresholdS === 10 &&
        bk.sensormeter.pings[0].intervalS === 30 && bk.sensormeter.pings[1].intervalS === 10, JSON.stringify(bk.sensormeter && bk.sensormeter.pings));
  check('Backup: Kanäle enthalten', Array.isArray(bk.channels) && bk.channels.length === 3 && bk.channels.some(c => c.name === 'sensormeter' && c.key === 'ABEiM0RVZneImaq7zN3u/w==' && c.scope === 'rhein-main'), JSON.stringify(bk.channels));

  // Zustand verstellen, dann Backup einspielen
  await runCli(page, 'set tx 5');
  await runCli(page, 'region remove de-by');
  const input = await page.$('#bkFile');
  await input.uploadFile(bkFile);
  await waitFor(async () => !(await page.$eval('#bkRestore', e => e.disabled)));
  check('Restore: Schlüssel in Vorschau maskiert', (await text('#bkOut')).includes('set prv.key ••••••••') && !(await text('#bkOut')).includes('abab'));
  check('Restore: Optionen Admin-Passwort + WLAN sichtbar', await page.$eval('#bkOpts', e => !e.hidden) &&
        await page.$eval('#bkAdmL', e => !e.hidden) && await page.$eval('#bkWifiL', e => !e.hidden) && !(await page.$eval('#bkWifi', e => e.checked || e.disabled)));
  check('Restore: Vorschau nennt ACL und WLAN', (await text('#bkOut')).includes('Zugriffsliste: 1 Einträge') && (await text('#bkOut')).includes('WLAN „TestNetz“ (mit Passwort)'), await text('#bkOut'));
  await page.click('#bkAdm');   // erster Durchlauf ohne Passwort/WLAN
  await page.click('#bkRestore');
  check('Restore: eingespielt', await waitFor(async () => (await text('#bkMsg')) === 'Eingespielt', 15000), await text('#bkMsg'));
  check('Restore: Auswahl danach zurückgesetzt', await page.$eval('#bkRestore', e => e.disabled) && (await page.$eval('#bkFile', e => e.value)) === '');
  check('Restore: tx wieder 17', await waitFor(async () => (await page.$eval('#f_tx', e => e.value)) === '17'));
  check('Restore: Region wieder da', await waitFor(async () => (await text('#regOut')).includes('de-by F')));
  // Zweiter Durchlauf: Admin-Passwort + WLAN aus dem Backup zurückholen
  await page.evaluate(() => fetch('/api/backup/apply', { method: 'POST', body: new URLSearchParams({ ssid: 'AnderesNetz', pass: 'x', hostname: 'anders' }) }));
  await runCli(page, 'password temp1');
  await page.$eval('#bkMsg', e => e.textContent = '');
  await input.uploadFile(bkFile);
  await waitFor(async () => (await text('#bkMsg')).includes('bereit'));
  check('Restore: WLAN-Option trotz anderer SSID wählbar (Passwort im Backup)', !(await page.$eval('#bkWifi', e => e.disabled)));
  await page.click('#bkWifi');
  await page.click('#bkRestore');
  check('Restore: Admin-Passwort + WLAN gemeldet', await waitFor(async () => (await text('#bkMsg')).includes('Admin-Passwort übernommen') && (await text('#bkMsg')).includes('WLAN übernommen'), 15000), await text('#bkMsg'));
  const ex2 = await page.evaluate(async () => (await fetch('/api/backup/extra?secrets=1')).json());
  check('Restore: WLAN/Hostname/Admin-Passwort wieder wie im Backup', await waitFor(async () => {
    const e = await page.evaluate(async () => (await fetch('/api/backup/extra?secrets=1')).json());
    return e.wifi.ssid === 'TestNetz' && e.wifi.pass === 'geheim123' && e.hostname === '' && e.adminPassword === 'password';
  }), JSON.stringify(ex2));
  // Backup ohne WLAN-Passwort und andere SSID -> Option gesperrt
  await page.evaluate(() => fetch('/api/backup/apply', { method: 'POST', body: new URLSearchParams({ ssid: 'AnderesNetz', pass: 'x' }) }));
  const lite = path.join(dl, 'lite.json');
  fs.writeFileSync(lite, JSON.stringify(Object.assign({}, bkLite, { commands: ['set tx 17'] })));
  await page.$eval('#bkMsg', e => e.textContent = '');
  await input.uploadFile(lite);
  await waitFor(async () => (await text('#bkMsg')).includes('bereit'));
  check('Restore: WLAN ohne Passwort bei anderer SSID gesperrt', await page.$eval('#bkWifi', e => e.disabled) && await page.$eval('#bkAdmL', e => e.hidden));
  await page.evaluate(() => fetch('/api/backup/apply', { method: 'POST', body: new URLSearchParams({ ssid: 'TestNetz', pass: 'geheim123' }) }));
  const apiErr = await page.evaluate(async () => { const r = await fetch('/api/backup/apply', { method: 'POST', body: new URLSearchParams({ ssid: 'Fremd' }) }); return r.status + ' ' + (await r.json()).error; });
  check('Restore-API: fremde SSID ohne Passwort abgelehnt', apiErr === '400 WLAN-Passwort fehlt im Backup', apiErr);

  // Passwort ändern -> in Konsole maskiert
  await page.type('#admPass', 'geheim123');
  await page.click('#admSave');
  check('Passwort: Antwort maskiert', await waitFor(async () => (await text('#admMsg')).includes('********')), await text('#admMsg'));
  await page.screenshot({ path: out + '/06_settings_after.png', fullPage: true });
  await page.click('nav a[data-v=console]');
  await sleep(1500);
  const conTxt = await page.$eval('#conOut', e => e.innerText);
  check('Passwort: nicht im Konsolen-Log', !conTxt.includes('geheim123') && conTxt.includes('password ********'));
  check('Restore: Schlüssel nicht im Konsolen-Log', !conTxt.includes('abababab'));

  await page.setViewport({ width: 390, height: 844 });
  await page.click('nav a[data-v=console]');
  await sleep(500);
  const overflow = await page.evaluate(() => document.documentElement.scrollWidth > window.innerWidth);
  check('Mobil: kein horizontales Scrollen', !overflow);
  await page.screenshot({ path: out + '/04_mobile.png' });
  await page.click('nav a[data-v=settings]');
  await sleep(800);
  const overflow2 = await page.evaluate(() => document.documentElement.scrollWidth > window.innerWidth);
  check('Mobil: Einstellungen ohne horizontales Scrollen', !overflow2);
  await page.screenshot({ path: out + '/07_settings_mobile.png', fullPage: true });

  // ---------- Firmware-Update (System) ----------
  {
    await page.setViewport({ width: 1200, height: 900 });
    const t = async sel => page.$eval(sel, e => e.textContent);
    const until = async (fn, ms = 8000) => { const s = Date.now(); while (Date.now() - s < ms) { if (await fn()) return true; await sleep(150); } return false; };
    const root = path.resolve(__dirname, '..', '..');
    const binV4 = path.join(root, '.pio/build/gateway_heltec_v4/firmware.bin');
    const binR8 = path.join(root, '.pio/build/gateway_heltec_v4_r8/firmware.bin');
    for (const b of [binV4, binR8]) {
      if (!fs.existsSync(b)) throw new Error('Vorbedingung fehlt: ' + b + ' (beide Envs bauen: tools\\host-test.ps1 -Build)');
    }
    const txt = path.join(dl, 'keine-firmware.txt');
    fs.writeFileSync(txt, 'Hallo, ich bin keine Firmware');
    const old = path.join(dl, 'alt.bin');
    fs.writeFileSync(old, Buffer.concat([Buffer.alloc(5000), Buffer.from('MCGW-FW-ID:MESHCORE-GW-HELTEC-V4:0.0.1:MCGW-FW-END'), Buffer.alloc(5000)]));

    // ---------- Hostname folgt Gerätename (E32) ----------
    await page.click('nav a[data-v=wifi]');
    const st = async () => (await page.evaluate(async () => (await fetch('/api/status')).json())).wifi;
    const devName = (await page.evaluate(async () => (await fetch('/api/status')).json())).node.name;
    const expHost = devName.toLowerCase().replace(/[ _]+/g, '-').replace(/[^a-z0-9-]/g, '');
    check('Hostname: automatisch = Gerätename', await until(async () => {
      const w = await st(); return w.hostname_auto === true && w.hostname === expHost; }), JSON.stringify(await st()) + ' / ' + devName);
    check('Hostname: Haken gesetzt, Feld gesperrt mit wirksamem Namen', await until(async () => page.evaluate(n =>
      hostAuto.checked && hostname.disabled && hostname.value === n, expHost)), await page.$eval('#hostname', e => e.value));
    check('Hostname: Status zeigt „(wie Gerätename)“', await until(async () => (await t('#w_host')) === expHost + ' (wie Gerätename)'), await t('#w_host'));
    await runCli(page, 'set name Werk Statt_GW2');
    check('Hostname: folgt neuem Gerätenamen', await until(async () => (await st()).hostname === 'werk-statt-gw2'), (await st()).hostname);
    await runCli(page, 'set name ' + devName);
    await page.$eval('#ssid', e => e.value = 'TestNetz');
    await page.click('#hostAuto');
    check('Hostname: Haken weg -> Feld frei', !(await page.$eval('#hostname', e => e.disabled)));
    await page.$eval('#hostname', e => e.value = '');
    await page.type('#hostname', 'Eigener Name');
    await page.$eval('#wifiForm', f => f.requestSubmit());
    check('Hostname: eigener Name gespeichert (bereinigt)', await until(async () => {
      const w = await st(); return w.hostname === 'eigener-name' && w.hostname_auto === false; }), JSON.stringify(await st()));
    await page.click('#hostAuto');
    await page.$eval('#wifiForm', f => f.requestSubmit());
    check('Hostname: Haken wieder an -> automatisch', await until(async () => (await st()).hostname_auto === true), JSON.stringify(await st()));
    await page.evaluate(() => fetch('/api/wifi', { method: 'POST', body: new URLSearchParams({ ssid: 'TestNetz', hostname: 'meshcore-gw' }) }));
    check('Hostname: alter Werkswert meshcore-gw = automatisch', await until(async () => (await st()).hostname_auto === true), JSON.stringify(await st()));
    await page.reload({ waitUntil: 'networkidle0' });

    await page.click('nav a[data-v=wifi]');
    check('OTA: Board angezeigt', await until(async () => (await t('#fw_proj')) === 'MESHCORE-GW-HELTEC-V4'), await t('#fw_proj'));
    const upload = async (file, force) => {
      await page.$eval('#otaForce', (e, f) => { e.checked = f; }, force);
      const inp = await page.$('#otaFile');
      await inp.uploadFile(file);
      await page.$eval('#otaFile', e => e.dispatchEvent(new Event('change')));
      await page.click('#otaBtn');
      await until(async () => /Abgelehnt|Installiert/.test(await t('#otaMsg')), 20000);
      return t('#otaMsg');
    };
    let m = await upload(txt, false);
    check('OTA: Textdatei abgelehnt', m.includes('kein Gateway-Firmware-Merkmal'), m);
    m = await upload(binR8, false);
    check('OTA: R8-Firmware auf V4 abgelehnt', m.includes('falsche Firmware: MESHCORE-GW-HELTEC-V4-R8'), m);
    m = await upload(old, false);
    check('OTA: Downgrade abgelehnt', m.includes('Downgrade') && m.includes('0.0.1'), m);
    m = await upload(old, true);
    check('OTA: Downgrade mit Erzwingen', m.includes('Installiert (0.0.1)'), m);
    await page.reload({ waitUntil: 'networkidle0' });   // Reload-Timer der Seite verwerfen
    await page.click('nav a[data-v=wifi]');
    m = await upload(binV4, false);
    check('OTA: echte V4-Firmware (gleiche Version) angenommen', /Installiert \(\d+\.\d+\.\d+\)/.test(m), m);
    await page.screenshot({ path: out + '/09_system_ota.png', fullPage: true });

    // ---------- Ereignis-Log (E27) ----------
    await page.reload({ waitUntil: 'networkidle0' });
    await page.click('nav a[data-v=wifi]');
    check('Log: Größe angezeigt', await until(async () => / KB von 128 KB$/.test(await t('#lg_size'))), await t('#lg_size'));
    check('Log: Download-Link', (await page.$eval('#lgDown', e => e.getAttribute('href'))) === '/api/log/download');
    const lg = await page.evaluate(async () => { const r = await fetch('/api/log/download'); return { cd: r.headers.get('Content-Disposition'), body: await r.text() }; });
    const hn = (await page.evaluate(async () => (await fetch('/api/status')).json())).wifi.hostname;
    check('Log: als Datei mit Hostname', lg.cd === 'attachment; filename="meshcore-gw-log-' + hn + '.txt"', lg.cd);
    check('Log: enthält Befehle, Ping-Alarm, Systemmeldung', /^\d{4}-\d\d-\d\d \d\d:\d\d:\d\d CMD   set tx 5$/m.test(lg.body) &&
          /MELD  #test -> PING-ALARM Testziel/.test(lg.body) && / SYS   Gateway gestartet/.test(lg.body), lg.body.slice(0, 300));
    check('Log: Passwörter/Schlüssel nie im Klartext', !lg.body.includes('temp1') && !lg.body.includes('geheim123') && !/abababab/.test(lg.body) &&
          lg.body.includes('CMD   password ********') && lg.body.includes('CMD   set prv.key ********') && lg.body.includes('CMD   set guest.password ********'),
          lg.body.split('\n').filter(l => /password|prv\.key/.test(l)).join(' / '));
    await page.click('#lgClear');
    check('Log: gelöscht', await until(async () => (await t('#lgMsg')) === 'Gelöscht'));
    const lg2 = await page.evaluate(async () => (await fetch('/api/log/download')).text());
    check('Log: nach Löschen nur Löschvermerk', lg2.trim().split('\n').length === 1 && lg2.includes('LOG   Log gelöscht'), lg2);
    await page.screenshot({ path: out + '/12_system_log.png', fullPage: true });
  }

  check('Keine JS-Fehler', errors.length === 0, errors.join(' | '));
  await browser.close();
  console.log(fails ? `${fails} FEHLER` : 'ALLE TESTS OK');
  process.exit(fails ? 1 : 0);
})();
