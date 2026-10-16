# Mock des Gateway-Webservers für UI-Tests ohne Hardware.
#
# - index.html kommt direkt aus src/web/index_html.h (immer aktueller Stand)
# - das Empfehlungsprofil wird aus src/Profile.cpp geparst (eine Quelle)
# - FakeCli bildet die get/set/region-Antwortformate von MeshCore 1.17.1
#   (CommonCLI.cpp) nach, Startzustand = Upstream-Repeater-Defaults
#
# Start: python -I mock_server.py [port]
import decimal, json, os, re, struct, sys, threading, time
from http.server import ThreadingHTTPServer, BaseHTTPRequestHandler
from urllib.parse import urlparse, parse_qs

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'src')
PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 8765


def index_html():
    src = open(os.path.join(ROOT, 'web', 'index_html.h'), encoding='utf-8').read()
    return src.split('R"HTML(', 1)[1].rsplit(')HTML"', 1)[0].encode('utf-8')


def load_profile():
    src = open(os.path.join(ROOT, 'Profile.cpp'), encoding='utf-8').read()
    name = re.search(r'profile::NAME = "([^"]*)"', src).group(1)
    entries = []
    for m in re.finditer(r'\{"([^"]*)", (nullptr|"[^"]*"), (nullptr|"[^"]*"), "([^"]*)"\}', src):
        unq = lambda s: None if s == 'nullptr' else s[1:-1]
        entries.append({'cmd': m.group(1), 'get': unq(m.group(2)), 'expect': unq(m.group(3)), 'why': m.group(4)})
    return {'name': name, 'entries': entries}


CAP = {'console': 150, 'packets': 300}
logs = {'console': [], 'packets': []}
seq = {'console': 1, 'packets': 1}
lock = threading.RLock()   # reentrant: FakeSm ruft push() unter Lock
t0 = time.time()


def push(name, t, x):
    with lock:
        logs[name].append({'s': seq[name], 'u': int((time.time() - t0) * 1000), 'e': 0, 't': t, 'x': x})
        seq[name] += 1
        if len(logs[name]) > CAP[name]:
            logs[name].pop(0)
        evmirror(name, t, x)


# Ereignis-Log wie EventLog.cpp + Mirrors aus main.cpp (E27)
EV_SECRET = ('password ', 'set prv.key', 'get prv.key', 'set guest.password', 'get guest.password',
             'set bridge.secret', 'get bridge.secret')
evlog = {'text': '', 'hide': False}


def evadd(tag, text):
    evlog['text'] += time.strftime('%Y-%m-%d %H:%M:%S') + ' %-5s ' % tag + text.replace('\n', ' | ') + '\n'


def evmirror(name, t, x):
    if name == 'console':
        if t == 'c':
            evlog['hide'] = x.startswith(EV_SECRET)
            if x.startswith('set ') and evlog['hide']:
                x = ' '.join(x.split(' ')[:2]) + ' ********'
            evadd('CMD', x)
        elif t == 'r':
            if not evlog['hide']: evadd('ANTW', x)
        else:
            evlog['hide'] = False; evadd('SYS', x)
    elif name == 'smlog':
        evadd({'A': 'MELD', 'X': 'MELDX'}.get(t, 'UEBW'), x)
    elif name == 'messages' and t == 'S':
        evadd('KANAL', x)


def ftoa(f):
    s = ('%.6f' % f).rstrip('0').rstrip('.')
    return s if s else '0'


def ftoa32(f):
    # wie StrHelper::ftoa auf echter HW: float32, abgeschnitten -> 869.618 wird 869.6179809
    d = decimal.Decimal(struct.unpack('f', struct.pack('f', f))[0])
    s = format(d.quantize(decimal.Decimal('1e-7'), rounding=decimal.ROUND_DOWN), 'f').rstrip('0')
    return s + '0' if s.endswith('.') else s


class FakeCli:
    """Nachbau der Antwortformate von CommonCLI (1.17.1), nur was die UI nutzt."""

    def __init__(self):
        self.p = {'name': 'Heltec Repeater', 'lat': 0.0, 'lon': 0.0, 'owner.info': '', 'guest.password': '',
                  'freq': 869.618, 'bw': 62.5, 'sf': 8, 'cr': 5, 'tx': 17, 'af': 1.0,
                  'radio.rxgain': 'on', 'radio.fem.rxgain': 'on', 'repeat': 'on',
                  'flood.max': 64, 'flood.max.unscoped': 64, 'flood.max.advert': 8,
                  'txdelay': 0.5, 'direct.txdelay': 0.3, 'rxdelay': 0.0, 'path.hash.mode': 0,
                  'loop.detect': 'off', 'cad': 'off', 'int.thresh': 0, 'agc': 0, 'multi.acks': 0,
                  'allow.read.only': 'off', 'advert.interval': 2, 'flood.advert.interval': 47,
                  'password': 'password'}
        self.regions = []   # [name, parent, flood, home]
        self.key = 'ab' * 64
        self.acl = {'c0ffee' + '11' * 29: 3}   # ein Admin-Client

    def region_tree(self):
        out = ['* F']
        def rec(parent, depth):
            for r in self.regions:
                if r[1] == parent:
                    out.append(' ' * depth + r[0] + ('^' if r[3] else '') + (' F' if r[2] else ''))
                    rec(r[0], depth + 1)
        rec(None, 1)
        return '\n'.join(out) + '\n'

    def find(self, n):
        n = n.lstrip('#')
        for r in self.regions:
            if r[0] == n:
                return r
        for r in self.regions:
            if r[0].startswith(n):
                return r
        return None

    def get(self, k):
        p = self.p
        if k == 'radio': return '> %s,%s,%d,%d' % (ftoa32(p['freq']), ftoa(round(p['bw'], 3)), p['sf'], p['cr'])
        if k == 'dutycycle': return '> %.1f%%' % (100.0 / (p['af'] + 1))
        if k == 'agc.reset.interval': return '> %d' % (p['agc'] * 4)
        if k == 'advert.interval': return '> %d' % (p['advert.interval'] * 2)
        if k == 'radio.fem.txgain': return 'Error: unsupported'
        if k == 'prv.key': return '> ' + self.key
        if k in ('txdelay', 'direct.txdelay', 'rxdelay'): return '> ' + ftoa32(p[k])
        if k in ('lat', 'lon'): return '> ' + ftoa(p[k])
        if k in p: return '> %s' % p[k]
        return '??: ' + k

    def set(self, k, v):
        p = self.p
        try:
            if k == 'radio':
                f, b, s, c = v.split(',')
                p['freq'], p['bw'], p['sf'], p['cr'] = float(f), float(b), int(s), int(c)
                return 'OK - reboot to apply'
            if k == 'dutycycle':
                dc = float(v)
                if dc < 1 or dc > 100: return 'ERROR: dutycycle must be 1-100'
                p['af'] = 100.0 / dc - 1
                return 'OK - %.1f%%' % (100.0 / (p['af'] + 1))
            if k == 'agc.reset.interval':
                p['agc'] = int(v) // 4
                return 'OK - interval rounded to %d' % (p['agc'] * 4)
            if k == 'advert.interval':
                m = int(v)
                if (0 < m < 60) or m > 240: return 'Error: interval range is 60-240 minutes'
                p['advert.interval'] = m // 2
                return 'OK'
            if k == 'flood.advert.interval':
                h = int(v)
                if (0 < h < 3) or h > 168: return 'Error: interval range is 3-168 hours'
                p[k] = h
                return 'OK'
            if k == 'radio.fem.txgain': return 'Error: unsupported'
            if k == 'radio.fem.rxgain':
                p[k] = v
                return 'OK - LoRa FEM RX gain ' + v
            if k == 'name':
                if re.search(r'[\[\]\\:,?*]', v): return 'Error, bad chars'
                p[k] = v
                return 'OK'
            if k == 'prv.key':
                self.key = v
                return 'OK, reboot to apply! New pubkey: ' + 'cd' * 32
            if k == 'repeat':
                p[k] = 'off' if v.startswith('off') else 'on'
                return 'OK - repeat is now ' + p[k].upper()
            if k in ('lat', 'lon', 'txdelay', 'direct.txdelay', 'rxdelay'):
                p[k] = float(v)
                return 'OK'
            if k in p:
                p[k] = int(v) if isinstance(p[k], int) else v
                return 'OK'
        except ValueError:
            return 'Error, invalid params'
        return 'unknown config: ' + k

    def run(self, cmd):
        if cmd == 'ver': return 'v1.17.1 (Build: 14 Aug 2026)'
        # Format wie MyMesh::formatNeighborsReply: Kennung:Sekunden_her:SNR*4
        if cmd == 'neighbors':
            return 'AB12CD34:125:49\nEF56AB78:4000:-12' + ('\n1F2E3D4C:3:20' if getattr(self, 'discovered', False) else '')
        if cmd == 'discover.neighbors':
            self.discovered = True
            return 'OK - Discover sent'
        if cmd.startswith('get '): return self.get(cmd[4:])
        if cmd.startswith('set '):
            k, _, v = cmd[4:].partition(' ')
            return self.set(k, v)
        if cmd.startswith('password '):
            self.p['password'] = cmd[9:][:15]
            return 'password now: ' + self.p['password']
        if cmd.startswith('setperm '):
            parts = cmd.split(' ')
            if len(parts) != 3 or not re.fullmatch(r'[0-9a-fA-F]{64}', parts[1]): return 'Err - bad params'
            perm = int(parts[2])
            if perm & 3 == 0: self.acl.pop(parts[1].lower(), None)   # Gast wird nicht gespeichert
            else: self.acl[parts[1].lower()] = perm
            return 'OK'
        if cmd == 'get acl':
            return 'ACL: %d Eintrag/Einträge' % len(self.acl) + ''.join('\n%02X %s' % (v, k) for k, v in self.acl.items())
        if cmd == 'region': return self.region_tree()
        parts = cmd.split(' ')
        if parts[0] == 'region' and len(parts) >= 2:
            a = parts[1]
            if a == 'save': return 'OK'
            if a == 'put' and len(parts) >= 3:
                parent = None
                if len(parts) >= 4:
                    pr = self.find(parts[3])
                    if not pr: return 'Err - unknown parent'
                    parent = pr[0]
                r = self.find(parts[2])
                if r and r[0] == parts[2]: r[1] = parent
                else: self.regions.append([parts[2], parent, True, False])
                return 'OK - (flood allowed)'
            r = self.find(parts[2]) if len(parts) >= 3 else None
            if a in ('allowf', 'denyf', 'home', 'get', 'remove') and not r: return 'Err - unknown region'
            if a == 'allowf': r[2] = True; return 'OK'
            if a == 'denyf': r[2] = False; return 'OK'
            if a == 'home':
                for x in self.regions: x[3] = False
                r[3] = True
                return ' home is now ' + r[0]
            if a == 'get': return ' %s %s' % (r[0], 'F' if r[2] else '')
            if a == 'remove': self.regions.remove(r); return 'OK'
        return 'Unknown command'


