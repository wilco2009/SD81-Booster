import sys, re

lineno = 0
labels = {}
params = []

varval = {'add':0x30, 'mul':0x50, 'div':0x60, 'sub':0x70}
varvar = {'add':0x44, 'sub':0x45, 'adc':0x46, 'sbc':0x47, 'not':0x48,
    'and':0x49, 'or':0x4A, 'xor':0x4b, 'mul':0x4c, 'div':0x4d}

class ParseError(Exception):
    def __init__(self):
        super(ParseError, self).__init__("Error in line " + str(lineno))

class LabelAlreadyDef(Exception):
    def __init__(self, lbl):
        super(LabelAlreadyDef, self).__init__("Label already defined: " + lbl)

charset_initial_re = r'[a-z_$.]'
charset_follow_re = r'[a-z0-9_$.]'
identifier_re = charset_initial_re + charset_follow_re + r'*'
param_re = r'''(?:
        (?P<var%(pn)s>v[0-9]+)
        |(?P<reg%(pn)s>ro?[0-9]+)
        |(?P<val%(pn)s>
            (?P<id%(pn)s>''' + identifier_re + r''')
            |(?P<hex%(pn)s>-?(?:[0-9][0-9a-f]*h|0x[0-9a-f]+))
            |(?P<dec%(pn)s>-?[0-9]+)
        )
    )'''
insns = \
    'ld|add|sub|adc|sbc|not|and|or|xor|mul|div|shl|shr|wait|djnz|jr|halt|equ'
instr_re = r'''
    ^[\ \t]*
    (?:
        (?P<label>''' + identifier_re + r''')
        (?::[\ \t]*|[\ \t]+)  # followed by colon or spaces
    )?  # label optional
    (?:
        # all instructions:
        (?P<instr>''' + insns + r''')
        # not followed by anything that might make them an identifier
        #(?!' '+ charset_follow_re +' ')
        (?:
            [\ \t]+  # if parameters present, a space is mandatory
            (?P<param1>''' + (param_re % {'pn':1}) + r''')
            [\ \t]*
            (?: ,  # comma
                [\ \t]*
                (?P<param2>''' + (param_re % {'pn':2})  + r''')
            )?  # second parameter optional if first one present
        )?  # all parameters optional
    )?  # instruction + parameters optional
    [\ \t]*
    (?:
        ;.*  # comment
    )?  # optional
    $

    '''

instr_comp = re.compile(instr_re, re.I|re.X)

class parser:
    def __init__(self, f):
        self.f = f
        self.pos = 0

    def gettoken(self):
        pass


def paramlen(n):
    assert len(params) == n, "Wrong parameter count in line %d" % lineno

def val(pn):
    ident = params[pn]['id']
    if ident:
        return labels[ident]
    val = params[pn]['hex']
    if val:
        if val[-1:] in {'h', 'H'}:
            val = val[:-1]
        return int(val, 16) & 0xFFFF
    val = params[pn]['dec']
    if val:
        return int(val, 10) & 0xFFFF
    raise ParseError()

def num(pn, kind):
    varreg = params[pn][kind]
    if varreg[1:2] in {'o', 'O'}:
        n = int(varreg[2:], 8)
    else:
        n = int(varreg[1:], 10)
    if n < 16:
        return n
    raise ParseError()

def num2(a, b):
    return num(0, a) << 4 | num(1, b)

def main(argv):
    global labels
    global params
    global lineno
    data = bytearray()
    f = open(argv[1], 'r', encoding='utf-8')
    try:
        lineno = 0
        for line in f:
            labels['$'] = len(data)//2
            lineno += 1
            match = instr_comp.search(line)
            if not match:
                raise ParseError()
            lbl = match.group('label')
            if lbl:
                if lbl in labels:
                    raise LabelAlreadyDef(lbl)
                labels[lbl] = (len(data) >> 1) & 0xFF

            instr = match.group('instr')
            if not instr:
                continue
            instr = instr.lower()
            # Reorganize match.group() into params[] list of dicts
            params = []
            for i in range(1, 3):
                if match.group('param%d' % i):
                    dic = {}
                    for j in ('var','reg','val','id','hex','dec'):
                        dic[j] = match.group(j + str(i))
                    params.append(dic)
            # params[0] and params[1] are dicts with var,reg,val,id, ...

            if instr == 'equ':
                assert lbl, "EQU without a label in line %d" % lineno
                paramlen(1)
                labels[lbl] = val(0)
                continue
            if instr == 'halt':
                paramlen(0)
                data += b'\xA0\x10'
            elif instr == 'ld':
                paramlen(2)
                if params[0]['var']:
                    if params[1]['var']:
                        # ld var,var
                        data.append(0x43)
                        data.append(num2('var','var'))
                    elif params[1]['reg']:
                        # ld var,reg
                        data.append(0x42)
                        data.append(num2('var','reg'))
                    else:
                        # ld var,value
                        data.append(0x20 | num(0, 'var'))
                        data.append(val(1) & 0xFF)
                elif params[0]['reg']:
                    if params[1]['var']:
                        # ld reg,var
                        data.append(0x41)
                        data.append(num2('reg','var'))
                    elif params[1]['reg']:
                        # ld reg,reg
                        data.append(0x40)
                        data.append(num2('reg','reg'))
                    else:
                        # ld reg,value
                        data.append(0x00 | num(0, 'reg'))
                        data.append(val(1) & 0xFF)
                else:
                    # ld value, ...???
                    raise ParseError()
            elif len(params) == 2 and params[0]['var'] and params[1]['val']:
                # instr var,value
                if instr == 'djnz':
                    data.append(0x80 | num(0, 'var'))
                    data.append((val(1) - (len(data)//2 + 1)) & 0xFF)
                elif instr in {'shl', 'shr'}:
                    data.append(0x4E if instr == 'shl' else 0x4F)
                    data.append(num(0, 'var') << 4
                        | (val(1) & 0xF))
                else:
                    if instr not in varval:
                        raise ParseError()
                    # instr var,value
                    data.append(varval[instr] | num(0, 'var'))
                    data.append(val(1) & 0xFF)
            elif instr == 'add' and len(params) == 2 and params[0]['reg']:
                if params[1]['var'] or params[1]['reg']:
                    # add reg,var / add reg,reg do not exist
                    raise ParseError()
                data.append(0x10 | num(0, 'reg'))
                data.append(val(1) & 0xFF)
            elif len(params) == 2 and params[0]['var'] and params[1]['var']:
                if instr not in varvar:
                    raise ParseError()
                # instr var,var
                data.append(varvar[instr])
                data.append(num2('var','var'))
            elif instr == 'wait':
                paramlen(1)
                if params[0]['var']:
                    # wait var
                    data.append(0xA0)
                    data.append(0x00 | num(0, 'var'))
                else:
                    # wait value
                    if not params[0]['val']:
                        raise ParseError()
                    v = val(0) & 0xFFF
                    data.append(0x90 | v >> 8)
                    data.append(v & 0xFF)
                    del v
            elif instr == 'jr':
                paramlen(1)
                if not params[0]['val']:
                    raise ParseError()
                data.append(0xA1)
                data.append((val(0) - (len(data)//2 + 1)) & 0xFF)
            else:
                raise ParseError()

    finally:
        f.close()

    for i in range(0, len(data), 2):
        data[i], data[i+1] = data[i+1], data[i]
    f = open(argv[2], 'wb')
    try:
        f.write(data)
    finally:
        f.close()


main(sys.argv)
