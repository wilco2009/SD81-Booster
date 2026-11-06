		db	.I,.N + $80
		dw	CmdIN

		db	.O,.U,.T + $80
		dw	CmdOUT

		db	.L,.D,.I,.R + $80
		dw	CmdLDIR

		db	.L,.D,.D,.R + $80
		dw	CmdLDDR

		db	.I,.N,.V + $80
		dw	CmdINV

		db	.B,.O,.L,.D + $80
		dw	CmdBOLD

		db	.H,.E,.X + $80
		dw	CmdHEX

		db	.S,.P,.R,.P,.A,.G,.E + $80
		dw	CmdSPRPAGE

		db	.S,.P,.R,.B,.A,.S,.E + $80
		dw	CmdSPRBASE

		db	.S,.P,.R,.I,.T,.E + $80
		dw	CmdSPRITE

		db	.S,.C,.R,.O,.L,.L + $80
		dw	CmdSCROLL

		db	.S,.C,.R,.O,.W,.S + $80
		dw	CmdSCROWS

		db	.C,.O,.L,.8,.0 + $80
		dw	CmdCOL80

		db	.C,.O,.L,.7,.0 + $80
		dw	CmdCOL70

		db	.C,.O,.L,.3,.2 + $80
		dw	CmdCOL32