import base64, hashlib
PUBLIC_KEY = bytes.fromhex('8b3387e9c5cdea6ac9e5edbaa115cd72')
logs['messages'] = []
seq['messages'] = 1
CAP['messages'] = 200


class FakeChannels:
    """Nachbau von ChannelStore (Schlüssel-/Namensregeln wie in der Firmware)."""
    MAX = 8

    def __init__(self):
        self.ch = [None] * self.MAX

    def add(self, kind, name, key, scope):
        name = (name or '').strip()
        if kind == 'public':
            name = name or 'Public'
            secret = PUBLIC_KEY
        elif kind == 'hashtag':
            if not name.startswith('#'): name = '#' + name
            name = name.lower()
            if len(name) < 2: return None, 'Name fehlt'
            if not re.fullmatch(r'#[a-z0-9_-]+', name): return None, 'Hashtag-Kanal: nur a-z, 0-9, - und _'
            secret = hashlib.sha256(name.encode()).digest()[:16]
        else:
            if not name: return None, 'Name fehlt'
            if name.startswith('#'): return None, 'Private Kanäle ohne #'
            if key:
                try:
                    secret = bytes.fromhex(key) if len(key) == 32 else base64.b64decode(key, validate=True)
                except Exception:
                    secret = b''
                if len(secret) != 16: return None, 'Schlüssel: 16 Byte als Base64 oder 32 Hex-Zeichen'
            else:
                secret = os.urandom(16)
        if scope and not re.fullmatch(r'#?[A-Za-z0-9._-]+', scope): return None, 'Scope: Regionsname aus a-z, 0-9, - _ .'
        for c in self.ch:
            if c and c['secret'] == secret: return None, 'Kanal mit diesem Schlüssel existiert schon: ' + c['name']
        for i in range(self.MAX):
            if self.ch[i] is None:
                self.ch[i] = {'name': name, 'secret': secret, 'scope': scope or ''}
                return i, None
        return None, 'Maximal 8 Kanäle'

    def json(self):
        out = []
        for i, c in enumerate(self.ch):
            if not c: continue
            kind = 'public' if c['secret'] == PUBLIC_KEY else ('hashtag' if c['name'].startswith('#') else 'private')
            out.append({'idx': i, 'name': c['name'], 'kind': kind, 'scope': c['scope'],
                        'hash': '%02x' % hashlib.sha256(c['secret']).digest()[0],
                        'key_b64': base64.b64encode(c['secret']).decode(), 'key_hex': c['secret'].hex()})
        return {'max': self.MAX, 'channels': out}


chans = FakeChannels()

# OTA wie src/OtaUpdate.cpp: Projekt-ID + Version aus platformio.ini (V4-Env)
_ini = open(os.path.join(ROOT, '..', 'platformio.ini'), encoding='utf-8').read()
GW_VERSION = re.search(r"GATEWAY_VERSION='\"([^\"]+)\"'", _ini).group(1)
GW_PROJECT = re.search(r"GATEWAY_PROJECT_ID='\"([^\"]+)\"'", _ini).group(1)   # erstes = V4
MARKER = re.compile(rb'MCGW-FW-ID:([A-Za-z0-9._+\-]+):([A-Za-z0-9._+\-]+):MCGW-FW-END')


def ver_tuple(v):
    m = re.match(r'v?(\d+)\.(\d+)\.(\d+)(?:-rc(\d+))?', v)
    if not m: return (0, 0, 0, -1)
    a, b, c, rc = m.groups()
    return (int(a), int(b), int(c), 10**9 if rc is None else int(rc))


import urllib.request

UNITS = {'temperature': '°C', 'humidity': '%', 'pressureHpa': 'hPa', 'lux': 'lx', 'eco2Ppm': 'ppm', 'chipTemp': '°C',
         'battery': 'V', 'uptimeH': 'h', 'noiseFloor': 'dBm'}
INTERNAL = 100        # Pseudo-Gerät "Gateway (intern)", wie sm::INTERNAL_DEVICE
CHIP_TEMP = 52.3
LABELS = {'temperature': 'Temperatur', 'humidity': 'Feuchte', 'pressureHpa': 'Luftdruck', 'lux': 'Helligkeit', 'eco2Ppm': 'eCO2', 'chipTemp': 'Chiptemperatur',
          'battery': 'Akku', 'uptimeH': 'Laufzeit', 'noiseFloor': 'Rauschen'}


