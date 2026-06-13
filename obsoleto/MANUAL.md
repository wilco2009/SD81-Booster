# SD81 Booster Manual


## BASIC EXTENSIONS

The BASIC has been extended through the LOAD and the SAVE commands. They provide many extensions to make use of most features of this interface while retaining full backward compatibility.

Throughout this document, `<string>` means any string expression. This may be a quoted string literal like `"PROG"`, a string variable like `A$`, addition of strings like `D$+"/PROG"`, functions returning a string like `CHR$ 38`, etc. etc.


### Loading and saving

The `RUN` command, when executed without any program loaded, will load from the root of the SD a program called `AUTOEXEC.P` and start it from line 0.

By default, the `LOAD <string>` and `SAVE <string>` commands work from tape, as usual. It's possible to turn on a mode where they load/save from/to SD by default. This mode is toggled as follows:

    LOAD FAST

where `FAST` is the token obtained with shift + F, not a word spelled letter by letter. This mode is shared with the SAVE command. In fact, you can also change the mode using `SAVE FAST` without any more arguments.

You can go back to default tape mode with:

    LOAD SLOW

(again, `SLOW` is the shift+D token).

To always load/save from/to SD regardless of mode:

    LOAD FAST <string>
    SAVE FAST <string>

as appropriate, where `<string>` is the file name or path.

Similarly, to always load or save from/to tape regardless of mode:

    LOAD SLOW <string>
    SAVE SLOW <string>

When loading from SD, the given string is first tried verbatim. If it matches a filename in the SD card, then that's the one that will be loaded. If not, a second attempt is made by appending ".P" to the string. If that fails too then an error results. See ERROR CODES below for the interpretation of the possible error codes.

When saving, the ".P" extension is always appended to the file name.

When a file ends in ".P", it will be loaded using the same method the ROM uses. Many files in .P format have junk at the end of the file, after the position where the file is supposed to end; the loader will only load up to the position where the tape loader in ROM would finish, though, and ignore the junk bytes.

However, when the file does not end in ".P" (as present in the SD card), the whole file is loaded into memory. If the file ends in ".ROM", then it will be loaded into address 0 and the CPU will be reset.

The default load address (except for .ROM files) is 16393 (4009 hex), which is where BASIC programs are loaded. To load a block at a different address, you can use:

    LOAD FAST <string> CODE <address>

Again, `CODE` is the token. In this case, the `FAST` token is mandatory for `CODE` to work; therefore the command `LOAD <string> CODE <address>` will still load at 16393 despite the use of `CODE`, even if the default mode is SD. There's a compatibility issue that prevents the form without `FAST` from working as expected.

Unlike Spectrum tapes, files in SD don't have any header or indicator of a default address where the file should be loaded; for this reason, the `<address>` after `CODE` is mandatory. Unlike the LOAD command in the Spectrum, where CODE also admits zero or two parameters, this command only admits one.

To save a block of memory to SD:

    SAVE FAST <string> CODE <address>,<length>

If a program has auto-run, you can stop it before executing with:

    LOAD FAST <string> THEN STOP

