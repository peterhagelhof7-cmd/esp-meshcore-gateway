// UI-Smoke-Test der Gateway-Weboberfläche gegen mock_server.py
const puppeteer = require('puppeteer-core');
const out = process.argv[2] || '.';
const sleep = ms => new Promise(r => setTimeout(r, ms));
let fails = 0;
const check = (name, ok, info = '') => { console.log((ok ? 'PASS ' : 'FAIL ') + name + (info ? ' -> ' + info : '')); if (!ok) fails++; };

(async () => {
  const browser = await puppeteer.launch({ executablePath: 'C:/Program Files/Google/Chrome/Application/chrome.exe', headless: true });
  const page = await browser.newPage();
  const errors = [];
  page.on('pageerror', e => errors.push(e.message));
  page.on('dialog', d => { console.log('DIALOG: ' + d.message().split('\n')[0]); d.dismiss(); });
  await page.setViewport({ width: 1200, height: 900 });
  await page.goto('http://127.0.0.1:' + (process.env.MOCK_PORT || 8765) + '/', { waitUntil: 'networkidle0' });
  await sleep(1500);

  check('Status: Name gesetzt', (await page.$eval('#n_name', e => e.textContent)) === 'Mock GW');
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

  await page.setViewport({ width: 390, height: 844 });
  await page.click('nav a[data-v=console]');
  await sleep(500);
  const overflow = await page.evaluate(() => document.documentElement.scrollWidth > window.innerWidth);
  check('Mobil: kein horizontales Scrollen', !overflow);
  await page.screenshot({ path: out + '/04_mobile.png' });

  check('Keine JS-Fehler', errors.length === 0, errors.join(' | '));
  await browser.close();
  console.log(fails ? `${fails} FEHLER` : 'ALLE TESTS OK');
  process.exit(fails ? 1 : 0);
})();