class FakeSm:
    """Vereinfachter Nachbau von SmConfig + SmPoller (Regeln ohne Mindestdauer)."""

    def __init__(self):
        self.cfg = {'settings': {'enabled': True, 'defaultChannel': '', 'maxPerHour': 6, 'offlineAfterFails': 3,
                                 'alertText': '', 'recoverText': '', 'offlineText': '', 'onlineText': ''},
                    'devices': [], 'rules': [], 'pings': []}
        self.ping_state = []
        self.dev_state = []
        self.rule_state = []
        self.sent = 0
        self.acks = []        # wie ack::Tracker (E39)
        self.ack_last = 0

    # --- Bestätigungspflicht (E39), Logik wie src/AckTracker.cpp ---
    def open_ack(self, kind, src, channel, text):
        self.acks = [a for a in self.acks if not (a['kind'] == kind and a['src'] == src)]
        used = {a['id'] for a in self.acks}
        while True:
            self.ack_last = 1 if self.ack_last >= 99 else self.ack_last + 1
            if self.ack_last not in used: break
        self.acks.append({'id': self.ack_last, 'kind': kind, 'src': src, 'channel': channel, 'text': text,
                          't': time.time(), 'repeats': 0, 'cleared': False})
        return self.ack_last

    def ack_of(self, kind, src):
        return next((a['id'] for a in self.acks if a['kind'] == kind and a['src'] == src), 0)

    @staticmethod
    def parse_ack(body):
        # Rückgabe (nr, ziel) oder None; nr 0 = alle, ziel '' = an alle (wie Tracker::parseAck)
        m = re.fullmatch(r'\s*(?:@\[([^\]]*)\]\s*)?ack[\s#]*(\d{1,2})?[\s.!]*', body, re.I)
        if not m: return None
        n = int(m.group(2)) if m.group(2) else 0
        return (n, m.group(1) or '') if (m.group(2) is None or 1 <= n <= 99) else None

    # --- Zabbix-Kennung (E41), wie src/ZbxFormat.cpp / SmPoller::emitEvent ---
    @staticmethod
    def src(kind, key):
        crc = 0xFFFF
        for b in key.encode():
            crc ^= b << 8
            for _ in range(8): crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
        return '%s%04x' % (kind, crc)

    @staticmethod
    def marker(src, ev, val, ack):
        v = '' if val is None else ('%.2f' % val).rstrip('0').rstrip('.')
        return ' #Z1;%s;%s;%s;%s' % (src, ev, v, ack or '')

    def send_ev(self, channel, text, src, ev, val=None, ack=0, count=True):
        st = self.cfg['settings']; mode = st.get('zbxMode', 0)
        if not src or not mode: return self.emit(channel, text, count)
        room = 160 - len(cli.p['name'].encode()) - 2
        mk = self.marker(src, ev, val, ack)
        t = text.encode()[:max(0, room - len(mk))].decode('utf-8', 'ignore')
        if mode == 1: return self.emit(channel, t + mk, count)
        self.emit(channel, text, count)
        self.emit(st.get('zbxChannel', ''), t + mk, count)

    def on_channel_text(self, channel, text):
        sender, _, body = text.partition(': ')
        if not _: sender, body = '?', text
        p = self.parse_ack(body)
        if p is None: return
        want, target = p
        if target and target.lower() != cli.p['name'].lower(): return   # an ein anderes Gateway
        hit = [a for a in self.acks if channel in (a['channel'], a.get('channel2')) and (not want or a['id'] == want)]
        if not hit:
            push('smlog', 'S', '%s: "%s" von %s - nichts Offenes dazu' % (channel, body, sender)); return
        self.acks = [a for a in self.acks if a not in hit]
        ids = ','.join(str(a['id']) for a in hit)
        self.emit(channel, 'ACK %s bestätigt (%s)' % (ids, sender), False)
        push('smlog', 'S', '[ACK %s] bestätigt von %s in %s' % (ids, sender, channel))

    def ack_web(self, i):
        a = next((x for x in self.acks if x['id'] == i), None)
        if not a: return 'Nummer nicht (mehr) offen'
        self.acks.remove(a)
        self.emit(a['channel'], 'ACK %d bestätigt (Web)' % i, False)
        push('smlog', 'S', '[ACK %d] in der Weboberfläche bestätigt' % i)
        return None

    def repeat_all(self):
        for a in self.acks:
            if a['repeats'] >= 3: continue
            self.send_ev(a['channel'], '[ACK %d] WDH %d/3%s: %s' % (a['id'], a['repeats'] + 1, ' (inzw. OK)' if a['cleared'] else '', a['text']),
                         a.get('zsrc'), 'W', a.get('zval'), a['id'])
            a['repeats'] += 1

    def save(self, j):
        st = j.get('settings', {})
        if st.get('zbxMode') == 2 and not st.get('zbxChannel'): return 'Zabbix-Kanal wählen (oder Kennung im Alarmtext)'
        devs = j.get('devices', [])
        if len(devs) > 8: return 'Maximal 8 Geräte'
        for i, d in enumerate(devs):
            if not d.get('name'): return 'Gerät %d: Name fehlt' % (i + 1)
            if d.get('type') == 'repeater':   # E42, wie SmConfig::parse
                if not re.fullmatch(r'[0-9a-fA-F]{64}', d.get('key', '')): return 'Gerät %s: öffentlicher Schlüssel (64 Hex-Zeichen) fehlt oder ungültig' % d['name']
                d['key'] = d['key'].lower()
                d['intervalS'] = min(43200, max(900, int(d.get('intervalS', 3600))))
                for k in ('host', 'port', 'user'): d.pop(k, None)
            else:
                d['type'] = 'sensormeter'
                if not re.fullmatch(r'[A-Za-z0-9._-]+', d.get('host', '')): return 'Gerät %s: Adresse ungültig' % d['name']
            oi = d.pop('origIdx', -1)
            if d.get('pass') == '***' and 0 <= oi < len(self.cfg['devices']):
                d['pass'] = self.cfg['devices'][oi]['pass']
            if d['type'] == 'repeater' and len(d.get('pass', '')) > 15: return 'Gerät %s: Passwort max. 15 Zeichen' % d['name']
        keys = [d['key'] for d in devs if d['type'] == 'repeater']
        if len(keys) != len(set(keys)): return 'Derselbe Repeater ist zweimal eingetragen'
        rules = j.get('rules', [])
        if len(rules) > 16: return 'Maximal 16 Regeln'
        for i, r in enumerate(rules):
            if r.get('device', -1) != INTERNAL and not (0 <= r.get('device', -1) < len(devs)): return 'Regel %d: Gerät ungültig' % (i + 1)
            if '.' not in r.get('path', ''): return 'Regel %d: Messwert fehlt' % (i + 1)
        pings = j.get('pings', [])
        if len(pings) > 5: return 'Maximal 5 Ping-Ziele'
        for i, pg in enumerate(pings):
            if not pg.get('name'): return 'Ping-Ziel %d: Bezeichner fehlt' % (i + 1)
            if not re.fullmatch(r'[A-Za-z0-9._-]+', pg.get('host', '')): return 'Ping-Ziel %s: Name/IP ungültig' % pg['name']
            pg.setdefault('intervalS', 10)
            iv = int(pg['intervalS'])
            if not (10 <= iv <= 3600): return 'Ping-Ziel %s: Prüfabstand 10-3600 s' % pg['name']
            if not (iv <= int(pg.get('thresholdS', 0)) <= 65535): return 'Ping-Ziel %s: Schwellwert %d-65535 s (mind. Prüfabstand)' % (pg['name'], iv)
        old_p = {p['host']: s for p, s in zip(self.cfg.get('pings', []), self.ping_state)}
        self.ping_state = [old_p.get(p['host'], {}) for p in pings]
        # Wie SmPoller::syncConfig: Zustand unveränderter Geräte/Regeln behalten
        dkey = lambda d: ('rpt', d['key']) if d.get('type') == 'repeater' else (d['host'], d.get('port', 80))
        rkey = lambda r, ds: (('intern', 0) if r['device'] == INTERNAL else dkey(ds[r['device']]), r['path'], r['op'], r['threshold'], r.get('hysteresis', 0), r.get('minDurationS', 0))
        old_d = {dkey(d): s for d, s in zip(self.cfg['devices'], self.dev_state)}
        old_r = {rkey(r, self.cfg['devices']): s for r, s in zip(self.cfg['rules'], self.rule_state)}
        self.dev_state = [old_d.get(dkey(d), {'polled': False}) for d in devs]
        self.rule_state = [old_r.get(rkey(r, devs), {'state': 'normal', 'value': None, 'lastEvent': ''}) for r in rules]
        kd = sum(1 for d in devs if dkey(d) in old_d); kr = sum(1 for r in rules if rkey(r, devs) in old_r)
        self.cfg = j
        push('smlog', 'S', 'Konfiguration übernommen (%d Geräte, %d Regeln mit Zustand behalten)' % (kd, kr))
        # neue Geräte abfragen; entfernte Repeater nicht bei jeder Änderung (Funkzeit)
        for i, d in enumerate(devs):
            if d['type'] != 'repeater' or not self.dev_state[i].get('polled'): self.poll(i)
            elif self.dev_state[i].get('online'): self.evaluate(i, d['name'], self.dev_state[i].get('values', []))   # wie syncConfig
        return None

    def fetch(self, d):
        if d['host'].startswith('sim-'):
            t = 35.0 if 'hot' in d['host'] else 21.0
            return {'sensor1': {'name': 'Intern', 'valid': True, 'temperature': t, 'humidity': 45.0}}
        with urllib.request.urlopen('http://%s:%d/api/sensors' % (d['host'], d.get('port', 80)), timeout=3) as r:
            return json.loads(r.read())

    @staticmethod
    def find(name):   # wie channel_store::findByName (Regeln verweisen per Name)
        return next((i for i, x in enumerate(chans.ch) if x and name and x['name'] == name), -1)

    def emit(self, name, text, count=True):
        name = name or self.cfg['settings']['defaultChannel']
        if not name:
            push('smlog', 'X', 'kein Kanal gewählt: ' + text); return
        ch = self.find(name)
        c = chans.ch[ch] if ch >= 0 else None
        if not c:
            push('smlog', 'X', 'Kanal "%s" existiert nicht: %s' % (name, text)); return
        if count:
            if self.sent >= self.cfg['settings']['maxPerHour']:
                push('smlog', 'X', 'Sendelimit erreicht, unterdrückt: ' + text); return
            self.sent += 1
        push('smlog', 'A', '%s -> %s' % (c['name'], text))
        push('messages', 'S', '\t'.join([str(ch), c['name'], '', '0', cli.p['name'] + ': ' + text]))
        zbxm.on_text(ch, c['name'], cli.p['name'] + ': ' + text)   # eigene Meldungen (wie GatewayMesh::sendChannelText)

    @staticmethod
    def render(tpl, dev, path, val, op, thr, state):
        k = path.split('.')[-1]
        f = lambda v: ('%.0f' % v) if abs(v - round(v)) < 0.05 else ('%.1f' % v)
        rep = {'{device}': dev, '{metric}': LABELS.get(k, k), '{value}': f(val) if val is not None else '?',
               '{unit}': UNITS.get(k, ''), '{op}': '<' if op else '>', '{threshold}': f(thr), '{state}': state}
        for a, b in rep.items(): tpl = tpl.replace(a, b)
        return tpl

    def poll(self, i):
        d = self.cfg['devices'][i]
        st = self.dev_state[i]
        if d.get('type') == 'repeater':   # per Funk über FakeRemote (E42)
            if st.get('busy'): return
            st['busy'] = True
            def done(ok, res, route, d=d, st=st):
                if self.cfg['devices'][i:i + 1] != [d]: return   # Konfig inzwischen geändert
                st.update(busy=False, polled=True, route=route)
                if not ok:
                    st.update(online=False, fails=st.get('fails', 0) + 1, error=res); return
                vals = [{'path': p, 'sensor': '', 'label': LABELS[p.split('.')[1]], 'unit': UNITS[p.split('.')[1]], 'value': v} for p, v in res]
                st.update(online=True, offline=False, fails=0, lastOk=time.time(), error='', values=vals)
                self.evaluate(i, d['name'], vals)
            rmt.poll(d['key'], d.get('pass', ''), done)
            return
        st['polled'] = True
        try:
            data = self.fetch(d)
            vals = []
            for sk, so in data.items():
                if not isinstance(so, dict) or so.get('valid') is False: continue
                for fk, fv in so.items():
                    if fk in ('valid', 'name') or not isinstance(fv, (int, float)) or isinstance(fv, bool): continue
                    vals.append({'path': '%s.%s' % (sk, fk), 'sensor': so.get('name', ''), 'label': LABELS.get(fk, fk),
                                 'unit': UNITS.get(fk, ''), 'value': fv})
            st.update(online=True, offline=False, fails=0, lastOk=time.time(), error='', values=vals)
        except Exception as e:
            st.update(online=False, fails=st.get('fails', 0) + 1, error=str(e)[:60])
            return
        self.evaluate(i, d['name'], vals)

    def internal_values(self):
        return [{'path': 'gateway.chipTemp', 'sensor': '', 'label': 'Chiptemperatur', 'unit': '°C', 'value': CHIP_TEMP}]

    def evaluate(self, i, name, vals):
        for ri, r in enumerate(self.cfg['rules']):
            if r['device'] != i or not r.get('enabled', True): continue
            d = {'name': name}
            v = next((x['value'] for x in vals if x['path'] == r['path']), None)
            rs = self.rule_state[ri]
            rs['value'] = v
            if v is None: continue
            viol = v > r['threshold'] if r['op'] == 0 else v < r['threshold']
            if viol and rs['state'] != 'alert':
                rs['state'] = 'alert'; rs['lastEvent'] = 'ALARM'
                tpl = r.get('text') or self.cfg['settings'].get('alertText') or '{state} {device}: {metric} {value}{unit} ({op}{threshold}{unit})'
                txt = self.render(tpl, d['name'], r['path'], v, r['op'], r['threshold'], 'ALARM')
                st = self.cfg['settings']
                zsrc = self.src('R', '%s|%s|%s' % (d['name'], r['path'], '<' if r['op'] else '>'))
                ack = 0
                if r.get('ackRequired'):
                    ch = r['channel'] or st['defaultChannel']
                    ack = self.open_ack('rule', ri, ch, txt)
                    a = self.acks[-1]
                    a.update(zsrc=zsrc if st.get('zbxMode') else '', zval=v, channel2=st.get('zbxChannel', '') if st.get('zbxMode') == 2 else '')
                    txt = '[ACK %d] %s' % (ack, txt)
                self.send_ev(r['channel'] or st['defaultChannel'], txt, zsrc, 'A', v, ack)

    def test(self, i):
        if not (0 <= i < len(self.cfg['rules'])): return 'Regel unbekannt'
        r = self.cfg['rules'][i]
        ch = r.get('channel') or self.cfg['settings']['defaultChannel']
        if self.find(ch) < 0: return 'kein gültiger Kanal'
        tpl = r.get('text') or self.cfg['settings'].get('alertText') or '{state} {device}: {metric} {value}{unit} ({op}{threshold}{unit})'
        name = 'Gateway' if r['device'] == INTERNAL else self.cfg['devices'][r['device']]['name']
        txt = self.render(tpl, name, r['path'], self.rule_state[i]['value'], r['op'], r['threshold'], 'TEST')
        if '{state}' not in tpl: txt = 'TEST ' + txt
        self.emit(ch, txt, False)
        return None

    def json(self):
        c = json.loads(json.dumps(self.cfg))
        for d in c['devices']:
            if d.get('pass'): d['pass'] = '***'
        devs = []
        for i, s in enumerate(self.dev_state):
            devs.append({'idx': i, 'polled': s.get('polled', False), 'online': s.get('online', False), 'offline': s.get('offline', False),
                         'fails': s.get('fails', 0), 'lastOkAgoS': int(time.time() - s['lastOk']) if s.get('lastOk') else None,
                         'busy': s.get('busy', False), 'route': s.get('route', ''),
                         'error': s.get('error', ''), 'values': s.get('values', [])})
        now = time.time()
        pst = []
        for i, s in enumerate(self.ping_state):
            st = 'unknown' if not s.get('checked') else ('down' if s.get('down') else ('up' if s.get('lastOkFlag') else 'failing'))
            pst.append({'idx': i, 'state': st, 'rttMs': s.get('rtt') if s.get('lastOkFlag') else None,
                        'noReplyS': int(now - s.get('lastOk', now)) if s.get('checked') and not s.get('lastOkFlag') else None,
                        'ip': '', 'error': s.get('error', ''), 'ack': self.ack_of('ping', i)})
        self.evaluate(INTERNAL, 'Gateway', self.internal_values())   # wie der Poller alle 60 s
        rules = [{'idx': i, 'state': s['state'], 'value': s['value'], 'lastEvent': s['lastEvent'], 'lastEventAgoS': None,
                  'ack': self.ack_of('rule', i)} for i, s in enumerate(self.rule_state)]
        acks = [{'id': a['id'], 'kind': a['kind'], 'src': a['src'], 'channel': a['channel'], 'text': a['text'],
                 'ageS': int(now - a['t']), 'repeats': a['repeats'], 'max': 3, 'cleared': a['cleared']} for a in self.acks]
        internal = {'idx': INTERNAL, 'polled': True, 'online': True, 'offline': False, 'fails': 0, 'lastOkAgoS': 3,
                    'error': '', 'values': self.internal_values()}
        return {'config': c, 'status': {'devices': devs, 'internal': internal, 'rules': rules, 'pings': pst, 'acks': acks,
                                        'limit': {'used': self.sent, 'max': self.cfg['settings']['maxPerHour']}}}


