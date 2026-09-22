; SD Interface for ZX81
; (C) Copyright 2023 Pedro Gimeno Fortea
; License: TBD

; Next 16K of ROM/RAM after the ZX81 BIOS

;		org	8192

		db	.S,.D,.8,.1	; "SD81" interface signature
; 8196 ($2004)
VERSION:	db	$15		; ROM version 1.5

; 8197 ($2005)
		pop	hl		; utility for finding your address
; 8198 ($2006)
		jp	(hl)		; utility for CALL (HL)

; 8199 ($2007)
		jp	GetMCUVersion
; 8202 ($200A)
		jp	WaitClkDiff
; 8205 ($200D)
		jp	WaitClkEq
; 8208 ($2010)
		jp	OutWaitDiff
; 8211 ($2013)
		jp	OutWaitEq
; 8214 ($2016)
		jp	WaitDiffBrk
; 8217 ($2019)
		jp	WaitEqBrk
; 8220 ($201C)
		jp	SendString
; 8223 ($201F)
		jp	SendStrLoop
; 8226 ($2022)
		jp	ReportStatus
; 8229 ($2025)
		jp	PrintBPaged
; 8232 ($2028)
		jp	Cmd64C
; 8235 ($202B)
		jp	Cmd128C
; 8238 ($202E)
		jp	GetPhase
; 8241 ($2031)
		jp	GetData
; 8244 ($2034)
		jp	SD81_RESET
; 8247 ($2037)
		jp	SD81LOADCMD
; 8250 ($203A)
		jp	SD81SAVECMD
; 8253 ($203D)
		jp	SD81RUNCMD


DataPort	equ	0xA7
ClkPort		equ	0xAF
MapperPort	equ	0xE7
SpulaPort	equ	0xFB	; Spectrum-mode border/beeper ULA-style port (low byte only decoded)

CMD_nop		equ	0x00
CMD_ver		equ	0x01
CMD_pwd		equ	0x02
CMD_cd		equ	0x03
CMD_del		equ	0x04
CMD_mkdir	equ	0x05
CMD_rmdir	equ	0x06
CMD_move	equ	0x07
CMD_copy	equ	0x08
CMD_load	equ	0x09
CMD_save	equ	0x0A
CMD_printfile	equ	0x0B
CMD_dir		equ	0x0C
CMD_nextch	equ	0x0D
CMD_free_txt	equ	0x0E
CMD_free_bin	equ	0x0F
CMD_opendir	equ	0x10
CMD_getrowlen	equ	0x11
CMD_getrow	equ	0x12
CMD_mc45_on	equ	0x13
CMD_mc45_off	equ	0x14
CMD_joy		equ	0x15
CMD_talk	equ	0x17
CMD_aysend	equ	0x18
CMD_ayread	equ	0x19
CMD_play	equ	0x1A
CMD_chars128	equ	0x1B
CMD_chars64	equ	0x1C
CMD_pages64	equ	0x1D
CMD_pages32	equ	0x1E
CMD_getFPGAVer	equ	0x1F
CMD_getbyte	equ	0x20
CMD_setbyte	equ	0x21
CMD_loadVGM	equ	0x22
CMD_stopVGM	equ	0x23
CMD_pauseVGM	equ	0x24
CMD_contVGM	equ	0x25
CMD_loopVGM	equ	0x26
CMD_loadPEG	equ	0x28
CMD_playPEG	equ	0x29
CMD_stopPEG	equ	0x2A
CMD_pausePEG	equ	0x2B
CMD_contPEG	equ	0x2C
CMD_loadPEB	equ	0x2D
CMD_ichr_on	equ	0x2E
CMD_ichr_off	equ	0x2F
CMD_std48k_on	equ	0x30
CMD_std48k_off	equ	0x31
CMD_rtc		equ	0x32
CMD_batt	equ	0x34
CMD_dst_on	equ	0x3C
CMD_dst_off	equ	0x3D
CMD_ntp_setserver equ	0x3E
CMD_ntp_setoffset equ	0x3F
CMD_ntp_sync	equ	0x40
CMD_chars256	equ	0x41	; LOAD *256C (siguiente slot libre en la tabla MCU)
; 0x42/0x43 (66/67) reservados para NET_READ/NET_WRITE -- se invocan desde
; codigo maquina directamente (ver claude/planning/net_bridge_emulator.md),
; no tienen entrada en la CmdList de LOAD * de aqui abajo.
CMD_romlock_on	equ	0x44	; LOAD *ROMLOCK
CMD_romlock_off	equ	0x45	; LOAD *ROMLOCK STOP
CMD_loadZ81	equ	0x46	; LOAD *Z81 "fichero" -- snapshot EightyOne

; ROM restart routines
ERROR_1		equ	08H
PRINT_A		equ	10H
GET_CHAR	equ	18H
NEXT_CHAR	equ	20H
BC_SPACES	equ	30H

; ROM routines
ERROR_3		equ	L0058
SLOW_FAST	equ	L0207
SET_FAST	equ	L02E7
REPORT_F	equ	L02F4
SAVE		equ	L02F6
LOAD		equ	L0340
REPORT_D	equ	L03A6
INITIAL		equ	L03E5
RAM_CHECK	equ	L03CB
LOC_ADDR	equ	L0918
CLS		equ	L0A2A
RECLAIM_2	equ	L0A60		; reclama BC bytes desde HL, ajustando punteros
STK_TO_A	equ	L0C02
REPORT_9	equ	L0CDC
CLASS_1		equ	L0D3C
REPORT_2	equ	L0D4B
CLASS_6		equ	L0D92
REPORT_C	equ	L0D9A
SYNTAX_Z	equ	L0DA6
GOTO_BC		equ	L0E84
FIND_INT	equ	L0EA7
REPORT_B	equ	L0EAD
RUN_COMMAND	equ	L0EAF
REPORT_4	equ	L0ED3
FAST		equ	L0F23
BREAK_1		equ	L0F46
SCANNING	equ	L0F55
LOOK_VARS	equ	L111C
STK_STO_s	equ	L12C3
LET		equ	L1321
STK_FETCH	equ	L13F8
STACK_BC	equ	L1520
FP_TO_A		equ	L15CD
REPORT_A	equ	L1CAF
PR_STR_x	equ	L0B6B+$000A	; This entry point has no label.

; System variables
ERR_NR		equ	$4000
FLAGS		equ	$4001
ERR_SP		equ	$4002
RAMTOP		equ	$4004
VERSN		equ	$4009
D_FILE		equ	$400C
VARS		equ	$4010
E_LINE		equ	$4014
CH_ADD		equ	$4016
DF_SZ		equ	$4022
LAST_K		equ	$4025
FLAGX		equ	$402D
S_POSN		equ	$4039
CDFLAG		equ	$403B
SV_END		equ	$407D

; IY relative system variables
iyVAL		equ	$4000		; value of iy
iyERR_NR	equ	FLAGS-ERR_NR
iyFLAGS		equ	FLAGS-iyVAL
iyFLAGX		equ	FLAGX-iyVAL
iyCDFLAG	equ	CDFLAG-iyVAL

WaitEqBrk:	ld	a,c
		cpl
		ld	c,a

WaitDiffBrk:	in	a,(ClkPort)
		xor	c
		ret	m		; Return if different
		call	BREAK_1
		jr	c,WaitDiffBrk	; Loop while break not pressed
		in	a,(ClkPort)	; Fresher data to minimize the chance
		xor	c		;  of a race
		ret	m
		out	(ClkPort),a	; Reset MCU
		call	WaitClkDiff
		rst	ERROR_1
		db	$0C		; REPORT-D

; Token SLOW found
SDSlowTkn:	rst	NEXT_CHAR	; NEXT-CHAR
		cp	.nl		; NEWLINE?
		jr	nz,UseTape	; Normal LOAD/SAVE if not
		call	SyntaxDone	; Stop here if checking syntax
SetTapeOn:
		ld	b,0		; tape mode ON
;		jr	SetTapeOnOff

SetTapeOnOff:	in	a,(ClkPort)
		ld	c,a
		ld	a,CMD_setbyte
		call	OutWaitDiff
		xor	a		; index 0 is mode
		call	OutWaitEq
OutBWaitDiff:	ld	a,b
;		jr	OutWaitDiff

OutWaitDiff:	out	(DataPort),a
WaitClkDiff:	in	a,(ClkPort)
		xor	c
		jp	p,WaitClkDiff	; wait until bit 7 of A and C differ
		ret

OutWaitEq:	out	(DataPort),a
WaitClkEq:	in	a,(ClkPort)
		xor	c
		jp	m,WaitClkEq	; wait until bit 7 of A and C equal
		ret

GetMCUVersion:	in	a,(ClkPort)
		ld	c,a

		ld	a,CMD_ver
		call	OutWaitDiff	; send VER command

		in	a,(DataPort)
		ld	b,a
		call	WaitClkEq
		ld	c,b		; low byte
		ld	b,0		; high byte = 0
		ret

; Returns the FPGA version byte (high nibble=major, low nibble=minor,
; same format as VERSION/GetMCUVersion) in C. The MCU reads this once at
; boot straight from the config flash (see FPGA_VERSION_FLASH_ADDR in the
; STM32 firmware) and just echoes the cached byte here -- same protocol
; shape as GetMCUVersion, different command code.
GetFPGAVersion:	in	a,(ClkPort)
		ld	c,a

		ld	a,CMD_getFPGAVer
		call	OutWaitDiff	; send GETFPGAVER command

		in	a,(DataPort)
		ld	b,a
		call	WaitClkEq
		ld	c,b
		ret

; Check default LOAD/SAVE mode (SD or tape)
; Input: C = clock
ChkTapeMode:	in	a,(ClkPort)
		ld	c,a
		ld	a,CMD_getbyte
		call	OutWaitDiff
		xor	a
		call	OutWaitEq
		in	a,(DataPort)
		ld	b,a
		call	WaitClkDiff
		ld	a,b
		or	a
		ret

UseTape:	ld	hl,FLAGS
		bit	1,(hl)		; Check if LOAD or SAVE mode
		res	1,(hl)		; restore it to normal
		jp	nz,LOAD
		jp	SAVE


; Entry point for the LOAD command handler
SD81LOADCMD:	set	1,(iy+iyFLAGS)	; 1 = LOAD mode
		jr	SaveLoadCommon

; Token FAST found - change mode if there's no name, otherwise use SD
SDFastTkn:	rst	NEXT_CHAR	; find token after FAST
		cp	.nl		; NEWLINE?
		jr	nz,SaLoFastRest	; check rest of the line if not
SDLoadSaveFast:
		call	SyntaxDone	; If checking syntax, we're done
		ld	b,1		; SD mode ON
		jr	SetTapeOnOff

; We abuse the PRINT flag, which should be reset, to save the LOAD/SAVE state
; Entry point for the SAVE command handler
SD81SAVECMD:	res	1,(iy+iyFLAGS)	; 0 = SAVE mode
SaveLoadCommon:	rst	GET_CHAR	; what's the first char after LOAD?
		cp	.SLOW		; "SLOW" token?
		jp	z,SDSlowTkn	; Process syntax with SLOW
		cp	.FAST		; "FAST" token?
		jr	z,SDFastTkn	; Process syntax with FAST
		bit	1,(iy+iyFLAGS)	; Save mode?
		jr	z,NoMoreArgs	; if SAVE, don't accept more variants
		cp	.star		; "*" character?
		jp	z,SDExtra	; Process extra commands
		cp	.LPRINT		; "LPRINT" token?
		jp	z,SDExtraPrn	; Same as * but to printer
		cp	.USR		; "USR" token?
		jp	z,CmdUSR	; Process USR command
		cp	.PEEK		; "PEEK" token?
		jp	z,CmdPEEK	; Process PEEK command
		cp	.THEN		; "THEN" token?
		jp	z,CmdTHEN	; Process the THEN family

		; No special arguments - check default mode
NoMoreArgs:	call	SYNTAX_Z	; checking syntax?
		jr	z,UseTape	; chain to regular tape routine if so
		call	ChkTapeMode	; are we in tape or SD mode?
		jr	z,UseTape	; if in Tape mode, use the ROM routine

		; We're running and in tape emulation mode; any arguments are
		; allowed after the string and will be ignored, but empty
		; strings are not allowed as filename.
		call	NAME_2
		ret	nc		; return if checking syntax
SaLoProg:	exx
		ld	b,0FFh		; Flag "Don't GOTO anywhere"
SaLoProg2:	ld	hl,(E_LINE)
		ld	de,VERSN	; loading address for BASIC program
		and	a
		sbc	hl,de		; DE' = address, HL' = length
		exx
		jp	SaLoBytes	; no more checks, CODE not interpreted

; Enter here when LOAD/SAVE FAST <something> is found
SaLoFastRest:	call	NAME_2		; Expect a string expression
		rst	GET_CHAR	; find char after the string expr.
		cp	.CODE		; "CODE" token?
		jr	z,SaLoCodeTkn	; CODE token found
		cp	.STRs		; "STR$" token? (SCREEN$ for us)
		jr	z,SaLoScreen
		cp	.THEN		; "THEN" token?
		jp	z,SaLoThen
		call	MustBeEOL	; Line must terminate here
		; handle LOAD FAST <string>
		jr	SaLoProg	; Load program if not

SaLoScreen:	exx
		ld	hl,(D_FILE)
		ld	de,33*24+1
		ex	de,hl
		ld	b,0FFh		; Flag "Don't GOTO anywhere"
		exx
		jr	SaLoBytes

SaLoCodeTkn:	rst	NEXT_CHAR	; skip CODE token

		push	de		; address of filename (from NAME_2)
		push	bc		; length of filename
		push	hl		; make room for length in stack
					; (value is irrelevant)

		call	CLASS_6		; Scan number, error C if string
		call	SYNTAX_Z	; Checking syntax?
		jr	z,SkipFetchAddr	; Skip fetching number if so
		call	FIND_INT	; Number to BC (handles overflow)
		pop	hl
		push	bc		; Replace (SP) with address param
