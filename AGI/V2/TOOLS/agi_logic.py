#!/usr/bin/env python3
"""Desensamblador del bytecode de las logicas de AGI v2.

Uso:
    agi_logic.py JUEGO [n ...]          # listado de las logicas (todas si no se dan numeros)
    agi_logic.py JUEGO --check          # solo comprueba que cada logica se decodifica entera y sincronizada
    agi_logic.py JUEGO --stats          # opcodes usados (cuantas veces y en cuantas logicas)

Formato de una logica: u16 tamano del codigo, el codigo, y la seccion de mensajes (ver agi_read.logic_messages).
Codigo: comandos (0x00-0xAF, con un numero fijo de argumentos de un byte); 0xFF abre un bloque `if` (condiciones hasta otro 0xFF,
despues u16 con lo que se salta si es falso); 0xFE es `goto` (u16 con signo, relativo al final de la palabra).
En las condiciones: 0xFD niega la siguiente, 0xFC abre/cierra un grupo OR.
La comprobacion de sincronia: el ultimo comando acaba justo en el final del codigo y los saltos caen en limites de instruccion.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import agi_read as ag

# comandos: (nombre, tipos de los argumentos); tipos: n numero, v variable, f bandera, o objeto animado, i objeto del inventario,
# m mensaje, s cadena, c control de menu
CMDS = [
    ('return', ''), ('increment', 'v'), ('decrement', 'v'), ('assignn', 'vn'), ('assignv', 'vv'),
    ('addn', 'vn'), ('addv', 'vv'), ('subn', 'vn'), ('subv', 'vv'), ('lindirectv', 'vv'),
    ('rindirect', 'vv'), ('lindirectn', 'vn'), ('set', 'f'), ('reset', 'f'), ('toggle', 'f'),
    ('set.v', 'v'), ('reset.v', 'v'), ('toggle.v', 'v'), ('new.room', 'n'), ('new.room.v', 'v'),
    ('load.logics', 'n'), ('load.logics.v', 'v'), ('call', 'n'), ('call.v', 'v'), ('load.pic', 'v'),
    ('draw.pic', 'v'), ('show.pic', ''), ('discard.pic', 'v'), ('overlay.pic', 'v'), ('show.pri.screen', ''),
    ('load.view', 'n'), ('load.view.v', 'v'), ('discard.view', 'n'), ('animate.obj', 'o'), ('unanimate.all', ''),
    ('draw', 'o'), ('erase', 'o'), ('position', 'onn'), ('position.v', 'ovv'), ('get.posn', 'ovv'),
    ('reposition', 'ovv'), ('set.view', 'on'), ('set.view.v', 'ov'), ('set.loop', 'on'), ('set.loop.v', 'ov'),
    ('fix.loop', 'o'), ('release.loop', 'o'), ('set.cel', 'on'), ('set.cel.v', 'ov'), ('last.cel', 'ov'),
    ('current.cel', 'ov'), ('current.loop', 'ov'), ('current.view', 'ov'), ('number.of.loops', 'ov'), ('set.priority', 'on'),
    ('set.priority.v', 'ov'), ('release.priority', 'o'), ('get.priority', 'ov'), ('stop.update', 'o'), ('start.update', 'o'),
    ('force.update', 'o'), ('ignore.horizon', 'o'), ('observe.horizon', 'o'), ('set.horizon', 'n'), ('object.on.water', 'o'),
    ('object.on.land', 'o'), ('object.on.anything', 'o'), ('ignore.objs', 'o'), ('observe.objs', 'o'), ('distance', 'oov'),
    ('stop.cycling', 'o'), ('start.cycling', 'o'), ('normal.cycling', 'o'), ('end.of.loop', 'of'), ('reverse.cycle', 'o'),
    ('reverse.loop', 'of'), ('cycle.time', 'ov'), ('stop.motion', 'o'), ('start.motion', 'o'), ('step.size', 'ov'),
    ('step.time', 'ov'), ('move.obj', 'onnnf'), ('move.obj.v', 'ovvvf'), ('follow.ego', 'onf'), ('wander', 'o'),
    ('normal.motion', 'o'), ('set.dir', 'ov'), ('get.dir', 'ov'), ('ignore.blocks', 'o'), ('observe.blocks', 'o'),
    ('block', 'nnnn'), ('unblock', ''), ('get', 'i'), ('get.v', 'v'), ('drop', 'i'),
    ('put', 'in'), ('put.v', 'iv'), ('get.room.v', 'vv'), ('load.sound', 'n'), ('sound', 'nf'),
    ('stop.sound', ''), ('print', 'm'), ('print.v', 'v'), ('display', 'nnm'), ('display.v', 'vvv'),
    ('clear.lines', 'nnn'), ('text.screen', ''), ('graphics', ''), ('set.cursor.char', 'm'), ('set.text.attribute', 'nn'),
    ('shake.screen', 'n'), ('configure.screen', 'nnn'), ('status.line.on', ''), ('status.line.off', ''), ('set.string', 'sm'),
    ('get.string', 'smnnn'), ('word.to.string', 'ns'), ('parse', 's'), ('get.num', 'mv'), ('prevent.input', ''),
    ('accept.input', ''), ('set.key', 'nnn'), ('add.to.pic', 'nnnnnnn'), ('add.to.pic.v', 'vvvvvvv'), ('status', ''),
    ('save.game', ''), ('restore.game', ''), ('init.disk', ''), ('restart.game', ''), ('show.obj', 'n'),
    ('random', 'nnv'), ('program.control', ''), ('player.control', ''), ('obj.status.v', 'v'), ('quit', 'n'),
    ('show.mem', ''), ('pause', ''), ('echo.line', ''), ('cancel.line', ''), ('init.joy', ''),
    ('toggle.monitor', ''), ('version', ''), ('script.size', 'n'), ('set.game.id', 'm'), ('log', 'm'),
    ('set.scan.start', ''), ('reset.scan.start', ''), ('reposition.to', 'onn'), ('reposition.to.v', 'ovv'), ('trace.on', ''),
    ('trace.info', 'nnn'), ('print.at', 'mnnn'), ('print.at.v', 'vvvv'), ('discard.view.v', 'v'), ('clear.text.rect', 'nnnnn'),
    ('set.upper.left', 'nn'), ('set.menu', 'm'), ('set.menu.item', 'mc'), ('submit.menu', ''), ('enable.item', 'c'),
    ('disable.item', 'c'), ('menu.input', ''), ('show.obj.v', 'v'), ('open.dialogue', ''), ('close.dialogue', ''),
    ('mul.n', 'vn'), ('mul.v', 'vv'), ('div.n', 'vn'), ('div.v', 'vv'), ('close.window', ''),
]

# condiciones 0x01-0x12; 'said' (0x0E) lleva una lista de palabras
CONDS = {
    0x01: ('equaln', 'vn'), 0x02: ('equalv', 'vv'), 0x03: ('lessn', 'vn'), 0x04: ('lessv', 'vv'),
    0x05: ('greatern', 'vn'), 0x06: ('greaterv', 'vv'), 0x07: ('isset', 'f'), 0x08: ('issetv', 'v'),
    0x09: ('has', 'i'), 0x0A: ('obj.in.room', 'iv'), 0x0B: ('posn', 'onnnn'), 0x0C: ('controller', 'c'),
    0x0D: ('have.key', ''), 0x0E: ('said', None), 0x0F: ('compare.strings', 'ss'), 0x10: ('obj.in.box', 'onnnn'),
    0x11: ('center.posn', 'onnnn'), 0x12: ('right.posn', 'onnnn'),
}


class DecodeError(Exception):
    pass


class Logic:
    """Una logica decodificada: lista de instrucciones (offset, texto, tipo) y los limites de bloques."""

    def __init__(self, data):
        self.d = data
        self.size = data[0] | (data[1] << 8)
        self.code = data[2:2 + self.size]
        try:
            self.msgs = ag.logic_messages(data)
        except (IndexError, ValueError):
            self.msgs = []
        self.ins = []                 # (offset, texto, nivel)
        self.starts = set()
        self.targets = []             # (origen, destino) de if y goto
        self.used = {}                # nombre -> veces
        self.decode()

    def u16(self, p):
        if p + 2 > len(self.code):
            raise DecodeError('palabra fuera del codigo en %d' % p)
        return self.code[p] | (self.code[p + 1] << 8)

    def byte(self, p):
        if p >= len(self.code):
            raise DecodeError('byte fuera del codigo en %d' % p)
        return self.code[p]

    def arg(self, t, v):
        if t == 'n':
            return str(v)
        if t == 'm':
            txt = self.msgs[v - 1] if 0 < v <= len(self.msgs) and self.msgs[v - 1] is not None else None
            return 'm%d%s' % (v, ' "%s"' % txt.replace('\n', '\\n')[:60] if txt else '')
        return '%s%d' % (t, v)

    def decode(self):
        p = 0
        n = len(self.code)
        blocks = []                   # finales pendientes de bloques `if`: (offset del final)
        while p < n:
            while blocks and blocks[-1] == p:
                blocks.pop()
                self.ins.append((p, '}', len(blocks)))
            if blocks and blocks[-1] < p:
                raise DecodeError('el bloque acaba en %d dentro de una instruccion (%d)' % (blocks[-1], p))
            self.starts.add(p)
            op = self.code[p]
            lvl = len(blocks)
            if op == 0xFF:
                q = p + 1
                parts = []
                neg = False
                orm = False
                grp = []
                while True:
                    c = self.byte(q)
                    q += 1
                    if c == 0xFF:
                        break
                    if c == 0xFD:
                        neg = not neg
                        continue
                    if c == 0xFC:
                        if orm:
                            parts.append('(' + ' || '.join(grp) + ')')
                            grp = []
                        orm = not orm
                        continue
                    if c not in CONDS:
                        raise DecodeError('condicion %02X desconocida en %d' % (c, q - 1))
                    name, types = CONDS[c]
                    if types is None:                       # said: n palabras de 16 bits
                        k = self.byte(q)
                        q += 1
                        ws = []
                        for _ in range(k):
                            ws.append(str(self.u16(q)))
                            q += 2
                        txt = '%s(%s)' % (name, ' '.join(ws))
                    else:
                        a = []
                        for t in types:
                            a.append(self.arg(t, self.byte(q)))
                            q += 1
                        txt = '%s(%s)' % (name, ', '.join(a))
                    self.used[name] = self.used.get(name, 0) + 1
                    if neg:
                        txt = '!' + txt
                        neg = False
                    (grp if orm else parts).append(txt)
                if orm:
                    raise DecodeError('grupo OR sin cerrar en %d' % p)
                ln = self.u16(q)
                q += 2
                end = q + ln
                if end > n:
                    raise DecodeError('el bloque if de %d sale del codigo (%d > %d)' % (p, end, n))
                self.ins.append((p, 'if (%s) {' % ' && '.join(parts), lvl))
                self.targets.append((p, end))
                blocks.append(end)
                p = q
                self.used['if'] = self.used.get('if', 0) + 1
                continue
            if op == 0xFE:
                off = self.u16(p + 1)
                if off >= 0x8000:
                    off -= 0x10000
                dest = p + 3 + off
                self.ins.append((p, 'goto @%d' % dest, lvl))
                self.targets.append((p, dest))
                self.used['goto'] = self.used.get('goto', 0) + 1
                p += 3
                continue
            if op >= len(CMDS):
                raise DecodeError('comando %02X desconocido en %d' % (op, p))
            name, types = CMDS[op]
            a = []
            q = p + 1
            for t in types:
                a.append(self.arg(t, self.byte(q)))
                q += 1
            self.ins.append((p, '%s(%s)' % (name, ', '.join(a)), lvl))
            self.used[name] = self.used.get(name, 0) + 1
            p = q
        while blocks and blocks[-1] == p:
            blocks.pop()
            self.ins.append((p, '}', len(blocks)))
        if blocks:
            raise DecodeError('bloques sin cerrar: %s' % blocks)
        self.starts.add(n)
        for o, t in self.targets:
            if t not in self.starts:
                raise DecodeError('el salto de %d cae en %d, que no es limite de instruccion' % (o, t))

    def listing(self):
        labels = {t for _, t in self.targets if t >= 0}
        out = []
        for off, txt, lvl in self.ins:
            if off in labels and not txt.startswith('}'):
                out.append('@%d:' % off)
            out.append('%5d  %s%s' % (off, '    ' * lvl if not txt.startswith('}') else '    ' * lvl, txt))
        return '\n'.join(out)


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 1
    g = ag.Game(argv[1])
    flags = [a for a in argv[2:] if a.startswith('--')]
    nums = [int(a) for a in argv[2:] if not a.startswith('--')]
    sel = nums or g.present('LOG')
    if '--check' in flags or '--stats' in flags:
        ok = bad = 0
        used = {}
        inlog = {}
        for n in sel:
            d = g.resource('LOG', n)
            if d is None:
                continue
            try:
                lg = Logic(d)
                ok += 1
                for k, v in lg.used.items():
                    used[k] = used.get(k, 0) + v
                    inlog[k] = inlog.get(k, 0) + 1
            except (DecodeError, IndexError) as e:
                bad += 1
                print('logica %d: %s' % (n, e))
        print('%s: %d logicas decodificadas, %d con error' % (os.path.basename(argv[1].rstrip('/\\')), ok, bad))
        if '--stats' in flags:
            for k in sorted(used, key=lambda k: -used[k]):
                print('  %-20s %6d en %3d logicas' % (k, used[k], inlog[k]))
        return 1 if bad else 0
    for n in sel:
        d = g.resource('LOG', n)
        if d is None:
            continue
        print('==== logica %d' % n)
        try:
            print(Logic(d).listing())
        except (DecodeError, IndexError) as e:
            print('ERROR: %s' % e)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
