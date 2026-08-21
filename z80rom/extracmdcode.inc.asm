CmdBOLD:	set	1,(iy+iyFLAGS)	; set BOLD command flag
		jr	CommonInvBold
CmdINV:		res	1,(iy+iyFLAGS)	; clear BOLD command flag
CommonInvBold:
		call	CLASS_1		; Fetch variable as with LET
		bit	6,(iy+iyFLAGS)	; Numeric variable?
		jr	nz,ReportC5	; Error if so
		ex	de,hl		; Address to DE
		rst	GET_CHAR	; Check char after variable
		call	MustBeEOL	; Check if EOL and end syntax checking
		bit	1,(iy+iyFLAGX)	; Does the variable exist?
		jp	nz,REPORT_2	; REPORT-2 if not
		ld	a,b
		or	c
		ret	z		; Return if empty string
		ld	h,b		; Prepare B and H for DJNZ looping
		ld	l,c
		ld	b,c		; B ready
		dec	hl
		inc	h		; H ready
		ld	c,$80		; Bit 7 set
		ld	l,0		; Start by not setting any bits
		bit	1,(iy+iyFLAGS)	; Check BOLD mode
		res	1,(iy+iyFLAGS)	; Clear printer flag for next command
		jr	z,invStrBit7	; Skip next instruction if in INV mode
		ld	l,c		; This value of L will set bit 7
invStrBit7:	ld	a,(de)
		xor	c		; Flip bit 7 of char
		or	l		; Set bit 7 or don't do anything else
		ld	(de),a		;   (depending on command)
		inc	de		; Next char
		djnz	invStrBit7	; Loop (low byte)
		dec	h
		jr	nz,invStrBit7	; More loop (high byte)
		ret			; Done

CmdLDIR:	res	1,(iy+iyFLAGS)	; Clear LDDR flag
		jr	CommonLDIR
CmdLDDR:	set	1,(iy+iyFLAGS)	; Set LDDR flag
CommonLDIR:	call	CLASS_6		; Source address to calculator stack
		call	MustBeComma	; Comma required after number
		call	CLASS_6		; Dest address to calculator stack
		call	MustBeComma	; Another comma required
		call	CLASS_6		; Count to calculator stack
		call	MustBeEOL	; Command must end here
		call	FIND_INT	; Pop count from calculator stack...
		push	bc		; ... and push it to machine stack
		call	FIND_INT	; Pop destination address
		push	bc		; Dest to machine stack
		call	FIND_INT	; Pop source address
		ld	h,b		; Source address to HL
		ld	l,c
		pop	de		; Dest to DE
		pop	bc		; Count to BC
		bit	1,(iy+iyFLAGS)	; Was the command LDDR?
		jr	nz,wasLDDR	; Jump if so
		ldir			; Do the LDIR
		ret			; Done

wasLDDR:	res	1,(iy+iyFLAGS)	; Clear printer flag
		lddr			; Do the LDDR
		ret			; Done

ReportC5:	rst	ERROR_1
		db	$0B		; REPORT-C


CmdUSR:		rst	NEXT_CHAR	; Skip USR token
		res	1,(iy+iyFLAGS)	; Clear PRN flag for external routines
		call	CLASS_6		; Target address to calculator stack
		; Exceptionally, don't err if it's not the end of the command,
		; so that routines can parse the rest of the line on their own
		call	SyntaxDone	; Done if checking syntax
		call	FIND_INT	; Calculator stack to BC
		push	bc
		ret			; Jump to routine


CmdIN:		call	CLASS_6		; Read port number
		cp	.TO
		jr	nz,ReportC5	; Error if not followed by TO
		rst	NEXT_CHAR	; Skip TO
		call	CLASS_1		; Get variable details
		rst	GET_CHAR	; We should be at EOL now
		bit	6,(iy+iyFLAGS)	; Numeric variable?
		jr	z,ReportC5	; Error if not
		call	MustBeEOL	; Check EOL and end checking syntax
		call	FIND_INT	; Fetch port number
		in	c,(c)		; Read given port into C
		ld	b,0		; Set high byte to 0
		call	STACK_BC	; Value to calculator stack...
		jp	LET		; ... and store it into the variable


