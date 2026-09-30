import sys
for f in sys.argv[1:]:
    try: d = open(f, 'rb').read()
    except Exception: print(f, 'no hay salida'); continue
    a = 116; end = (d[3] | d[4] << 8) - 16393
    print(f, 'NXTLIN', d[32] | d[33] << 8)
    while a < end:
        ln = d[a] << 8 | d[a+1]; l = d[a+2] | d[a+3] << 8
        print('  %5d' % ln, d[a+4:a+4+l].hex(' '))
        a += 4 + l