import subprocess


def real_ping(host):
    """Echter Ping über Windows-ping (1 Paket, 1 s). Rückgabe (ok, rtt_ms, fehler)."""
    try:
        # Windows-ping schreibt in der OEM-Codepage (cp850), nicht cp1252
        out = subprocess.run(['ping', '-n', '1', '-w', '1000', host], capture_output=True, timeout=5).stdout.decode('cp850', 'replace')
    except Exception as e:
        return False, None, str(e)[:40]
    m = re.search(r'(?:Zeit|time)[=<](\d+)\s*ms', out)
    if m and ('TTL=' in out or 'ttl=' in out):
        return True, int(m.group(1)), ''
    return False, None, 'keine Antwort'


PING_CHECK_S = 2   # im Mock schneller als die Firmware (10 s), damit der Test kurz bleibt


def ping_loop():
    while True:
        time.sleep(1)
        with lock:
            now = time.time()
            targets = [(i, dict(p)) for i, p in enumerate(smm.cfg.get('pings', []))
                       if i < len(smm.ping_state) and now - smm.ping_state[i].get('lastCheck', 0) >= p.get('intervalS', PING_CHECK_S)]
        for i, p in targets:
            if not p.get('enabled', True): continue
            ok, rtt, err = real_ping(p['host'])
            with lock:
                if i >= len(smm.ping_state): continue
                s = smm.ping_state[i]
                now = time.time()
                s.setdefault('lastOk', now)
                s.update(checked=True, lastOkFlag=ok, error=err, lastCheck=now)
                if ok:
                    s['rtt'] = rtt; s['lastOk'] = now
                    if s.get('down'):
                        s['down'] = False
                        smm.emit('', smm.render(smm.cfg['settings'].get('pingUpText') or 'PING-OK {device} ({host}) antwortet wieder',
                                                p['name'], 'x', None, 0, 0, 'PING-OK').replace('{host}', p['host']))
                elif not s.get('down') and now - s['lastOk'] >= p['thresholdS']:
                    s['down'] = True
                    secs = int(now - s['lastOk'])
                    tpl = smm.cfg['settings'].get('pingDownText') or 'PING-ALARM {device} ({host}) seit {value} s ohne Antwort'
                    smm.emit('', smm.render(tpl, p['name'], 'x', secs, 0, 0, 'PING-ALARM').replace('{host}', p['host']))


logs['smlog'] = []
seq['smlog'] = 1
CAP['smlog'] = 100
smm = FakeSm()