SkipFetchAddr:	bit	1,(iy+iyFLAGS)	; SAVE mode?
		jr	nz,SkipFetchCnt	; LOAD only has an addr parameter
		rst	GET_CHAR	; find next token after expression
		call	MustBeComma	; expect comma
		call	CLASS_6		; Scan another number
		call	SYNTAX_Z	; Checking syntax?
		jr	z,SkipFetchCnt	; Skip fetching number if so
		call	FIND_INT	; this will set BC to the count
SkipFetchCnt:	rst	GET_CHAR	; find next token
		push	bc		; result of cnt if it was fetched
		exx
		pop	hl		; store the count in HL'
					; (or whatever BC had if not saving)
		pop	de		; store the address in DE'
		ld	b,0FFh		; Flag "Don't GOTO anywhere"
		exx
		pop	bc		; restore length of string
		pop	de		; restore pointer to string
		call	MustBeEOL	; End of line expected

SaLoBytes:
		call	BC_1_255	; ensure filename is valid

		in	a,(ClkPort)
		ld	c,a		; bit 7 of C = clock at start

		ld	a,CMD_load	; otherwise "relative jump out of rng"

		bit	1,(iy+iyFLAGS)	; save mode?
		jp	z,SDSave	; continue in SDSave if so

		call	OutWaitDiff	; wait for ack of our write

		call	SendString	; Send filename (inverts bit 7 of C)

		exx
		push	de		; Copy DE'...
		exx
		pop	de		; ... to DE

		in	a,(DataPort)
		ld	l,a		; Length low byte

		call	WaitClkDiff	; wait for ack of our read
		; bit 7 of C is the inverted clock now

		in	a,(DataPort)
		ld	h,a		; Length high byte

		call	WaitClkEq	; wait for ack of this other read
		; bit 7 of C equals the clock now

		ld	a,h
		ld	b,l		; for DJNZ
		dec	hl		; adjust H for DJNZ looping
		inc	h		; adjust H for DJNZ looping
		or	b		; length 0? (A is lenHigh, B = lenLow)
		jr	nz,SDLoad_2	; continue normally if not

		; Length is zero; the status byte comes next.
		; Also entry point for a final status byte in many routines.
ReportStatus:
		in	a,(DataPort)	; read status code
		ld	l,a

		; Wait for a clock change and then report the error
		call	WaitClkDiff

		ld	a,l
		or	a
		ret	z		; zero status = all good
		add	a,.F-.1		; status 1 = error G and so on
		ld	l,a
		jp	ERROR_3		; stores L as ERR_NR

SDLoad_2:
		call	SET_FAST	; because we'll mess with system
					; variables including DFILE

		bit	7,c		; Is clock 0 or 1?
		ld	c,$80
		jr	nz,SDLoad_3	; Clock is 1, wait for 0

SDLoadLoop:	in	a,(DataPort)
		ld	(de),a
		inc	de

SDLoadW1:	in	a,(ClkPort)
		rlca
		jr	nc,SDLoadW1	; wait for 1

		djnz	SDLoad_3
		dec	h
		jr	z,SDLoadFinish	; exit with bit 7 of C set if EOT

SDLoad_3:	in	a,(DataPort)
		ld	(de),a
		inc	de

SDLoadW0:	in	a,(ClkPort)
		rlca
		jr	c,SDLoadW0	; wait for 0

		djnz	SDLoadLoop
		dec	h
		jr	nz,SDLoadLoop
		ld	c,b		; B is 0 after DJNZ, set C = 0 because
					; clock is 0 at this point

SDLoadFinish:	; On entry, bit 7 of C = current clock
		in	a,(DataPort)	; Read status byte
		ld	b,a

		call	WaitClkDiff	; Wait for ack of status read

		ld	a,b
		or	a		; status not zero?
		jr	nz,SDBadLoad	; oops, things went wrong

		call	SLOW_FAST	; ok, we can calm down now
		exx			; retrieve BC' where line info is
		ld	a,b
		inc	a
		ret	z		; done if it was FF
		inc	a
		jp	nz,GOTO_BC	; if it wasn't FE, assume it's an int
		rst	ERROR_1
		db	$08		; REPORT-9

		; Inform the user and issue a RESET
SDBadLoad:
		ld	hl,ErrMsgAdr
		ld	de,SV_END
		ld	(D_FILE),de
		ld	bc,ErrMsgLen
		ldir

		add	a,.G-1		; error from G on
		ld	(SV_END+ErrCodeOfs),a

		set	6,(iy+iyCDFLAG)	; SLOW mode
		call	SLOW_FAST	; make it effective

SDLoadWaitBrk:	call	BREAK_1		; check BREAK key
		jr	c,SDLoadWaitBrk	; keep waiting until BREAK pressed
		jp	INITIAL		; initialize overwritten stuff



SDSave:
		call	SET_FAST	; SET-FAST to freeze system vars ASAP

		ld	a,CMD_save
		call	OutWaitDiff	; wait for ack of our write

		call	SendString	; send filename to save

		ld	a,c		; copy current clock to C'
		exx			; DE = target, HL = length
		ld	c,a

		ld	a,l
		call	OutWaitDiff	; send low byte

		ld	a,h
		call	OutWaitEq	; send high byte

		ld	b,l		; for DJNZ loop
		dec	hl		; adjust for DJNZ loop
		inc	h		; adjust for DJNZ loop

SDSave_1:	call	SendStrLoop
		dec	h
		jr	nz,SDSave_1

		call	SLOW_FAST	; restore previous mode
		jp	ReportStatus	; exit via status reporting


LoadStop:	exx
		ld	b,$FE		; stop after finishing
		exx
		rst	NEXT_CHAR
		jr	LoadExpectEOL

; Process LOAD FAST name THEN
SaLoThen:	bit	1,(iy+iyFLAGS)
		jr	z,ReportC1	; Syntax error if in SAVE mode
		rst	NEXT_CHAR	; what's next?
		cp	.STOP
		jr	z,LoadStop
		cp	.GOTO
		jr	nz,ReportC1	; error if not STOP or GOTO
		rst	NEXT_CHAR	; skip GOTO
		push	bc
		push	de
		exx
		call	CLASS_6
		call	SYNTAX_Z
		jr	z,LoadNoINT
		call	FIND_INT	; BC' = number
		rst	GET_CHAR	; restore last char to check for NL
LoadNoINT:	exx
		pop	de
		pop	bc
LoadExpectEOL:
		call	MustBeEOL	; Line must end here
		exx			; SaLoProg2 expects alternate regs
		jp	SaLoProg2

; SAVE/LOAD flag is no longer needed, because extra commands can only be used
; with the LOAD command, so we know it's set at this point. So we use it as
; the printer flag from this point on, which is what it is.
SDExtra:	res	1,(iy+iyFLAGS)	; Clear printer flag
SDExtraPrn:	; Printer Flag is set if using this entry point

		rst	NEXT_CHAR	; skip *, also grabs CH_ADD in HL
					; (we're going to handle CH_ADD
					; manually in this routine, to make
					; it easier and faster to backtrack
					; and retry). Also better to report
					; syntax error at the beginning of the
					; command (by not updating X_PTR).
		ld	de,CmdList
NextCmd:
		push	hl
NextCmdChar:
		ld	a,(de)		; Grab next command char from table
		inc	de
		ld	c,a		; keep bit 7 safe
RetryChar:	and	$7F
		cp	(hl)		; Compare with command line
		jr	nz,Mismatch
		inc	hl
		bit	7,c
		jr	z,NextCmdChar

		; Found end of command marker in the table. It will only be a
		; match if the next char is not alphanumeric.
MatchTryAgain:	ld	a,(hl)
		inc	hl		; prepare next character
		cp	.cursor
		jr	z,MatchTryAgain	; if cursor, skip
		cp	.0
		jr	c,Matched
		cp	.Z+1
		jr	nc,Matched	; if not between 0 and Z, accept
		jr	FindEndOfEntry	; not this command, skip to the next
Matched:
		dec	hl		; go back to position of terminator
		ld	(CH_ADD),hl	; update lexer pointer
		pop	hl		; clear HL from stack
		ex	de,hl
		ld	e,(hl)
		inc	hl
		ld	d,(hl)
		rst	GET_CHAR	; prepare token for command
		ex	de,hl
		jp	(hl)

Mismatch:
		; Could still be the cursor!
		ld	a,(hl)
		inc	hl
		cp	.cursor
		ld	a,c
		jr	z,RetryChar
		; Actual mismatch
FindEndOfEntry:	rlca
		jr	c,SkipHandlerPtr
FindEndLoop:	ld	a,(de)
		inc	de
		rlca
		jr	nc,FindEndLoop
SkipHandlerPtr:	inc	de
		inc	de

		pop	hl		; restart from the beginning
		ld	a,(de)
		or	a
		jr	nz,NextCmd
ReportC1:	rst	ERROR_1
		db	$0B		; REPORT-C

; LOAD *VER
CmdVER:
		call	MustBeEOL	; command must end here

		ld	a,.R
		rst	PRINT_A
		ld	a,.O
		rst	PRINT_A
		ld	a,.M
		rst	PRINT_A
		ld	a,.colon
		rst	PRINT_A
		ld	a,(VERSION)
		ld	b,a

		call	VerFromBCD

		xor	a		; space
		rst	PRINT_A
		ld	a,.M
		rst	PRINT_A
		ld	a,.C
		rst	PRINT_A
		ld	a,.U
		rst	PRINT_A
		ld	a,.colon
		rst	PRINT_A

		call	GetMCUVersion
		ld	a,c
		ld	b,a		; GetMCUVersion pone B a 0 (uso en BC de 16 bits en
					; otro sitio) -- VerFromBCD saca el digito menor de
					; B, asi que hay que recargarlo con el byte real o
					; el minor de MCU sale siempre en 0

VerFromBCD:
		rlca
		rlca
		rlca
		rlca
		call	Hex1
		ld	a,.dot
		rst	PRINT_A
		ld	a,b
Hex1:
		and	$0F
		add	a,.0
Print_A_ret:
		rst	PRINT_A
		ret

; LOAD *FPGA
; Prints the FPGA bitstream version (stamped into the config flash at
; build time, see FPGA/SD81V2.1000/append_fpga_version.py). Separate
; command from LOAD *VER on purpose: an old ROM/MCU pairing that doesn't
; know CMD_getFPGAVer would just hang waiting for a reply if we folded
; this into the existing *VER output format instead.
CmdFPGA:	call	MustBeEOL	; command must end here

		ld	a,.F
		rst	PRINT_A
		ld	a,.P
		rst	PRINT_A
		ld	a,.G
		rst	PRINT_A
		ld	a,.A
		rst	PRINT_A
		ld	a,.colon
		rst	PRINT_A

		call	GetFPGAVersion
		ld	a,c
		jp	VerFromBCD

; LOAD *DIR
; LOAD *DIR <string>
CmdDIR:
		ld	bc,0		; length 0
		cp	.nl
		jr	z,GotStrDir
		call	NAME_2		; get filename expression
		rst	GET_CHAR
GotStrDir:	call	MustBeEOL	; expect end of line here

		; Similar to BC_1_255 but BC can be 0 too
		ld	a,b
		or	a		; Length > 255?
		jp	nz,REPORT_F	; Error F if so
		ld	b,c

		ld	a,CMD_dir
PrintFileEntry:
		ld	c,ClkPort
		in	c,(c)		; Set bit 7 of C to current clock

		call	OutWaitDiff	; Send cmd, wait until different

		call	SendString	; SendString requires different and
					; returns equal clock

		jr	PrintCmdOutput

; LOAD *PWD
CmdPWD:
		ld	b,CMD_pwd
		jr	PrintOnlyCmd

; LOAD *FREE
CmdFREE:
		ld	b,CMD_free_txt
PrintOnlyCmd:
		cp	.nl
		jp	nz,REPORT_F
		call	SyntaxDone	; Stop here if checking syntax

		in	a,(ClkPort)
		ld	c,a

		call	OutBWaitDiff	; Send cmd, wait until different

		; Fall through to PrintCmdOutput

		; PrintCmdOutput accepts any phase and returns with bit 7 of C
		; = inverted clock,
PrintCmdOutput:
		in	a,(ClkPort)
		ld	c,a		; Read current clock

		ld	a,CMD_nextch
		call	OutWaitDiff	; Send nextch command

		in	a,(DataPort)	; Read next char
		ld	b,a		; Store in B
		call	WaitClkEq

		ld	a,b
		cp	$6F		; terminator?
		jp	z,ReportStatus	; NOTE: inverts the phase!
		call	PrintBPaged	; Print value in B with paging
		jr	PrintCmdOutput	; Loop until finished

PrintBPaged:
		; Test if the character can be printed without error 5
		ld	hl,(S_POSN)	; Screen position
		cp	.nl+1		; Char is NEWLINE?
		jr	z,CheckRow	; skip if so to check row
		ld	a,l		; 33 minus column number
		dec	a		; At end of line?
		jr	nz,CheckRow	; Skip if not
		ld	l,$21		; Adjust coordinate to where the
		dec	h		;   line will actually be printed
CheckRow:	ld	a,(DF_SZ)	; size of bottom of screen
		cp	h		; Hit end of screen?
		jr	nz,OkToPrint	; Can print if not

		push	bc		; save phase and character
		ld	bc,$0121	; bottom of the screen
		call	LOC_ADDR	; go to given line/column
		ld	a,.dot
		rst	PRINT_A
		rst	PRINT_A
		rst	PRINT_A		; three dots
		ld	hl,CDFLAG
		ld	c,(hl)		; save FAST/SLOW bit in C
		set	6,(hl)		; set SLOW mode flag
		call	SLOW_FAST	; make SLOW mode effective

WaitRelease:	call	BREAK_1		; check BREAK key
		jr	nc,D_ReFast	; error D if break pressed
		ld	hl,(LAST_K)
		ld	a,h
		and	l		; check FF FF
		inc	a
		jr	nz,WaitRelease	; while key pressed