; Code almost like POKE, but we handle the syntax too
; (also, in POKE it seems that the check for ERR_NR is redundant)
CmdOUT:		call	CLASS_6		; Read port number
		call	MustBeComma	; Expect a comma after it
		call	CLASS_6		; Read data number
		call	MustBeEOL	; Check if EOL and stop syntax check
		call	FP_TO_A		; Fetch data value
		jp	c,ReportB	; Error if out of range
		jr	z,OutPositive	; If positive value, all good
		neg			; Negate
OutPositive:	push	af		; Preserve data value
		call	FIND_INT	; Fetch port number
		pop	af		; Restore data value
		out	(c),a		; Do the output
		ret


CmdPOKE:	rst	NEXT_CHAR	; Skip POKE token
		call	CLASS_6		; Read address
		call	MustBeComma	; Must be followed by a comma
		call	CLASS_6		; Read word to poke
		call	MustBeEOL	; Check if EOL and stop syntax check
		call	FIND_INT	; word to poke in BC
		push	bc		; save word in stack
		call	FIND_INT	; address in BC
		pop	de		; word to poke in DE now
		ld	h,b
		ld	l,c		; address in HL now
		ld	(hl),e		; store value
		inc	hl
		ld	(hl),d
		ret

CmdCLEAR:	rst	NEXT_CHAR	; Skip CLEAR token
		call	CLASS_6		; Read address
		call	MustBeEOL	; Expect line ending and finish syntax
		call	FIND_INT	; RAMTOP address in BC
		inc	bc		; CLEAR uses 1 less, compensate
		ld	hl,(E_LINE)	; Start of workspace
		ld	de,$0028	; TEST_ROOM uses $24; we add 4 because
					;   of GOSUB marker and ERR_SP address
		add	hl,de		; (not expected to overflow)
		ex	de,hl		; sum to DE
		ld	h,b
		ld	l,c		; HL = requested address
		sbc	hl,de		; Requested - (workspace+24)
		jp	c,REPORT_4	; Error 4 if overflow
		add	hl,de		; Restore value of requested address
		ld	(RAMTOP),hl	; store value
		dec	hl
		ld	(hl),$3E	; GOSUB marker
		dec	hl
		ld	sp,hl
		dec	hl
		dec	hl
		ld	(ERR_SP),hl
		jp	L0676		; Jump to main loop

CmdTHEN:	rst	NEXT_CHAR	; Skip THEN token
		cp	.nl
		ret	z		; LOAD THEN does nothing
		cp	.POKE		; was the command a POKE?
		jr	z,CmdPOKE	; Process 16-bit POKE command if so
		cp	.CLEAR		; was the command a POKE?
		jr	z,CmdCLEAR	; Process 16-bit POKE command if so
		cp	.PRINT
		jp	z,CmdPRINT
		cp	.LPRINT
		jp	z,CmdLPRINT
ReportC6:	rst	ERROR_1
		db	$0B		; REPORT-C

CmdPEEK:	rst	NEXT_CHAR	; Skip PEEK token
		call	CLASS_6		; Read address
		cp	.TO
		jr	nz,ReportC6	; Error if not followed by TO
		rst	NEXT_CHAR	; Skip TO token
		call	CLASS_1		; Get variable details
		bit	6,(iy+iyFLAGS)	; Numeric variable?
		jr	z,ReportC6	; Error if not
		rst	GET_CHAR	; We should be at EOL now
		call	MustBeEOL	; Err if not, stop if checking syntax
		call	FIND_INT	; Fetch address
		ld	h,b
		ld	l,c		; HL = address
		ld	c,(hl)		; Read word at (HL) into BC
		inc	hl
		ld	b,(hl)
		call	STACK_BC	; store word in calculator stack
		jp	LET		; exit via LET

CmdHEX:		call	CLASS_6		; Read address
		call	MustBeComma	; Must be followed by a comma
		call	SCANNING	; read string expression
		call	MustBeString	; error if not a string
		call	MustBeEOL	; err if not EOL; stop syntax check
		call	STK_FETCH	; retrieve string parameters
		push	de
		push	bc
		call	FIND_INT	; retrieve stacked address
		pop	hl		; length
		pop	de		; address
		jp	HexDecodeCommon

