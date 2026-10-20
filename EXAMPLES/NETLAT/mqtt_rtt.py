#!/usr/bin/env python3
"""NETLAT - mqtt_rtt.py: mide desde el PC la ida y vuelta a un servidor MQTT (publica en un topic al que el mismo cliente esta suscrito y
cuenta el tiempo hasta que llega el eco). Es la parte "internet + servidor" de lo que mide el Z80 en la prueba 4 de NETLAT: la
referencia con la que comparar, sin la placa, el ESP32 ni el puente. Solo usa la biblioteca estandar.

    python mqtt_rtt.py                         # test.mosquitto.org:1883, 50 muestras
    python mqtt_rtt.py broker.ejemplo.org 1883 100

Lo que sale (en ms): minimo, media, mediana, percentil 95 y maximo. La conexion es TCP sin cifrar, igual que la del ESP32 por defecto.
"""
import os
import socket
import statistics
import struct
import sys
import time


def enc_str(s):
    b = s.encode()
    return struct.pack(">H", len(b)) + b


def remaining(n):
    out = b""
    while True:
        d = n % 128
        n //= 128
        out += bytes([d | (0x80 if n else 0)])
        if not n:
            return out


def packet(first, body):
    return bytes([first]) + remaining(len(body)) + body


def read_packet(sock):
    h = sock.recv(1)
    if not h:
        raise ConnectionError("el servidor ha cerrado")
    n, mul = 0, 1
    while True:
        b = sock.recv(1)[0]
        n += (b & 127) * mul
        mul *= 128
        if not b & 128:
            break
    body = b""
    while len(body) < n:
        body += sock.recv(n - len(body))
    return h[0], body


def main():
    host = sys.argv[1] if len(sys.argv) > 1 else "test.mosquitto.org"
    port = int(sys.argv[2]) if len(sys.argv) > 2 else 1883
    n = int(sys.argv[3]) if len(sys.argv) > 3 else 50
    topic = "sd81lat/" + os.urandom(6).hex()
    t0 = time.perf_counter()
    s = socket.create_connection((host, port), timeout=10)
    s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    t_tcp = (time.perf_counter() - t0) * 1000
    cid = "sd81lat" + os.urandom(3).hex()
    s.sendall(packet(0x10, enc_str("MQTT") + bytes([4, 2]) + struct.pack(">H", 30) + enc_str(cid)))
    t1 = time.perf_counter()
    typ, body = read_packet(s)
    assert typ >> 4 == 2 and body[1] == 0, "el servidor rechaza la conexion"
    t_conn = (time.perf_counter() - t1) * 1000
    s.sendall(packet(0x82, struct.pack(">H", 1) + enc_str(topic) + bytes([0])))
    typ, body = read_packet(s)
    assert typ >> 4 == 9, "no se confirma la suscripcion"
    print(f"{host}:{port}   TCP {t_tcp:.0f} ms, CONNECT {t_conn:.0f} ms (una vez)")
    samples = []
    for i in range(n):
        t = time.perf_counter()
        s.sendall(packet(0x30, enc_str(topic) + b"x"))          # PUBLISH QoS 0
        while True:
            typ, body = read_packet(s)
            if typ >> 4 == 3:
                break
        samples.append((time.perf_counter() - t) * 1000)
        time.sleep(0.05)
    s.sendall(bytes([0xE0, 0]))
    s.close()
    samples.sort()
    p95 = samples[min(len(samples) - 1, int(len(samples) * 0.95))]
    print(f"ida y vuelta de un PUBLISH (n={n}): min {samples[0]:.0f}  media {statistics.mean(samples):.0f}  mediana {statistics.median(samples):.0f}  "
          f"p95 {p95:.0f}  max {samples[-1]:.0f}  (ms)")


if __name__ == "__main__":
    main()