WaitPress:	ld	hl,(LAST_K)
		ld	a,h
		and	l		; check FF FF
		inc	a
		jr	z,WaitPress	; while FF FF
		call	BREAK_1		; check BREAK key
		jr	nc,D_ReFast	; error D if BREAK pressed
		bit	6,c		; was FAST mode set?
		call	z,FAST		; restore FAST if so

		; Clear the screen to ready it for the next page
		call	CLS

		pop	bc

OkToPrint:	ld	a,b
		cp	.nl
		jp	z,Print_A_ret
		push	bc
		ld	bc,0
		call	PR_STR_x	; print character and loop
		pop	bc
		ret

D_ReFast:	bit	6,c		; was FAST mode active?
		call	z,FAST		; set FAST mode if so
		rst	ERROR_1
		db	$0C		; REPORT-D

; LOAD *MD <string>
CmdMD:		ld	a,CMD_mkdir
		jr	SendCmdAndStr

; LOAD *RD <string>
CmdRD:		ld	a,CMD_rmdir
		jr	SendCmdAndStr

; LOAD *CD <string>
CmdCD:		ld	a,CMD_cd
		jr	SendCmdAndStr

; LOAD *OPENDIR <string>
CmdOPENDIR:	ld	a,CMD_opendir
		jr	SendCmdAndStr

; LOAD *DEL <string>
CmdDEL:
		ld	a,CMD_del
SendCmdAndStr:
		push	af
		call	NAME_2		; get string expression
		jr	nc,SkipChkLen	; if checking syntax, skip range check
		call	BC_1_255	; length must be in proper range
SkipChkLen:
		rst	GET_CHAR
		pop	hl		; command code to H
		call	MustBeEOL	; Line must end here

		in	a,(ClkPort)
		ld	c,a		; clock phase to bit 7 of C

		ld	a,h		; command code
		call	OutWaitDiff	; send it

		call	SendString	; send string parameter

		jp	ReportStatus	; read status and take approp. action


; LOAD *COPY <string> TO <string>
CmdCOPY:	ld	a,CMD_copy
		jr	SendStrToStr

; LOAD *MOVE <string> TO <string>
CmdMOVE:	ld	a,CMD_move
SendStrToStr:	push	af		; Save command code

		call	NAME_2		; Fetch string expression
		jr	nc,SkipBCCheck	; If checking syntax, don't check len
		call	BC_1_255	; Check length in range 1..255
SkipBCCheck:
		pop	af		; Restore command code
		ld	b,a		; Save command code in B
					; string length still in C

		rst	GET_CHAR
		cp	.TO		; String followed by TO token?
		jr	nz,ReportC2	; it should
		rst	NEXT_CHAR

		push	de		; Save 1st string pointer
		push	bc		; Save length and command code
		call	NAME_2		; fetch next string
		rst	GET_CHAR
		pop	hl		; L = first string length, H = command
		cp	.nl
		jr	nz,ReportC2
		call	SYNTAX_Z
		jr	nz,ActualCopyMove
		pop	de		; clean up stack
		ret			; no action if checking syntax
ActualCopyMove:
		call	BC_1_255	; second string length

		in	a,(ClkPort)
		ld	c,a

		ld	a,h		; command code, CMD_copy or CMD_move
		call	OutWaitDiff

		ex	de,hl		; HL = 2nd string
		ex	(sp),hl		; stacked addr = 2nd string, HL = 1st
		ex	de,hl		; DE = 1st string, HL preserved

		ld	h,b		; save second string length
		ld	b,l		; retrieve first string length
		call	SendString	; send first string

		pop	de		; retrieve second string address

		ld	a,c		; invert expected clock for SendString
		cpl
		ld	c,a

		ld	b,h		; second string length
		call	SendString	; send second string
		jp	ReportStatus	; retrieve status and exit

GetStrExpr:	call	SCANNING	; Read an expression
MustBeString:	bit	6,(iy+iyFLAGS)	; Test if string type
		ret	z		; Return if string

ReportC2:	rst	ERROR_1
		defb	$0B		; REPORT-C

MustBeComma:	cp	.comma
		jr	nz,ReportC2
		rst	NEXT_CHAR
		ret

; LOAD *ROW <number> TO <string-variable>
CmdROW:		call	CLASS_6		; Read row number
		cp	.TO		; must be followed by token "TO"
		jr	nz,ReportC2
		rst	NEXT_CHAR	; skip TO
		call	CLASS_1		; get variable details
		call	MustBeString	; error if numeric variable
		rst	GET_CHAR	; we should be at EOL now
		call	MustBeEOL	; error otherwise

		ld	bc,255		; make room for a max size string in
		rst	BC_SPACES	; the workspace (might fail with OOM)

		push	de		; preserve workspace address
		call	FIND_INT	; row number to BC
		pop	de		; restore workspace address
		ld	h,b
		ld	l,c		; row number to HL

		in	a,(ClkPort)
		ld	c,a
		ld	a,CMD_getrow
		call	OutWaitDiff
		ld	a,l
		call	OutWaitEq
		ld	a,h
		call	OutWaitDiff
		ld	h,b		; H = length from getrowlen
		in	a,(DataPort)	; string length
		ld	b,a		; for DJNZ
		ld	h,b		; preserve string length for stacking
		call	WaitClkEq

		inc	b
		dec	b
		jr	z,GotRow	; length zero

		push	de		; save pointer to start of workspace

GetStrLoop:	in	a,(DataPort)
		ld	(de),a		; store next character in workspace
		inc	de
		call	WaitClkDiff
		xor	c
		ld	c,a		; make bit 7 of C = clock again
		djnz	GetStrLoop

		pop	de		; restore pointer

GotRow:		call	ReportStatus	; retrieve status, err if not zero

		ld	c,h		; Set BC = length (B is already 0)

		; Store string into the calc. stack with DE=pointer, BC=length
		call	STK_STO_s
		jp	LET		; Store stack into the variable whose
					; parameters were fetched before, and
					; return through LET

; LOAD *MAP <number> TO <numeric-variable>
; LOAD *MAP <number>,<number>
CmdPAGE:	call	CLASS_6		; Read block number
		cp	.TO		; LOAD *MAP block TO var?
		jp	z,MapRead	; Handle that case elsewhere (jp: destino
					; demasiado lejos para jr, desde que
					; CmdZ81 crecio el fichero por en medio)
		call	MustBeComma	; Expect a comma and skip it
		call	CLASS_6		; Read page number
		call	MustBeEOL	; Check EOL and end if checking syntax
		call	FIND_INT	; BC = page number
		ld	a,b
		or	a		; Error B if it's > 255
		jr	nz,ReportB
		ld	a,c
		cp	64		; Valid range is between 0 and 63
		jr	nc,ReportB
		push	bc		; Page number to stack
		call	FIND_INT	; BC = block number
		ld	a,b
		or	a		; Error B if it's > 255
		jr	nz,ReportB
		ld	a,c
		cp	8		; Valid range is between 0 and 7
		jr	nc,ReportB

		pop	de		; Retrieve page number

		ld	b,e		; Page number in B
		ld	a,c		; Block number in A
		rrca			; Prepare to shift block number into E
		rrca
		rrca
		rlca
		rl	e
		rlca
		rl	e
		rlca
		rl	e		; Page number and block number in E
		bit	5,b		; Page number < 32?
		jr	z,SkipFullMode	; If so, no need to change mode

		in	a,(ClkPort)
		ld	c,a
		ld	a,CMD_pages64
		call	OutWaitDiff	; Send command to set full paging mode

SkipFullMode:	ld	c,MapperPort
		out	(c),e
		ret

ReportB:	rst	ERROR_1
		defb	$0A		; REPORT-B

ReportC3:	rst	ERROR_1
		defb	0Bh		; REPORT-C

; LOAD *ICHR [STOP]
CmdICHR:	ld	bc,CMD_ichr_on*256 + CMD_ichr_off
		jp	CMD_ONOFF_BC	; jp: destino demasiado lejos para jr

; LOAD *RAM48 [STOP]
CmdRAM48:	ld	bc,CMD_std48k_on*256 + CMD_std48k_off
		jp	CMD_ONOFF_BC	; jp: destino demasiado lejos para jr

; LOAD *ROMLOCK [STOP]
; Interruptor maestro de los "puertos POKE" del bloque 0 (2038-2062, 2090-
; 2098, sprites): con ROMLOCK activo, escribir en esas direcciones no hace
; nada, exactamente como si fuera ROM real -- para programas antiguos que
; escriben ahi por su cuenta (p.ej. para detectar RAM/ROM) sin saber que
; este interface las usa como puertos de configuracion. Se controla por el
; canal de configuracion MCU->FPGA (comm_cmd), no por el bus del Z80, asi
; que ningun programa puede activarlo/desactivarlo sin querer escribiendo
; en memoria. Por defecto (tras encender o *ROMLOCK STOP) los puertos
; funcionan con normalidad.
CmdROMLOCK:	ld	bc,CMD_romlock_on*256 + CMD_romlock_off
		jp	CMD_ONOFF_BC	; jp: destino demasiado lejos para jr

; Primera de hasta 7 paginas consecutivas (56K, el maximo de un snapshot con
; MEMRANGE 2000-FFFF) que ocupa la memoria de un snapshot .Z81 segun va
; llegando por el puerto (ver CmdZ81 mas abajo): pagina Z81StagePage0+n para
; el bloque (1+n) de destAddr/longitud -- es decir, la memoria recibida
; llega YA a la pagina que va a ser su destino definitivo, vista de una en
; una por el bloque 7 mientras se recibe (nunca por el bloque 0, que es de
; solo lectura). "Copiar" cada bloque a su destino
; real es luego solo una reasignacion de pagina (ver Z81DoRestore), sin
; tocar un solo byte -- salvo el bloque 1, que necesita el programita de
; restauracion final que arma Z81DoRestore (ver alli el porque). No hay un
; asignador de paginas en este proyecto: es la misma convencion informal ya
; usada en otros sitios (paginas libres a partir de la 8), elegida aqui
; bastante mas alta para no chocar con paginas que el usuario ya tenga
; asignadas a mano con *MAP antes de cargar el snapshot.
Z81StagePage0	equ	24

; A=numero de pagina, B=numero de bloque (0-7) -> mapea esa pagina en ese
; bloque. Solo paginacion "simple" (paginas 0-31); un snapshot no necesita
; las paginas altas de la paginacion completa.
Z81MapPage:	add	a,a
		add	a,a
		add	a,a
		or	b
		ld	c,MapperPort
		out	(c),a
		ret

; A=byte alto de una direccion o longitud alineada a multiplo de 2000h ->
; A=numero de bloque (0-7) al que corresponde. Cada bloque ocupa 2000h
; bytes, y 2000h=1<<13, asi que el numero de bloque son los 3 bits altos
; del byte alto (bits 13-15 de la direccion completa), de ahi el >>5 (los 3
; bits altos de un byte, desplazados a los bits bajos). Solo tiene sentido
; si la direccion/longitud esta realmente alineada (bits bajos a cero) --
; siempre cierto para un MEMRANGE real del propio interface.
Z81HighToBlock:	and	0E0h
		rrca
		rrca
		rrca
		rrca
		rrca
		ret

; CmdZ81 (LOAD *Z81, mas abajo) NO usa BC_SPACES ni la pila normal del
; sistema para su area de trabajo: ambas pueden caer en cualquiera de los
; bloques 2-7, y Z81DoRestore reasigna esos bloques a medio camino -- si la
; propia pila (o el buffer) vive ahi, la ejecucion se desincroniza en el
; momento exacto en que se reasigna el bloque que la contiene (visto en una
; traza real: SP cayo en el bloque 2, y la primera vez que Z81ReassignLoop
; reasigno ese bloque, el siguiente RET leyo basura). En vez de eso, todo
; el trabajo de CmdZ81 (buffer + pila propia) vive en una zona FIJA dentro
; del propio bloque 1 (nuestro propio codigo, que nunca reasignamos
; nosotros mismos -- solo lo hace el programita final, cuando ya hemos
; saltado fuera): el ultimo 1K de esa pagina, que normalmente ocupa el
; juego de 128 caracteres definibles y que no hace falta mientras dura este
; comando. No hace falta restaurar SP al terminar: o bien acabamos
; saltando al snapshot (que pone su propio SP), o bien acabamos en
; ERROR_3, que resetea SP desde ERR_SP sin mirar el valor que tuviera antes.
Z81ScratchArea	equ	3C00h	; ultimo 1K de la pagina 1 (bloque 1): 8K de
				; RAM escribible, con el binario ocupando
				; solo ~5K -- este K de arriba, el del
				; juego de caracteres, esta libre ahora mismo
Z81TempStackTop	equ	4000h	; pila propia de CmdZ81, crece hacia abajo
				; desde aqui (justo un byte por encima del
				; final del bloque 1 -- la primera PUSH ya
				; cae dentro, en 3FFEh)

; Bloque prestado temporalmente, solo para el caso (raro) en que la pila del
; snapshot cae dentro del propio bloque 1 -- ver el "Z81BuildFrame" mas
; abajo para el porque hace falta. Bloque 2 es una eleccion arbitraria (ese
; caso siempre tiene bloque_objetivo=1, asi que nunca puede coincidir).
Z81BorrowBlock	equ	2

