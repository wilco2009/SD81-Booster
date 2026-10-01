; programa de prueba de la cosimulacion del depurador
        org  6000h
start:  di
        ld   sp,7F00h
        ld   ix,0A5A5h
        ld   iy,05A5Ah
        ld   de,2222h
        ld   hl,3333h
        exx
        ld   bc,4444h
        ld   de,5555h
        ld   hl,6666h
        exx
        ld   a,77h
        ex   af,af'
        ld   a,88h
        ex   af,af'
        xor  a
        ld   (cnt),a
rloop:  ld   a,r                ; R tiene que avanzar 11 entre los dos LD A,R
        ld   c,a                ; aunque haya paradas por medio
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        ld   a,r
        sub  c
        and  7Fh
        cp   11
        jr   nz,rbad
        push hl                 ; la pila se usa: tiene que seguir bien
        ld   hl,(cnt16)
        inc  hl
        ld   (cnt16),hl
        pop  hl
        ld   a,(cnt)
        inc  a
        ld   (cnt),a
        cp   200
        jr   nz,rloop
wait:   jr   wait               ; el script pone PC = escape
escape: ld   (s_hl),hl
        ld   (s_de),de
        ld   (s_ix),ix
        ld   (s_iy),iy
        ld   (s_sp),sp
        exx
        ld   (s_hl2),hl
        ld   (s_de2),de
        ld   (s_bc2),bc
        exx
        ex   af,af'
        ld   (s_a2),a
        ex   af,af'
        ld   a,1
        ld   (finished),a
done:   jr   done
rbad:   ld   a,1
        ld   (fail),a
        jr   done

        org  6100h
cnt:    defb 0
cnt16:  defw 0
fail:   defb 0
finished: defb 0
s_hl:   defw 0
s_de:   defw 0
s_ix:   defw 0
s_iy:   defw 0
s_sp:   defw 0
s_hl2:  defw 0
s_de2:  defw 0
s_bc2:  defw 0
s_a2:   defb 0
        end
