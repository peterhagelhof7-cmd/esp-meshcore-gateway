// Test des Web-Flashers (docs/index.html, E35) ohne Hardware: die serielle
// Verbindung wird durch nachgebaute Boards ersetzt (gwFlasher.makeLoader).
// Geprüft werden Board-Erkennung, Abbruch bei falschem Board, Datei-Prüfung
// (SHA-256, Board-Merkmal) und die an writeFlash übergebenen Daten.
//   python -I -m http.server 8766 --directory ..\..\docs   (eigenes Fenster)
//   node flasher_test.js
const puppeteer = require('puppeteer-core');
const crypto = require('crypto');
const fs = require('fs');
const path = require('path');
const chrome = process.env.CHROME || 'C:/Program Files/Google/Chrome/Application/chrome.exe';
const base = 'http://127.0.0.1:' + (process.env.FLASHER_PORT || 8766) + '/';
const docs = path.join(__dirname, '../../docs');
let fails = 0;
const check = (name, ok, info) => { console.log((ok ? 'PASS ' : 'FAIL ') + name + (info !== undefined ? ' -> ' + info : '')); if (!ok) fails++; };
const sleep = ms => new Promise(r => setTimeout(r, ms));

(async () => {
  const manifest = JSON.parse(fs.readFileSync(path.join(docs, 'fw/manifest.json'), 'utf8'));
  const browser = await puppeteer.launch({ executablePath: chrome, headless: true });
  const page = await browser.newPage();
  const errors = [];
  page.on('pageerror', e => errors.push(e.message));
  page.on('dialog', d => { errors.push('Dialog: ' + d.message()); d.dismiss(); });
  await page.goto(base, { waitUntil: 'networkidle0' });
  const text = sel => page.$eval(sel, e => e.textContent);
  const waitFor = async (fn, ms = 10000) => { const t = Date.now(); while (Date.now() - t < ms) { if (await fn()) return true; await sleep(100); } return false; };

  check('Seite: Version aus manifest.json', await waitFor(async () => (await text('#fwVer')).startsWith(manifest.version + ' (')), await text('#fwVer'));
  check('Seite: Installieren anfangs gesperrt', await page.$eval('#btnFlash', e => e.disabled));

  // --- Variantenwahl ---
  const pick = info => page.evaluate(i => { const r = gwFlasher.pickVariant(i, gwFlasher.manifest); return r.error ? 'ERR ' + r.error : r.key; }, info);
  const s3 = 'ESP32-S3 (QFN56) (revision v0.2)';
  check('Wahl: S3 + 2 MB PSRAM -> v4', (await pick({ chip: s3, psramCap: 2, flashSize: '16MB' })) === 'v4');
  check('Wahl: S3 + 8 MB PSRAM -> r8', (await pick({ chip: s3, psramCap: 1, flashSize: '16MB' })) === 'r8');
  let r = await pick({ chip: s3, psramCap: 0, flashSize: '16MB' });
  check('Wahl: S3 ohne PSRAM -> abgelehnt', r.startsWith('ERR ESP32-S3 ohne PSRAM'), r);
  r = await pick({ chip: 'ESP32-D0WD-V3 (revision v3.1)', psramCap: -1, flashSize: '4MB' });
  check('Wahl: klassischer ESP32 -> abgelehnt', r.startsWith('ERR Kein ESP32-S3'), r);
  r = await pick({ chip: s3, psramCap: 2, flashSize: '8MB' });
  check('Wahl: S3 mit 8 MB Flash -> abgelehnt', r.startsWith('ERR Flash 8MB'), r);

  // --- Datei-Prüfung ---
  for (const key of ['v4', 'r8']) {
    r = await page.evaluate(async k => { try { const f = await gwFlasher.loadFiles(k, gwFlasher.manifest); return f.map(x => x.address + ':' + x.data.length).join(','); } catch (e) { return 'ERR ' + e.message; } }, key);
    const exp = manifest.variants[key].files.map(f => f.address + ':' + f.size).join(',');
    check('Dateien ' + key + ': Größe, SHA-256, Merkmal ok', r === exp, r);
  }
  r = await page.evaluate(async () => {
    const bad = async (u, o) => { const res = await fetch(u, o); const b = new Uint8Array(await res.arrayBuffer()); if (u.endsWith('firmware.bin')) b[5000] ^= 0xff;
      return { ok: true, status: 200, arrayBuffer: async () => b.buffer }; };
    try { await gwFlasher.loadFiles('v4', gwFlasher.manifest, bad); return 'kein Fehler'; } catch (e) { return e.message; } });
  check('Dateien: verändertes Byte -> Prüfsumme falsch', r === 'firmware.bin: Prüfsumme falsch', r);
  r = await page.evaluate(async () => {
    // R8-Firmware unter V4 einsortiert (Größe/Hash passend gemacht) -> Merkmal muss es fangen
    const m = JSON.parse(JSON.stringify(gwFlasher.manifest));
    const r8 = m.variants.r8.files.find(f => f.name === 'firmware.bin');
    const v4 = m.variants.v4.files.find(f => f.name === 'firmware.bin');
    v4.size = r8.size; v4.sha256 = r8.sha256;
    const swap = (u, o) => fetch(u.replace('fw/v4/firmware.bin', 'fw/r8/firmware.bin'), o);
    try { await gwFlasher.loadFiles('v4', m, swap); return 'kein Fehler'; } catch (e) { return e.message; } });
  check('Dateien: R8-Firmware als V4 -> Merkmal fehlt', r.includes('trägt nicht das Merkmal MESHCORE-GW-HELTEC-V4'), r);

  // --- Kompletter Ablauf mit nachgebautem Board ---
  // Board-Attrappe im Browser: zählt writeFlash-Aufrufe, rechnet MD5 über
  // calculateMD5Hash wie esptool-js und vergleicht mit Node-MD5.
  const install = async (board) => page.evaluate(b => {
    window.__w = { calls: [], md5: [], after: 0, disc: 0 };
    let psramReads = 0;
    gwFlasher.makeLoader = async () => ({
      chip: {
        getChipDescription: async () => b.chip,
        getChipFeatures: async () => b.features,
        getPsramCap: async () => { psramReads++; return (b.changeAfter && psramReads > b.changeAfter) ? b.changeTo : b.psramCap; },
        readMac: async () => 'f8:5b:1b:00:00:01',
      },
      detectFlashSize: async () => b.flashSize,
      writeFlash: async (o) => {
        __w.calls.push({ files: o.fileArray.map(f => '0x' + f.address.toString(16) + ':' + f.data.length), eraseAll: o.eraseAll, mode: o.flashMode, freq: o.flashFreq, size: o.flashSize, compress: o.compress });
        o.fileArray.forEach((f, i) => { __w.md5.push(o.calculateMD5Hash(f.data)); o.reportProgress(i, 1, 1); });
      },
      after: async () => { __w.after++; },
      transport: { disconnect: async () => { __w.disc++; } },
    });
  }, board);
  const state = () => page.evaluate(() => ({ check: checkMsg.className + ' | ' + checkMsg.textContent, flash: flashMsg.className + ' | ' + flashMsg.textContent,
    btn: btnFlash.disabled, w: window.__w, prog: prog.value }));

  // 1) Standard-V4
  await install({ chip: s3, features: ['Wi-Fi', 'BLE', 'Embedded PSRAM 2MB (AP_3v3)'], psramCap: 2, flashSize: '16MB' });
  await page.click('#btnCheck');
  await waitFor(async () => (await state()).check.includes('on'));
  let s = await state();
  check('V4: erkannt, passende Firmware angezeigt', s.check.includes('ok') && s.check.includes('Erkannt: Heltec WiFi LoRa 32 V4 (ESP32-S3R2') && s.check.includes('MESHCORE-GW-HELTEC-V4 ' + manifest.version), s.check);
  check('V4: Chipdaten angezeigt', (await text('#iFeat')).includes('PSRAM 2MB') && (await text('#iFlash')) === '16MB');
  check('V4: Installieren freigegeben', !s.btn);
  await page.click('#btnFlash');
  await waitFor(async () => (await state()).flash.includes(' ok '));
  s = await state();
  check('V4: Erfolgsmeldung', s.flash.startsWith('msg on ok') && s.flash.includes('MESHCORE-GW-HELTEC-V4 ' + manifest.version + ' ist installiert'), s.flash);
  const exp = f => manifest.variants[f].files.map(x => '0x' + x.address.toString(16) + ':' + x.size);
  check('V4: writeFlash mit 4 Dateien an 0x0/0x8000/0xe000/0x10000', s.w.calls.length === 1 && JSON.stringify(s.w.calls[0].files) === JSON.stringify(exp('v4')), JSON.stringify(s.w.calls));
  check('V4: Parameter wie PlatformIO (dio, 80m, 16MB, komprimiert, löschen)', s.w.calls[0].mode === 'dio' && s.w.calls[0].freq === '80m' && s.w.calls[0].size === '16MB' && s.w.calls[0].compress === true && s.w.calls[0].eraseAll === true, JSON.stringify(s.w.calls[0]));
  const md5 = f => crypto.createHash('md5').update(fs.readFileSync(path.join(docs, 'fw', f))).digest('hex');
  check('V4: MD5 für die Nachprüfung korrekt', JSON.stringify(s.w.md5) === JSON.stringify(manifest.variants.v4.files.map(f => md5('v4/' + f.name))), s.w.md5.join(','));
  check('V4: Neustart + Trennen, Fortschritt 100 %', s.w.after === 1 && s.w.disc >= 1 && s.prog === 100, JSON.stringify({ a: s.w.after, d: s.w.disc, p: s.prog }));

  // 2) V4-R8 ohne Löschen
  await install({ chip: s3, features: ['Wi-Fi', 'BLE', 'Embedded PSRAM 8MB (AP_3v3)'], psramCap: 1, flashSize: '16MB' });
  await page.click('#btnCheck');
  await waitFor(async () => (await state()).check.includes('Erkannt'));
  await page.$eval('#optErase', e => { e.checked = false; });
  await page.click('#btnFlash');
  await waitFor(async () => (await state()).flash.includes(' ok '));
  s = await state();
  check('R8: erkannt und R8-Firmware geschrieben', s.check.includes('V4-R8') && s.w.calls.length === 1 && JSON.stringify(s.w.calls[0].files) === JSON.stringify(exp('r8')), JSON.stringify(s.w.calls));
  check('R8: ohne Löschen -> Einstellungen bleiben (Meldung)', s.w.calls[0].eraseAll === false && s.flash.includes('behält WLAN und Einstellungen'), s.flash);
  await page.$eval('#optErase', e => { e.checked = true; });

  // 3) Ungeeignetes Board -> nichts schreiben
  await install({ chip: s3, features: ['Wi-Fi', 'BLE'], psramCap: 0, flashSize: '8MB' });
  await page.click('#btnCheck');
  await waitFor(async () => (await state()).check.includes('on'));
  s = await state();
  check('Nach Prüfung: Chipdaten sichtbar', await page.$eval('#chipInfo', e => getComputedStyle(e).display !== 'none'));
  check('Fremdes S3-Board: abgelehnt, Installieren gesperrt, getrennt', s.check.includes('bad') && s.check.includes('Nicht geeignet') && s.btn === true && s.w.calls.length === 0 && s.w.disc >= 1, s.check);

  // 4) Board-Wechsel zwischen Prüfen und Installieren -> Abbruch vor dem Schreiben
  await install({ chip: s3, features: ['Wi-Fi'], psramCap: 2, flashSize: '16MB', changeAfter: 1, changeTo: 1 });
  await page.click('#btnCheck');
  await waitFor(async () => (await state()).check.includes('Erkannt'));
  await page.click('#btnFlash');
  await waitFor(async () => (await state()).flash.includes('bad'));
  s = await state();
  check('Board gewechselt: Abbruch, nichts geschrieben', s.flash.includes('Board hat sich geändert') && s.w.calls.length === 0 && s.btn === true, s.flash);

  // 5) Kein Gerät gewählt
  await page.evaluate(() => { gwFlasher.makeLoader = async () => { const e = new Error('No port selected by the user.'); e.name = 'NotFoundError'; throw e; }; });
  await page.click('#btnCheck');
  await waitFor(async () => (await state()).check.includes('bad'));
  check('Kein Gerät gewählt: verständliche Meldung', (await state()).check.includes('Kein Gerät ausgewählt'), (await state()).check);
  check('Kein Gerät gewählt: alte Chipdaten ausgeblendet', await page.$eval('#chipInfo', e => getComputedStyle(e).display === 'none'));

  // Echte Bibliothek vom CDN: lädt und liefert ESPLoader/Transport (ohne Verbindung)
  r = await page.evaluate(async () => { try { const m = await import('https://cdn.jsdelivr.net/npm/esptool-js@0.6.1/bundle.js');
    return typeof m.ESPLoader + '/' + typeof m.Transport + '/' + typeof CryptoJS.MD5; } catch (e) { return 'ERR ' + e.message; } });
  check('esptool-js 0.6.1 + crypto-js vom CDN ladbar', r === 'function/function/function', r);
  check('Keine JS-Fehler/Dialoge', errors.length === 0, errors.join(' | '));
  await page.setViewport({ width: 390, height: 900 });
  const overflow = await page.evaluate(() => document.documentElement.scrollWidth > window.innerWidth);
  check('Handybreite: kein waagerechtes Scrollen', !overflow);
  await page.screenshot({ path: path.join(process.env.TEMP, 'claude', 'flasher_mobile.png'), fullPage: true });
  await page.setViewport({ width: 1000, height: 900 });
  await page.screenshot({ path: path.join(process.env.TEMP, 'claude', 'flasher.png'), fullPage: true });
  await browser.close();
  console.log(fails ? fails + ' FEHLER' : 'ALLE FLASHER-TESTS OK');
  process.exit(fails ? 1 : 0);
})();