; Offsets dentro de Z81ScratchArea que usa CmdZ81. Los primeros 34 bytes
; (offset 0-33) son cabecera+registros, tal cual llegan por el puerto (ver
; tabla en el comentario de CmdZ81 mas abajo). El resto es trabajo local,
; nunca viaja por el puerto:
;   34    : pagina real de Z81BorrowBlock, guardada mientras esta prestado
;           (solo se usa/es valida si la pila del snapshot cae en el
;           bloque 1 -- ver Z81BuildFrame)
;   35-36 : FrameAddr (SP del snapshot - 22): donde va el marco de registros
;   37-38 : ProgramAddr (FrameAddr - tamano del programita): donde empieza
;           el propio programita de restauracion final
;   39    : tamano del programita (sin contar el marco)
;   40-41 : EntryAddr: direccion a la que saltar para arrancarlo
;   42-43 : WriteBase: direccion real donde copiarlo (puede diferir de
;           ProgramAddr si hay que verlo, de momento, por la ventana de
;           Z81BorrowBlock)
;   44    : bandera "el bloque 1 forma parte del volcado" (0/1)
;   45    : bloque (1-7) donde cae la pila del snapshot
;   46-103: area de construccion del programita+marco (58 bytes, el maximo
;           posible)
; Todo esto lo arma y consume Z81DoRestore, ver alli para el porque.
Z81OffSavedPage0 equ	34
Z81OffFrameAddr	equ	35
Z81OffProgAddr	equ	37
Z81OffProgSize	equ	39
Z81OffEntry	equ	40
Z81OffWriteBase	equ	42
Z81OffHasBlock1	equ	44
Z81OffTargetBlk	equ	45
Z81OffConstruct	equ	46
Z81BufSize	equ	104	; tamano total usado en Z81ScratchArea (46+58);
				; solo informativo, cabe de sobra en el 1K
				; disponible (ver Z81ScratchArea mas arriba)

; LOAD *Z81 "fichero" -- restaura un snapshot completo (registros + memoria)
; en formato .Z81 de EightyOne. El MCU (cmd_loadZ81) hace todo el trabajo de
; parsear el fichero de texto; aqui solo recibimos, en orden fijo:
;   direccion_destino(2) longitud_memoria(2) bloque_registros(30) memoria(N) status(1)
; Los primeros 34 bytes (cabecera+registros) caben de sobra en Z81ScratchArea
; (ver mas arriba el porque no se usa el workspace de BASIC para esto). La
; memoria (hasta 56K) se recibe DIRECTAMENTE en las paginas que van a ser su
; destino definitivo (pagina 23+bloque, vistas de una en una por el bloque
; 7 mientras llegan -- NUNCA por el bloque 0, que es de solo lectura: los
; "pokes" en su rango de direcciones son puertos de configuracion, no RAM
; real, asi que nada de lo que se escriba ahi se queda) -- asi que "copiar"
; al destino real de cada bloque 2-7 es solo una reasignacion de pagina (un
; OUT), sin tocar un solo byte. El bloque 1 es la unica excepcion: no se
; puede reasignar mientras el propio codigo que hace la reasignacion se
; ejecuta desde ahi, asi que esa reasignacion (mas la restauracion de
; registros) se aplaza a un programita minusculo que Z81DoRestore construye
; sobre la marcha y coloca en un hueco justo por debajo de la pila que va a
; tener el propio snapshot (ver Z81DoRestore mas abajo para el porque).
CmdZ81:		call	GetStrExpr	; leer expresion de cadena (fichero)
		call	MustBeEOL	; fin de linea, fin de comprobacion sintaxis
		call	STK_FETCH	; DE=puntero, BC=longitud del nombre
		call	BC_1_255	; validar nombre de fichero

		in	a,(ClkPort)
		ld	c,a
		ld	a,CMD_loadZ81
		call	OutWaitDiff	; enviar comando
		call	SendString	; enviar nombre; bit7 de C = reloj actual

		; NO llamamos a SET_FAST aqui (a diferencia de SDLoad_2): lo
		; probamos pensando que la interrupcion de modo SLOW corrompia
		; BC a medio volcado, pero el fallo real era otro (Z81MapPage
		; pisando BC, ya corregido dos veces) -- la reproduccion
		; exacta, byte a byte identica, con y sin SET_FAST lo
		; descarto. Y SET_FAST apaga el generador de NMI sin volver a
		; encenderlo, lo que rompe cualquier snapshot que dependiera
		; de esa NMI para salir de un HALT (el bucle estandar de
		; refresco de DFILE en modo SLOW se sincroniza por NMI, no por
		; INT enmascarable -- por eso no importa que IFF1 del snapshot
		; sea 0). Dejar la NMI tal cual estaba es lo correcto -- y como
		; el bloque 0 (donde vive el vector $0066 de la NMI) no se
		; toca nunca en todo este comando, ni siquiera hace falta
		; apagarla mientras tanto: $0066 siempre tiene la ROM real.

		; A partir de aqui, ni la pila del sistema ni BC_SPACES: ver
		; el comentario de Z81ScratchArea, mas arriba, para el porque.
		ld	sp,Z81TempStackTop
		ld	de,Z81ScratchArea	; DE -> inicio del area de trabajo,
					; para el bucle de recepcion de
					; cabecera+registros que viene ahora

		; No podemos fiarnos de que el bit7 de C siga reflejando el
		; reloj real llegados aqui: el emulador (y puede que tambien
		; el MCU real, no comprobado) resuelve la ultima espera de
		; SendStrLoop reusando el primer toggle de ESTA respuesta, sin
		; un toggle propio para el ultimo caracter del nombre -- asi
		; que en vez de comparar contra un C arrastrado (como hacen
		; BattRead/DateRead, que ademas solo es fiable 2 bytes,
		; despues se desincroniza igual), miramos el reloj FISICO tal
		; cual esta ahora y alternamos explicitamente segun toque,
		; igual que ya hace SDLoad_2/SDLoadLoop/SDLoad_3 con el
		; volcado del LOAD normal. Inmune a cualquier desfase de
		; arranque.
		ld	b,34
		in	a,(ClkPort)
		rlca
		jr	c,Z81HdrR0	; reloj a 1 ahora: el primer byte se
					; espera a 0

Z81HdrR1:	in	a,(DataPort)
		ld	(de),a
		inc	de
Z81HdrW1:	in	a,(ClkPort)
		rlca
		jr	nc,Z81HdrW1	; esperar a que suba a 1
		djnz	Z81HdrR0
		jr	Z81HdrDone

Z81HdrR0:	in	a,(DataPort)
		ld	(de),a
		inc	de
Z81HdrW0:	in	a,(ClkPort)
		rlca
		jr	c,Z81HdrW0	; esperar a que baje a 0
		djnz	Z81HdrR1

Z81HdrDone:
		ld	hl,Z81ScratchArea
		ld	e,(hl)
		inc	hl
		ld	d,(hl)		; DE = direccion destino final (tambien
					; queda en el propio buffer, offset
					; 0-1, por si hace falta releerla mas
					; adelante -- Z81DoRestore lo hace)
		inc	hl
		ld	a,(hl)
		inc	hl
		ld	h,(hl)
		ld	l,a		; HL = longitud del volcado de memoria
		push	hl		; guardar longitud (para el chequeo de
					; longitud 0 de mas abajo)

		; Pagina de aparcamiento INICIAL = 23 + bloque_de(destAddr).
		; Antes se aparcaba siempre en Z81StagePage0 (pagina 24, la
		; del bloque 1) sin mirar destAddr -- solo valido si el
		; volcado empieza justo ahi. Ahora la memoria se recibe
		; directamente en la pagina que va a ser su destino
		; definitivo (23+bloque), asi que hace falta acertar desde
		; el primer byte. D todavia vale el byte alto de destAddr
		; (los push de arriba no lo tocan).
		ld	a,d
		call	Z81HighToBlock	; A = bloque de destAddr (asume
					; destAddr alineado a multiplo de 2000h)
		add	a,23
		ld	d,a		; D = pagina de aparcamiento inicial --
					; se queda aqui hasta el bucle de
					; recepcion, un poco mas abajo, que la
					; usa tal cual

		; Aparcamos SIEMPRE por el bloque 7 (nunca por el 0, que es de
		; solo lectura -- ver el comentario grande de CmdZ81, mas
		; arriba -- ni por el 6, que ya dio problemas en una version
		; anterior de este mismo diseno cuando MEMRANGE tambien lo
		; cubria). El bloque 7 SI puede acabar siendo destino real
		; (MEMRANGE cubre los 7 bloques 1-7 en el caso tipico), pero
		; eso no es un problema aqui: para cuando el bloque 7 reciba
		; SU PROPIO volcado (el ultimo de los 7 fragmentos, si el
		; volcado llega hasta ahi), ya no queda nada mas por aparcar
		; despues, asi que no hay ningun conflicto entre "sitio de
		; aparcamiento" y "destino real" como si lo habia con el
		; bloque 6 en el diseno anterior (bloque 6 tenia que aparcar
		; datos de bloques posteriores DESPUES de que le tocara su
		; propio volcado real).
		;
		; DI aqui protege el resto de Z81DoRestore (el POP en cadena
		; de los registros del snapshot, mas abajo): si saltara una
		; interrupcion en mitad de eso, con SP ya reapuntado dentro
		; del marco de registros, corrompe la pila (visto en una
		; version anterior de este diseno). El unico EI real lo hace
		; el propio programita de restauracion final, condicionado al
		; IFF1 del snapshot, justo antes de saltar a su PC.
		di

		ld	a,d		; A = pagina de aparcamiento inicial
					; (seguia en D desde mas arriba)
		ld	b,7
		call	Z81MapPage	; aparcar ahi la primera pagina que llegue
					; (no toca D: sigue valiendo lo mismo)

		; --- Recepcion de la memoria, directamente en su pagina final ---
		ld	hl,0E000h	; puntero de escritura en el bloque 7

		pop	bc		; BC = longitud restante (nada entre esto
					; y el bucle vuelve a tocar B o C)
		ld	a,b
		or	c
		jr	z,Z81MemDone	; longitud 0: nada que recibir

		; Mismo patron adaptativo que Z81HdrLoop (ver el comentario de
		; ahi arriba) -- no fiarse de C, mirar el reloj fisico y
		; alternar explicitamente.
		in	a,(ClkPort)
		rlca
		jr	c,Z81RecvR0	; reloj a 1 ahora: el primer byte se
					; espera a 0

Z81RecvR1:	in	a,(DataPort)
		ld	(hl),a
		inc	hl
Z81RecvW1:	in	a,(ClkPort)
		rlca
		jr	nc,Z81RecvW1	; esperar a que suba a 1
		dec	bc
		ld	a,b
		or	c
		jr	z,Z81MemDone	; ya no quedan bytes que recibir
		ld	a,h
		or	l
		jr	nz,Z81RecvR0	; seguimos en la misma pagina; el
					; siguiente byte se espera a 0
		inc	d		; pagina de aparcamiento siguiente
		push	bc		; Z81MapPage pisa BC (B=bloque, y C lo usa
					; para MapperPort) -- proteger la
					; longitud restante
		ld	a,d
		ld	b,7
		call	Z81MapPage
		pop	bc
		ld	hl,0E000h
		jr	Z81RecvR0

Z81RecvR0:	in	a,(DataPort)
		ld	(hl),a
		inc	hl
Z81RecvW0:	in	a,(ClkPort)
		rlca
		jr	c,Z81RecvW0	; esperar a que baje a 0
		dec	bc
		ld	a,b
		or	c
		jr	z,Z81MemDone	; ya no quedan bytes que recibir
		ld	a,h
		or	l
		jr	nz,Z81RecvR1	; seguimos en la misma pagina; el
					; siguiente byte se espera a 1
		inc	d
		push	bc		; (ver comentario identico arriba)
		ld	a,d
		ld	b,7
		call	Z81MapPage
		pop	bc
		ld	hl,0E000h
		jr	Z81RecvR1

Z81MemDone:
		; --- Byte de estado final ---
		; Ojo: NO hay que esperar ningun toggle aqui. Tanto
		; Z81HdrLoop (cuando la memoria esta vacia) como Z81RecvLoop
		; (en el caso normal) ya esperan, como su propia ultima
		; accion antes de saltar aqui, el toggle que prepara
		; PRECISAMENTE este byte de estado (es el ultimo elemento del
		; buffer, y "leer byte N" es lo que dispara la preparacion
		; del byte N+1 en el emulador). Volver a mirar el reloj y
		; esperar otra vez -- como hacia esto antes -- espera un
		; toggle que ya no va a llegar (no hay "byte N+2" que
		; preparar), y cuelga o pesca cualquier basura que toque el
		; reloj mas tarde por otro motivo.
		in	a,(DataPort)
		ld	l,a
		ld	a,l
		or	a
		jr	z,Z81DoRestore	; estado 0 = todo ok

		add	a,.F-.1		; mismo mapeo de codigo de error que
					; ReportStatus (status 1 = REPORT-G...)
		ld	l,a
		; No hay nada que desapilar aqui: la longitud ya se saco en
		; BC antes del bucle de recepcion (tanto si hubo bucle como
		; si la longitud era 0), y el buffer vive en una direccion
		; fija (Z81ScratchArea), no en la pila.

		; No hace falta deshacer ningun aparcamiento: el bloque 0 (la
		; ROM real que necesita ERROR_3) nunca se ha tocado -- solo el
		; bloque 7, que da igual en que pagina se quede. Basta con EI
		; (el DI de mas arriba, en Z81HdrDone, se queda activo hasta
		; aqui).
		ei
		jp	ERROR_3		; almacena L como ERR_NR

; B=numero de bloque -> A = pagina actualmente mapeada en ese bloque
Z81ReadPage:	ld	c,MapperPort
		in	a,(c)
		ret

; --- Plantillas del "programita de restauracion final" ---------------------
; Bytes de maquina puros, nunca se ejecutan aqui: Z81DoRestore los copia (y
; parchea los pocos operandos que dependen del snapshot concreto) en el area
; de trabajo del bloque reservado, y desde ahi al destino real. Se explica
; el porque de todo esto en el comentario de Z81DoRestore, mas abajo.
Z81TplBlock1Out:
		; bloque1 -> pagina Z81StagePage0 (24): siempre la misma
		; pagina fija, asi que el operando de este LD A,n es una
		; constante de ensamblado, no hace falta parchearlo nunca.
		db	03Eh,Z81StagePage0*8+1	; LD A,(24<<3)|1
		db	00Eh,MapperPort		; LD C,MapperPort
		db	0EDh,079h		; OUT (C),A
Z81TplBlock1OutLen equ	$-Z81TplBlock1Out

Z81TplJP:
		db	0C3h,000h,000h		; JP nn -- operando parcheado
