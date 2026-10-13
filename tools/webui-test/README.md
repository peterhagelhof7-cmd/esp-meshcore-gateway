# Web-UI-Test ohne Hardware

Testet `src/web/index_html.h` gegen einen Mock des Gateway-Webservers (Python, keine Abhängigkeiten).

```powershell
cd tools\webui-test
npm install                         # einmalig, puppeteer-core nutzt das installierte Chrome
python -I mock_server.py 8765       # in eigenem Fenster laufen lassen
node test.js <ordner-fuer-screenshots>
```

Der Mock simuliert `/api/status`, `/api/cli`, `/api/console` und `/api/packets` inklusive
Paketverkehr alle 0,5 s. Er ersetzt **keinen** Test auf echter Hardware.