def mock_ota(body, ctype):
    boundary = ctype.split('boundary=')[1].encode()
    force, data = False, None
    for part in body.split(b'--' + boundary):
        head, _, content = part.partition(b'\r\n\r\n')
        content = content[:-2] if content.endswith(b'\r\n') else content
        if b'name="force"' in head: force = content.strip() == b'1'
        if b'name="firmware"' in head: data = content
    if data is None: return 400, {'error': 'keine Datei empfangen'}
    m = MARKER.search(data)
    if not m: return 400, {'error': 'kein Gateway-Firmware-Merkmal gefunden (falsche Datei?)'}
    pid, ver = m.group(1).decode(), m.group(2).decode()
    if pid != GW_PROJECT: return 400, {'error': 'falsche Firmware: %s, dieses Gerät ist %s' % (pid, GW_PROJECT)}
    if ver_tuple(ver) < ver_tuple(GW_VERSION) and not force:
        return 400, {'error': 'Downgrade %s -> %s abgelehnt ("Downgrade erzwingen" setzen)' % (GW_VERSION, ver)}
    return 200, {'ok': True, 'version': ver, 'bytes': len(data)}

cli = FakeCli()
PROFILE = load_profile()
jobs = {}
job_id = [1]


def run_job(jid, cmds, quiet):
    for c in cmds:
        time.sleep(0.01)
        with lock:
            rep = cli.run(c)
        is_query = c.startswith('get ') or c.startswith('region get ') or c in ('region', 'ver', 'neighbors')
        if not (quiet and is_query):
            masked = 'password ********' if c.startswith('password ') else ('set prv.key ********' if c.startswith('set prv.key ') else c)
            push('console', 'c', masked)
            if rep:
                if c.startswith('password '): rep = 'password now: ********'
                for part in rep.rstrip('\n').split('\n'):
                    push('console', 'r', part)
        with lock:
            jobs[jid]['replies'].append({'cmd': c, 'reply': rep})
    with lock:
        jobs[jid]['done'] = True


def submit(cmds, quiet):
    with lock:
        jid = job_id[0]; job_id[0] += 1
        jobs[jid] = {'id': jid, 'done': False, 'replies': []}
    threading.Thread(target=run_job, args=(jid, cmds, quiet), daemon=True).start()
    return jid


push('console', 's', 'Gateway gestartet, MeshCore v1.17.1')
for i in range(5):
    push('packets', 'R', f'len=40 ADVERT(4) route=F hops={i} payload=30 SNR=7.5 RSSI=-90 score=900')
    push('packets', 'T', f'len=40 ADVERT(4) route=F hops={i+1} payload=30')


# hostname '' = automatisch, folgt dem Gerätenamen (E32)
BOOT_ID = os.urandom(4).hex()
gwcfg = {'ssid': 'TestNetz', 'pass': 'geheim123', 'hostname': '', 'reboots': 0,
         'ip': '', 'mask': '', 'gw': '', 'dns1': '', 'dns2': '', 'pending': False}
IP_RE = re.compile(r'^(25[0-5]|2[0-4]\d|1?\d?\d)(\.(25[0-5]|2[0-4]\d|1?\d?\d)){3}$')
ipcheck = {'state': 'idle', 'detail': ''}


class FakeRemote:
    """Fernzugriff auf andere Repeater (E42) wie src/RemoteAdmin.cpp, Funk simuliert:
    Antwort nach ~1 s; Repeater 'Funkloch' antwortet nie; Befehl 'stumm' bleibt ohne Antwort."""
    REP = [  # key, name, typ, hops, admin-pw, gast-pw, akku mV, temp, laufzeit s
        ('a1b2c3d4' + '11' * 28, 'Solar-Hof', 'repeater', 2, 'adminpw', 'gast', 3712, 31.5, 93784),
        ('c1d2e3f4' + '22' * 28, 'Kirchturm', 'repeater', 0, 'turm', '', 4105, 18.0, 1800),
        ('d4c3b2a1' + '33' * 28, 'Funkloch', 'repeater', 5, 'x', 'x', 0, 0, 0),
        ('e5e5e5e5' + '44' * 28, 'Vereinsheim', 'room', 1, 'raum', 'raum', 0, 0, 0)]

    def __init__(self):
        self.jobs = {}; self.next = 1; self.con = None; self.sess = {}; self.delay = 1.0

    def rep(self, key):
        return next((r for r in self.REP if r[0] == key), None)

    def name(self, key):
        r = self.rep(key)
        return r[1] if r else key[:8].upper()

    def known(self):
        return [{'key': r[0], 'name': r[1], 'type': r[2], 'hops': r[3], 'snr': 6.5, 'agoS': 40 + 30 * i} for i, r in enumerate(self.REP)]

    def new_job(self, kind):
        jid = self.next; self.next += 1
        self.jobs[jid] = {'state': 'running', 'kind': kind, 'ms': 0, 'flood': True, 'hops': 0, 'admin': False,
                          'hidden': False, 'error': '', 'reply': '', 't': time.time(), 'poll': None}
        return jid

    def finish(self, jid, **kw):
        j = self.jobs[jid]; j.update(kw); j['ms'] = int((time.time() - j['t']) * 1000)
        if j['state'] == 'running': j['state'] = 'done'

    def login(self, key, pw):
        if len(pw) > 15: return 0, 'Passwort max. 15 Zeichen (Grenze von MeshCore)'
        jid = self.new_job('login')
        push('console', 's', '[%s] Anmeldung …' % self.name(key))
        def run():
            time.sleep(self.delay)
            with lock:
                r = self.rep(key)
                if not r or r[1] == 'Funkloch' or pw not in (r[4], r[5]):
                    self.finish(jid, state='failed', error='keine Antwort (Passwort falsch oder Repeater nicht erreichbar)')
                    push('console', 's', '[%s] %s' % (self.name(key), self.jobs[jid]['error'])); return
                admin = pw == r[4]
                self.sess[key] = {'admin': admin, 'hops': r[3]}
                self.con = key
                self.finish(jid, admin=admin)
                push('console', 's', '[%s] angemeldet als %s (Flood, %d ms)%s' % (r[1], 'Admin' if admin else 'Gast',
                     self.jobs[jid]['ms'], '' if admin else ' - CLI braucht das Admin-Passwort'))
        threading.Thread(target=run, daemon=True).start()
        return jid, ''

    def cmd(self, c):
        if not self.con: return 0, 'nicht angemeldet'
        if not self.sess[self.con]['admin']: return 0, 'nur als Gast angemeldet - CLI braucht das Admin-Passwort'
        if any(j['kind'] == 'cli' and j['state'] == 'running' for j in self.jobs.values()): return 0, 'vorheriger Befehl läuft noch'
        if not c: return 0, 'leerer Befehl'
        key = self.con; name = self.name(key)
        jid = self.new_job('cli')
        hidden = c.startswith(EV_SECRET)
        self.jobs[jid]['hidden'] = hidden
        push('console', 'c', '[%s] %s' % (name, 'password ********' if c.startswith('password ') else c))
        def run():
            time.sleep(self.delay)
            with lock:
                if c == 'stumm':
                    self.finish(jid, state='failed', error='keine Antwort (nochmal senden)')
                    push('console', 's', '[%s] keine Antwort (nochmal senden)' % name); return
                rep = {'ver': 'v1.18.0 (Build: 15-Oct-2026)', 'get name': '> ' + name, 'clock': '12:00 - 16/10/2026 UTC',
                       'get prv.key': '> ' + '9f' * 32}.get(c, 'OK' if c.startswith('set ') else 'Unknown command')
                self.finish(jid, reply=rep, flood=False, hops=self.sess[key]['hops'])
                push('console', 's' if hidden else 'r', '[%s] %s' % (name, '(Antwort enthält ein Geheimnis, nur in der Konsole angezeigt)' if hidden else rep))
        threading.Thread(target=run, daemon=True).start()
        return jid, ''

    def poll(self, key, pw, done):
        """Überwachung: done(ok, values|fehler, route) nach der simulierten Funkzeit"""
        def run():
            time.sleep(self.delay)
            with lock:
                r = self.rep(key)
                if not r or r[1] == 'Funkloch' or pw not in (r[4], r[5]):
                    done(False, 'keine Antwort auf Anmeldung (Passwort falsch oder nicht erreichbar)', ''); return
                vals = [('repeater.battery', r[6] / 1000.0), ('repeater.temperature', r[7]), ('repeater.uptimeH', r[8] / 3600.0),
                        ('repeater.noiseFloor', -115)]
                done(True, vals, 'direkt %d Hop%s, 1200 ms' % (r[3], '' if r[3] == 1 else 's') if r[3] else 'Flood, 1500 ms')
        threading.Thread(target=run, daemon=True).start()

    def console(self):
        if not self.con: return None
        s = self.sess[self.con]
        return {'key': self.con, 'name': self.name(self.con), 'admin': s['admin'], 'flood': False, 'hops': s['hops'], 'idleS': 3,
                'busy': any(j['kind'] == 'cli' and j['state'] == 'running' for j in self.jobs.values())}


rmt = FakeRemote()


