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
  await sleep(2000);
  check('Keine JS-Fehler', errors.length === 0, errors.join(' | '));
  await page.screenshot({ path: process.env.TEMP + '/claude/live_settings.png', fullPage: true });
  await browser.close();
  console.log(fails ? fails + ' FEHLER' : 'ALLE LIVE-PRÜFUNGEN OK');
  process.exit(fails ? 1 : 0);
})();
