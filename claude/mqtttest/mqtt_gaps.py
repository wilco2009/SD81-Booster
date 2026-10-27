#!/usr/bin/env python3
"""Mira desde el broker local el ritmo de los mensajes de un juego en red y avisa de los huecos.

Uso:  python mqtt_gaps.py [host] [-g umbral_ms (60)] [-t topic ('#')]

Se suscribe a todo y, por topic, mide el tiempo entre mensajes seguidos. Los juegos en tiempo real (Pong, SPORTS) mandan uno por
fotograma (20 ms), asi que un hueco largo es un tiron EN ORIGEN: el mensaje salio tarde del ESP32/STM32/Z80 (o por el WiFi de subida).
Si aqui el ritmo es regular y el tiron se ve solo en la pantalla del que recibe, el retraso esta en la bajada (WiFi del receptor,
ESP32, STM32) o en el propio juego. Cada 10 s imprime un resumen por topic: mensajes, mediana, p95 y maximo del intervalo.
Necesita el broker local (claude/mqtttest/mosquitto.conf); con los publicos tambien vale, pero verias su jitter, no el de la placa.
"""
import argparse
import functools
print = functools.partial(print, flush=True)
import os
import socket
import statistics
import struct
import time


def rl(n):
    out = bytearray()
    while True:
        b = n % 128
        n //= 128
        out.append(b | (128 if n else 0))
        if not n:
            return bytes(out)


def s(txt):
    b = txt.encode()
    return struct.pack(">H", len(b)) + b


def pkt(first, body):
    return bytes([first]) + rl(len(body)) + body


class Reader:
    def __init__(self, sock):
        self.sock = sock
        self.buf = b""

    def need(self, n):
        while len(self.buf) < n:
            chunk = self.sock.recv(65536)
            if not chunk:
                raise ConnectionError("cerrado")
            self.buf += chunk

    def packet(self):
        self.need(2)
        i = 1
        mult, ln = 1, 0
        while True:
            self.need(i + 1)
            b = self.buf[i]
            i += 1
            ln += (b & 127) * mult
            mult *= 128
            if not b & 128:
                break
        self.need(i + ln)
        first, body = self.buf[0], self.buf[i:i + ln]
        self.buf = self.buf[i + ln:]
        return first, body


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("host", nargs="?", default="127.0.0.1")
    ap.add_argument("-g", type=float, default=60.0)
    ap.add_argument("-t", default="#")
    a = ap.parse_args()
    host, _, port = a.host.partition(":")
    sock = socket.create_connection((host, int(port or 1883)), timeout=8)
    sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    cid = "gaps-%08x" % int.from_bytes(os.urandom(4), "big")
    sock.sendall(pkt(0x10, s("MQTT") + bytes([4, 2]) + struct.pack(">H", 0) + s(cid)))
    rd = Reader(sock)
    rd.packet()
    sock.sendall(pkt(0x82, struct.pack(">H", 1) + s(a.t) + b"\x00"))
    rd.packet()
    sock.settimeout(None)
    print("escuchando %s en %s; avisa de huecos > %.0f ms (Ctrl+C para salir)" % (a.t, a.host, a.g))
    last, gaps, t_sum = {}, {}, time.time()
    try:
        while True:
            first, body = rd.packet()
            now = time.time()
            if first >> 4 != 3:
                continue
            tl = struct.unpack(">H", body[:2])[0]
            topic = body[2:2 + tl].decode(errors="replace")
            if topic in last:
                g = (now - last[topic]) * 1000
                gaps.setdefault(topic, []).append(g)
                if g > a.g:
                    print("%s  HUECO %6.0f ms en %s" % (time.strftime("%H:%M:%S"), g, topic))
            last[topic] = now
            if now - t_sum >= 10:
                t_sum = now
                for t, v in sorted(gaps.items()):
                    if len(v) < 3:
                        continue
                    v.sort()
                    print("%s  %-40s n=%4d  med %5.1f  p95 %5.1f  max %6.1f ms" % (
                        time.strftime("%H:%M:%S"), t[:40], len(v), statistics.median(v), v[int(len(v) * 0.95) - 1], v[-1]))
                gaps = {}
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