Z81TplJPLen	equ	$-Z81TplJP
Z81TplJPOperand	equ	1

; Solo hace falta si la pila del snapshot cae en el propio bloque 1 (ver
; Z81BuildFrame): devuelve Z81BorrowBlock a la pagina que tenia prestada
; para escribir/entrar en el programita. El operando (pagina<<3)|bloque se
; calcula en Z81ConstructNow a partir de la pagina real guardada (offset
; Z81OffSavedPage0) -- Z81BorrowBlock es fijo, pero la pagina no.
Z81TplRestoreBorrow:
		db	03Eh,000h		; LD A,n -- operando parcheado
		db	00Eh,MapperPort		; LD C,MapperPort
		db	0EDh,079h		; OUT (C),A
Z81TplRestoreBorrowLen equ $-Z81TplRestoreBorrow
Z81TplRestoreBorrowOp equ 1

Z81TplTail:
		db	031h,000h,000h		; LD SP,nn -- operando
						; parcheado (FrameAddr)
		db	0E1h			; POP HL
		db	0D1h			; POP DE
		db	0C1h			; POP BC
		db	0F1h			; POP AF
		db	0D9h			; EXX
		db	0E1h			; POP HL'
		db	0D1h			; POP DE'
		db	0C1h			; POP BC'
		db	008h			; EX AF,AF'
		db	0F1h			; POP AF'
		db	0D9h			; EXX
		db	008h			; EX AF,AF'
		db	0DDh,0E1h		; POP IX
		db	0FDh,0E1h		; POP IY
		db	000h			; EI/NOP -- parcheado
		db	0C9h			; RET -- recupera PC (y de paso
						; deja SP en su valor final)
Z81TplTailLen	equ	$-Z81TplTail
Z81TplTailSPOp	equ	1
Z81TplTailEIOp	equ	19

; Todo lo que llega hasta aqui (Z81MemDone con status=0) tiene ya la memoria
; del snapshot en sus paginas de aparcamiento definitivas (23+bloque, una
; por cada bloque de destAddr/longitud -- ver Z81HdrDone/Z81RecvLoop mas
; arriba): "copiar" cada bloque 2-7 a su destino real es solo REASIGNAR esa
; pagina al bloque que le corresponde, sin tocar un solo byte.
;
; El bloque 1 es la excepcion: no se puede reasignar mientras el propio
; codigo que hace la reasignacion se ejecuta desde ahi (esto costo varias
; rondas de depuracion entenderlo bien -- ver el historial de este fichero).
; Ni siquiera escribir a traves de OTRO bloque que este viendo la MISMA
; pagina fisica vale como truco: sigue siendo la misma pagina, y acaba
; pisando el propio codigo que la esta escribiendo.
;
; La solucion (propuesta por el propio autor del hardware): aplazar la
; reasignacion del bloque 1 a un programita minusculo, construido sobre la
; marcha, que se coloca en un hueco justo por debajo de la pila que va a
; tener el snapshot -- que para entonces ya esta en su pagina definitiva (la
; del bloque 1, si la pila cae ahi, se ve todavia via Z81BorrowBlock,
; prestado solo para este momento; en cualquier otro bloque, ya es visible
; directamente porque ese bloque ya se ha reasignado arriba). Justo debajo
; de ese programita se deja un "marco" con los registros (en el orden
; exacto que hace falta para sacarlos con POP en cadena, PC el ultimo,
; recuperado por un RET final en vez de un JP con operando parcheado). Es
; una apuesta -- no hay ningun hueco de memoria que sepamos SEGURO que esta
; libre, ya que no conocemos como usa la RAM el programa que se ha cargado
; -- pero el margen de seguridad que cualquier programa razonable deja por
; debajo de su propia pila es, en la practica, mas que suficiente para los
; ~60 bytes que hacen falta.
;
; NOTA: todo esto asume que destAddr/longitud (offsets 0-3 del bloque
; reservado) estan alineados a multiplo de 2000h -- cierto para cualquier
; MEMRANGE real de este interface, que siempre cubre bloques enteros.
Z81DoRestore:
		ld	ix,Z81ScratchArea	; IX -> area de trabajo, de aqui en
					; adelante usado como puntero fijo
					; (IX+n) para todo el trabajo local
					; (direccion fija: ya no hace falta
					; sacarla de ningun sitio)

		ld	e,(ix+0)
		ld	d,(ix+1)	; DE = direccion destino final (destAddr,
					; releida del buffer)

		ld	l,(ix+2)
		ld	h,(ix+3)	; HL = longitud total del volcado (releida
					; de la cabecera: BC ya la conto hasta 0
					; durante la recepcion)

		ld	(ix+Z81OffHasBlock1),0

		ld	a,h
		or	l
		jr	z,Z81BuildFrame	; longitud 0: nada que reasignar

		push	hl
		ld	a,d
		call	Z81HighToBlock	; A = bloque donde empieza el volcado
		ld	b,a
		cp	1
		jr	nz,Z81NotBlock1Start
		ld	(ix+Z81OffHasBlock1),1
Z81NotBlock1Start:
		pop	hl
		ld	a,h
		call	Z81HighToBlock	; A = numero de bloques cubiertos
		ld	c,a

Z81ReassignLoop:
		ld	a,c
		or	a
		jr	z,Z81BuildFrame
		ld	a,b
		cp	1
		jr	z,Z81ReassignSkip
		push	bc
		add	a,23		; A = pagina definitiva de ese bloque
		call	Z81MapPage	; (B = numero de bloque, todavia valido)
		pop	bc
Z81ReassignSkip:
		inc	b
		dec	c
		jr	Z81ReassignLoop

Z81BuildFrame:
		; --- Bloque (1-7) donde cae la pila del snapshot ---
		ld	l,(ix+6)
		ld	h,(ix+7)	; HL = SP del snapshot
		dec	hl		; el ultimo byte que de verdad cae "justo
					; por debajo" de esa pila
		ld	a,h
		call	Z81HighToBlock
		ld	(ix+Z81OffTargetBlk),a

		; --- FrameAddr = SP_snapshot - 22 (10 pares de registros +
		; PC, en el orden exacto que va a sacar el programita final
		; con POP/.../RET) ---
		ld	l,(ix+6)
		ld	h,(ix+7)
		ld	de,-22
		add	hl,de
		ld	(ix+Z81OffFrameAddr),l
		ld	(ix+Z81OffFrameAddr+1),h

		; --- Tamano del programita (sin el marco), segun el caso ---
		ld	c,Z81TplTailLen	; el Tail comun siempre esta
		ld	a,(ix+Z81OffHasBlock1)
		or	a
		jr	z,Z81SizeDone	; bloque 1 no esta en el volcado: nada
					; mas que anadir
		ld	a,c
		add	a,Z81TplBlock1OutLen
		ld	c,a
		ld	a,(ix+Z81OffTargetBlk)
		cp	1
		jr	nz,Z81SizeDone	; la pila NO cae en el bloque 1: no
					; hace falta el JP interno ni devolver
					; Z81BorrowBlock
		ld	a,c
		add	a,Z81TplJPLen
		add	a,Z81TplRestoreBorrowLen
		ld	c,a
Z81SizeDone:
		ld	(ix+Z81OffProgSize),c

		; --- ProgramAddr = FrameAddr - tamano del programita ---
		ld	l,(ix+Z81OffFrameAddr)
		ld	h,(ix+Z81OffFrameAddr+1)
		ld	e,c
		ld	d,0
		or	a		; limpiar carry
		sbc	hl,de
		ld	(ix+Z81OffProgAddr),l
		ld	(ix+Z81OffProgAddr+1),h

		; --- Donde escribir/entrar: si la pila cae en el bloque 1
		; (unico bloque que a estas alturas puede seguir SIN
		; reasignar), hay que construir via la ventana de
		; Z81BorrowBlock, prestado para la ocasion (con su pagina real
		; guardada para devolverselo mas tarde, desde el propio
		; programita -- ver Z81ConstructNow); en cualquier otro caso
		; el bloque destino ya esta en su pagina definitiva (por
		; Z81ReassignLoop, o porque nunca formaba parte del volcado y
		; no se ha tocado) y se puede escribir/entrar directamente.
		ld	a,(ix+Z81OffTargetBlk)
		cp	1
		jr	nz,Z81EntryDirect

		ld	b,Z81BorrowBlock
		call	Z81ReadPage	; A = pagina real actual de Z81BorrowBlock
		ld	(ix+Z81OffSavedPage0),a

		ld	a,Z81StagePage0
		ld	b,Z81BorrowBlock
		call	Z81MapPage	; Z81BorrowBlock -> pagina 24 (los datos
					; del bloque 1)

		ld	l,(ix+Z81OffProgAddr)
		ld	h,(ix+Z81OffProgAddr+1)
		ld	a,h
		and	01Fh
		or	Z81BorrowBlock*32 ; misma pagina, vista por Z81BorrowBlock
		ld	h,a
		ld	(ix+Z81OffWriteBase),l
		ld	(ix+Z81OffWriteBase+1),h
		ld	(ix+Z81OffEntry),l
		ld	(ix+Z81OffEntry+1),h
		jr	Z81ConstructNow

Z81EntryDirect:
		ld	l,(ix+Z81OffProgAddr)
		ld	h,(ix+Z81OffProgAddr+1)
		ld	(ix+Z81OffWriteBase),l
		ld	(ix+Z81OffWriteBase+1),h
		ld	(ix+Z81OffEntry),l
		ld	(ix+Z81OffEntry+1),h

Z81ConstructNow:
		; Construir el programita en el area local (bloque 1, la
		; nuestra -- IX+Z81OffConstruct en adelante): [OUT bloque1, si
		; aplica] [JP interno + OUT que devuelve Z81BorrowBlock, solo
		; si la pila cae en el bloque 1] [LD SP + POPs + EI/NOP +
		; RET], seguido justo despues por el marco de 22 bytes con
		; los registros.
		push	ix
		pop	hl
		ld	de,Z81OffConstruct
		add	hl,de
		push	hl
		pop	iy		; IY = cursor de escritura local

		ld	a,(ix+Z81OffHasBlock1)
		or	a
		jr	z,Z81CTail

		push	iy
		pop	de
		ld	hl,Z81TplBlock1Out
		ld	bc,Z81TplBlock1OutLen
		ldir
		push	iy
		pop	hl
		ld	de,Z81TplBlock1OutLen
		add	hl,de
		push	hl
		pop	iy

		ld	a,(ix+Z81OffTargetBlk)
		cp	1
		jr	nz,Z81CTail

		; JP interno: venimos ejecutando por la ventana de
		; Z81BorrowBlock (todavia viendo la pagina 24); el OUT que
		; acabamos de copiar deja el bloque 1 correctamente mapeado,
		; asi que hay que saltar a la MISMA direccion pero vista ya
		; por la ventana REAL del bloque 1, antes de devolverle su
		; pagina real a Z81BorrowBlock -- si siguieramos ejecutando
		; via su ventana, ese OUT (el siguiente paso) se comeria el
		; terreno que estamos pisando.
		push	iy
		pop	de
		ld	hl,Z81TplJP
		ld	bc,Z81TplJPLen
		ldir

		ld	l,(ix+Z81OffProgAddr)
		ld	h,(ix+Z81OffProgAddr+1)
		ld	de,Z81TplBlock1OutLen+Z81TplJPLen
		add	hl,de		; HL = direccion real (bloque 1) donde
					; empieza lo que sigue
		ld	(iy+Z81TplJPOperand),l
		ld	(iy+Z81TplJPOperand+1),h

		push	iy
		pop	hl
		ld	de,Z81TplJPLen
		add	hl,de
		push	hl
		pop	iy

		; Devolver a Z81BorrowBlock su pagina real (guardada en
		; Z81OffSavedPage0) -- esto se ejecuta ya via la ventana REAL
		; del bloque 1 (tras el JP de arriba), asi que es seguro.
		push	iy
		pop	de
		ld	hl,Z81TplRestoreBorrow
		ld	bc,Z81TplRestoreBorrowLen
		ldir

		ld	a,(ix+Z81OffSavedPage0)
		add	a,a
		add	a,a
		add	a,a		; a = pagina<<3
		or	Z81BorrowBlock	; a = (pagina<<3)|bloque, igual que hace
					; Z81MapPage -- aqui no se puede llamar,
					; hace falta el byte ya calculado
		ld	(iy+Z81TplRestoreBorrowOp),a

		push	iy
		pop	hl
		ld	de,Z81TplRestoreBorrowLen
		add	hl,de
		push	hl
		pop	iy

Z81CTail:
		push	iy
		pop	de
		ld	hl,Z81TplTail
		ld	bc,Z81TplTailLen
		ldir

		ld	a,(ix+Z81OffFrameAddr)
		ld	(iy+Z81TplTailSPOp),a
		ld	a,(ix+Z81OffFrameAddr+1)
		ld	(iy+Z81TplTailSPOp+1),a

		ld	a,(ix+31)	; IFF1 del snapshot
		or	a
		ld	a,0FBh		; EI
		jr	nz,Z81CPatchEI
		ld	a,000h		; NOP (no activar interrupciones)
Z81CPatchEI:	ld	(iy+Z81TplTailEIOp),a

		push	iy
		pop	hl
		ld	de,Z81TplTailLen
		add	hl,de
		push	hl
		pop	iy		; IY -> justo donde empieza el marco

		; --- I/R/IM: se aplican aqui mismo, directamente (no hace
		; falta aplazarlos: no interfieren con nada de lo anterior
		; ni de lo que viene) ---
		ld	a,(ix+28)
		ld	i,a
		ld	a,(ix+29)
		ld	r,a
		ld	a,(ix+30)
		cp	1
		jr	c,Z81CSetIM0
		jr	z,Z81CSetIM1
		im	2
		jr	Z81CIMDone
Z81CSetIM0:	im	0
		jr	Z81CIMDone
