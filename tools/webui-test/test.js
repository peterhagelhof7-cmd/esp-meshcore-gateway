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
  await page.screenshot({ path: out + '/01_status.png' });

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
  check('Einst.: Profil-Tabelle', rows >= 20, rows + ' Zeilen');
  const crBefore = await page.$eval('#f_rc', e => e.value);
  check('Einst.: CR vorher 5 (Upstream-Default)', crBefore === '5', crBefore);
  check('Einst.: fem.txgain als nicht verfügbar markiert', await page.$eval('#fb_radio\\.fem\\.txgain', e => e.classList.contains('na')));
  check('Einst.: Regionen leer', await waitFor(async () => (await text('#regOut')).trim() === '* F'), await text('#regOut'));
  await page.screenshot({ path: out + '/05_settings_before.png', fullPage: true });

  await page.click('#profApply');
  check('Einst.: Profil übernommen', await waitFor(async () => (await text('#profMsg')) === 'Alles wie empfohlen'), await text('#profMsg'));
  await waitFor(async () => (await page.$eval('#f_rc', e => e.value)) === '8');
  check('Einst.: CR jetzt 8', (await page.$eval('#f_rc', e => e.value)) === '8');
  check('Einst.: dutycycle 10', (await page.$eval('#f_dutycycle', e => e.value)) === '10.0', await page.$eval('#f_dutycycle', e => e.value));
  check('Einst.: Neustart-Banner sichtbar', await page.$eval('#rebootBanner', e => e.classList.contains('on')));
  await waitFor(async () => (await text('#regOut')).includes('rhein-main'));
  const reg = await text('#regOut');
  check('Einst.: Regionen angelegt', ['de-he F', 'de-by F', 'rhein-main F'].every(r => reg.includes(r)), JSON.stringify(reg));

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

  // Zustand verstellen, dann Backup einspielen
  await runCli(page, 'set tx 5');
  await runCli(page, 'region remove de-by');
  const input = await page.$('#bkFile');
  await input.uploadFile(bkFile);
  await waitFor(async () => !(await page.$eval('#bkRestore', e => e.disabled)));
  check('Restore: Schlüssel in Vorschau maskiert', (await text('#bkOut')).includes('set prv.key ••••••••') && !(await text('#bkOut')).includes('abab'));
  await page.click('#bkRestore');
  check('Restore: eingespielt', await waitFor(async () => (await text('#bkMsg')) === 'Eingespielt', 15000), await text('#bkMsg'));
  check('Restore: tx wieder 17', await waitFor(async () => (await page.$eval('#f_tx', e => e.value)) === '17'));
  check('Restore: Region wieder da', await waitFor(async () => (await text('#regOut')).includes('de-by F')));

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

  check('Keine JS-Fehler', errors.length === 0, errors.join(' | '));
  await browser.close();
  console.log(fails ? `${fails} FEHLER` : 'ALLE TESTS OK');
  process.exit(fails ? 1 : 0);
})();
