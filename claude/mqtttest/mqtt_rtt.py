#!/usr/bin/env python3
"""Ida y vuelta a un broker MQTT desde el PC: publica en un topic propio al que esta suscrito y mide lo que tarda en volver.

Uso:  python mqtt_rtt.py [host[:puerto]] [...]   (por defecto los tres brokers publicos de NET.CFG)
      opciones: -n muestras (100)   -i intervalo en ms (100)

Sin dependencias (MQTT 3.1.1 a mano). Sirve para separar "es el servidor / internet" de "es el WiFi / el ESP32 / el STM32":
si aqui salen 30-40 ms con poca variacion y la placa va a tirones, el culpable no es el broker.
"""
import argparse
import os
import socket
import statistics
import struct
import sys
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


def read_pkt(sock, deadline):
    def rd(n):
        buf = b""
        while len(buf) < n:
            sock.settimeout(max(0.01, deadline - time.time()))
            chunk = sock.recv(n - len(buf))
            if not chunk:
                raise ConnectionError("cerrado")
            buf += chunk
        return buf
    first = rd(1)[0]
    mult, ln = 1, 0
    while True:
        b = rd(1)[0]
        ln += (b & 127) * mult
        mult *= 128
        if not b & 128:
            break
    return first, rd(ln) if ln else b""


def run(host, port, n, gap):
    cid = "rtt-%08x" % int.from_bytes(os.urandom(4), "big")
    topic = "sd81rtt/%s" % cid
    t0 = time.time()
    sock = socket.create_connection((host, port), timeout=8)
    sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    conn_ms = (time.time() - t0) * 1000
    sock.sendall(pkt(0x10, s("MQTT") + bytes([4, 2]) + struct.pack(">H", 30) + s(cid)))
    f, b = read_pkt(sock, time.time() + 8)
    if f >> 4 != 2 or len(b) < 2 or b[1] != 0:
        raise ConnectionError("CONNACK rechazado %r" % b)
    sock.sendall(pkt(0x82, struct.pack(">H", 1) + s(topic) + b"\x00"))
    f, b = read_pkt(sock, time.time() + 8)
    if f >> 4 != 9:
        raise ConnectionError("SUBACK esperado")
    rtts, lost = [], 0
    for i in range(n):
        t = time.time()
        sock.sendall(pkt(0x30, s(topic) + b"%06d" % i))
        try:
            while True:
                f, b = read_pkt(sock, t + 2.0)
                if f >> 4 == 3:
                    break
            rtts.append((time.time() - t) * 1000)
        except (socket.timeout, TimeoutError):
            lost += 1
        time.sleep(gap / 1000.0)
    sock.close()
    return conn_ms, rtts, lost


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("brokers", nargs="*", default=["test.mosquitto.org", "broker.hivemq.com", "broker.emqx.io"])
    ap.add_argument("-n", type=int, default=100)
    ap.add_argument("-i", type=int, default=100)
    a = ap.parse_args()
    print("%-28s %7s %6s %6s %6s %6s %6s %5s" % ("broker", "conecta", "min", "med", "p95", "max", "desv", "perd"))
    for b in a.brokers:
        host, _, port = b.partition(":")
        try:
            conn, r, lost = run(host, int(port or 1883), a.n, a.i)
        except Exception as e:  # noqa: BLE001
            print("%-28s ERROR: %s" % (b, e))
            continue
        if not r:
            print("%-28s sin respuestas" % b)
            continue
        r.sort()
        print("%-28s %6.0fms %6.1f %6.1f %6.1f %6.1f %6.1f %5d" % (
            b, conn, r[0], statistics.median(r), r[int(len(r) * 0.95) - 1], r[-1], statistics.pstdev(r), lost))


if __name__ == "__main__":
    sys.exit(main())