Z81CSetIM1:	im	1
Z81CIMDone:

		; --- Marco de registros: HL,DE,BC,AF,HL',DE',BC',AF',IX,IY
		; (20 bytes, offsets 8-27 del bloque, ya contiguos y en ese
		; mismo orden) + PC (offset 4-5) al final, para que el RET
		; final del programita lo saque de la pila el ultimo.
		push	ix
		pop	hl
		ld	de,8
		add	hl,de		; HL -> buffer+8 (registro HL guardado)
		push	iy
		pop	de		; DE = cursor de escritura (marco)
		ld	bc,20
		ldir			; tras esto, DE ya apunta al siguiente
					; hueco libre -- no hace falta
					; recalcularlo para lo que sigue

		push	ix
		pop	hl
		ld	bc,4
		add	hl,bc		; HL -> buffer+4 (PC) -- con BC, no con
					; DE, para no perder el cursor que
					; acaba de dejar el ldir anterior
		ld	bc,2
		ldir

		; --- Copiar todo (programita+marco) a su destino real ---
		ld	a,(ix+Z81OffProgSize)
		add	a,22
		ld	c,a
		ld	b,0		; BC = tamano total (cabe de sobra en
					; un byte: maximo 58)

		push	ix
		pop	hl
		ld	de,Z81OffConstruct
		add	hl,de		; HL = origen (area local, bloque 1)

		ld	e,(ix+Z81OffWriteBase)
		ld	d,(ix+Z81OffWriteBase+1)
		ldir

		ld	l,(ix+Z81OffEntry)
		ld	h,(ix+Z81OffEntry+1)
		jp	(hl)		; a partir de aqui, el propio
					; programita recien copiado hace el
					; resto: OUT(es) de pagina, POPs y un
					; RET final al PC del snapshot

; LOAD *FULLPAG [STOP] -- activa paginacion completa (512K, CMD_pages64)
; o, con STOP, vuelve a la paginacion simple (256K, CMD_pages32). El
; propio ROM ya activa CMD_pages64 solo (ver mas arriba, al paginar un
; bloque a pagina >=32), pero no habia forma explicita de volver atras
; ni de activarlo a mano sin necesidad de tocar paginas altas.
CmdFULLPAG:	ld	bc,CMD_pages64*256 + CMD_pages32
		jr	CMD_ONOFF_BC

; LOAD *MC45 [STOP]
CmdMC45ONOFF:	ld	bc,CMD_mc45_on*256 + CMD_mc45_off
CMD_ONOFF_BC:	cp	.STOP		; Token STOP?
		jr	nz,MC45cmddone	; Jump if not
		ld	b,c		; Change to OFF command
		rst	NEXT_CHAR	; skip STOP
MC45cmddone:	call	MustBeEOL	; Done with syntax, time for action

		in	a,(ClkPort)
		ld	c,a
		jp	OutBWaitDiff

; Handle the *MAP block TO var syntax here
MapRead:	rst	NEXT_CHAR	; Skip TO
		call	CLASS_1		; Get variable details
		rst	GET_CHAR	; We should be at EOL now
		bit	6,(iy+iyFLAGS)	; Numeric variable?
		jp	z,ReportC3	; Error if not (jp: destino demasiado
					; lejos para jr)
		call	MustBeEOL	; Check if EOL and end syntax check
		call	FIND_INT	; Get value
		rlc	b		; Zero high byte?
		jp	nz,ReportB	; Error if not (jp: demasiado lejos para jr)
		ld	a,c
		cp	8		; Low byte in [0..7]?
		jp	nc,ReportB	; Error if not (jp: demasiado lejos para jr)
		ld	b,c		; Block number to A10-A8
		ld	c,MapperPort	; Mapper port to A7-A0
		in	c,(c)		; Read page for that block
		ld	b,0		; Set high byte to 0
		call	STACK_BC	; Value to calculator stack...
		jp	LET		; ... and store it into the variable


; LOAD *64C
Cmd64C:		ld	bc,$1E00+CMD_chars64	; Set normal 64-char mode
		jr	Common64_128C

; LOAD *256C
; Direccion por defecto $3800: alineada a 2K (I=$38, bits bajos a 0 --
; la tabla de 256 caracteres necesita esa alineacion porque los 3 bits
; bajos de I se ignoran en hardware, ver SEL_256CHARS en SD81.v),
; inmediatamente debajo de los $3C00 de *128C, y precargada con el
; charset de la ROM en SD_RESET igual que *128C -- invocarlo sin haber
; redefinido nada no cambia lo que se ve.
Cmd256C:	ld	bc,$3800+CMD_chars256	; Set extended 256-char mode
		jr	Common64_128C

; LOAD *128C
Cmd128C:	ld	bc,$3C00+CMD_chars128	; Set extended 128-char mode
Common64_128C:	cp	.nl
		jr	z,chEol
		push	bc		; Preserve command in C
		call	CLASS_6		; Parse or evaluate numeric expression
		call	SYNTAX_Z	; Checking syntax?
		jr	z,chEol2	; Skip fetching number if so
		call	FP_TO_A		; Fetch expression value
		jp	c,ReportB	; Error if out of range (jp: demasiado
					; lejos para jr)
		jr	z,chEol2	; If positive value, all good
		neg			; Negate
chEol2:		pop	bc		; restore command in C
		ld	b,a		; Set charset addr to argument
		rst	GET_CHAR

chEol:		call	MustBeEOL	; Check if EOL and end syntax check
		ld	h,c		; Save command
		in	a,(ClkPort)
		ld	c,a		; Read current clock into bit 7 of C
		ld	a,h		; Restore saved command
		call	OutWaitDiff	; Send command to set the char mode
		ld	a,b
		ld	i,a		; Set I to point to the high byte
		ret

; LOAD *WRX [STOP]
; WRX in the second 8K (I in $20-$3F): FPGA-mapped register, no MCU
; involved, so this is a plain POKE instead of the MCU protocol used
; by MC45/128C/etc above.
CmdWRX:		ld	bc,170*256+85	; B=ON value, C=OFF value
		cp	.STOP		; Token STOP?
		jr	nz,WRXcmddone	; Jump if not
		ld	b,c		; Change to OFF value
		rst	NEXT_CHAR	; skip STOP
WRXcmddone:	call	MustBeEOL	; Done with syntax, time for action
		ld	a,b
		ld	(2058),a
		ret

; LOAD *DBUF <block> | LOAD *DBUF STOP
; Double buffer: <block> is the front buffer block (0-7), same FPGA
; register encoding as the raw POKE (168+block to enable, 85 to
; disable) - see the "Double buffer (present-blit)" appendix.
CmdDBUF:	cp	.STOP		; Token STOP?
		jr	z,DBUFoff
		call	CLASS_6		; Read block number
		call	MustBeEOL	; Check EOL and end syntax check
		call	FIND_INT	; BC = block number
		ld	a,b
		or	a		; Error B if it's > 255
		jp	nz,ReportB
		ld	a,c
		cp	8		; Valid range is between 0 and 7
		jp	nc,ReportB
		or	168		; 168+block = enable value
		jr	DBUFsend
DBUFoff:	rst	NEXT_CHAR	; skip STOP
		call	MustBeEOL	; Check EOL and end syntax check
		ld	a,85		; disable value
DBUFsend:	ld	(2057),a
		ret

; LOAD *SFAST [STOP]
; Superfast text mode - no HFILE needed (uses DFILE/ROMTABLE, not extended RAM).
CmdSFAST:	ld	bc,170*256+85	; B=ON value, C=OFF value
		cp	.STOP		; Token STOP?
		jr	nz,SFASTcmddone	; Jump if not
		ld	b,c		; Change to OFF value
		rst	NEXT_CHAR	; skip STOP
SFASTcmddone:	call	MustBeEOL	; Done with syntax, time for action
		ld	a,b
		ld	(2045),a
		ret

; LOAD *SFHR <address> | LOAD *SFHR STOP
; Superfast native HiRes mode. <address> is HFILE, the screen file address
; in extended RAM.
CmdSFHR:	cp	.STOP		; Token STOP?
		jr	z,SFxxOff
		call	CLASS_6		; Read HFILE address
		call	MustBeEOL	; Check EOL and end syntax check
		call	FIND_INT	; BC = HFILE address
		ld	a,c
		ld	(2043),a	; HFILE low
		ld	a,b
		ld	(2044),a	; HFILE high
		ld	a,171		; Superfast HiRes native
		jr	SFxxSend

; LOAD *SFSP <address> | LOAD *SFSP STOP
; Superfast Spectrum HiRes mode. <address> is HFILE, same as SFHR.
CmdSFSP:	cp	.STOP		; Token STOP?
		jr	z,SFxxOff
		call	CLASS_6		; Read HFILE address
		call	MustBeEOL	; Check EOL and end syntax check
		call	FIND_INT	; BC = HFILE address
		ld	a,c
		ld	(2043),a	; HFILE low
		ld	a,b
		ld	(2044),a	; HFILE high
		ld	a,172		; Superfast HiRes Spectrum
		jr	SFxxSend
SFxxOff:	rst	NEXT_CHAR	; skip STOP
		call	MustBeEOL	; Check EOL and end syntax check
		ld	a,85		; disable value
SFxxSend:	ld	(2045),a
		ret

; LOAD *BORDER <colour>
; Set border pattern ink (POKE 2046, low nibble; only visible if the
; border pattern is enabled via POKE 2047) and the Superfast HiRes
; Spectrum mode border/background colour (ULA-style port, low 3 bits).
; The native-mode border background colour is set with LOAD *COLOR instead.
CmdBORDER:	call	CLASS_6		; Read colour number
		call	MustBeEOL	; Check EOL and end syntax check
		call	FIND_INT	; BC = colour number
		ld	a,b
		or	a		; Error B if it's > 255
		jp	nz,ReportB
		ld	a,c
		cp	16		; Valid range is between 0 and 15
		jp	nc,ReportB
		ld	(2046),a	; native/superfast text mode border ink
		ld	a,c
		and	7		; Spectrum mode ULA port only has 3 bits
		out	(SpulaPort),a
		ret

; LOAD *COLOR [<border colour>[,<mode>]] | LOAD *COLOR STOP
; Enable/disable Chroma81 colour mode and set the native-mode border
; background colour (port 7FEFh: bit5=enable, bit4=mode, bits3-0=border
; colour). <border colour> defaults to 7 (white), <mode> defaults to 0
; (character-code colour table) if omitted; <mode> must be 0 or 1.
CmdCOLOR:	cp	.STOP		; Token STOP?
		jr	z,COLORoff
		cp	.nl		; No argument given?
		jr	z,COLORdflt
		call	CLASS_6		; Read border colour number
		cp	.comma		; Mode argument given too?
		jr	nz,COLORb1arg
		rst	NEXT_CHAR	; skip comma
		call	CLASS_6		; Read mode number
		call	MustBeEOL	; Check EOL and end syntax check
		call	FIND_INT	; BC = mode number (pushed last)
		ld	a,b
		or	a		; Error B if it's > 255
		jp	nz,ReportB
		ld	a,c
		cp	2		; Valid range is 0 or 1
		jp	nc,ReportB
		push	bc		; save mode (FIND_INT clobbers DE)
		call	FIND_INT	; BC = border colour number
		pop	de		; mode number back, in E
		jr	COLORchkb
COLORb1arg:	call	MustBeEOL	; Check EOL and end syntax check
		call	FIND_INT	; BC = border colour number
		ld	e,0		; default mode
COLORchkb:	ld	a,b
		or	a		; Error B if it's > 255
		jp	nz,ReportB
		ld	a,c
		cp	16		; Valid range is between 0 and 15
		jp	nc,ReportB
		jr	COLORgotn
COLORdflt:	call	MustBeEOL	; Check EOL and end syntax check
		ld	c,7		; default border colour
		ld	e,0		; default mode
COLORgotn:	ld	a,c
		and	$0F
		or	$20		; enable bit
		ld	d,a		; stash enable+border
		ld	a,e		; mode (0 or 1)
		rlca
		rlca
		rlca
		rlca			; mode -> bit4
		or	d
		jr	COLORsend
COLORoff:	rst	NEXT_CHAR	; skip STOP
		call	MustBeEOL	; Check EOL and end syntax check
		xor	a		; disable value
COLORsend:	ld	bc,32751	; port 7FEFh (Chroma81 mode/enable/border)
		out	(c),a
		ret

; LOAD *JOY <string>
CmdJOY:		call	GetStrExpr	; Read a string expression
		call	MustBeEOL	; The line must end after the expr.
		call	ExpectShortStr	; Fetch and check string length
					; (returns B = length)

		in	a,(ClkPort)	; Read clock phase
		ld	c,a		; Store in C (required by OutWaitDiff)

		ld	a,CMD_joy	; Send CMD_joy command
		call	OutWaitDiff
		call	SendString	; Send the string
		jp	ReportStatus	; Exit with status report


; LOAD *PLAY <string>[,<string>[,<string>]]
CmdPLAY:	call	GetStrExpr	; Read string expression
		cp	.nl
		jr	z,playOneOnly	; 1 string arg only
		call	MustBeComma	; Expect a comma and skip it
		call	GetStrExpr	; Read another string expression
		cp	.nl
		jr	z,playTwoOnly	; exactly 2 string args
		call	MustBeComma	; Expect and skip another comma
		call	GetStrExpr	; Read last string expression
		call	MustBeEOL	; Check if EOL and end syntax checking

		call	ExpectShortStr	; String must be <= 255 chars

Entry2params:	push	bc		; Push to machine stack
		push	de

		call	ExpectShortStr	; <= 255 chars

Entry1param:	push	bc		; Push to machine stack
		push	de

		call	ExpectShortStr

		in	a,(ClkPort)
		ld	c,a

		ld	a,CMD_play
		call	OutWaitDiff

		call	SendString

		ld	a,c
		cpl			; invert bit 7 of clock
		pop	de		; next string
		pop	bc
		ld	c,a

		call	SendString

		; Can't use SendString in this case because we want the last
		; send to be waited for with the possibility of a break.

		pop	de
		pop	hl		; length in H and L

		ld	a,l		; send length
