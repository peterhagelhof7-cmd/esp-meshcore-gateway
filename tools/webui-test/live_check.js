// Nur-Lese-Prüfung der Weboberfläche gegen ein echtes Gateway (ändert nichts).
//   $env:GW_PASS='…'; node live_check.js 10.10.0.151
const puppeteer = require('puppeteer-core');
const host = process.argv[2] || '10.10.0.151';
const chrome = process.env.CHROME || 'C:/Program Files/Google/Chrome/Application/chrome.exe';
let fails = 0;
const check = (name, ok, info) => { console.log((ok ? 'PASS ' : 'FAIL ') + name + (info !== undefined ? ' -> ' + info : '')); if (!ok) fails++; };
const sleep = ms => new Promise(r => setTimeout(r, ms));

(async () => {
  const browser = await puppeteer.launch({ executablePath: chrome, headless: true });
  const page = await browser.newPage();
  const errors = [];
  page.on('pageerror', e => errors.push(e.message));
  page.on('dialog', d => { console.log('DIALOG (abgelehnt): ' + d.message()); d.dismiss(); });
  await page.authenticate({ username: 'admin', password: process.env.GW_PASS || '' });
  await page.goto('http://' + host + '/', { waitUntil: 'networkidle2' });
  const text = sel => page.$eval(sel, e => e.textContent);
  const waitFor = async (fn, ms = 15000) => { const t = Date.now(); while (Date.now() - t < ms) { if (await fn()) return true; await sleep(250); } return false; };

  await page.click('nav a[data-v=settings]');
  check('Profil: alles wie empfohlen', await waitFor(async () => (await text('#profMsg')) === 'Alles wie empfohlen'), await text('#profMsg'));
  await waitFor(async () => (await page.$eval('#f_txdelay', e => e.value)) !== '');
  const f = await page.evaluate(() => ({ rf: f_rf.value, txd: f_txdelay.value, valid: setForm.checkValidity(),
    invalid: [...setForm.querySelectorAll('input')].filter(i => !i.checkValidity()).map(i => i.id + '=' + i.value),
    save: setSave.disabled, msg: setMsg.textContent }));
  check('Formular: Frequenz gerundet', f.rf === '869.618', f.rf);
  check('Formular: txdelay gerundet', f.txd === '0.7', f.txd);
  check('Formular: Browser-Validierung ok (Speichern möglich)', f.valid, f.invalid.join(', '));
  check('Formular: keine Scheinänderung', f.save === true && f.msg === '', JSON.stringify(f.msg));

  await page.click('nav a[data-v=sm]').catch(() => {});
  const intRow = async () => page.$$eval('#smDevBody tr', r => { const x = r.find(t => t.children[0].textContent === 'Gateway (intern)'); return x ? [...x.children].map(c => c.textContent) : null; });
  check('Überwachung: Zeile Gateway (intern) mit Chiptemperatur', await waitFor(async () => { const r = await intRow(); return r && /Chiptemperatur \d+(\.\d)?°C/.test(r[3]); }), JSON.stringify(await intRow()));
  check('Status: Chiptemperatur', /^\d+(\.\d)? °C$/.test(await text('#g_chip')), await text('#g_chip'));

  // Private Kanäle: Anzeige = Hex wie in der App (nur lesen, klickt nur „anzeigen“)
  await page.click('nav a[data-v=channels]');
  await waitFor(async () => (await page.$$eval('#chBody tr', r => r.length)) > 0);
  const api = await page.evaluate(async () => (await (await fetch('/api/channels')).json()).channels.filter(c => c.kind === 'private'));
  for (const c of api) {
    const shown = await page.$$eval('#chBody tr', (rows, n) => { const r = rows.find(x => x.children[0].textContent === n); r.querySelector('button.chip').click(); return r.children[4].textContent; }, c.name);
    check('Kanal ' + c.name + ': Schlüssel als Hex angezeigt', shown.startsWith(c.key_hex) && /^[0-9a-f]{32}$/.test(c.key_hex), shown);
  }
  await sleep(1000);
  check('Keine JS-Fehler', errors.length === 0, errors.join(' | '));
  await page.screenshot({ path: process.env.TEMP + '/claude/live_settings.png', fullPage: true });
  await browser.close();
  console.log(fails ? fails + ' FEHLER' : 'ALLE LIVE-PRÜFUNGEN OK');
  process.exit(fails ? 1 : 0);
})();
