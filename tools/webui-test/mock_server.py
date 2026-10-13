# Mock des Gateway-Webservers für UI-Tests ohne Hardware.
# Liefert index.html und simuliert /api/status, /api/cli, /api/console, /api/packets.
import json, sys, time, threading
from http.server import ThreadingHTTPServer, BaseHTTPRequestHandler
from urllib.parse import urlparse, parse_qs

import os
# index.html direkt aus dem Firmware-Header ziehen, damit immer der aktuelle Stand getestet wird.
HDR = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'src', 'web', 'index_html.h')
PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 8765

def index_html():
    src = open(HDR, encoding='utf-8').read()
    return src.split('R"HTML(', 1)[1].rsplit(')HTML"', 1)[0].encode('utf-8')
CAP = {'console': 150, 'packets': 300}
logs = {'console': [], 'packets': []}
seq = {'console': 1, 'packets': 1}
lock = threading.Lock()
t0 = time.time()

def push(name, t, x):
    with lock:
        logs[name].append({'s': seq[name], 'u': int((time.time() - t0) * 1000), 'e': 0, 't': t, 'x': x})
        seq[name] += 1
        if len(logs[name]) > CAP[name]:
            logs[name].pop(0)

push('console', 's', 'Gateway gestartet, MeshCore v1.17.1')
for i in range(5):
    push('packets', 'R', f'len=40 ADVERT(4) route=F hops={i} payload=30 SNR=7.5 RSSI=-90 score=900')
    push('packets', 'T', f'len=40 ADVERT(4) route=F hops={i+1} payload=30')

REPLIES = {'ver': 'v1.17.1 (Build: 14 Aug 2026)', 'neighbors': 'AB12CD34:1760000000:28\nEF56AB78:1760000100:-12',
           'get radio': '> 869.618,62.5,8,8'}

class H(BaseHTTPRequestHandler):
    def log_message(self, *a): pass
    def send_json(self, obj, code=200):
        b = json.dumps(obj).encode()
        self.send_response(code); self.send_header('Content-Type', 'application/json')
        self.send_header('Content-Length', str(len(b))); self.end_headers(); self.wfile.write(b)
    def do_GET(self):
        u = urlparse(self.path); q = parse_qs(u.query)
        if u.path == '/':
            b = index_html()
            self.send_response(200); self.send_header('Content-Type', 'text/html; charset=utf-8')
            self.send_header('Content-Length', str(len(b))); self.end_headers(); self.wfile.write(b); return
        if u.path == '/api/status':
            return self.send_json({'gateway': {'version': '0.1.0', 'uptime_ms': 1000, 'free_heap': 180000, 'cpu_mhz': 240},
              'node': {'name': 'Mock GW', 'pubkey': 'ab' * 32, 'role': 'repeater', 'firmware': 'v1.17.1', 'build_date': '14 Aug 2026',
                       'lat': 0, 'lon': 0, 'forwarding': True},
              'radio': {'freq': 869.618, 'bw': 62.5, 'sf': 8, 'cr': 8, 'tx_power': 10},
              'stats_core': {'battery_mv': 0, 'uptime_secs': 3725, 'errors': 0, 'queue_len': 0},
              'stats_radio': {'noise_floor': -112, 'last_rssi': -90, 'last_snr': 7.5, 'tx_air_secs': 3, 'rx_air_secs': 12},
              'stats_packets': {'recv': 10, 'sent': 5, 'flood_tx': 5, 'direct_tx': 0, 'flood_rx': 10, 'direct_rx': 0, 'recv_errors': 0},
              'wifi': {'mode': 'station', 'ssid': 'TestNetz', 'ip': '192.168.1.50', 'rssi': -55, 'mac': 'AA:BB:CC:DD:EE:FF', 'hostname': 'meshcore-gw'}})
        if u.path in ('/api/console', '/api/packets'):
            name = u.path.split('/')[-1]
            since = int(q.get('since', ['0'])[0])
            with lock:
                L = logs[name]
                oldest = L[0]['s'] if L else seq[name]
                gap = since > 0 and since < oldest
                out = [l for l in L if l['s'] >= max(since, oldest)][:60]
                nxt = (out[-1]['s'] + 1) if out else max(since, oldest)
            return self.send_json({'lines': out, 'next': nxt, 'gap': gap})
        self.send_response(404); self.end_headers()
    def do_POST(self):
        u = urlparse(self.path)
        body = self.rfile.read(int(self.headers.get('Content-Length', 0))).decode()
        if u.path == '/api/cli':
            cmd = parse_qs(body, keep_blank_values=True).get('cmd', [''])[0]
            def run():
                time.sleep(0.05)
                push('console', 'c', cmd)
                rep = REPLIES.get(cmd, 'Unknown command' if cmd else '')
                for part in rep.split('\n'):
                    if rep: push('console', 'r', part)
            threading.Thread(target=run).start()
            return self.send_json({'ok': True})
        self.send_response(404); self.end_headers()

def traffic():
    i = 0
    while True:
        time.sleep(0.5); i += 1
        push('packets', 'R' if i % 2 else 'T', f'len={20+i%30} GRP_TXT(5) route=F hops=1 payload=25 #{i}')
threading.Thread(target=traffic, daemon=True).start()
ThreadingHTTPServer(('127.0.0.1', PORT), H).serve_forever()
