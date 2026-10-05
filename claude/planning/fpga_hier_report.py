import re, sys, collections
txt = open(sys.argv[1], encoding='utf-8', errors='replace').read()
mods = re.split(r'\nmodule ', '\n' + txt)
res = {}
inst_of = {}
for m in mods[1:]:
    name = m.split(' ', 1)[0].strip()
    body = m
    cnt = collections.Counter()
    for t in re.findall(r'\n\s*(LUT[1-6]|LUT6_2|FD\w*|LD\w*|RAM\w+|SRL\w+|MUXCY|XORCY|MUXF\d|RAMB\w+|DCM\w*|PLL\w*|BUFG\w*|IBUF\w*|OBUF\w*|IOBUF\w*|INV)\s+#?\(?', body):
        cnt[t] += 1
    subs = re.findall(r'\n\s*(sprite_slot_INST_\d+|i2s_tx|ay_3_8192\w*|sim_int\w*|m1_tracker\w*|trace_wr\w*|beeper\w*|clk\w+|bramdp_w\w*)\s+(\S+)\s*\(', body)
    res[name] = (cnt, subs)
def lut(c): return sum(v for k, v in c.items() if k.startswith('LUT') and not k.startswith('LUT6_2')) + c.get('LUT6_2', 0)
def ff(c): return sum(v for k, v in c.items() if k.startswith('FD') or k.startswith('LD'))
def ram(c): return sum(v for k, v in c.items() if k.startswith('RAM16') or k.startswith('RAM32') or k.startswith('RAM64'))
rows = []
for n, (c, s) in res.items():
    rows.append((n, lut(c), ff(c), ram(c), c.get('MUXCY', 0), c.get('RAMB16BWER', 0), [x[1] for x in s][:3]))
for r in sorted(rows, key=lambda r: -r[1]):
    print('%-26s LUT=%5d FF=%5d DRAM(1xN)=%4d MUXCY=%4d BRAM=%2d' % r[:6])
sp = [r for r in rows if r[0].startswith('sprite_slot')]
print('sprite_slot total', len(sp), sum(r[1] for r in sp), sum(r[2] for r in sp), sum(r[3] for r in sp))