; Numero de sprites de hardware -- DEBE coincidir con NUM_SPRITES en SD81.v.
; Unico sitio a tocar aqui si ese valor cambia.
SPR_COUNT equ 32

; LOAD *SPRCOL  <n>,"<16 hex chars>"  -- 8 bytes of per-row colour (2105-2112)
; LOAD *SPRPIX  <n>,"<16 hex chars>"  -- 8 bytes of pixel data     (2113-2120)
; LOAD *SPRMASK <n>,"<16 hex chars>"  -- 8 bytes of mask           (2121-2128)
; Loads one of the three 8-byte blocks of hardware sprite <n> (0 to
; SPR_COUNT-1) without needing to remember its POKE address. Equivalent to
; POKE 2100,<n> followed by LOAD *HEX <address>,"<16 chars>". Position/
; enable is separate, see LOAD *SPRITE.
; NOTA sobre la estructura: MustBeEOL acaba en SyntaxDone, que durante el
; chequeo de sintaxis hace "pop hl" para descartar la direccion de retorno
; del comando. Como el despachador entra al comando con jp (hl) (no con
; call), MustBeEOL TIENE que llamarse desde el nivel superior de la rutina
; del comando y sin nada nuestro apilado, o se descarta el valor equivocado
; y la pila queda corrompida. Por eso el analisis comun (SprParseArgs) NO
; incluye MustBeEOL, y la direccion base del bloque se carga despues.
CmdSPRCOL:	call	SprParseArgs	; <n>,"<hex>" (sin MustBeEOL)
		call	MustBeEOL	; err if not EOL; stop syntax check
		ld	bc,2105		; colour rows
		jr	SprBlockTail
CmdSPRPIX:	call	SprParseArgs
		call	MustBeEOL
		ld	bc,2113		; pixel rows
		jr	SprBlockTail
CmdSPRMASK:	call	SprParseArgs
		call	MustBeEOL
		ld	bc,2121		; mask rows
		; fall through to SprBlockTail

; A partir de aqui solo se ejecuta en tiempo de ejecucion (MustBeEOL no
; retorna durante el chequeo de sintaxis), asi que ya es seguro apilar.
SprBlockTail:	push	bc		; block base address, recovered below
		call	STK_FETCH	; retrieve string parameters
		push	de
		push	bc
		call	FIND_INT	; retrieve stacked sprite number
		ld	a,b
		or	a		; sprite number must fit in one byte
		jp	nz,REPORT_B
		ld	a,c
		cp	SPR_COUNT	; valid range is 0..SPR_COUNT-1
		jp	nc,REPORT_B
		ld	(2100),a	; select sprite (same as POKE 2100,<n>)
		pop	hl		; string length (hex chars)
		pop	de		; string pointer
		ld	a,h
		or	a		; must be a short string (<256 chars)
		jp	nz,REPORT_A
		ld	a,l
		cp	16		; must be EXACTLY 16 hex chars (8 bytes)
		jp	nz,REPORT_A
		pop	bc		; block base address
		jp	HexDecodeCommon

; Analisis comun de "<n>,"<hex>"" -- deja A con el caracter siguiente para
; que el llamante haga MustBeEOL. Ver la nota de arriba sobre por que
; MustBeEOL no puede ir aqui dentro.
SprParseArgs:	call	CLASS_6		; Read sprite number
		call	MustBeComma	; Must be followed by a comma
		call	SCANNING	; read string expression
		jp	MustBeString	; error if not a string, and return

