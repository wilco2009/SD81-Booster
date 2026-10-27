; mcu.asm - acceso del Z80 al MCU del SD81 Booster: handshake y ficheros de la SD (de SCUMM/AGI/V2/ENGINE/mcu.asm, sin f_seek ni sonido)
;
; El handshake: cada llamada mira el reloj del MCU; BC, DE y HL se conservan (A no sobrevive a mcu_send).
; Ficheros (ordenes del MCU 53 fopen, 55 fread, 57 fclose), en trozos de 512 bytes como maximo.

DataPort        equ $A7
ClkPort         equ $AF
CMD_FOPEN       equ 53
CMD_FREAD       equ 55
CMD_FCLOSE      equ 57
CHUNK           equ 512

mcu_send:       push bc
                ld b,a
                in a,(ClkPort)
                ld c,a
                ld a,b
                out (DataPort),a
mcu_sw:            in a,(ClkPort)
                xor c
                jp p,mcu_sw
                pop bc
                ret

mcu_recv:       push bc
                in a,(ClkPort)
                ld c,a
                in a,(DataPort)
                ld b,a
mcu_rw:            in a,(ClkPort)
                xor c
                jp p,mcu_rw
                ld a,b
                pop bc
                ret

; HL = nombre acabado en 0 -> A = manejador ($FF si no se puede abrir)
f_open:         ld b,0
                push hl
fop_l:           ld a,(hl)
                or a
                jr z,fop_e
                inc hl
                inc b
                jr fop_l
fop_e:           pop hl
                ld a,CMD_FOPEN
                call mcu_send
                ld a,b
                call mcu_send
                inc b
                jr fop_t
fop_s:           ld a,(hl)
                inc hl
                call mcu_send
fop_t:           djnz fop_s
                jp mcu_recv

; A = manejador, DE = cuantos (<= CHUNK), HL = destino -> HL avanza; A = estado
f_read:         ld c,a
                ld a,CMD_FREAD
                call mcu_send
                ld a,c
                call mcu_send
                ld a,e
                call mcu_send
                ld a,d
                call mcu_send
frd_l:            call mcu_recv
                ld (hl),a
                inc hl
                dec de
                ld a,d
                or e
                jr nz,frd_l
                jp mcu_recv

; A = manejador, HL = destino, BC = cuantos (cualquier cantidad) -> HL avanza
f_read_n:       ld (rd_h),a
                ld (rd_dst),hl
                ld (rd_cnt),bc
frn_l:           ld bc,(rd_cnt)
                ld a,b
                or c
                ret z
                ld hl,CHUNK
                or a
                sbc hl,bc
                jr c,frn_full            ; quedan mas de CHUNK
                ld d,b
                ld e,c
                jr frn_go
frn_full:        ld de,CHUNK
frn_go:          push de
                ld hl,(rd_dst)
                ld a,(rd_h)
                call f_read
                ld (rd_dst),hl
                pop de
                ld hl,(rd_cnt)
                or a
                sbc hl,de
                ld (rd_cnt),hl
                jr frn_l

f_close:        ld c,a
                ld a,CMD_FCLOSE
                call mcu_send
                ld a,c
                call mcu_send
                jp mcu_recv

rd_h:           defb 0
rd_dst:         defw 0
rd_cnt:         defw 0