class FakeZbx:
    """Zabbix-Weiterleitung (E41) wie src/ZbxForwarder.cpp, Zabbix selbst simuliert:
    Server 'offline' = nicht erreichbar, Host mit 'unbekannt' = abgelehnt, sonst angenommen."""
    MARK = re.compile(r' #Z1;([^;]{1,7});([AEWOFNHT]);(-?[0-9.]*);(\d{0,2})\s*$')

    def __init__(self):
        self.cfg = {'enabled': False, 'server': '', 'port': 10051, 'rxHost': '', 'hostPrefix': '', 'channels': '',
                    'autoAck': True, 'forwardUnmarked': False, 'heartbeatMin': 5}
        self.st = {'received': 0, 'forwarded': 0, 'rejected': 0, 'errors': 0, 'acks': 0, 'dropped': 0, 'pending': 0,
                   'lastOkAgoS': None, 'heartbeatAgoS': None, 'lastError': '', 'lastTest': ''}
        self.pend = []

    def send(self, host, key, value):
        if self.cfg['server'] in ('', 'offline'): return -1, 'keine Verbindung zu %s:%d' % (self.cfg['server'], self.cfg['port'])
        if 'unbekannt' in host: return 0, 'abgelehnt (Host "%s" / Item %s fehlt, nicht Trapper oder Allowed hosts?)' % (host, key)
        return 1, ''

    def on_text(self, ch_idx, channel, text):
        c = self.cfg
        if not c['enabled']: return
        allowed = [x.strip() for x in c['channels'].split(',') if x.strip()]
        if allowed and channel not in allowed: return
        sender, _, body = text.partition(': ')
        if not _: return
        m = self.MARK.search(body)
        if not m and not c['forwardUnmarked']: return
        self.st['received'] += 1
        if m:
            ev = {'src': m.group(1), 'ev': m.group(2), 'state': {'A': 'ALARM', 'E': 'ALARM', 'W': 'ALARM', 'F': 'ALARM', 'O': 'OK', 'N': 'OK'}.get(m.group(2), 'INFO')}
            if m.group(3): ev['value'] = float(m.group(3))
            if m.group(4): ev['ack'] = int(m.group(4))
            ev.update(text=body[:m.start()], gw=sender, channel=channel, rx=cli.p['name'])
            item = {'host': c['hostPrefix'] + sanitize_host(sender), 'key': 'meshcore.event', 'value': json.dumps(ev, ensure_ascii=False),
                    'ack': int(m.group(4)) if (m.group(4) and c['autoAck']) else 0, 'sender': sender, 'ch': ch_idx, 'chname': channel}
        else:
            item = {'host': c['rxHost'] or eff_host(), 'key': 'meshcore.rx.unparsed', 'value': json.dumps({'text': body, 'gw': sender}),
                    'ack': 0, 'sender': sender, 'ch': ch_idx, 'chname': channel}
        self.pend.append(item)
        self.process()

    def process(self):
        keep = []
        for it in self.pend:
            r, err = self.send(it['host'], it['key'], it['value'])
            if r == 1:
                self.st['forwarded'] += 1; self.st['lastOk'] = time.time(); self.st['lastError'] = ''
                push('smlog', 'A', 'Zabbix <- %s %s: %s' % (it['host'], it['key'], it['value'][:80]))
                if it['ack']:
                    a = '@[%s] ACK %d' % (it['sender'], it['ack'])
                    if it['sender'].lower() == cli.p['name'].lower():   # eigene Meldung: lokal bestätigen
                        smm.on_channel_text(it['chname'], 'Zabbix: ' + a)
                    else:
                        push('messages', 'S', '\t'.join([str(it['ch']), it['chname'], '', '0', cli.p['name'] + ': ' + a]))
                        push('smlog', 'A', '%s -> %s' % (it['chname'], a))
                    self.st['acks'] += 1
            elif r == 0:
                self.st['rejected'] += 1; self.st['lastError'] = err; push('smlog', 'X', 'Zabbix: ' + err)
            else:
                self.st['errors'] += 1; self.st['lastError'] = err; push('smlog', 'X', 'Zabbix: ' + err); keep.append(it)
        self.pend = keep

    def save(self, j):
        if not re.fullmatch(r'[A-Za-z0-9._-]*', j.get('server', '')): return 'Zabbix-Server: IP oder Hostname'
        if j.get('enabled') and not j.get('server'): return 'Zabbix-Server fehlt'
        self.cfg.update(j)
        self.process()   # nach Korrektur: wartende gleich senden
        return None

    def test(self):
        r, err = self.send(self.cfg['rxHost'] or eff_host(), 'meshcore.rx.heartbeat', '123')
        self.st['lastTest'] = ('OK: %s / meshcore.rx.heartbeat angenommen' % (self.cfg['rxHost'] or eff_host())) if r == 1 else 'Fehler: ' + err
        if r == 1: self.st['hb'] = time.time()

    def json(self):
        s = dict(self.st)
        s['pending'] = len(self.pend)
        s['lastOkAgoS'] = int(time.time() - s.pop('lastOk')) if 'lastOk' in s else None
        s['heartbeatAgoS'] = int(time.time() - s.pop('hb')) if 'hb' in s else None
        s['rxHost'] = self.cfg['rxHost'] or eff_host()
        return {'config': dict(self.cfg), 'status': s}


zbxm = FakeZbx()


def mock_probe(ip):
    # wie net_check::probe gegen das Mock-Netz 192.168.1.0/24 (Gateway .50, belegt: .1 Router und .99)
    if ip == '192.168.1.50': return 'own', 'aktuelle Adresse des Gateways'
    if not ip.startswith('192.168.1.'): return 'uncheckable', 'anderes Subnetz als 192.168.1.50'
    if ip in ('192.168.1.1', '192.168.1.99'): return 'present', 'antwortet auf ARP (MAC 11:22:33:44:55:66)'
    return 'absent', 'keine Antwort auf ARP und Ping'


def apply_ip(f):
    # wie GatewayConfig::applyIp (vereinfacht). Rückgabe Fehlertext oder None
    ip = f('ip').strip()
    if not ip:
        gwcfg.update(ip='', mask='', gw='', dns1='', dns2='', pending=False); return None
    vals = {k: f(k).strip() for k in ('ip', 'mask', 'gw', 'dns1', 'dns2')}
    if not IP_RE.match(vals['ip']): return 'IP-Adresse ungültig'
    if not IP_RE.match(vals['mask']): return 'Subnetzmaske ungültig (z. B. 255.255.255.0)'
    if not IP_RE.match(vals['gw']): return 'Gateway (Router) ungültig'
    n = lambda s: [int(x) for x in s.split('.')]
    m, a, g = n(vals['mask']), n(vals['ip']), n(vals['gw'])
    if [x & y for x, y in zip(a, m)] != [x & y for x, y in zip(g, m)]: return 'Gateway liegt nicht im Subnetz der IP-Adresse'
    if ip != gwcfg['ip']: gwcfg['pending'] = True
    gwcfg.update(vals)
    return None


def sanitize_host(name):
    # wie NetworkManager::sanitizeHostname
    out = ''
    for c in name.lower():
        if c.isascii() and (c.isalnum() or c == '-'):
            out += c
        elif c in ' _' and out and out[-1] != '-':
            out += '-'
    out = out.lstrip('-')[:31].rstrip('-')
    return out or 'meshcore-gw'


def host_setting(inp):
    # wie GatewayConfig::hostSetting
    inp = inp.strip()
    if not inp:
        return ''
    h = sanitize_host(inp)
    return '' if h == 'meshcore-gw' else h


