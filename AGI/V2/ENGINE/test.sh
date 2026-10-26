#!/bin/sh
# Prueba del visor en el arnes de PC de SCUMM (necesita SCUMM/V2/ENGINE/test/build/test_scumm.exe y imagenes convertidas):
#   sh test.sh KQ1        (usa ../dump/KQ1/hr/PIC001..003 y 005, pero no la 004: se comprueba que N salta los huecos)
set -e
cd "$(dirname "$0")"
G=${1:-KQ1}
E=../../../SCUMM/V2/ENGINE/test
pasmo agiview.asm agiview.bin agiview.sym
rm -rf ../dump/testsd && mkdir -p ../dump/testsd
for n in 001 002 003 005; do cp ../dump/$G/hr/PIC$n.GFX ../dump/testsd/; done
# N en el fotograma 4, 8 y 12 (002, 003 y 005: se salta la 004); B en el 16 (003); ESPACIO en el 20 (vuelve a BASIC)
$E/build/test_scumm.exe agiview.bin agiview.sym ../dump/testsd 40 ../dump/test_view.bin "N:4,-:6,N:8,-:10,N:12,-:14,B:16,-:18"
python -I - <<'EOF'
d = open('../dump/test_view.bin', 'rb').read()
PL = 168 * 32
def img(n):
    f = open('../dump/testsd/PIC%03d.GFX' % n, 'rb').read()
    return f[256:256 + PL], f[256 + PL:256 + 2 * PL]
bm, at = d[0xA000:0xA000 + PL], d[0xC000:0xC000 + PL]
ok = [(n, img(n) == (bm, at)) for n in (1, 2, 3, 5)]
print('la pantalla final es la imagen:', [n for n, k in ok if k])
assert [n for n, k in ok if k] == [3], 'esperaba la 003 (N, N, N salta la 004, B)'
print('OK')
EOF