And if you want to start it at a specific line (even if it doesn't auto-start):

    LOAD FAST <string> THEN GOTO <number>


### Other commands

Most other commands are prefixed with either of these:

    LOAD *
    LOAD LPRINT

In this case, the command that comes after the `*` or `LPRINT` needs to be spelled letter by letter. If a command has parameters, there must be at least one space between the command and the first parameter. If the command does not output any text (e.g. the CD command) then both prefixes are equivalent.

If `LPRINT` is used instead of `*`, then the commands that output text will redirect said text to the ZX Printer.

In particular, these are the commands to obtain the current version of the interface:

    LOAD *VER
    LOAD LPRINT VER

That command will print both the ROM version and the MCU version. The ROM version can also be obtained by PEEKing the address 8196 (2004 hex); the top four bits are the major version number and the bottom four bits are the minor one. The MCU version can be obtained with USR 8199 and has the same format. For example, this BASIC program retrieves the MCU version and prints it:

    10 LET VERSION=USR 8199
    20 LET MAJOR=INT(VERSION/16)
    30 LET MINOR=VERSION-16*MAJOR
    40 PRINT "MCU VERSION IS ";CHR$ (MAJOR+28);".";CHR$ (MINOR+28)


### File management

Print the directory listing:

    LOAD *DIR
    LOAD *DIR <string>
    LOAD LPRINT DIR
    LOAD LPRINT DIR <string>

The `<string>` may include a path and wildcards. The wildcards are Unix-style, not DOS-style, meaning in particular that `*.*` will only match those filenames that include a dot in their name, while `*` will match all files no matter whether they have a dot or not. A question mark `?` will match any character, but exactly one; for example `LOAD *DIR "/MYDIR/*.??"` will list all files in `/MYDIR/` that have any string followed by a dot and two characters, e.g. `README.MD`, but won't list files ending e.g. in `.P` or in `.BIN`.

If the directory listing does not fit in the screen, the bottom line will show an ellipsis "..." and wait for a key. Pressing the BREAK/SPACE key will interrupt the listing with error D. Any other key will result in the screen being cleared and the listing continuing from the top of the screen.

Print free space:

    LOAD *FREE
    LOAD LPRINT FREE

Note that free space calculation may take several seconds.

Change current directory:

    LOAD *CD <string>

Print currently selected (working) directory:

    LOAD *PWD
    LOAD LPRINT PWD

Create a subdirectory in the current directory:

    LOAD *MD <string>

Remove a subdirectory from the current directory (must be empty):

    LOAD *RD <string>

Remove a file:

    LOAD *DEL <string>

(no wildcards allowed).

Rename or move a file:

    LOAD *MV <string> TO <string>

where `TO` is the token obtained with shift + 4, not spelled letter by letter.

Copying a file is similar but using CP instead of MV:

    LOAD *CP <string> TO <string>

Note that the date/time of the file is not preserved.

Accessing the directory entries from within a program is possible with a combination of two commands:

    LOAD *OPENDIR <string>

The string is interpreted as a full path and/or a wildcard mask. This leaves the given directory ready for the next command. If there is a wildcard mask, then there will be a limit of 512 entries maximum that can be retrieved.

After an `OPENDIR` command, the `ROW` command will place the desired line into a string variable:

    LOAD *ROW <number> TO <string-variable>

Here, `<number>` is any numeric expression for a row number, and `<string-variable>` is, well, a string variable, i.e. a letter followed by the `$` symbol. The directory entry with the given row number will be stored into the string variable. The first row is 1. Rows out of range return an empty string.


### Sound

This command will play the given string(s):

    LOAD *PLAY <string>[,<string>[,<string>]]

This will play up to three strings, each in a different channel. The maximum length of each string is 255 characters. The syntax for the strings is based on the ZX Spectrum 128 PLAY command; for details check the PLAY COMMAND section below.

This command will allow use of the speech feature:

    LOAD *SAY <string>

The string is assumed to be in hexadecimal. Bytes in the range E0 to EF are bank numbers and select a bank; any other byte in the range 80 to FF is a sound from that bank. Bytes with a code less than 80 are illegal. At start, bank E4 is selected.

Files in VGM (Video Game Music) format can be played in background while leaving the CPU free, as long as they only contain AY-3-8910 or AY-3-8912 data. The corresponding support commands are:

Prepare (but don't play) a certain VGM file. Only one can be active at a time.

    LOAD *VGM <string>

Set the current VGM file to looping mode:

    LOAD *VGMLOOP

When the VGM reaches the end, it will start from the beginning

Set the current VGM file to non-looping mode:

    LOAD *VGMLOOP STOP

(STOP is the token, as usual). When the VGM reaches the end, it won't start from the beginning.

Pause a playing VGM:

    LOAD *VGM THEN PAUSE

where THEN and PAUSE are the tokens.

Resume a paused VGM, or start playing if it isn't started:

    LOAD *VGM THEN CONT
    LOAD *VGM THEN RUN

where THEN and CONT/RUN are the tokens. Both versions are equivalent.

Stop a VGM:

    LOAD *VGM THEN STOP

where THEN and STOP are the tokens.

The interface also contains a user-programmable sound effects generator (PEG). It is a virtual machine that executes commands and can manipulate AY registers directly. It also contains variables to store values that can be used to help with certain effects. To create PEG programs, you can use a PEG assembler to facilitate the task; there's a simple one, written in Python, provided in the repository. To enter PEG programs via BASIC, you need to enter the virtual machine instructions in hexadecimal notation.

There are up to three "threads"; the machine can contain up to 256 words. Each word is an instruction and is 16 bits long.

The commands to support PEG are:

    LOAD *PEG <address>,<hexstring>

This loads the words in the hexadecimal string starting on the given addresss. Since instructions are 16-bit words, the length of the hexadecimal string needs to be a multiple of 4; otherwise an error A (invalid argument) will be raised. The string may contain a maximum of 508 hexadecimal characters (that's 127 PEG commands).

Command words must be given in big-endian order.

This command plays a PEG that starts at a given address, in the given thread:

    LOAD *PEG THEN RUN <thread>,<addr>

A PEG can be stopped:

    LOAD *PEG THEN STOP <thread>

paused:

    LOAD *PEG THEN PAUSE <thread>

and resumed:

    LOAD *PEG THEN CONT <thread>

See the section PEG DESCRIPTION below about the PEG commands and how they are encoded.


### User-defined characters

The following command sets a mode where 128 characters can be used instead of 64, and allows redefining them by POKEing to the memory between 15360 and 16383 (in hexadecimal, between 3C00 and 3FFF):

    LOAD *128C

To revert to normal mode:

    LOAD *64C

Note that in 128 character mode, the hardware inverts the top 64 characters, so for those you need to store the inverse of the graphic you want to display. By default, the region 3C00-3FFF is loaded with two copies of the character set in the ROM, so there's no apparent difference between the two modes until that region is changed.

On a cold reset, LOAD *128C will have the same character set as the ROM. If you have modified it and want to restore it, or want to ensure that a previous application has not set a different one, this sequence should do it (see description of `*LDIR` command below):

    LOAD *LDIR 7680,15360,512
    LOAD *LDIR 7680,15872,512


### HRG Compatibility mode

For some high-resolution games to work, the internal character generator must be turned on. By default it is off. Enabling the internal character generator disables user-defined characters.

The syntax for enabling/disabling this mode is:

    LOAD *ICHR
    LOAD *ICHR STOP

where STOP is the token. The first version enables the internal character generator, the second one disables it and allows using user-defined characters.


### Joystick Mapping

The joystick port is a standard Atari port with one fire button. The joystick actions emulate keypresses, and the keys can be configured with this command:

    LOAD *JOY <string>

The string must have 5 characters, and they are the keys that the interface will press for the following joystick actions in order:

- Up
- Down
- Left
- Right
- Fire button

The Shift key is represented by an Asterisk character (`*`), and the Enter key by a Less Than symbol (`<`). The space (` `) and the period (`.`) represent themselves. All alphanumeric characters are permitted. Other codes, including inverses, are illegal and will cause an error. The string must have at least 5 characters.

For example, for configuring the direction keys for movement and `0` for fire, you would use this command:

    LOAD *JOY "76580"

Or for the classic Q up, A down, O left, P right, M fire:

    LOAD *JOY "QAOPM"

The joystick configuration is not persistent. You need to enter one every time you boot the interface. A suggestion for running specific games with different joystick setups is to write a short BASIC program that configures the joystick and loads the game, and load that instead of the game.


### Memory management

The memory address space is organized in 8 blocks of 8 kbytes, and the RAM is organized in 64 pages of 8 kbytes. You can make visible any RAM page at any block; see section MEMORY PAGING below for details.

To change the page for a certain block of memory, use this command:

    LOAD *MAP <block>,<page>

Both `<block>` and `<page>` are numbers or numeric expressions. `<block>` is a number from 0 to 7 and `<page>` from 0 to 63. If `<page>` is between 32 and 63, the paging mode will be changed to full mode; otherwise it will be unchanged. See section MEMORY PAGING below for more information.

To read the currently selected page for a certain block:

    LOAD *MAP <block> TO <variable>

The `<block>` should be between 0 and 7. The `<variable>` will be set to the page number assigned to that block, between 0 and 63.


### Compatibility with 48K RAM expansions

The interface has a powerful paging system with 8 KB granularity that allows you to use up to 512 KB of RAM. However, some 48 KB expansions operate in a slightly different way. By default, in this interface, if you place usable RAM in blocks 6 or 7 (the 48-56K or 56-64K areas respectively), you can run machine code in these pages, provided that bit 6 of each opcode is 1.

When using the available 48K memory expansions, on the other hand, attempting to execute code at these addresses forces the opcodes to be read from block 2 or 3 (16-24K or 24-32K respectively). This is how they allow the screen to continue to be drawn even when there is RAM in that area. However, that's not how this interface behaves by default. For that reason, there is a command that enables or disables a mode that enables compatibility with these expansions:

    LOAD *RAM48

enables compatibility with 48K memory expansions; the following command:

    LOAD *RAM48 STOP

(STOP is the token, as usual) disables it. This mode persists after reset and power-off, because it's stored in battery-backed SRAM. When the computer boots with RAM48 mode active, blocks 6 and 7 are initialized with pages 6 and 7 respectively; when it boots in normal mode, blocks 6 and 7 are assigned pages 2 and 3 respectively on boot. Of course you can change the pages at will; see the Memory Paging section.


### Execution of machine code on blocks 4 and 5

The following command enables machine code execution on blocks 4 and 5 (addresses 32768 to 49151, 8000-BFFFh) and disables the possibility to have the video memory in that range. PLEASE SEE THE CAVEATS IN [README.md](README.md) BEFORE USING.

    LOAD *MC45

This is also called the "M1NOT" mode because of an internal mod with that name for the same purpose, although the technique used is different.

This command disables machine code execution on blocks 4 and 5 and re-enables the possibility to have video memory in that area:

    LOAD *MC45 STOP

where STOP is the token.


### Other BASIC extensions

There are some commands to facilitate certain operations from BASIC.

To invert all characters in a string variable (by flipping bit 7 of every character):

    LOAD *INV <string_var>

String slices are supported, e.g. `LOAD *INV A$(2 TO 7)`.

To force them to inverse video (by setting bit 7):

    LOAD *BOLD <string_var>

If you want to make the characters normal, first force them to inverse video with `BOLD` then flip them with `INV`.

To copy or fill blocks of memory, you can use these:

    LOAD *LDIR <source>,<dest>,<count>
    LOAD *LDDR <source>,<dest>,<count>

`LDIR` will copy `<count>` bytes from `<source>` to `<dest>` in ascending order; `LDDR` will do the same in descending order. They are just an interface to their machine code counterparts, so they have the same caveats including what happens when source and destination overlap.

To execute a machine code routine, you don't need to use `RAND USR <number>`; you can now use this command (where `USR` is the token):

    LOAD USR <number>

This has three advantages. First, the random number generator is not messed with. Second, machine code routines invoked this way are called from the top level of BASIC, instead of being called from the calculator. This leaves the extra set of registers `BC'`, `DE'` and `HL'` available, and both the machine stack and the calculator stack are clean when the routine is entered. Finally, the rest of the command after the numeric expression is not parsed, therefore the called routine can perform its own parsing via `RST 18h` and `RST 20h`. Note however that expressions or subexpressions containing number literals won't work when calling the `SCANNING` ROM routine, because numbers in numeric expressions need to be converted (by appending the number indicator, which is character 126, and the binary floating-point version of the number) during the syntax checking phase, and that can't be done. However, expressions containing workarounds such as `VAL "number"` or `CODE "character"` can be parsed by the `SCANNING` ROM routine.

As with normal USR, on entry to the routine, `BC` will contain the address called. Unlike normal USR, everything to the right of the USR will be treated as the address to call, so for example `LOAD USR 1+1` is the same as `LOAD USR 2` instead of calculating USR 1 and then adding 1 to the return value.

You can use IN and OUT from BASIC, just like in the Spectrum, with the difference that IN is not a function, but a command that sets a variable:

    LOAD *OUT <port>,<value>
    LOAD *IN <port> TO <variable>

`TO` is the token, as usual.

The following command allows loading hexadecimal byte sequences to memory:

    LOAD *HEX <address>,<hexstring>

For example, `LOAD *HEX 30000,"0A014020"` will load address 30000 with 10 (0Ah), address 30001 with 1, address 30002 with 64 (40h) and address 30003 with 32 (20h).

For the following commands, don't spell them, use the tokens instead.

16-bit PEEK:

    LOAD PEEK <address> TO <variable>

16-bit POKE:

    LOAD THEN POKE <address>,<value16>

Set the last address with RAM that BASIC can use:

    LOAD THEN CLEAR <address>

NOTE: Despite the name, and unlike what other implementations do (including the Spectrum one), this clears the GOSUB stack but *not* the variables. Issue a separate CLEAR if you want to clear the variables as well.


## PLAY COMMAND

The internal AY emulator has up to three voices. The PLAY command accepts up to three strings; the first string goes to voice 1, the second to voice 2 and the third to voice 3.

The syntax for each string is a series of commands, as follows:

`T<number>` sets the tempo. This command will be ignored in voices 2 or 3; it only takes effect in voice 1. The number is between 60 and 240 and the value is in beats per minute (bpm). The default when not specified is 120 bpm. A beat is defined as the duration of a crotchet.

`<number>` alone sets the note or rest duration from the point where it appears onwards. The following values are defined:

              Duration in
    Value  seconds at 60 bpm  Name/Description
     1            0.25        Semiquaver (1/4 Crotchet)
     2            0.375       Dotted semiquaver (3/8 Crotchet)
     3            0.5         Quaver (1/2 Crotchet)
     4            0.75        Dotted quaver (3/4 Crotchet)
     5            1           Crotchet
     6            1.5         Dotted crotchet (3/2 Crotchet)
     7            2           Minim (2 Crotchets)
     8            3           Dotted minim (3 crotchets)
     9            4           Semibreve (4 crotchets)
    10            0.1666667   Semiquaver Triplet (1/6 Crotchet)
    11            0.3333333   Quaver Triplet (1/3 Crotchet)
    12            0.6666667   Crotchet Triplet (2/3 Crotchet)

To allow a duration after another number, for example the tempo, you can use the letter N or a space as a separator. For example either `T60N3` or `T60 3` sets tempo 60 and duration 3.

Durations can be joined by a ligature (dash, `-` sign) to add the durations together. For example, `4-3A` is the same as `3-4A` which is 1/2+3/8 = 7/8 crotchet or 0.875 seconds at 60 bpm.

`O<number>` (that's the capital letter o, not the number zero) sets the octave number. Valid range is from 0 to 8. Default is 4.

`£` plays a rest.

`C`/`D`/`E`/`F`/`G`/`A`/`B` plays the given note. If the letter is in inverse video, it plays a note from the next octave.

`$` makes the next note play flat.

`=` makes the next note play sharp.

`V<number>` sets the volume of the voice from 0 (off) to 15 (loudest) (broken for now)

`W<number>` selects one of 8 envelope effects, from 0 to 7:

    W0    Decay then stay off                        |\______...
    
    W1    Increase then stay off                     /|______...
                                                        ______
    W2    Decay then stay on                         |\|     ...
                                                      _______
    W3    Increase then stay on                      /       ...
    
    W4    Decay repeatedly                           |\|\|\|\...
    
    W5    Increase repeatedly                        /|/|/|/|...
    
    W6    Increase then decay then repeat again      /\/\/\/\...
    
    W7    Decay then increase then repeat again      |\/\/\/\...

`X<number>` sets the time for each ramp of the volume effect, from 0 to 65535; 65535 is longest. 6927 is 1 second.

`U` enables the use of volume envelopes for a channel. Note that the envelope generator is shared by all channels: you can't have different channels play notes with envelope independently as would be desirable; all enabled channels will play the envelope at the same time. That's a quirk of the chip, don't blame us!

When using the `U` command on a channel, the volume setting stops taking effect, as the volume is now controlled by the envelope generator. Use the `V` command to set a volume and undo the effect of the `U` command.

`(`...`)` repeats a section once.

`)` without a matching `(` repeats the string from the start indefinitely.

`H` forces termination of the whole PLAY command (even if there were other voices playing). Useful when there's another voice in an infinite loop.

`M<number>` selects which channels are active and in what mode. 1, 2, 4 are for channel A, B, C respectively in tone mode. 8, 16, 32 are for channel A, B, C respectively in noise mode. Add them together to form a number between 0 and 63 (0 means all off; 63 means all channels are tone and noise at the same time).


## PEG DESCRIPTION

The Programmable Effects Generator is a virtual machine that can execute a certain set of instructions, that can access the registers of the AY emulator in order to reproduce sound effects that can be programmed.

The virtual machine can access the 16 AY registers, and it also contains 16 variables for general purpose usage. The AY registers are named R0-R15 (in decimal, unlike the AY documentation which uses octal notation); to refer to an unspecified one we use the letter R. The variables are named as V0-V15, and we refer to an unspecified one as V. The registers are 8 bits wide; the variables are 16 bits wide.

Some commands use offsets. Offsets are always relative to the address of the instruction following the one that uses it; however, the assembler makes this transparent and accepts absolute positions. But once you have a self-contained PEG program, you can relocate it anywhere within the PEG memory area.

Constant values are specified as X (4 bit constant), XX (8 bit constant) or XXX (12 bit constant) below. Time for WAIT is specified in milliseconds.

Here's the list of commands and their encoding in hexadecimal:

    LD   R,XX     0R XX     Loads a register with an 8 bit value
    ADD  R,XX     1R XX     Adds an 8 bit value to a register
    LD   V,XX     2R XX     Loads a variable with an 8 bit value (rest zeros)
    ADD  V,XX     3R XX     Adds an 8 bit value to a variable
    LD   R,R      40 RR     Loads a register with another one
    LD   R,V      41 RV     Loads a register with a variable
    LD   V,R      42 VR     Loads a variable with a register
    LD   V,V      43 VV     Loads a variable with another one
    ADD  V,V      44 VV     Adds the second variable to the first one
    SUB  V,V      45 VV     Subtracts the second variable from the first one
    ADC  V,V      46 VV     Like ADD but taking carry into account
    SBC  V,V      47 VV     Like SUB but taking carry into account
    NOT  V,V      48 VV     Sets the 1st var to the 2nd, with all bits negated
    AND  V,V      49 VV     Bitwise AND of both variables, result in first
    OR   V,V      4A VV     Bitwise OR of both variables, result in first
    XOR  V,V      4B VV     Bitwise XOR of both variables, result in first
    MUL  V,V      4C VV     Multiply two variables (*)
    DIV  V,V      4D VV     Divide two variables (**)
    SHR  V,X      4E VX     Shift the bits of a variable to the right
    SHL  V,X      4F VX     Shift the bits of a variable to the left
    MUL  V,XX     5V XX     Multiply a variable by a constant (*)
    DIV  V,XX     6V XX     Divide a variable by a constant (**)
    SUB  V,XX     7V XX     Subtract a constant from a variable
    DJNZ V,XX     8V XX     Decrement and jump by offset XX if result not zero
    WAIT XXX      9X XX     Wait for the given time before continuing execution
    WAIT V        A0 0V     Wait for the time specified in the given var
    HALT          A0 10     Stop effect
    JR   XX       A1 XX     Jump by given offset

(*) Multiplying two 16-bit values yields a 32-bit value. The 32-bit result is stored with the low-order word in the given destination register, and the high-order word in the next, numerically. For example MUL V2,V5 will store the lowest 16 bits in V2 and the highest 16 bits in V3.

(**) Division gives the quotient in the destination register, and the remainder in the next register numerically. For example, DIV V2,V5 gives the quotient in V2 and the remainder in V3.
    

## MEMORY PAGING

The interface supports up to 512 KB of memory. This memory is divided into sixty-four 8 KB pages, while the address space of the Z80 is divided into eight 8 KB blocks.

Here's how the Z80 address space is organized into blocks:

- Block 0: from 0000 to 1FFF
- Block 1: from 2000 to 3FFF
- Block 2: from 4000 to 5FFF
- Block 3: from 6000 to 7FFF
- Block 4: from 8000 to 9FFF
- Block 5: from A000 to BFFF
- Block 6: from C000 to DFFF
- Block 7: from E000 to FFFF

Block 0 is always read-only; the rest of blocks are always read-write.

Each block can contain any of the 64 pages the 512 KB RAM is divided into. A page can be mapped to more than one block simultaneously. The initial page assignment is as follows:

- Blocks 0 to 5: pages 0 to 5 respectively
- Blocks 6 and 7: pages 2 and 3 respectively

Blocks 6 and 7 need to mirror what blocks 2 and 3 contain, for full compatibility with the ZX81 video generation software and circuitry. This is for general use, but if you know that the display file is entirely contained in block 2, then block 6 must be mapped to the same page as block 2, but you can map block 7 to any page you wish, and similarly, if the display file is entirely contained in block 3, then block 7 needs to have the same page as block 3, but block 6 is available to be mapped at will. Blocks 4 and 5 can always be mapped to any page.

Note that due to the peculiarities of the ZX81 hardware, blocks 4-7 can only be used for data, and not to execute code, because any instruction within these blocks whose opcode has bit 6 equal to 0 (opcodes in range 0-63 and 128-191) would be interpreted as a NOP.

You can modify the ROM by mapping page 0 to any write-enabled block and making the changes there; or you can entirely replace the ROM by filling any desired page with the new ROM's contents and then mapping it to block 0.

The default ROM of the interface expects block 1 to contain the routines that handle the BASIC command extensions; on boot these routines are copied to page 1, which is in turn mapped to block 1. If you don't need the extra BASIC commands, you can place any ROM in block 0 and then use block 1 as desired.

The mapping interface can work in two modes. In the simpler one, only 256 KB of memory are accessible through the 8 bits of the port; there's another more complex scheme that allows access to all 512 KB. See MACHINE CODE PROGRAMMING below for details.


## BOOTING WITH A DIFFERENT ROM

If the SD card contains a file /SYS/0.ROM, or /SYS/1.ROM, etc. up to /SYS/9.ROM, then keeping the corresponding number key pressed during boot will load that ROM and reset the machine. This allows other ROMs to be loaded without needing to use the LOAD command.

The load will happen when *releasing* the number key.


## MACHINE CODE PROGRAMMING

The interface uses three I/O ports: one for the memory mapper (see MEMORY PAGING above for how paging works), and the other two for communication with the MCU that controls access to the SD card and emulates the AY and speech features.


### Memory mapper

The I/O port for memory mapping is E7 hex. In simple paging mode (max. 256 KB), only the 8 bits written to the port are used. The bits are divided as follows:

- D2, D1, D0: block number (0 to 7)
- D7, D6, D5, D4, D3: page number (0-31)

Only thirty-two 8 KB pages are available this way, hence 256 KB.

In full paging mode, instead of using D7-D3 for the page number, port address bits A13-A8 are used.

The full range of page numbers can be written to by loading B with the page number, C with E7 hex, and any other register with the block number, and using the instruction `OUT (C),r`. For example, if A contains the block number (0-7) and B contains the page number (0-63), this code maps page B to block A:

            ld      c,0E7h          ; mapper port
            out     (c),a           ; perform the page selection

Simple/full paging mode is changed through an MCU command; see MCU communication and MCU commands below.


### MCU communication

There are two I/O ports involved in the communication with the MCU: a data port (port A7 hex) and a control port (port AF hex).

The data port is the main communications port. Commands, parameters and data are written to this port, and responses are read from it.

Writing any value to the control port causes a soft reset of the MCU. Note that using this command while the MCU is saving or copying a file may result in SD card corruption, so use with care. The modified ROM automatically uses this feature on reset, just in case the MCU was somehow stuck from a previous operation.

When reading the control port, only bit 7 matters; the rest of bits read are undefined. Bit 7 is the *clock bit*, which is used for synchronizing the communication between the Z80 and the MCU.

Every time a read from or a write to the data port is performed, the clock bit is toggled. The initial state of the clock bit at boot is undefined. The Z80 must wait for this bit to change before the next read or write; otherwise a desynchronization may happen and either the Z80 or the MCU may become stuck in an endless waiting loop and need a reset.

At the beginning, as well as after every command is completed, the MCU is waiting for a command from the Z80. The Z80 initiates a command by writing its code to the data port. When the clock changes after that, it means that either:

- the MCU is ready for receiving the next byte (a parameter, e.g. after sending the CD command),
- the MCU has written a response byte (e.g. if the command was a VERSION command), or
- the command is finished and the MCU is ready for another command (e.g. if the command was a NOP command).

A typical sequence for sending a command + parameter and waiting for a response may look as follows (this example uses the GETBYTE command to retrieve the byte with index 16 from an internal 256-byte memory in the MCU, see the MCU commands section below for details):

            in      a,(0AFh)        ; read the clock bit at start
            ld      c,a             ; keep it safe
    
            ld      a,20h           ; code of the GETBYTE command
            out     (0A7h),a        ; send the command
    
    WAIT1:  in      a,(0AFh)        ; read the clock
            xor     c               ; compare with value at start
            jp      p,WAIT1         ; wait until current != starting clock
    
            ld      a,16            ; index of internal byte
            out     (0A7h),a        ; send as parameter
    
    WAIT2:  in      a,(0AFh)        ; read the clock
            xor     c               ; compare with value at start
            jp      m,WAIT2         ; wait until current == starting clock
    
            in      a,(0A7h)        ; get the byte from that index
            ld      b,a             ; keep it safe
    
    WAIT3:  in      a,(0AFh)        ; read the clock
            xor     c               ; compare with value at start
            jp      p,WAIT3         ; wait until current != starting clock
    
            ; Now B contains the required byte.
            ; The command is completed and the Z80 is ready to send
            ; another command immediately.

It's recommended to always wait for a clock change after the last write or read to/from the data port that completes the command. This ensures that the MCU is ready to read the next command and the Z80 can send another one immediately.

Care must be taken to ensure that every command is fully completed with all the correct amounts of bytes transmitted and/or received, and all clock changes are waited for, in order to avoid a loss of sync, which would cause a freeze sooner or later.


### MCU commands

TO SEE "SD81 Booster TECHNICAL DOCUMENTATION"


## EXTRA ROM

The extra ROM starts at address 8192 (2000 hex). Here's a map of the initial contents:

    Address (hex)   Content
    2000            String "SD81" (in the ZX81 character encoding).
    2004            Version byte, e.g. 10h means version 1.0.
    2005            Routine that gets the return address of the caller in HL
                    (can be used for relocatable code to find the address
                    where it is located).
    2006            Routine that when called, executes a JP (HL); can be used
                    to emulate the non-existing instruction CALL (HL).
    
    Starting at 2007 there's a table of jumps:
    
    2007            GetMCUVersion
                    Input: nothing
                    Sets B to 0 and C to the MCU version; trashes A.
                    The version has the same format as the byte at 2004h.
    200A            WaitClkDiff
                    Input: C bit 7 = bit to compare against the clock
                    Output: A trashed (bit 7 = opposite of bit 7 of C)
                    Waits for the clock bit to be different from bit 7 of C.
    200D            WaitClkEq
                    Input: C bit 7 = bit to compare against the clock
                    Like WaitClkDiff but waits for the clock to be equal to
                    bit 7 of C instead.
    2010            OutWaitDiff
                    Input: A = byte to send; C bit 7 = bit to compare against
                    the clock
                    Like WaitClkDiff, but sends A to the data port before
                    waiting.
    2013            OutWaitEq
                    Input: A = byte to send; C bit 7 = bit to compare against
                    the clock
                    Like OutWaitDiff but waits for clock to be equal.
    2016            WaitDiffBrk
                    Like WaitClkDiff but allows pressing BREAK to interrupt.
                    Note: Resets the MCU if BREAK pressed.
                    Trashes A, C; does not return if BREAK pressed.
    2019            WaitEqBrk
                    Like WaitDiffBrk but waits for clock to be equal.
    201C            SendString
                    Waits for clock to change, then sends a string to the MCU
                    prefixed by the length (8 bits).
                    Input: C bit 7 = inverse of current clock; B = length;
                    DE = address of string. Trashes A, B and DE; returns with
                    C bit 7 = current clock. If B = 0, only the length is sent.
    201F            SendStrLoop
                    Routine used by SendString, but useful on its own.
                    Sends B bytes to the MCU starting at DE, without waiting
                    for any clock change. B = 0 means send 256 bytes.
                    Input: C bit 7 = current clock; rest of registers same as
                    SendString.
    2022            ReportStatus
                    Reads a byte from the MCU. If it is 0, it does nothing
                    else; otherwise it reports a BASIC error, where value 1
                    reports error G, value 2 = error H and so on.
                    Input: C bit 7 = current clock; trashes A.
    2025            PrintBPaged
                    Tries to print a character, but if there's no room for it,
                    rather than give error 5 it shows an ellipsis "..." at the
                    bottom line and waits for a key. Pressing SPACE breaks;
                    any other key causes the screen to be cleared before
                    printing the character.
                    Input: B = character to print (must be between 0 and 63 or
                    between 128 and 191; other characters may cause a crash).
                    Trashes A, BC, DE, HL.
    2028            Cmd64C
                    Switches to normal 64 character mode. Same as `LOAD *64C`.
                    Trashes A and C.
    202B            Cmd128C
                    Switches to 128 character mode. Same as `LOAD *128C`.
                    Trashes A and C.
    202E            GetPhase
                    Gets the current clock status in bit 7 of C. Rest of bits
                    of C are undefined on return. Flags are modified.
    2031            GetData
                    Reads the port of incoming data from the MCU. Result in A.
                    Flags not affected.
    2034            SD81_RESET
                    Entry point for a RESET. On entry, NMI must be disabled
                    with OUT ($FD),A, maskable interrupts must be disabled via
                    DI, and HL must point to the address where to continue
                    after the initialization of the interface is finished. It
                    jumps to HL with BC=7FFFh. It does not use the stack or
                    any RAM at all.
    2037            SD81LOADCMD
                    Entry point for a modified ZX81 BASIC ROM to execute a
                    LOAD command with all the features mentioned above.
    203A            SD81SAVECMD
                    Entry point for a modified ZX81 BASIC ROM to execute a
                    SAVE command with all the features mentioned above.
    203D            SD81RUNCMD
                    Entry point for a modified ZX81 BASIC ROM to execute a
                    RUN command with all the features mentioned above.


## ERROR CODES

See [SD81 Booster TECHNICAL DOCUMENTATION](DOC/SD81%20Booster%20TECHNICAL%20DOCUMENTATION.md)
