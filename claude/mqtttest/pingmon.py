#!/usr/bin/env python3
# Vigila varios destinos a la vez con ping y apunta cuando alguno deja de contestar y cuando vuelve, todo con la misma hora,
# para saber DONDE se corta el camino cuando el ESP32 pierde la conexion con el broker (ver claude/planning/latencia_red.md).
#
#   python pingmon.py ESP32=192.168.68.57 ROUTER=192.168.68.1 REPETIDOR=192.168.68.x GOOGLE=8.8.8.8
#
# Cada destino es NOMBRE=IP. Un ping por destino cada --interval segundos (0.5 por defecto, 1 s de espera maxima).
# Por pantalla y en pingmon.log solo salen los cambios (PERDIDO / RECUPERADO tras N s) y, cada minuto, un resumen por destino
# (enviados, perdidos, rtt medio y maximo). Ctrl+C: totales y corte mas largo de cada uno.
import re, subprocess, sys, threading, time, platform, argparse

ap = argparse.ArgumentParser()
ap.add_argument('targets', nargs='+', help='NOMBRE=IP')
ap.add_argument('--interval', type=float, default=0.5)
ap.add_argument('--log', default='pingmon.log')
ap.add_argument('--min-lost', type=int, default=2, help='pings seguidos perdidos para dar un corte por bueno (1 se pierde sin querer)')
args = ap.parse_args()

WIN = platform.system() == 'Windows'
lock = threading.Lock()
logf = open(args.log, 'a', buffering=1, encoding='utf-8')
stop = threading.Event()
stats = {}

def say(msg):
    line = time.strftime('%H:%M:%S') + '.%03d ' % (int(time.time() * 1000) % 1000) + msg
    with lock:
        print(line, flush=True)
        logf.write(line + '\n')

def probe(ip):
    cmd = ['ping', '-n', '1', '-w', '1000', ip] if WIN else ['ping', '-c', '1', '-W', '1', ip]
    try:
        out = subprocess.run(cmd, capture_output=True, text=True, errors='replace', timeout=5).stdout
    except Exception:
        return None
    if 'TTL=' not in out.upper() and 'TTL=' not in out:
        return None        # sin TTL no es una respuesta del destino (p.ej. "host inaccesible" que contesta el router)
    m = re.search(r'[=<]\s*(\d+)\s*ms', out)
    return float(m.group(1)) if m else 0.0

def watch(name, ip):
    s = stats[name] = dict(sent=0, lost=0, longest=0.0, win_sent=0, win_lost=0, rtts=[])
    run_lost = 0; down_since = None; next_t = time.time(); last_min = time.time()
    while not stop.is_set():
        r = probe(ip)
        s['sent'] += 1; s['win_sent'] += 1
        if r is None:
            s['lost'] += 1; s['win_lost'] += 1; run_lost += 1
            if run_lost == args.min_lost:
                down_since = time.time() - (args.min_lost - 1) * args.interval
                say('%-10s PERDIDO' % name)
        else:
            s['rtts'].append(r)
            if down_since is not None:
                d = time.time() - down_since
                s['longest'] = max(s['longest'], d)
                say('%-10s RECUPERADO tras %.1f s' % (name, d))
                down_since = None
            run_lost = 0
        if time.time() - last_min >= 60:
            last_min = time.time()
            rt = s['rtts']
            say('%-10s minuto: enviados=%d perdidos=%d rtt medio=%s max=%s ms' % (
                name, s['win_sent'], s['win_lost'],
                '%.0f' % (sum(rt) / len(rt)) if rt else '-', '%.0f' % max(rt) if rt else '-'))
            s['win_sent'] = s['win_lost'] = 0; s['rtts'] = []
        next_t += args.interval
        time.sleep(max(0, next_t - time.time()))
    if down_since is not None:
        s['longest'] = max(s['longest'], time.time() - down_since)

threads = []
for t in args.targets:
    if '=' not in t:
        sys.exit('cada destino es NOMBRE=IP, no "%s"' % t)
    name, ip = t.split('=', 1)
    th = threading.Thread(target=watch, args=(name, ip), daemon=True)
    th.start(); threads.append(th)
say('vigilando ' + ' '.join(args.targets) + ' (Ctrl+C para parar)')
try:
    while True: time.sleep(0.5)
except KeyboardInterrupt:
    stop.set()
    for th in threads: th.join(timeout=3)
    say('TOTALES:')
    for n, s in stats.items():
        say('  %-10s enviados=%d perdidos=%d corte mas largo=%.1f s' % (n, s['sent'], s['lost'], s['longest']))