; LOAD *SPRITE <n>,<x>,<y>
; Selects hardware sprite <n> (0 to SPR_COUNT-1), sets its position and
; enables it.
; <x> is 0-318, <y> is 0-255; coordinate 32 in both axes is the top-left
; corner of the screen (see the sprite coordinate system in the manual).
; Equivalent to: POKE 2100,<n> / POKE 2102,<x_low> / POKE 2103,<x_high> /
; POKE 2104,<y> / POKE 2101,1.
;
; LOAD *SPRITE <n> STOP hides sprite <n> (POKE 2101,0) without touching its
; position or graphic data.
; NOTA: FIND_INT recupera los parametros apilados en orden INVERSO al de
; lectura (aqui: Y, luego X, luego n) -- hay que seleccionar el sprite con
; POKE 2100,n ANTES de escribir su posicion, o la posicion se escribe sobre
; el sprite que estuviera seleccionado por la sentencia ANTERIOR. Por eso X
; e Y se apilan (igual que CmdPOKE apila el dato mientras calcula la
; direccion) y no se escriben hasta tener n validado y seleccionado.
CmdSPRITE:	call	CLASS_6		; Read sprite number (leaves next char in A)
		cp	.STOP
		jr	z,SprHide
		call	MustBeComma	; must be a comma before X
		call	CLASS_6		; Read X (0-318)
		call	MustBeComma	; must be a comma before Y
		call	CLASS_6		; Read Y (0-255)
		call	MustBeEOL
		call	FIND_INT	; BC = Y (last pushed, first popped)
		ld	a,b
		or	a		; Y must fit in one byte
		jp	nz,REPORT_B
		ld	a,c
		push	af		; stash Y until the sprite is selected
		call	FIND_INT	; BC = X
		ld	hl,318
		or	a		; clear carry for the subtraction
		sbc	hl,bc
		jp	c,REPORT_B	; X must be 0-318
		push	bc		; stash X (C=low, B=high) until selected
		call	FIND_INT	; BC = sprite number n
		ld	a,b
		or	a
		jp	nz,REPORT_B
		ld	a,c
		cp	SPR_COUNT	; valid range is 0..SPR_COUNT-1
		jp	nc,REPORT_B
		ld	(2100),a	; select sprite FIRST
		pop	bc		; X back (C=low, B=high)
		ld	a,c
		ld	(2102),a	; x_low
		ld	a,b
		ld	(2103),a	; x_high
		pop	af		; Y back
		ld	(2104),a	; y_pos
		ld	a,1
		ld	(2101),a	; enable
		ret
SprHide:	rst	NEXT_CHAR	; skip STOP token
		call	MustBeEOL
		call	FIND_INT	; BC = sprite number n
		ld	a,b
		or	a
		jp	nz,REPORT_B
		ld	a,c
		cp	SPR_COUNT
		jp	nc,REPORT_B
		ld	(2100),a	; select sprite
		xor	a
		ld	(2101),a	; disable
		ret

; LOAD *SCROLL <n>
; Set the Superfast fine horizontal scroll offset (POKE 2090, 0-7 pixels).
; Applies to all Superfast submodes; no effect in native mode. The extra
; column revealed on the right (Superfast text) is the DFILE NEWLINE byte
; of each row, which software can fill with real content.
CmdSCROLL:	call	CLASS_6		; Read offset number
		call	MustBeEOL
		call	FIND_INT	; BC = offset number
		ld	a,b
		or	a
		jp	nz,REPORT_B
		ld	a,c
		cp	8		; valid range is 0-7
		jp	nc,REPORT_B
		ld	(2090),a
		ret

HexDecodeCommon:
		bit	0,l		; length must be even
		jp	nz,REPORT_A	; Invalid argument otherwise
		ld	a,h
		or	l
		ret	z		; Zero length
		dec	hl
		inc	l
		inc	h
; Maybe we should make a first pass over the string to detect invalid hex
; chars and err before loading any bytes, but currently they are detected as
; we go because it's faster.
CodeNext256:	push	hl
CodeNextByte:	ld	a,(de)
		sub	.0
		cp	16
		jp	nc,REPORT_B
		inc	de
		rlca
		rlca
		rlca
		rlca
		ld	h,a
		dec	l
		ld	a,(de)
		sub	.0
		cp	16
		jp	nc,REPORT_B
		inc	de
		or	h
		ld	(bc),a
		inc	bc
		dec	l
		jr	nz,CodeNextByte
		pop	hl
		ld	l,0
		dec	h
		jr	nz,CodeNext256
		ret