PlayNextSend:
		out	(DataPort),a	; send next byte (length or data)

		ld	a,l
		or	a		; check if length zero
		jr	z,PlayWaitBreak
		call	WaitClkDiff
		xor	c
		ld	c,a		; Make C equal to clock again

		ld	a,(de)
		inc	de
		dec	l
		jr	PlayNextSend


PlayWaitBreak:	call	WaitDiffBrk
		xor	c
		ld	c,a
		jp	ReportStatus

ExpectShortStr:	call	STK_FETCH	; Pop params from calculator stack
		ld	a,b
		or	a
		ld	b,c
		ret	z		; If high byte is 0, OK; else error

ReportA:	rst	ERROR_1
		db	$09		; REPORT-A

playTwoOnly:	call	SyntaxDone
		ld	c,0
		jr	Entry2params

playOneOnly:	call	SyntaxDone
		ld	c,0
		push	bc
		push	bc
		jr	Entry1param

; LOAD *SAY <string>
; TODO: maybe join it with LOAD *JOY since the args are the same
CmdSAY:		call	GetStrExpr	; Read a string expression
		call	MustBeEOL	; The line must end after the expr.
		call	ExpectShortStr	; Fetch and check string length
					; (returns B = length)

		in	a,(ClkPort)	; Read clock phase
		ld	c,a		; Store in C (required by OutWaitDiff)

		ld	a,CMD_talk	; Send CMD_talk command
		call	OutWaitDiff
		call	SendString	; Send the string
		jp	ReportStatus	; Exit with status report

; VGM play/loop/stop/pause/resume
; LOAD *VGM <filename>
; LOAD *VGM THEN <RUN|CONT|PAUSE|STOP>
CmdVGM:		cp	.THEN		; LOAD *VGM THEN <command>
		jr	z,VGMThen
		ld	a,CMD_loadVGM
		jp	SendCmdAndStr

VGMThen:	rst	NEXT_CHAR	; skip THEN token
		ld	b,CMD_pauseVGM
		cp	.PAUSE
		jr	z,DoVGM
		ld	b,CMD_stopVGM
		cp	.STOP
		jr	z,DoVGM
		ld	b,CMD_contVGM
		cp	.CONT
		jr	z,DoVGM
		cp	.RUN
		jr	nz,ReportC4
DoVGM:		rst	NEXT_CHAR	; skip token after THEN
		call	MustBeEOL	; line and syntax checking end here
		in	a,(ClkPort)
		ld	c,a
		ld	a,b
		jp	OutWaitDiff

; LOAD *VGMLOOP [STOP]
CmdVGMLOOP:	cp	.STOP
		ld	b,1
		jr	nz,VGMLOOP_stop
		ld	b,0
		rst	NEXT_CHAR
VGMLOOP_stop:	call	MustBeEOL
		in	a,(ClkPort)
		ld	c,a
		ld	a,CMD_loopVGM
		call	OutWaitDiff
		ld	a,b
		jp	OutWaitEq

MustBeEOL:	cp	.nl
		jp	z,SyntaxDone
ReportC4:	rst	ERROR_1
		defb	$0B		; REPORT-C


; Syntax:
;   LOAD *PEG addr,"hexcode"
;   LOAD *PEG THEN RUN thread,addr
;   LOAD *PEG THEN STOP thread
;   LOAD *PEG THEN PAUSE thread
;   LOAD *PEG THEN CONT thread
CmdPEG:		cp	.THEN
		jr	z,PEGThen
		call	CLASS_6		; Get numeric expression
		call	MustBeComma	; Expect a comma and skip it
		call	GetStrExpr	; Read a string expression
		call	MustBeEOL	; line must end here, and syntax check
		call	STK_FETCH	; get string parameters
		srl	b
		jp	nz,ReportA	; limit length to 511
		rr	c
		jp	c,ReportA	; length must be even
		push	de		; save them
		push	bc
		call	STK_TO_A	; fetch integer into A
		ld	e,a
		pop	bc
		pop	hl
		ld	b,c		; length/2 in B
		rrc	c
		jp	c,ReportA	; length must be multiple of 4
		in	a,(ClkPort)
		ld	c,a		; fetch clock bit
		ld	a,CMD_loadPEG
		call	OutWaitDiff	; send command
		xor	c
		ld	c,a		; make bit 7 of C = clock again
		ld	a,e
		call	OutWaitDiff	; send address
		jp	SendHexStr	; send hex string as bytes and return

PEGThen:	rst	NEXT_CHAR	; skip THEN token
		cp	.RUN
		jr	nz,PEGOthers	; if other than RUN
		rst	NEXT_CHAR	; skip RUN token
		call	CLASS_6		; read thread number
		call	MustBeComma	; expect a comma
		call	CLASS_6		; read address
		call	MustBeEOL	; expect EOL
		call	STK_TO_A	; retrieve address
		push	af
		call	STK_TO_A	; retrieve thread
		pop	de		; D = address
		ld	e,a		; E = thread
		in	a,(ClkPort)
		ld	c,a
		ld	a,CMD_playPEG
		call	OutWaitDiff	; send command
		ld	a,e
		call	OutWaitEq	; send thread
		ld	a,d
		jp	OutWaitDiff	; send address and exit

PEGOthers:	ld	b,CMD_pausePEG
		cp	.PAUSE
		jr	z,DoPEG
		ld	b,CMD_stopPEG
		cp	.STOP
		jr	z,DoPEG
		ld	b,CMD_contPEG
		cp	.CONT
		jr	nz,ReportC4
DoPEG:		rst	NEXT_CHAR	; skip token
		push	bc		; save command
		call	CLASS_6		; fetch thread
		pop	bc		; balance stack
		call	MustBeEOL	; expect EOL and end if syntax check
		push	bc		; save command again
		call	STK_TO_A	; get thread
		pop	bc		; restore saved command
		ld	e,a		; save thread
		in	a,(ClkPort)
		ld	c,a		; load clock
		ld	a,b
		call	OutWaitDiff	; send command
		ld	a,e
		jp	OutWaitEq	; send thread and exit

; LOAD *PEB <addr>,<filename>
CmdPEB:		call	CLASS_6		; parse address
		call	MustBeComma
		call	GetStrExpr	; Read a string expression
		call	MustBeEOL	; end of command
		call	STK_FETCH	; fetch filename
		call	BC_1_255	; check filename validity
		push	de		; keep it safe
		push	bc
		call	STK_TO_A	; fetch address
		pop	bc
		pop	de
		ld	h,a		; keep address safe
		in	a,(ClkPort)
		ld	c,a
		ld	a,CMD_loadPEB
		call	OutWaitDiff	; send loadPEB command
		call	SendString	; send filename
		ld	a,h
		call	OutWaitDiff	; send address
		xor	c
		ld	c,a		; revert clock to equal
		jp	ReportStatus	; fetch status and return


		include	"extracmdcode.inc.asm"

; Check if BC is in range 1 to 255 and report error F if not
BC_1_255:	ld	a,b
		or	a		; length > 255?
		jr	nz,ReportF	; REPORT-F if so
		or	c		; length = 0?
		ld	b,c
		ret	nz
ReportF:	rst	ERROR_1
		db	$0E		; REPORT-F, invalid file name

; Input: Inverted clock in bit 7 of C; B = character count;
; DE = pointer to string
; Returns with bit 7 of C equal to current clock
SendString:	ld	a,b

		call	OutWaitEq	; send length

		ld	a,b
		or	a
		ret	z

; Alternate entry point, sends B bytes from (DE) with bit 7 of C = direct clk
SendStrLoop:	ld	a,(de)
		inc	de
		out	(DataPort),a
		ld	a,c
		cpl			; invert bit 7 of C
		ld	c,a
SendStrLoopW:	in	a,(ClkPort)
		xor	c
		jp	m,SendStrLoopW	; wait until equal
		djnz	SendStrLoop	; keep sending until length exhausted
		ret


; Input: Inverted clock in bit 7 of C; B = byte count (= character count / 2);
; HL = pointer to hex string
; Returns with bit 7 of C equal to current clock
SendHexStr:	ld	a,b
		out	(DataPort),a	; send length

		call	WaitClkEq

		ld	a,b
		or	a
		ret	z

; Alternate entry point, sends B bytes from (DE) with bit 7 of C = direct clk
SendHexLoop:	ld	a,(hl)
		inc	hl
		sub	.0
		rlca
		rlca
		rlca
		rlca
		add	a,(hl)
		inc	hl
		sub	.0

		out	(DataPort),a
		ld	a,c
		cpl			; invert bit 7 of C
		ld	c,a
SendHexLoopW:	in	a,(ClkPort)
		xor	c
		jp	m,SendHexLoopW	; wait until equal
		djnz	SendHexLoop	; keep sending until length exhausted
		ret

; LOAD *BAT [ TO <string-var>]
CmdBAT:		set	6,(iy+iyFLAGX)	; Abuse bit 6 of FLAGX (whether INPUT
					; is numeric or string) to indicate
					; whether to print or to store
		cp	.TO
		jr	nz,JustPrintBatt
		rst	NEXT_CHAR	; skip TO
		call	CLASS_1		; get variable details; clears FLAGX
		call	MustBeString	; error if numeric variable
		rst	GET_CHAR	; we should be at EOL now
JustPrintBatt:	call	MustBeEOL	; end syntax chk if so, else error
		; Syntax check passed, now we can take action
		ld	bc,5		; reserve 5 bytes in the workspace
		rst	BC_SPACES	; (may cause OOM)

		in	a,(ClkPort)
		ld	c,a
		ld	a,CMD_batt
		call	OutWaitDiff
		xor	c
		ld	c,a		; make bit 7 of C == clock again

		ld	b,5		; length of batt voltage string
		push	de		; save pointer to start of workspace

BattRead:	in	a,(DataPort)
		ld	(de),a		; store next character in workspace
		inc	de
		call	WaitClkDiff
		xor	c
		ld	c,a		; make bit 7 of C == clock again
		djnz	BattRead

		call	ReportStatus	; retrieve status byte

		pop	de		; restore pointer to workspace

		bit	6,(iy+iyFLAGX)	; Are we printing or storing?
		jr	nz,BattPrint	; jump if printing
		ld	bc,5		; length of string

		; Store string into the calc. stack with DE=pointer, BC=length
BattStore:	call	STK_STO_s
		jp	LET		; Store stack into the variable whose
					; parameters were fetched before, and
					; return through LET

BattPrint:	ld	b,5		; 5 bytes
BattPrintNext:	ld	a,(de)
		inc	de
		rst	PRINT_A		; Print each
		djnz	BattPrintNext
		ret

; LOAD THEN PRINT <filename-str>
CmdPRINT:	res	1,(iy+iyFLAGS)	; Printer flag
		; fall through

; LOAD THEN LPRINT <filename-str>
CmdLPRINT:	rst	NEXT_CHAR	; Skip over PRINT or LPRINT
		call	NAME_2		; fetch filename, BC=length, DE=addr
		rst	GET_CHAR
		call	MustBeEOL

		call	BC_1_255	; check validity of filename
		ld	a,CMD_printfile
		jp	PrintFileEntry


; LOAD *RTC [ = <string> | TO <string-var>]
CmdRTC:
		cp	.equal		; LOAD *RTC=string ?
		jr	z,DateSet
		set	6,(iy+iyFLAGX)	; Flag "Display it, don't store it"
		cp	.TO		; LOAD *RTC TO strvar ?
		jr	nz,DateDisplay
		rst	NEXT_CHAR	; skip TO
		call	CLASS_1		; Get variable details; clears FLAGX
		call	MustBeString	; Error if numeric var
		rst	GET_CHAR	; Retrieve character again
DateDisplay:	call	MustBeEOL	; Line must end here; end syntax check
		ld	bc,22		; length("YYYY-MM-DD hh:mm:ss.cc")
		rst	BC_SPACES	; make room for 22 chars

		in	a,(ClkPort)
		ld	c,a
		ld	a,CMD_rtc
		call	OutWaitDiff
		xor	a
		call	OutWaitEq	; Send 0 for string to set datetime
		ld	b,22
		push	de

DateRead:	in	a,(DataPort)
		ld	(de),a		; store next character in workspace
		inc	de
		call	WaitClkDiff
		xor	c
		ld	c,a		; make bit 7 of C == clock again
		djnz	DateRead

		call	ReportStatus	; retrieve status byte

		pop	de		; restore pointer to workspace

		bit	6,(iy+iyFLAGX)	; Are we printing or storing?
		jr	nz,DatePrint	; jump if printing
		ld	bc,22		; length of string
		jr	BattStore	; fetch calc and execute LET command

DatePrint:	ld	b,22		; 22 bytes
		jr	BattPrintNext

DateSet:	rst	NEXT_CHAR	; Skip = sign
		call	GetStrExpr	; Read string expression
		call	MustBeEOL	; Line ends here; end of syntax check
		call	ExpectShortStr	; Fetch and check string length

		in	a,(ClkPort)
		ld	c,a
		ld	a,CMD_rtc
		call	OutWaitDiff

		call	SendString	; Send from DE for B bytes
		jp	ReportStatus


; LOAD *SUMMER [STOP] - interruptor manual de horario de verano (DST), leido
; por el ESP32 de /SYS/NTP.CFG al sincronizar via NTP - ver CmdNTP mas abajo.
; No hay deteccion automatica de zona horaria/DST, el usuario lo cambia el
; mismo dos veces al ano, igual que con cualquier otro reloj.
CmdSUMMER:	ld	bc,CMD_dst_on*256 + CMD_dst_off
		jp	CMD_ONOFF_BC	; (jp: destino demasiado lejos para jr)