def eff_host():
    return gwcfg['hostname'] or sanitize_host(cli.p['name'])


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
            p = cli.p
            return self.send_json({'gateway': {'version': GW_VERSION, 'uptime_ms': 1000, 'free_heap': 180000, 'cpu_mhz': 240, 'chip_temp_c': CHIP_TEMP,
                                   'project': GW_PROJECT, 'partition': 'app0', 'image_state': 'gültig',
                                   'min_free_heap': 150000, 'psram_total': 2097152, 'psram_free': 1800000, 'psram_min_free': 1750000},
              'node': {'name': p['name'], 'pubkey': 'ab' * 32, 'role': 'repeater', 'firmware': 'v1.18.0', 'build_date': '15 Oct 2026',
                       'lat': p['lat'], 'lon': p['lon'], 'forwarding': p['repeat'] == 'on'},
              'radio': {'freq': p['freq'], 'bw': p['bw'], 'sf': p['sf'], 'cr': p['cr'], 'tx_power': p['tx']},
              'stats_core': {'battery_mv': 0, 'uptime_secs': 3725, 'errors': 0, 'queue_len': 0},
              'stats_radio': {'noise_floor': -112, 'last_rssi': -90, 'last_snr': 7.5, 'tx_air_secs': 3, 'rx_air_secs': 12},
              'stats_packets': {'recv': 10, 'sent': 5, 'flood_tx': 5, 'direct_tx': 0, 'flood_rx': 10, 'direct_rx': 0, 'recv_errors': 0},
              'wifi': {'mode': 'station', 'ssid': gwcfg['ssid'], 'ip': '192.168.1.50', 'rssi': -55, 'mac': 'AA:BB:CC:DD:EE:FF', 'hostname': eff_host(), 'hostname_auto': not gwcfg['hostname'],
                       'static': False, 'mask': '255.255.255.0', 'gw': '192.168.1.1', 'dns': '192.168.1.1',
                       'cfg': {k: gwcfg[k] for k in ('ip', 'mask', 'gw', 'dns1', 'dns2', 'pending')}, 'ip_note': ''}})
        if u.path == '/api/wifi/checkip':
            with lock:
                return self.send_json(dict(ipcheck))
        if u.path == '/api/zbx':
            with lock:
                return self.send_json(zbxm.json())
        if u.path == '/api/backup/extra':
            secrets = q.get('secrets', ['0'])[0] == '1'
            with lock:
                sm = json.loads(json.dumps(smm.cfg)) if secrets else smm.json()['config']
                out = {'hostname': gwcfg['hostname'], 'wifi': dict({'ssid': gwcfg['ssid']}, **{k: gwcfg[k] for k in ('ip', 'mask', 'gw', 'dns1', 'dns2')}),
                       'acl': [{'perm': v, 'pubkey': k} for k, v in cli.acl.items()], 'sensormeter': sm, 'zabbix': dict(zbxm.cfg)}
                if secrets:
                    out['wifi']['pass'] = gwcfg['pass']; out['adminPassword'] = cli.p['password']
            return self.send_json(out)
        if u.path == '/api/log/info':
            with lock:
                n = len(evlog['text'].encode())
            return self.send_json({'fileBytes': n, 'pendingBytes': 0, 'maxBytes': 131072, 'lastFlushAgoS': 5,
                                   'flushIntervalS': 900, 'flushes': 1, 'dropped': 0})
        if u.path == '/api/log/download':
            with lock:
                b = evlog['text'].encode()
            self.send_response(200); self.send_header('Content-Type', 'text/plain; charset=utf-8')
            if 'inline' not in q:
                self.send_header('Content-Disposition', 'attachment; filename="meshcore-gw-log-%s.txt"' % eff_host())
            self.send_header('Content-Length', str(len(b))); self.end_headers(); self.wfile.write(b); return
        if u.path in ('/api/console', '/api/packets', '/api/messages', '/api/sm/log'):
            name = 'smlog' if u.path == '/api/sm/log' else u.path.split('/')[-1]
            since = int(q.get('since', ['0'])[0])
            with lock:
                L = logs[name]
                oldest = L[0]['s'] if L else seq[name]
                gap = since > 0 and since < oldest
                out = [l for l in L if l['s'] >= max(since, oldest)][:60]
                nxt = (out[-1]['s'] + 1) if out else max(since, oldest)
            return self.send_json({'lines': out, 'next': nxt, 'gap': gap})
        if u.path == '/api/cli/job':
            jid = int(q.get('id', ['0'])[0])
            with lock:
                j = jobs.get(jid)
                j = json.loads(json.dumps(j)) if j else None
            return self.send_json(j) if j else self.send_json({'error': 'Auftrag unbekannt'}, 404)
        if u.path in ('/api/v1/channels', '/api/v1/messages'):   # Kanal-API (E43), wie WebUi.cpp
            key = gwcfg.get('apiKey', '')
            if not key: return self.send_json({'error': 'Kanal-API ist ausgeschaltet (System > Kanal-API)'}, 403)
            got = self.headers.get('X-API-Key', '')
            a = self.headers.get('Authorization', '')
            if not got and a.startswith('Bearer '): got = a[7:]
            if got.strip() != key: return self.send_json({'error': 'API-Schlüssel fehlt oder falsch (Header X-API-Key)'}, 401)
            with lock:
                if u.path == '/api/v1/channels':
                    cl = [{k: c[k] for k in ('idx', 'name', 'kind', 'scope')} for c in chans.json()['channels']]
                    return self.send_json({'boot': BOOT_ID, 'gateway': cli.p['name'], 'channels': cl})
                since = int(q.get('since', ['0'])[0]); flt = q.get('channel', [''])[0]
                L = logs['messages']
                oldest = L[0]['s'] if L else seq['messages']
                gap = 0 < since < oldest
                rows = [l for l in L if l['s'] >= max(since, oldest)][:50]
                nxt = (rows[-1]['s'] + 1) if rows else max(since, oldest)
                out = []
                for l in rows:
                    f = l['x'].split('\t', 4)
                    if l['t'] not in 'MS' or len(f) < 5 or (flt and f[1] != flt): continue
                    snd, sep, txt = f[4].partition(': ')
                    if not sep: snd, txt = '', f[4]
                    out.append({'seq': l['s'], 'time': l['e'], 'dir': 'rx' if l['t'] == 'M' else 'tx', 'channelIdx': int(f[0]), 'channel': f[1],
                                'sender': snd, 'text': txt, 'snr': float(f[2]) if f[2] else None, 'hops': int(f[3] or 0)})
            return self.send_json({'boot': BOOT_ID, 'messages': out, 'next': nxt, 'gap': gap})
        if u.path == '/api/apikey':
            k = gwcfg.get('apiKey', '')
            return self.send_json({'enabled': bool(k), 'key': k, 'sendChannel': gwcfg.get('apiSendCh', ''), 'sendPerHour': 10})
        if u.path == '/api/remote':
            with lock:
                return self.send_json({'known': rmt.known(), 'console': rmt.console()})
        if u.path == '/api/remote/job':
            with lock:
                j = rmt.jobs.get(int(q.get('id', ['0'])[0]))
                if not j: return self.send_json({'error': 'Auftrag unbekannt'}, 404)
                out = {k: j[k] for k in ('state', 'ms', 'flood', 'hops', 'admin', 'hidden', 'error', 'reply')}
                if out['state'] == 'running': out['ms'] = int((time.time() - j['t']) * 1000)
            return self.send_json(out)
        if u.path == '/api/profile':
            return self.send_json(PROFILE)
        if u.path == '/api/channels':
            with lock:
                return self.send_json(chans.json())
        if u.path == '/api/sm':
            with lock:
                return self.send_json(smm.json())
        self.send_response(404); self.end_headers()

    def do_POST(self):
        u = urlparse(self.path)
        raw = self.rfile.read(int(self.headers.get('Content-Length', 0)))
        if u.path == '/api/ota':
            code, obj = mock_ota(raw, self.headers.get('Content-Type', ''))
            return self.send_json(obj, code)
        body = raw.decode()
        form = parse_qs(body, keep_blank_values=True)
        if u.path == '/api/wifi':
            ssid = form.get('ssid', [''])[0].strip()
            if not ssid:
                return self.send_json({'error': 'ssid fehlt'}, 400)
            with lock:
                if 'ip' in form:
                    err = apply_ip(lambda k: form.get(k, [''])[0])
                    if err: return self.send_json({'error': err}, 400)
                gwcfg['ssid'] = ssid
                if form.get('pass', [''])[0]: gwcfg['pass'] = form['pass'][0]
                if 'hostname' in form: gwcfg['hostname'] = host_setting(form['hostname'][0])
                gwcfg['reboots'] += 1
            return self.send_json({'ok': True, 'reboot': True})
        if u.path == '/api/wifi/checkip':
            ip = form.get('ip', [''])[0].strip()
            if not IP_RE.match(ip): return self.send_json({'error': 'IP-Adresse ungültig'}, 400)
            with lock:
                ipcheck.update(state='running', detail='')
            def done(ip=ip):
                time.sleep(0.8)
                st, d = mock_probe(ip)
                with lock: ipcheck.update(state=st, detail=d)
            threading.Thread(target=done, daemon=True).start()
            return self.send_json({'ok': True})
        if u.path == '/api/sm/ack':
            with lock:
                err = smm.ack_web(int(form.get('id', ['0'])[0]))
            return self.send_json({'error': err}, 400) if err else self.send_json({'ok': True})
        if u.path == '/api/mock/chanmsg':   # nur Mock: Kanalnachricht "von außen" empfangen
            f = lambda k: form.get(k, [''])[0]
            with lock:
                ch = smm.find(f('channel'))
                if ch < 0: return self.send_json({'error': 'Kanal unbekannt'}, 404)
                push('messages', 'M', '\t'.join([str(ch), f('channel'), '7.0', '1', f('text')]))
                smm.on_channel_text(f('channel'), f('text'))
                zbxm.on_text(ch, f('channel'), f('text'))
            return self.send_json({'ok': True})
        if u.path == '/api/zbx/save':
            try:
                j = json.loads(form.get('json', [''])[0])
            except Exception as e:
                return self.send_json({'error': 'JSON: ' + str(e)}, 400)
            with lock:
                err = zbxm.save(j)
            return self.send_json({'error': err}, 400) if err else self.send_json({'ok': True})
        if u.path == '/api/zbx/test':
            with lock:
                zbxm.test()
            return self.send_json({'ok': True})
        if u.path == '/api/mock/ackrepeat':   # nur Mock: alle offenen um eine Wiederholung weiter
            with lock:
                smm.repeat_all()
            return self.send_json({'ok': True})
        if u.path == '/api/cli':
            return self.send_json({'ok': True, 'id': submit([form.get('cmd', [''])[0]], False)})
        if u.path == '/api/cli/batch':
            cmds = form.get('cmd', [])
            if not cmds or len(cmds) > 40:
                return self.send_json({'error': '1 bis 40 Befehle pro Auftrag'}, 400)
            return self.send_json({'ok': True, 'id': submit(cmds, form.get('quiet', ['0'])[0] == '1')})
        if u.path == '/api/channels':
            f = lambda k: form.get(k, [''])[0]
            a = f('action')
            with lock:
                if a == 'add':
                    idx, err = chans.add(f('kind'), f('name'), f('key'), f('scope'))
                    return self.send_json({'error': err}, 400) if err else self.send_json({'ok': True, 'idx': idx})
                i = int(f('idx') or -1)
                if not (0 <= i < chans.MAX) or not chans.ch[i]:
                    return self.send_json({'error': 'Kanal unbekannt'}, 404)
                if a == 'delete':
                    chans.ch[i] = None
                    return self.send_json({'ok': True})
                if a == 'scope':
                    sc = f('scope')
                    if sc and not re.fullmatch(r'#?[A-Za-z0-9._-]+', sc):
                        return self.send_json({'error': 'Scope: Regionsname aus a-z, 0-9, - _ .'}, 400)
                    chans.ch[i]['scope'] = sc
                    return self.send_json({'ok': True})
            return self.send_json({'error': 'unbekannte Aktion'}, 400)
        if u.path == '/api/sm/save':
            try:
                j = json.loads(form.get('json', [''])[0])
            except Exception as e:
                return self.send_json({'error': 'JSON: ' + str(e)}, 400)
            with lock:
                err = smm.save(j)
            return self.send_json({'error': err}, 400) if err else self.send_json({'ok': True})
        if u.path == '/api/apikey':
            act = form.get('action', [''])[0]
            if act == 'new': gwcfg['apiKey'] = os.urandom(16).hex(); evadd('SYS', 'Kanal-API: neuer Schlüssel erzeugt (alter ungültig)')
            elif act == 'off': gwcfg['apiKey'] = ''; evadd('SYS', 'Kanal-API ausgeschaltet')
            elif act == 'sendch':
                ch = form.get('channel', [''])[0].strip()
                if ch and smm.find(ch) < 0: return self.send_json({'error': 'Kanal unbekannt'}, 400)
                gwcfg['apiSendCh'] = ch
            else: return self.send_json({'error': 'action = new, off oder sendch'}, 400)
            return self.send_json({'ok': True, 'enabled': bool(gwcfg['apiKey']), 'key': gwcfg['apiKey'],
                                   'sendChannel': gwcfg.get('apiSendCh', ''), 'sendPerHour': 10})
        if u.path == '/api/v1/send':   # E44, wie WebUi.cpp handleApiSend
            key = gwcfg.get('apiKey', '')
            if not key: return self.send_json({'error': 'Kanal-API ist ausgeschaltet (System > Kanal-API)'}, 403)
            if self.headers.get('X-API-Key', '').strip() != key: return self.send_json({'error': 'API-Schlüssel fehlt oder falsch (Header X-API-Key)'}, 401)
            allowed = gwcfg.get('apiSendCh', '')
            ch = form.get('channel', [''])[0].strip()
            if not allowed: return self.send_json({'error': 'Senden über die API ist nicht freigegeben (System > Kanal-API)'}, 403)
            if ch != allowed: return self.send_json({'error': 'Senden nur in Kanal "%s" erlaubt' % allowed}, 403)
            with lock:
                i = smm.find(ch)
                if i < 0: return self.send_json({'error': 'Kanal existiert nicht mehr'}, 404)
                t = ' '.join(form.get('text', [''])[0].replace('\t', ' ').split('\n')).strip()
                if not t: return self.send_json({'error': 'leerer Text'}, 400)
                room = 160 - len(cli.p['name'].encode()) - 2
                b = t.encode()
                cut = len(b) > room
                if cut: t = b[:room].decode('utf-8', 'ignore')
                now = time.time()
                gwcfg['apiSent'] = [x for x in gwcfg.get('apiSent', []) if now - x < 3600]
                if len(gwcfg['apiSent']) >= 10: return self.send_json({'error': 'Sendelimit erreicht (10 pro Stunde)'}, 429)
                gwcfg['apiSent'].append(now)
                push('messages', 'S', '\t'.join([str(i), ch, '', '0', cli.p['name'] + ': ' + t]))
                evadd('API', 'Senden in %s von %s%s' % (ch, self.client_address[0], ' (gekürzt)' if cut else ''))
            return self.send_json({'ok': True, 'truncated': cut, 'text': t})
        if u.path == '/api/remote/login':
            key = form.get('key', [''])[0].strip().lower()
            if not re.fullmatch(r'[0-9a-f]{64}', key): return self.send_json({'error': 'Öffentlicher Schlüssel: 64 Hex-Zeichen'}, 400)
            with lock:
                jid, err = rmt.login(key, form.get('pass', [''])[0])
            return self.send_json({'ok': True, 'id': jid}) if jid else self.send_json({'error': err}, 400)
        if u.path == '/api/remote/cmd':
            with lock:
                jid, err = rmt.cmd(form.get('cmd', [''])[0].strip())
            return self.send_json({'ok': True, 'id': jid}) if jid else self.send_json({'error': err}, 400)
        if u.path == '/api/remote/logout':
            with lock:
                if rmt.con: push('console', 's', '[%s] Fernkonsole beendet' % rmt.name(rmt.con))
                rmt.con = None
            return self.send_json({'ok': True})
        if u.path == '/api/sm/poll':
            i = int(form.get('idx', ['-1'])[0])
            with lock:
                if not (0 <= i < len(smm.cfg['devices'])): return self.send_json({'error': 'Gerät unbekannt'}, 404)
                smm.poll(i)
            return self.send_json({'ok': True})
        if u.path == '/api/sm/test':
            with lock:
                err = smm.test(int(form.get('idx', ['-1'])[0]))
            return self.send_json({'error': err}, 400) if err else self.send_json({'ok': True})
        if u.path == '/api/channels/send':
            i = int(form.get('idx', ['-1'])[0]); t = form.get('text', [''])[0].strip()
            with lock:
                c = chans.ch[i] if 0 <= i < chans.MAX else None
            if not c: return self.send_json({'error': 'Kanal unbekannt'}, 404)
            if not t or len(t.encode()) > 160: return self.send_json({'error': 'Text 1-160 Zeichen'}, 400)
            push('messages', 'S', '\t'.join([str(i), c['name'], '', '0', cli.p['name'] + ': ' + t]))
            return self.send_json({'ok': True})
        if u.path == '/api/reboot':
            return self.send_json({'ok': True})
        if u.path == '/api/log/clear':
            with lock:
                evlog['text'] = ''; evadd('LOG', 'Log gelöscht')
            return self.send_json({'ok': True})
        if u.path == '/api/backup/apply':
            f = lambda k: form.get(k, [''])[0]
            pw = f('adminPassword')
            if pw and (len(pw) > 15 or ' ' in pw):
                return self.send_json({'error': 'Admin-Passwort: 1-15 Zeichen, keine Leerzeichen'}, 400)
            with lock:
                if 'ssid' in form:
                    if 'pass' not in form and f('ssid') != gwcfg['ssid']:
                        return self.send_json({'error': 'WLAN-Passwort fehlt im Backup'}, 400)
                    if 'ip' in form:
                        err = apply_ip(f)
                        if err: return self.send_json({'error': 'Backup: ' + err}, 400)
                        if gwcfg['ip']: gwcfg['pending'] = True
                    gwcfg['ssid'] = f('ssid')
                    if 'pass' in form: gwcfg['pass'] = f('pass')
                    if 'hostname' in form: gwcfg['hostname'] = host_setting(f('hostname'))
                    gwcfg['reboots'] += 1
            if pw: submit(['password ' + pw], False)
            return self.send_json({'ok': True, 'reboot': 'ssid' in form})
        self.send_response(404); self.end_headers()


def traffic():
    i = 0
    while True:
        time.sleep(0.5); i += 1
        push('packets', 'R' if i % 2 else 'T', f'len={20+i%30} GRP_TXT(5) route=F hops=1 payload=25 #{i}')
        if i % 4 == 0:   # alle 2 s eine "empfangene" Nachricht im ersten Kanal
            with lock:
                c = next(((k, x) for k, x in enumerate(chans.ch) if x), None)
            if c:
                push('messages', 'M', '\t'.join([str(c[0]), c[1]['name'], '6.5', '2', 'Funker%d: Hallo Nr. %d' % (i % 3, i)]))


threading.Thread(target=traffic, daemon=True).start()
threading.Thread(target=ping_loop, daemon=True).start()
ThreadingHTTPServer(('127.0.0.1', PORT), H).serve_forever()
