# Listador .P -> .B81 como zx81BasicLister::RenderLineAsText (opciones por defecto)
import sys
K = [" ","\\' ","\\ '","\\''","\\. ","\\: ","\\.'","\\:'","\\##","\\,,","\\~~",
     "\"","\xa3","$",":","?","(",")",">","<","=","+","-","*","/",";",",","."]
K += [str(d) for d in range(10)] + [chr(65+i) for i in range(26)] + ["RND","INKEY$","PI"]
K += ["\\%02X" % c for c in range(0x43, 0x80)]
K += ["% ","\\.:","\\:.","\\..","\\':","\\ :","\\'.","\\ .","\\@@","\\;;","\\!!",
      "%\"","%\xa3","%$","%:","%?","%(","%)","%>","%<","%=","%+","%-","%*","%/","%;","%,","%."]
K += ["%"+str(d) for d in range(10)] + ["%"+chr(65+i) for i in range(26)]
K += ["\"\"","AT ","TAB ","\\C3","CODE ","VAL ","LEN ","SIN ","COS ","TAN ","ASN ","ACS ","ATN ",
      "LN ","EXP ","INT ","SQR ","SGN ","ABS ","PEEK ","USR ","STR$ ","CHR$ ","NOT ","**"," OR "," AND ",
      "<=",">=","<>"," THEN "," TO "," STEP "," LPRINT "," LLIST "," STOP "," SLOW "," FAST ",
      " NEW "," SCROLL "," CONT "," DIM "," REM "," FOR "," GOTO "," GOSUB "," INPUT "," LOAD ",
      " LIST "," LET "," PAUSE "," NEXT "," POKE "," PRINT "," PLOT "," RUN "," SAVE "," RAND ",
      " IF "," CLS "," UNPLOT "," CLEAR "," RETURN "," COPY "]
assert len(K) == 256, len(K)

def rem_has_mc(d, a, n):
    while n > 0:
        c = d[a]; a += 1; n -= 1
        if (0x43 <= c < 0x80 and not (c == 0x76 and n <= 0)) or c == 0xC3: return True
    return False

def fmt_num(n):
    s = "%5d" % n
    if s[0] != ' ':
        s = ' ' + chr(ord('A') + int(s[1])) + s[2:]
    return s[1:]

def list_prog(d):
    out = []
    dfile = d[3] | d[4] << 8
    a, end = 116, dfile - 16393
    while a + 4 <= end:
        ln = d[a] << 8 | d[a+1]
        if ln >= 0x4000: break
        l = d[a+2] | d[a+3] << 8
        a += 4
        text = fmt_num(ln) + " "
        last_sp = True; ctl = False; inrem = inq = False
        p, rem = a, l
        while rem > 0:
            c = d[p]; p += 1; rem -= 1
            if rem <= 0 and c == 0x76: break
            kw = K[c]
            if not inrem and kw == "\"": inq = not inq
            elif not inq and kw == " REM ": inrem = True
            if not inrem and c == 0x7E:
                p += 5; rem -= 5; last_sp = False; continue
            if ctl:
                text += "\\%02X" % c; last_sp = False
            elif len(kw) > 1:
                if not inq and inrem: ctl = rem_has_mc(d, p, rem)
                st = 1 if (last_sp and kw[0] == ' ') else 0
                text += kw[st:]; last_sp = kw[-1] == ' '
            else:
                text += kw; last_sp = False
        out.append(text)
        a += l
    return out

d = open(sys.argv[1], 'rb').read()
if sys.argv[1].upper().endswith('.P81'):   # quita el nombre
    i = 0
    while d[i] < 0x80: i += 1
    d = d[i+1:]
open(sys.argv[2], 'w', encoding='latin-1', newline='\n').write('\n'.join(list_prog(d)) + '\n')