; LOAD *NTP [ = <string> | [+|-]<number> ]
; No argumento (fin de linea) -> fuerza una sincronizacion NTP inmediata
; = <string>                  -> ajusta el servidor NTP
; [+|-]<numero>                -> ajusta el offset UTC en horas (con signo)
; NOTA: FIND_INT (usada tambien por CmdROW/CmdBORDER/etc.) NUNCA admite
; numeros negativos - internamente comprueba el carry de FP-TO-BC y salta
; directo a REPORT-B si es negativo, antes de devolver el control aqui. Por
; eso el signo "-" se gestiona a mano abajo: se lee solo la MAGNITUD
; (siempre positiva) con CLASS_6/FIND_INT, y si el signo era "-" se niega
; el byte ya en C mediante complemento a dos, sin pasarle nunca un valor
; negativo a FIND_INT.
CmdNTP:		cp	.equal		; LOAD *NTP=string ?
		jp	z,NtpServerSet	; (jp: destino demasiado lejos para jr)
		cp	.nl		; LOAD *NTP (a secas, fin de linea)?
		jp	z,NtpForceSync	; (jp: destino demasiado lejos para jr)
		cp	.minus		; signo - ?
		jr	z,NtpOffsetNeg
		cp	.plus		; signo + opcional (positivo)
		jr	nz,NtpOffsetPos	; ni + ni -, numero positivo directo
		rst	NEXT_CHAR	; saltar el +

NtpOffsetPos:	call	CLASS_6		; Leer magnitud (siempre >=0)
		call	MustBeEOL	; La linea debe terminar aqui
		call	FIND_INT	; magnitud a BC

		ld	a,b		; validar que cabe en 0..127
		or	a
		jp	nz,ReportB	; B<>0 -> claramente >255
		ld	a,c
		cp	128
		jp	nc,ReportB	; positivo debe caber en 0..127
		ld	e,c
		jr	NtpOffsetSend

NtpOffsetNeg:	rst	NEXT_CHAR	; saltar el -
		call	CLASS_6		; Leer magnitud (siempre >=0)
		call	MustBeEOL	; La linea debe terminar aqui
		call	FIND_INT	; magnitud a BC

		ld	a,b		; validar que cabe en 0..128 (-128..0)
		or	a
		jp	nz,ReportB	; B<>0 -> claramente >255
		ld	a,c
		cp	129
		jp	nc,ReportB	; magnitud debe caber en 0..128
		xor	a
		sub	c		; A = -magnitud (complemento a 2)
		ld	e,a

NtpOffsetSend:	in	a,(ClkPort)	; E = byte de offset (con signo) a enviar
		ld	c,a

		ld	a,CMD_ntp_setoffset
		call	OutWaitDiff

		ld	a,e
		call	OutWaitEq
		jp	ReportStatus

NtpServerSet:	rst	NEXT_CHAR	; Skip = sign
		call	GetStrExpr	; Read string expression
		call	MustBeEOL	; Line ends here; end of syntax check
		call	ExpectShortStr	; Fetch and check string length

		in	a,(ClkPort)
		ld	c,a
		ld	a,CMD_ntp_setserver
		call	OutWaitDiff

		call	SendString	; Send from DE for B bytes
		jp	ReportStatus

NtpForceSync:	in	a,(ClkPort)
		ld	c,a
		ld	a,CMD_ntp_sync
		jp	OutWaitDiff	; fire-and-forget, sin respuesta de estado


SD_RESET:	ld	a,$F7		; Check keyboard row 1-5
		in	a,($FE)
		ld	d,a		; Store in D
		ld	a,$EF		; Check keyboard row 6-0
		in	a,($FE)
		ld	e,a		; Store in E
		and	d
		cpl
		and	$1F
		jr	nz,LoadROM	; If any pressed, jump to load a ROM

		; Copy the character set to RAM four times: $3800-$3FFF (2K),
		; so *256C's default table ($3800) looks identical to the ROM
		; font out of the box, same as *128C's default ($3C00) already
		; did with just the first two copies.
		ld	hl,$1E00
		ld	de,$3800
		ld	bc,$0200
		ldir			; copy 1: $3800-$39FF
		ld	b,$02
		ld	h,$1E
		ldir			; copy 2: $3A00-$3BFF
		ld	b,$02
		ld	h,$1E
		ldir			; copy 3: $3C00-$3DFF (*128C's table)
		ld	b,$02
		ld	h,$1E
		ldir			; copy 4: $3E00-$3FFF
		ld	hl,RAM_CHECK	; load continuation address

SD81_RESET:	; Start by waiting for a possible pending change. There should
		; not be any, but just in case, we wait. After a time out, or
		; after a change if there's one, write a reset to the MCU and
		; then wait for another change indefinitely. There's a short
		; time window for a race condition; let's hope it never
		; happens.

		ld	d,206		; timeout: 206 loops
		in	a,(ClkPort)	; read current clock in bit 7 of A
		ld	c,a		; store it in bit 7 of C
sdreset_1:	dec	d
		jr	z,sdreset_2	; skip if timed out (normal path)
		ld	b,3
sdreset_delay:	djnz	sdreset_delay	; delay
		in	a,(ClkPort)
		xor	c
		jp	p,sdreset_1	; wait for clock to change
		xor	c		; back to originally read value
		ld	c,a		; toggle bit 7 of C
sdreset_2:	out	(ClkPort),a	; send RESET signal (data is ignored)
sdreset_3:	in	a,(ClkPort)
		xor	c
		jp	p,sdreset_3	; wait for clock to change again

		ld	bc,$7FFF	; Set the same BC as the ROM
		jp	(hl)		; continue reset into RAM-CHECK
					; (or jump to user's code)

LoadROM:	ld	c,.1		; Decode the specific character
		ld	b,5
loadrom_1:	rrc	d
		jr	nc,loadrom_name
		inc	c
		djnz	loadrom_1
		ld	a,e
		ld	c,.0
		rrc	e
		jr	nc,loadrom_name
		ld	c,.9+1
loadrom_2:	rrc	e
		dec	c
		jr	c,loadrom_2

loadrom_name:
		ld	a,$E7		; Check top row again
		in	a,($FE)		; (both half-rows at the same time)
		cpl
		and	$1F
		jr	nz,loadrom_name	; Wait until all keys released

		ld	sp,$4400	; We need a stack to call routines
		ld	h,c		; Save ROM name
		call	GetPhase

		ld	a,CMD_load	; send Load command
		call	OutWaitDiff
		ld	a,10		; length of "/SYS/#.ROM"
		call	OutWaitEq	; send it
		ld	de,SlashSysSlash; folder
		ld	b,5		; length of "/SYS/"
		call	SendStrLoop	; send "/SYS/"
		ld	a,h		; send the character of the key
		call	OutWaitDiff
		ld	c,a		; sync clocks for SendStrLoop
		ld	de,DotROM	; string ".ROM"
		ld	b,4		; 4 characters
		call	SendStrLoop
		in	a,(DataPort)	; Read status
		or	a
loadrom_3:	jr	z,loadrom_3	; Freeze here if OK; MCU will reset us
		rst	0		; Reset


GetPhase:	ld	c,ClkPort
		in	c,(c)
		ret

GetData:	in	a,(DataPort)
		ret

; This is our version of the NAME (L03A8) routine customized for SD.
NAME_2:		CALL	SCANNING	; read expression
		LD	A,(FLAGS)
		ADD	A,A		; check bits 6 and 7
		JP	M,REPORT_C	; to REPORT-C if bit 6 set (numeric)
		JP	C,STK_FETCH	; exit via STK-FETCH if bit 7 set

		; bit 7 clear - checking syntax
		RET			; return NC if checking syntax

SyntaxDone:	bit	7,(iy+iyFLAGS)	; Checking syntax?
		ret	nz		; Return normally if not
		pop	hl		; Drop caller address
		ret			; Return to process next BASIC line

SD81RUNCMD:	rst	GET_CHAR	; Check if end of command
		cp	.nl
nzRUN:		jp	nz,RUN_COMMAND	; Normal RUN command if not
		ld	hl,(D_FILE)	; Check if program empty
		ld	de,$407D	; Address of start of BASIC
		and	a
		sbc	hl,de
		jr	nz,nzRUN	; If not empty, use normal RUN cmd
		ld	hl,(E_LINE)
		ld	de,(VARS)
		scf			; Skip $80 at the end of vars
		sbc	hl,de		; Check if VARS is empty
		jr	nz,nzRUN	; If not, execute normal RUN
		ld	de,VERSN	; Load address
		ld	b,l		; Start at line 0 (HL is now 0)
		ld	c,l
		exx			; Put those into DE' and BC'
		ld	de,AUTOEXEC_P	; String "/AUTOEXEC.P"
		ld	bc,AUTOEXEC_P_LEN
		set	1,(iy+iyFLAGS)	; LOAD mode
		jp	SaLoBytes

AUTOEXEC_P:	db	.slash,.A,.U,.T,.O,.E,.X,.E,.C,.dot,.P
AUTOEXEC_P_LEN	equ	$-AUTOEXEC_P

CmdList:
		db	.M,.A,.P + $80
		dw	CmdPAGE

		db	.R,.A,.M,.4,.8 + $80
		dw	CmdRAM48

		db	.R,.O,.M,.L,.O,.C,.K + $80
		dw	CmdROMLOCK

		db	.Z,.8,.1 + $80
		dw	CmdZ81

		include	"extracmdlist.inc.asm"

		db	.P,.L,.A,.Y + $80
		dw	CmdPLAY

		db	.S,.A,.Y + $80
		dw	CmdSAY

		db	.P,.E,.G + $80
		dw	CmdPEG

		db	.V,.G,.M + $80
		dw	CmdVGM

		db	.V,.E,.R + $80
		dw	CmdVER

		db	.F,.P,.G,.A + $80
		dw	CmdFPGA

		db	.1,.2,.8,.C + $80
		dw	Cmd128C

		db	.6,.4,.C + $80
		dw	Cmd64C

		db	.2,.5,.6,.C + $80
		dw	Cmd256C

		db	.R,.O,.W + $80
		dw	CmdROW

		db	.D,.I,.R + $80
		dw	CmdDIR

		db	.D,.E,.L + $80
		dw	CmdDEL

		db	.C,.D + $80
		dw	CmdCD

		db	.M,.D + $80
		dw	CmdMD

		db	.R,.D + $80
		dw	CmdRD

		db	.M,.V + $80
		dw	CmdMOVE

		db	.C,.P + $80
		dw	CmdCOPY

		db	.P,.W,.D + $80
		dw	CmdPWD

		db	.P,.E,.B + $80
		dw	CmdPEB

		db	.F,.R,.E,.E + $80
		dw	CmdFREE

		db	.O,.P,.E,.N,.D,.I,.R + $80
		dw	CmdOPENDIR

		db	.M,.C,.4,.5 + $80
		dw	CmdMC45ONOFF

		db	.V,.G,.M,.L,.O,.O,.P + $80
		dw	CmdVGMLOOP

		db	.I,.C,.H,.R + $80
		dw	CmdICHR

		db	.J,.O,.Y + $80
		dw	CmdJOY

		db	.B,.A,.T + $80
		dw	CmdBAT

		db	.R,.T,.C + $80
		dw	CmdRTC

		db	.N,.T,.P + $80
		dw	CmdNTP

		db	.S,.U,.M,.M,.E,.R + $80
		dw	CmdSUMMER

		db	.W,.R,.X + $80
		dw	CmdWRX

		db	.D,.B,.U,.F + $80
		dw	CmdDBUF

		db	.S,.F,.A,.S,.T + $80
		dw	CmdSFAST

		db	.S,.F,.H,.R + $80
		dw	CmdSFHR

		db	.S,.F,.S,.P + $80
		dw	CmdSFSP

		db	.B,.O,.R,.D,.E,.R + $80
		dw	CmdBORDER

		db	.C,.O,.L,.O,.R + $80
		dw	CmdCOLOR

		db	.F,.U,.L,.L,.P,.A,.G + $80
		dw	CmdFULLPAG

		db	$FF

SlashSysSlash:	db	.slash,.S,.Y,.S,.slash
DotROM:		db	.dot,.R,.O,.M

ErrMsgAdr:
		db	.nl,.nl,.nl,.nl,.nl,.nl,.nl,.nl,.nl,.nl,.nl,.nl
		db	.star
		db	.L,.O,.A,.D,.sp,.E,.R,.R,.O,.R,.sp,.quote
ErrCodeOfs	equ	$ - ErrMsgAdr
		db	.0,.quote,.comma,.sp,.B,.R,.E,.A,.K,.sp
		db	.T,.O,.sp,.R,.E,.S,.E,.T
		db	.star
		db	.nl,.nl,.nl,.nl,.nl,.nl,.nl,.nl,.nl,.nl,.nl,.nl
		db	.nl
ErrMsgLen	equ	$ - ErrMsgAdr

.sp		equ	0
.quote		equ	11
.colon		equ	14
.qm		equ	15
.lp		equ	16
.rp		equ	17
.equal		equ	20
.plus		equ	21
.minus		equ	22
.star		equ	23
.slash		equ	24
.comma		equ	26
.dot		equ	27
.0		equ	28
.1		equ	29
.2		equ	30
.3		equ	31
.4		equ	32
.5		equ	33
.6		equ	34
.7		equ	35
.8		equ	36
.9		equ	37
.A		equ	38
.B		equ	39
.C		equ	40
.D		equ	41
.E		equ	42
.F		equ	43
.G		equ	44
.H		equ	45
.I		equ	46
.J		equ	47
.K		equ	48
.L		equ	49
.M		equ	50
.N		equ	51
.O		equ	52
.P		equ	53
.Q		equ	54
.R		equ	55
.S		equ	56
.T		equ	57
.U		equ	58
.V		equ	59
.W		equ	60
.X		equ	61
.Y		equ	62
.Z		equ	63

.nl		equ	118
.cursor		equ	127

.CODE		equ	196
.PEEK		equ	211
.USR		equ	212
.STRs		equ	213
.OR		equ	217
.THEN		equ	222
.TO		equ	223
.LPRINT		equ	225
.STOP		equ	227
.SLOW		equ	228
.FAST		equ	229
.CONT		equ	232
.GOTO		equ	236
.PAUSE		equ	242
.POKE		equ	244
.PRINT		equ	245
.RUN		equ	247
.CLEAR		equ	253
