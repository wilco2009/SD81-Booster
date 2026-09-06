------------------------------------------------------------------------

![](media/image1.png){width="4.375in" height="3.125in"}

**User Manual**

*Expansion interface for Sinclair ZX81*

**SD · AY Sound · Speech · 512KB RAM · RTC**

Version 1.0

*Open hardware and open source software*

**Table of Contents**

[Quick Start Guide](#quick-start-guide)

[1. Introduction](#introduction)

[2. Hardware Description](#hardware-description)

[2.1 Right Side Panel](#right-side-panel)

[2.2 Left Side Panel and Rear Panel](#left-side-panel-and-rear-panel)

[2.3 Top Panel --- Status LEDs](#top-panel-status-leds)

[2.4 STAT LED Status Table](#stat-led-status-table)

[3. Box Contents](#box-contents)

[4. Installation](#installation)

[4.1 Before Connecting the Interface](#before-connecting-the-interface)

[4.2 Connection](#connection)

[4.3 Installation Check](#installation-check)

[5. microSD Card Preparation](#microsd-card-preparation)

[5.1 Card Format](#card-format)

[5.2 Permitted Characters in Filenames](#permitted-characters-in-filenames)

[5.3 Recommended Folder Structure](#recommended-folder-structure)

[5.4 The AUTOEXEC Program](#the-autoexec-program)

[6. Getting Started](#getting-started)

[6.1 Load Modes: SD or Tape](#load-modes-sd-or-tape)

[6.2 Loading Your First Program from SD](#loading-your-first-program-from-sd)

[7. Loading and Saving from SD](#loading-and-saving-from-sd)

[7.1 Load a Program](#load-a-program)

[7.2 Save a Program](#save-a-program)

[7.3 Load and Save Memory Blocks (Machine Code)](#load-and-save-memory-blocks-machine-code)

[7.4 Always Load from Tape (Regardless of Mode)](#always-load-from-tape-regardless-of-mode)

[7.5 Game Compatibility Notes](#game-compatibility-notes)

[7.6 Recognised File Formats](#recognised-file-formats)

[8. File and Directory Management](#file-and-directory-management)

[8.1 View SD Contents](#view-sd-contents)

[8.2 Change Directory](#change-directory)

[8.3 Create and Delete Folders](#create-and-delete-folders)

[8.4 Delete, Rename and Copy Files](#delete-rename-and-copy-files)

[8.5 Free Space on SD](#free-space-on-sd)

[8.6 T81 Directories (feature in alpha stage)](#t81-directories-feature-in-alpha-stage)

[9. Additional Features](#additional-features)

[9.1 WAV Audio File Playback](#wav-audio-file-playback)

[9.2 Real Time Clock --- RTC Command](#real-time-clock-rtc-command)

[9.3 RTC Battery Status --- BAT Command](#rtc-battery-status-bat-command)

[9.4 Extended RAM Mode --- RAM48 Command](#extended-ram-mode-ram48-command)

[9.5 Text File Display --- THEN PRINT Command](#text-file-display-then-print-command)

[Sistema de ayuda integrado](#_Toc229078001)

[9.6 Programmable Joystick --- JOY Command](#programmable-joystick-joy-command)

[10. Sound](#sound)

[10.1 PLAY Command --- Music with the AY chip](#play-command-music-with-the-ay-chip)

[Notas](#_Toc229078005)

[Duration](#_Toc229078006)

[Tempo](#_Toc229078007)

[Octava](#_Toc229078008)

[Repeticiones](#_Toc229078009)

[Efectos de volumen (envolvente)](#_Toc229078010)

[10.2 VGM Player --- Background Music](#vgm-player-background-music)

[10.3 PEG Effects Generator --- Programmable Sound Effects](#peg-effects-generator-programmable-sound-effects)

[10.4 Speech Synthesis --- SAY Command](#speech-synthesis-say-command)

[Background Playback](#background-playback)

[How the synthesiser works](#how-the-synthesiser-works)

[11. Memory Management](#memory-management)

[11.1 Basic Concepts: Blocks and Pages](#basic-concepts-blocks-and-pages)

[11.2 MAP Command --- Assign pages to blocks](#map-command-assign-pages-to-blocks)

[11.3 MC45 Mode --- Machine Code in Blocks 4 and 5](#mc45-mode-machine-code-in-blocks-4-and-5)

[11.4 Boot with Alternative ROM](#boot-with-alternative-rom)

[11.5 User-Definable Characters (128C / 64C / 256C)](#user-definable-characters-128c-64c-256c)

[Example: Spectrum-style start screen](#example-spectrum-style-start-screen)

[12. Advanced BASIC Extensions](#advanced-basic-extensions)

[12.1 String Manipulation](#string-manipulation)

[12.2 Memory Block Copy and Fill](#memory-block-copy-and-fill)

[12.3 Machine Code Execution](#machine-code-execution)

[12.4 Input/Output Port Access](#inputoutput-port-access)

[12.5 16-bit Memory Access](#bit-memory-access)

[12.6 Directory Access from a Program](#directory-access-from-a-program)

[13. Program Examples](#program-examples)

[13.1 Real Time Clock (RTC)](#real-time-clock-rtc)

[13.2 RTC Battery Status (BAT)](#rtc-battery-status-bat)

[13.3 MC45 Mode Check](#mc45-mode-check)

[13.4 Superfast Mode --- Speed Demonstration](#superfast-mode-speed-demonstration)

[13.5 Spectrum Image Loading](#spectrum-image-loading)

[14. Error Codes](#error-codes)

[15. For Programmers](#for-programmers)

[15.1 Expansion ROM Map](#expansion-rom-map)

[15.2 I/O Ports and MCU Protocol](#io-ports-and-mcu-protocol)

[VSYNC Synchronisation](#vsync-synchronisation)

[15.3 Memory Mapper (port E7h)](#memory-mapper-port-e7h)

[15.4 Debug Console (USB-C Port)](#debug-console-usb-c-port)

[15.5 Complete MCU Command Table](#complete-mcu-command-table)

[Error codes returned by commands](#error-codes-returned-by-commands)

[System commands](#system-commands)

[Filesystem commands](#filesystem-commands)

[Hardware control commands](#hardware-control-commands)

[Speech synthesis commands](#speech-synthesis-commands)

[AY / sound commands](#ay-sound-commands)

[VGM commands](#vgm-commands)

[PEG commands](#peg-commands)

[RTC and battery commands](#rtc-and-battery-commands)

[16. Troubleshooting](#troubleshooting)

[16.1 Interface Does Not Boot or ZX81 Freezes](#interface-does-not-boot-or-zx81-freezes)

[16.2 microSD Card Problems](#microsd-card-problems)

[16.3 Clock Loses Time on Power Off](#clock-loses-time-on-power-off)

[16.4 Joystick Does Not Respond](#joystick-does-not-respond)

[16.5 Sound Does Not Work](#sound-does-not-work)

[16.6 Firmware Update Errors](#firmware-update-errors)

[16.7 Using the Debug Console as a Diagnostic Tool](#using-the-debug-console-as-a-diagnostic-tool)

[17. Firmware Update](#firmware-update)

[17.1 Microcontroller (MCU) Update](#microcontroller-mcu-update)

[17.2 FPGA Update](#fpga-update)

[18. Glossary](#glossary)

[19. Firmware Version History](#firmware-version-history)

[20. References](#references)

[The SD81 Booster Project](#the-sd81-booster-project)

[Tools](#tools)

[Technical Reference Documentation](#technical-reference-documentation)

[ZX81 ROM](#zx81-rom)

[Project Credits](#project-credits)

[Appendix A --- Complete PLAY Command Reference](#appendix-a-complete-play-command-reference)

[Duration Table](#duration-table)

[Efectos de envolvente (W)](#envelope-table-w)

[Parameter Summary Table](#_Toc229078075)

[Appendix B --- PEG Effects Generator Reference](#appendix-b-peg-effects-generator-reference)

[Appendix C --- Speech Synthesiser Dictionary](#appendix-c-speech-synthesiser-dictionary)

[Recognised Words (selection by length)](#recognised-words-selection-by-length)

[Punctuation and Pauses](#punctuation-and-pauses)

[Direct Allophones (advanced use)](#direct-allophones-advanced-use)

[Appendix D --- Memory Paging System](#appendix-d-memory-paging-system)

[Initial Page Assignment](#initial-page-assignment)

[Reglas de uso](#_Toc229078083)

[ROM Modification](#rom-modification)

[Appendix E --- Chroma81 Interface Port (7FEFh)](#appendix-e-chroma81-interface-port-7fefh)

[Escritura (OUT 7FEFh)](#writing-out-7fefh)

[Border colour format (bits 2-0, GRB format)](#border-colour-format-bits-2-0-grb-format)

[Lectura (IN 7FEFh)](#reading-in-7fefh)

[Appendix F --- Superfast and Spectrum Modes](#appendix-f-superfast-and-spectrum-modes)

[The Video Problem in the Original ZX81](#the-video-problem-in-the-original-zx81)

[Superfast Mode](#superfast-mode)

[Spectrum Mode](#spectrum-mode)

[Border control](#_Toc229078093)

[VSYNC Synchronisation](#vsync-synchronisation-1)

[Resumen de POKEs de control](#control-pokes-summary)

[Appendix G --- Audio Technical Reference: AY chip, VGM and allophones](#appendix-g-audio-technical-reference-ay-chip-vgm-and-allophones)

[AY-3-8910/12 Chip Registers](#ay-3-891012-chip-registers)

[Opcodes del reproductor VGM](#vgm-player-opcodes)

[SP0256-AL2 Allophone Table](#sp0256-al2-allophone-table)

*When opening the document, right-click on the table of contents and select \'Update Field\' to see page numbers.*

# Quick Start Guide

If you have just opened the box and want to get started as quickly as possible, follow these five steps:

**1. Prepare the microSD card**

- Format a microSD card in FAT32.

- Copy the SYS folder and all its contents to the root of the card. Without this folder the interface will not boot.

- Copy your .P files to the card, organised in folders if you wish.

**2. Connect the interface**

- Turn off the ZX81.

- Connect the SD81 Booster to the rear expansion port of the ZX81.

- Insert the microSD card into the interface slot.

**3. Power on and check**

- Turn on the ZX81. It should boot normally showing the K cursor.

- The STAT LED on the top panel should light up solid green.

**4. Load your first program**

> LOAD FAST \"NAME\"

Replace NAME with the filename without the .P extension.

**5. Want to explore what\'s on the SD?**

> LOAD \*DIR

| **ℹ** | *For more details on any of these steps, refer to the corresponding sections of the manual.* |
|------|------------------------------------------------------------------|

# 1. Introduction

The SD81 Booster is an open hardware expansion interface for the Sinclair ZX81 that considerably extends the computer\'s original capabilities. Its main features include:

- Loading and saving from microSD card in .P format, the same used by the most popular emulators.

- Up to 512 KB of RAM via a memory mapper in 8 KB blocks.

- AY-3-8910/12 sound chip emulation with support for the PLAY command.

- VGM file player running in the background.

- Speech synthesis with audio samples stored in the microcontroller\'s internal memory.

- Up to 128 user-definable characters, with compatibility with the QuickSilva interface character definition mode.

- Chroma81 interface compatibility, providing RGB colour video output for the ZX81.

- Full compatibility with the original tape routines and the ZX Printer.

# 2. Hardware Description

The SD81 Booster is a compact enclosure that connects to the ZX81\'s rear expansion port. On its exterior you will find all the connectors and controls needed to make use of its features.

| **ℹ** | *The images in this section are provisional 3D design renders and will be replaced with photographs of the final product before launch.* |
|------|------------------------------------------------------------------|

## 2.1 Right Side Panel

![](media/image2.png){width="5.0in" height="3.125in"}

*Right side view: RESET, QSILVA, MICRO-SD and USB*

The right side panel presents, from left to right:

- RESET button: resets the entire system (ZX81 + interface).

- QSILVA button: toggles between the QuickSilva interface character set and the standard ZX81 ROM character set. Each press switches between one and the other.

- MICRO-SD slot: insert the microSD card here with your programs, ROMs and system data.

- USB-C port: has two uses. As a firmware update port in case USB recovery is needed (see section 17). As a debug console: when connected to a computer a serial port associated with the CH340 chip appears (may require drivers). With a serial terminal program it is possible to monitor the interface\'s status and error messages in real time.

## 2.2 Left Side Panel and Rear Panel

![](media/image3.png){width="5.0in" height="2.5520833333333335in"}

*Left side view: JOYSTICK DB9 connector*

- JOYSTICK connector (DB9): joystick port compatible with standard 9-pin joysticks (Atari/Commodore type). The mapping of joystick buttons to ZX81 keys is fully programmable via the LOAD \*JOY command (see section 9.6).

![](media/image4.png){width="5.0in" height="3.1354166666666665in"}

*Rear view: RGB SCART connector*

- RGB connector (SCART): RGB colour video output compatible with the Chroma81 interface. Allows the ZX81 to be connected to monitors and televisions with SCART input for high quality colour image.

## 2.3 Top Panel --- Status LEDs

![](media/image5.png){width="3.75in" height="3.59375in"}

*Top view: STAT and SD LEDs*

The top panel incorporates two indicator lights:

- STAT LED (status): indicates the general state of the interface through different colours and blink patterns (see table in section 2.4).

- SD LED (card access): blinks during read or write operations on the microSD card.

## 2.4 STAT LED Status Table

The STAT LED indicates the state of the interface through colour and blink combinations, organised in three phases:

<table style="width:100%;">
<colgroup>
<col style="width: 34%" />
<col style="width: 65%" />
</colgroup>
<thead>
<tr>
<th style="text-align: center;"><strong>Colour / Pattern</strong></th>
<th style="text-align: center;"><strong>Meaning</strong></th>
</tr>
</thead>
<tbody>
<tr>
<td colspan="2" style="text-align: center;"><strong>Firmware update</strong></td>
</tr>
<tr>
<td style="text-align: center;"><strong>Blue/Red blink</strong></td>
<td style="text-align: center;">Error initialising SD card</td>
</tr>
<tr>
<td style="text-align: center;"><strong>Solid yellow</strong></td>
<td style="text-align: center;">Updating firmware</td>
</tr>
<tr>
<td style="text-align: center;"><strong>White/Red blink</strong></td>
<td style="text-align: center;">Update error</td>
</tr>
<tr>
<td colspan="2" style="text-align: center;"><strong>Boot</strong></td>
</tr>
<tr>
<td style="text-align: center;"><strong>Red blinking</strong></td>
<td style="text-align: center;">Initialising serial port</td>
</tr>
<tr>
<td style="text-align: center;"><strong>Orange blinking</strong></td>
<td style="text-align: center;">Waiting for FPGA</td>
</tr>
<tr>
<td style="text-align: center;"><strong>Blue/Red blink</strong></td>
<td style="text-align: center;">Error initialising SD card</td>
</tr>
<tr>
<td style="text-align: center;"><strong>Orange/Red blink</strong></td>
<td style="text-align: center;">Error writing ROM to RAM</td>
</tr>
<tr>
<td style="text-align: center;"><strong>Yellow blink</strong></td>
<td style="text-align: center;">Initialising RTC</td>
</tr>
<tr>
<td style="text-align: center;"><strong>Solid green</strong></td>
<td style="text-align: center;">System initialised successfully</td>
</tr>
<tr>
<td colspan="2" style="text-align: center;"><strong>Operation</strong></td>
</tr>
<tr>
<td style="text-align: center;"><strong>Solid green</strong></td>
<td style="text-align: center;">Interface ready, Quick Silva mode off</td>
</tr>
<tr>
<td style="text-align: center;"><strong>Solid cyan</strong></td>
<td style="text-align: center;">Interface ready, Quick Silva mode on</td>
</tr>
</tbody>
</table>

# 3. Box Contents

When you open the SD81 Booster box you will find:

- 1× SD81 Booster interface

- 1× microSD card (pre-formatted)

- 1× CR2032 button battery (pre-installed in the interface)

- This user manual

| **⚠** | *If any of these items are missing or damaged, contact the seller before connecting the interface.* |
|------|------------------------------------------------------------------|

# 4. Installation

### 4.1 Before Connecting the Interface

- Make sure the ZX81 is powered off before connecting or disconnecting the interface.

- The SD81 Booster connects to the ZX81\'s rear expansion port.

- The interface is not compatible with devices that replace the ZX81\'s internal ROM.

### 4.2 Connection

1.  Turn off the ZX81.

2.  Align the SD81 Booster connector with the ZX81\'s rear expansion port. Make sure the pins are correctly aligned.

3.  Push gently until the connector is fully inserted. Do not force the connection.

4.  Insert the microSD card into the interface slot (see section 5).

5.  Turn on the ZX81.

| **⚠** | *Connecting or disconnecting the interface while the ZX81 is powered on may damage both the interface and the computer.* |
|------|------------------------------------------------------------------|

### 4.3 Installation Check

When powering on the ZX81 with the SD81 Booster correctly installed, the computer should boot normally showing the usual K cursor. The interface does not modify the system boot process.

To verify the interface is working, type the following command in BASIC and press ENTER:

> LOAD \*VER

| **ℹ** | *The asterisk \* is obtained by pressing SHIFT + B. The interface will display the installed firmware version.* |
|------|------------------------------------------------------------------|

If the computer freezes or does not boot correctly, disconnect the interface and make sure the connector is properly aligned.

# 5. microSD Card Preparation

### 5.1 Card Format

The SD81 Booster requires a microSD card formatted in FAT32. The card included in the box is already correctly formatted and prepared.

If you use your own card, follow these steps:

6.  Format the card in FAT32: on Windows, right-click on the card → Format → FAT32. On macOS or Linux, use a disk formatting tool and select FAT32.

7.  Copy the SYS folder and all its contents (included in the interface software package) to the root of the SD card. This folder contains essential files for the interface to operate.

| **⚠** | *The interface does not support exFAT or NTFS format. The SYS folder is essential: it contains the ROM files needed for the interface to boot. Without it, the interface will not work.* |
|------|------------------------------------------------------------------|

### 5.2 Permitted Characters in Filenames

Due to the ZX81 keyboard limitations, only the following characters may be used in file and folder names:

- Letters: A to Z (always uppercase when saving)

- Numbers: 0 to 9

- Symbols: . , ; \$ ( ) = + -

- The / character is used as a directory separator and cannot be used in filenames.

| **💡** | *Avoid ending a filename with a space or a period, as some operating systems may have trouble reading that file from a computer.* |
|------|------------------------------------------------------------------|

### 5.3 Recommended Folder Structure

The interface will search the SD card root by default. You can organise your programs in subfolders. The following structure is recommended:

> /
>
> ├── AUTOEXEC.P ← Program that loads at boot
>
> ├── JUEGOS/
>
> │ ├── MANIC.P
>
> │ └── PACMAN.P
>
> ├── DEMOS/
>
> └── SYS/ ← System folder (mandatory, do not modify)

### 5.4 The AUTOEXEC Program

If a file called AUTOEXEC.P exists in the SD root, it will be automatically loaded and executed when the RUN command is typed on a ZX81 with no program loaded. This is useful for creating custom startup menus.

# 6. Getting Started

### 6.1 Load Modes: SD or Tape

By default, the BASIC LOAD and SAVE commands work with tape, exactly as on a ZX81 without an interface. This ensures full compatibility with original tape software.

To use the SD card, you have two options:

**Option A --- Activate SD mode permanently:** Type in BASIC:

> LOAD FAST

| **ℹ** | *The word FAST is obtained with SHIFT + F, not by typing the letters one by one. From that point on all LOAD and SAVE commands will use the SD by default. If you need to load from tape while in SD mode, use LOAD SLOW \"name\" to force audio loading for that specific operation.* |
|------|------------------------------------------------------------------|

To return to tape mode by default:

> LOAD SLOW

| **ℹ** | *SLOW is obtained with SHIFT + D. From that point on all LOAD and SAVE commands will use tape by default, and FAST must be explicitly added to load from SD.* |
|------|------------------------------------------------------------------|

**Option B --- Load from SD without changing mode:** Add FAST directly to the load command (see section 7).

### 6.2 Loading Your First Program from SD

Once SD mode is active (or using LOAD FAST), loading a program is straightforward. Replace NAME with the name of your file (without the .P extension):

> LOAD FAST \"NAME\"

Press ENTER. The program will load in a few moments and will start automatically if it has auto-run.

| **💡** | *If you do not remember the exact filename, you can view the SD contents with the command LOAD \*DIR (see section 8).* |
|------|------------------------------------------------------------------|

# 7. Loading and Saving from SD

### 7.1 Load a Program

To load a program from the SD card:

> LOAD FAST \"NAME\"

The interface will first try the exact filename. If not found, it will automatically try adding the .P extension.

To load from a subfolder:

> LOAD FAST \"JUEGOS/PACMAN\"

To load and run from a specific line number:

> LOAD FAST \"NAME\" THEN GOTO 100

To load without auto-running:

> LOAD FAST \"NAME\" THEN STOP

| **ℹ** | *THEN, GOTO y STOP are ZX81 BASIC tokens, not written letter by letter.* |
|------|------------------------------------------------------------------|

### 7.2 Save a Program

To save a program to the SD card:

> SAVE FAST \"NAME\"

The interface always appends the .P extension to the saved file.

| **⚠** | *If a file with that name already exists, it will be overwritten without warning.* |
|------|------------------------------------------------------------------|

### 7.3 Load and Save Memory Blocks (Machine Code)

To load a block of data to a specific memory address:

> LOAD FAST \"NAME\" CODE 30000

| **ℹ** | *Unlike the ZX Spectrum, the address after CODE is mandatory. Files on SD do not store the load address in a header.* |
|------|------------------------------------------------------------------|

| **⚠** | *The FAST token is mandatory for CODE to work correctly. If LOAD \"NAME\" CODE 30000 is used without FAST (even with SD mode active), the file will be loaded at the default address 16393 (4009h), ignoring the specified address. This is a firmware compatibility limitation.* |
|------|------------------------------------------------------------------|

To save a memory block to SD:

> SAVE FAST \"NAME\" CODE 30000,2048

Where 30000 is the start address and 2048 is the length in bytes.

### 7.4 Always Load from Tape (Regardless of Mode)

If SD mode is active but you want to load a specific file from tape:

> LOAD SLOW \"NAME\"

### 7.5 Game Compatibility Notes

Some games require special initialisation before loading. The most common cases are:

- **LOAD \*128C** before loading: the game uses user-definable characters mode.

- **LOAD FAST** before loading: some games have multiple files. Running LOAD FAST without a name activates SD mode for all subsequent LOAD and SAVE commands, allowing the game to load its additional files automatically.

### 7.6 Recognised File Formats

The LOAD FAST command automatically detects the file type by extension and behaves differently accordingly:

| **Extension** | **Comportamiento** |
|-----------|-------------------------------------------------------------|
| **.P** | Standard ZX81 BASIC program. The interface calculates the real program size from the system variables and discards trailing bytes at the end of the file. |
| **.81** | Same as .P. |
| **.P81** | Multi-program format. The interface skips the embedded filename before reading the data. |
| **.ROM** | ROM file. The interface loads the content at address 0 of the address space and resets the system. Control does not return to BASIC. |
| **.WAV** | Uncompressed audio file (PCM). The interface plays it directly instead of loading it into memory. |
| **Otras** | The file is loaded entirely into memory as-is, without any processing. |

| **⚠** | *Loading a file with the .ROM extension via LOAD FAST causes an immediate system reset. Make sure the file contains a valid ROM before loading it, as a corrupted file could leave the system in an unrecoverable state until it is rebooted with another ROM.* |
|------|------------------------------------------------------------------|

# 8. File and Directory Management

### 8.1 View SD Contents

To list the files in the current directory:

> LOAD \*DIR

To list the files in a specific folder:

> LOAD \*DIR \"JUEGOS\"

Wildcards in Unix style can be used:

- \* matches any number of characters (including none).

- ? matches exactly one character.

| **ℹ** | *\* matches all files regardless of whether they have an extension or not. In contrast, \*.\* only matches files that contain a dot in their name. To list all files, use \*, not \*.\*.* |
|------|------------------------------------------------------------------|

For example, to list only .P files:

> LOAD \*DIR \"\*.P\"

| **ℹ** | *If the listing does not fit on screen, \... will appear on the bottom line. Press any key to continue, or SPACE to cancel.* |
|------|------------------------------------------------------------------|

### 8.2 Change Directory

To change to a subdirectory:

> LOAD \*CD \"JUEGOS\"

To go back to the root:

> LOAD \*CD \"/\"

View the current directory:

> LOAD \*PWD

### 8.3 Create and Delete Folders

To create a subdirectory in the current directory:

> LOAD \*MD \"NEWFOLDER\"

To remove an empty folder:

> LOAD \*RD \"EMPTYFOLDER\"

### 8.4 Delete, Rename and Copy Files

To delete a file:

> LOAD \*DEL \"FILE.P\"

To rename or move a file:

> LOAD \*MV \"OLD.P\" TO \"NEW.P\"

To copy a file:

> LOAD \*CP \"SOURCE.P\" TO \"DEST.P\"

| **ℹ** | *TO is a BASIC token (SHIFT + 4), not typed letter by letter.* |
|:----:|------------------------------------------------------------------|
| **ℹ** | *The date and time of the destination file is not preserved; the copied file will have the date and time of the moment of copying.* |

### 8.5 Free Space on SD

> LOAD \*FREE

| **ℹ** | *This calculation may take several seconds to complete.* |
|-------|----------------------------------------------------------|

### 8.6 T81 Directories (feature in alpha stage)

| **⚠** | *This feature is currently in alpha stage. It may contain bugs and its behaviour or interface could change in future versions. Not recommended for production use.* |
|------|------------------------------------------------------------------|

The SD81 Booster supports a special file format with the .T81 extension that acts as a container for multiple ZX81 programs, similar to how a ZIP file contains several files. This allows distributing program collections in a single file.

To access the contents of a T81 file, use the LOAD \*CD command as if it were a normal directory:

> LOAD \*CD \"COLECCION.T81\"

From that point on, LOAD, LOAD \*DIR and LOAD \*OPENDIR commands operate on the T81 file contents instead of the FAT filesystem, allowing navigation and loading of programs in the same way as normal files.

To exit the T81 directory and return to the normal filesystem:

> LOAD \*CD \"/\"

| **ℹ** | *Alpha stage limitations: Write operations (LOAD \*DEL, LOAD \*MD, LOAD \*RD, LOAD \*MV, LOAD \*CP, SAVE) are not supported inside a T81 directory and will return an error.* |
|------|------------------------------------------------------------------|

# 9. Additional Features

## 9.1 WAV Audio File Playback

The interface can play uncompressed WAV audio files directly from the SD card simply by loading them with the usual command:

> LOAD FAST \"SOUND.WAV\"

If the file has the .WAV extension, the interface detects it automatically and plays it instead of trying to load it as a program. No special command is needed.

| **ℹ** | *Only uncompressed WAV audio files (PCM) are supported. Other audio formats are not compatible.* |
|------|------------------------------------------------------------------|

## 9.2 Real Time Clock --- RTC Command

The SD81 Booster incorporates a real time clock (RTC) with backup battery. The LOAD \*RTC command allows checking and setting the time and date from BASIC.

**Read the current date and time:**

> LOAD \*RTC

**Store the value in a string variable:**

> LOAD \*RTC TO R\$

**Set the date and time:**

> LOAD \*RTC=\"date/time\"

Six input formats are supported:

| **Format** | **Example** | **Description** |
|-----------------------|-----------------------|--------------------------|
| YYYY-MM-DD HH:MM:SS.CC | 2025-04-30 18:30:00.00 | Full date and time with hundredths |
| YYYY-MM-DD HH:MM:SS | 2025-04-30 18:30:00 | Full date and time |
| YYYY-MM-DD | 2025-04-30 | Date only |
| HH:MM:SS.CC | 18:30:00.00 | Time only with hundredths |
| HH:MM:SS | 18:30:00 | Time with seconds |
| HH:MM | 18:30 | Time (hours and minutes) |

**Examples:**

> LOAD \*RTC=\"2025-04-30 18:30:00\"
>
> LOAD \*RTC=\"2025-04-30\"
>
> LOAD \*RTC=\"18:30:00\"

**Example in a BASIC program:**

> 10 LOAD \*RTC TO R\$
>
> 20 PRINT \"Date and time: \";R\$

## 9.3 RTC Battery Status --- BAT Command

The SD81 Booster incorporates a CR2032 button battery that keeps the clock running when the ZX81 is powered off. This battery comes pre-installed from the factory and has an estimated life of several years under normal use conditions. When it runs out, it can be replaced by any standard CR2032 battery available from electronics shops.

The battery charge level can be checked from BASIC with the LOAD \*BAT command.

**Display battery status on screen:**

> LOAD \*BAT

**Store the status in a string variable:**

> LOAD \*BAT TO B\$

| **💡** | *If the clock frequently loses time when the computer is powered off, check the battery status with this command to see if it needs replacing.* |
|------|------------------------------------------------------------------|

## 9.4 Extended RAM Mode --- RAM48 Command

The LOAD \*RAM48 command activates extended RAM mode of 48 KB, which extends the memory available for BASIC programs and data beyond the usual limits.

**Activate extended RAM mode:**

> LOAD \*RAM48

**Deactivate to restore standard compatibility:**

> LOAD \*RAM48 STOP

| **ℹ** | *STOP is the BASIC token, not typed letter by letter. If any program has compatibility issues with RAM48 mode active, deactivate it with LOAD \*RAM48 STOP before loading it.* |
|------|------------------------------------------------------------------|

## 9.5 Text File Display --- THEN PRINT Command

The LOAD THEN PRINT command is the equivalent of the MS-DOS TYPE command or Linux cat: it displays the content of a text file directly on the ZX81 screen.

**Display a text file on screen:**

> LOAD THEN PRINT \"FILE\"

**Send the content to the ZX Printer:**

> LOAD THEN LPRINT \"FILE\"

**Redirect other command output to the ZX Printer:**

The LOAD LPRINT prefix can also be used with commands that normally display text on screen, to redirect their output directly to the ZX Printer:

> LOAD LPRINT DIR
>
> LOAD LPRINT DIR \"\*.P\"
>
> LOAD LPRINT FREE
>
> LOAD LPRINT PWD
>
> LOAD LPRINT VER

### Built-in Help System

If an asterisk \* is prepended to the filename, the interface automatically searches for a file with that name in the /MAN/ folder on the SD and appends the .TXT extension. This allows implementing a help system similar to the Linux man command:

> LOAD THEN PRINT \"\*PLAY\"

This command would look for the file /MAN/PLAY.TXT on the SD and display its content on screen. You can create your own help files in that folder to document your programs or commands.

**Example --- display game instructions:**

> 10 LOAD THEN PRINT \"\*INSTRUCTIONS\"

This would display the content of /MAN/INSTRUCTIONS.TXT, ideal for showing the instructions of a game or program from within the BASIC program itself.

## 9.6 Programmable Joystick --- JOY Command

The SD81 Booster incorporates a DB9 joystick port whose button mapping is fully configurable. The LOAD \*JOY command allows assigning a ZX81 key to each direction and the fire button of the joystick.

**Syntax:**

> LOAD \*JOY \"up/down/left/right/fire\"

The configuration string contains exactly five characters, one for each joystick function in this order: up / down / left / right / fire.

**Example:**

> LOAD \*JOY \"QAOP \"

- Up → key Q

- Down → key A

- Left → key O

- Right → key P

- Fire → space key

| **💡** | *Check the controls of each game before configuring the joystick. Many ZX81 games use different key combinations, and with this command you can map them to any standard 9-pin joystick without modifying the software.* |
|------|------------------------------------------------------------------|

## 9.7 WiFi Module (optional)

The WiFi module is an optional accessory based on an ESP32-C3 microcontroller that connects to the SD81 Booster and adds a file server accessible over WiFi: it lets you list, upload, download, delete and organise the content of the microSD card from the browser of a phone, tablet or computer, without removing the card from the interface.

|  |  |
|:----:|------------------------------------------------------------------|
| **ℹ** | *The WiFi module is not included by default with the SD81 Booster - it is an optional accessory installed separately. If your interface does not have it installed, this section does not apply.* |

**WiFi network configuration:**

For the module to connect to your network, create a text file named WIFI.CFG inside the SYS folder of the microSD card, with the network name on one line and the password on the next:

> MyWiFiNetwork
>
> MyPassword

You can add more than one network by repeating the same pattern (name, then password), one after another in the same file:

> HomeWiFi
>
> homepassword
>
> WorkWiFi
>
> workpassword

The module tries the networks in the order they appear, until it connects to the first one available - useful so you do not have to edit the file every time you move to a different location (up to 4 networks).

|  |  |
|:----:|------------------------------------------------------------------|
| **💡** | *Save the file as plain text, with no formatting. Networks must be 2.4GHz - the WiFi module is not compatible with 5GHz networks.* |

**Accessing the file server:**

With the module connected to your network, you need its address to reach it from a browser. There are several ways to find it, from most to least convenient:

**From the ZX81 itself (simplest, no other device needed):**

> LOAD THEN PRINT \"\*IP\"

Displays the module current IP address on screen.

**By name, without typing the IP:**

If your device supports mDNS (built in on macOS, iOS, Android and Linux), go directly to http://sd81booster.local

|  |  |
|:----:|------------------------------------------------------------------|
| **ℹ** | *On Windows, if you do not have any Apple software installed, the browser may not resolve \".local\" addresses. This is not required for anything else - you can just use LOAD THEN PRINT \"\*IP\" or the direct IP address instead.* |

**Alternative: router panel:**

You can also find the assigned IP address in the list of connected devices in your router administration panel (look for \"esp32\" or similar).

**Available functions:**

- List and browse the SD card content by folder.

- Upload files, including several at once or a whole folder (in desktop browsers).

- Download any file to the device you are browsing from.

- Delete files or folders.

- Create new folders.

**NTP time synchronization:**

The WiFi module can keep the interface\'s clock accurate by automatically syncing it with a time server (NTP) as soon as it connects to the network - useful if the RTC battery runs low or simply to avoid setting the date by hand.

To enable it, create a text file called NTP.CFG in the SYS folder of the microSD card:

> SERVER=pool.ntp.org
>
> MODE=SERVER
>
> UTCOFFSET=1
>
> DST=1

SERVER is the NTP server address. MODE=SERVER enables syncing (use MODE=LOCAL, or don\'t create the file, to keep it disabled). UTCOFFSET is the time difference from UTC, in hours (can be negative). DST adds one extra hour for summer time - it\'s a manual switch you flip yourself twice a year, just like any other clock.

You can also edit this configuration, and force an immediate sync, from the module\'s own web page (the \"NTP time settings\" link on the file server\'s front page).

**From BASIC, without needing the web page:**

- LOAD \*NTP - forces an immediate sync.

- LOAD \*NTP=\"server\" - changes the NTP server.

- LOAD \*NTP +n / LOAD \*NTP -n - sets the UTC offset, in hours.

- LOAD \*SUMMER / LOAD \*SUMMER STOP - turns summer time on or off.

|  |  |
|:----:|------------------------------------------------------------------|
| **ℹ** | *Syncing only happens once, at boot or when connecting to the network - it doesn\'t repeat periodically. If there\'s no network available or the server doesn\'t respond, the clock is left untouched.* |

**Updating the WiFi module firmware:**

The WiFi module can update its own firmware from the SD card, with no need to connect it to a computer. Copy the ESP32_FW.BIN file (available in the project repository) to the root of the SD card - for example by uploading it through the file server itself - and restart the interface.

During the update, the STAT LED blinks pink. If it finishes successfully, it turns solid green. If the process fails, the LED stays solid yellow - try again by copying the file once more.

|  |  |
|:----:|------------------------------------------------------------------|
| **⚠** | *Do not power off the interface or remove the SD card while the STAT LED is blinking pink.* |

**Initial / recovery programming (via USB):**

The module needs to be programmed over USB **the first time** (or if it stops responding and the SD card update above cannot run). After that, all further updates can use the SD card method described above.

This requires the **Arduino IDE** with ESP32 board support installed, selecting board **ESP32C3 Dev Module**. The exact board settings (upload speed, flash size, etc.) are documented in **FIRMWARE/README_update.md** in the project repository.

# 10. Sound

The SD81 Booster incorporates an AY-3-8910/12 sound chip emulator, the same used by computers such as the ZX Spectrum 128K or Amstrad CPC. This allows playing music with up to three independent voices, plus noise and envelope effects.

## 10.1 PLAY Command --- Music with the AY chip

The PLAY command allows playing music directly from BASIC using a text string that describes the notes, duration, tempo and other parameters. It supports up to three simultaneous voices.

**Basic syntax:**

> LOAD \*PLAY \"string1\"
>
> LOAD \*PLAY \"string1\",\"string2\",\"string3\"

Each string corresponds to one voice. Between one and three strings can be used simultaneously.

**Simple example --- melody in one voice:**

> LOAD \*PLAY \"T120O45C5E5G9C\"

This plays the notes C, E, G, and C (C major chord) at 120 beats per minute in octave 4.

| **ℹ** | *For a complete description of all parameters, including envelope effects and noise mode, see Appendix A of this manual.* |
|------|------------------------------------------------------------------|

## 10.2 VGM Player --- Background Music

The SD81 Booster can play VGM (Video Game Music) format files in the background while the ZX81 runs any other program. This allows adding music to your own programs without using any CPU time.

| **ℹ** | *Only VGM files containing data from the AY-3-8910 or AY-3-8912 chip are supported. VGMs with other sound chips are not supported.* |
|------|------------------------------------------------------------------|

**Prepare a VGM file:**

> LOAD \*VGM \"MUSIC\"

Loads the MUSIC.VGM file from the SD and prepares it for playback, without starting it yet.

**Start playback:**

> LOAD \*VGM THEN RUN

**Pause and resume:**

> LOAD \*VGM THEN PAUSE
>
> LOAD \*VGM THEN CONT

**Stop playback:**

> LOAD \*VGM THEN STOP

**Activate loop mode** (music restarts when it ends):

> LOAD \*VGMLOOP

**Disable loop mode:**

> LOAD \*VGMLOOP STOP

| **ℹ** | *THEN, RUN, CONT, PAUSE y STOP they are BASIC tokens of the ZX81, they are not written letter by letter.* |
|------|------------------------------------------------------------------|

**Typical usage example in a BASIC program:**

> 10 LOAD \*VGM \"MUSIC\"
>
> 20 LOAD \*VGMLOOP
>
> 30 LOAD \*VGM THEN RUN
>
> 40 REM \-\-- program continues while music plays in background \-\--

## 10.3 PEG Effects Generator --- Programmable Sound Effects

The PEG (Programmable Effects Generator) is a small virtual machine integrated in the interface that executes sound effect programs completely independently of the Z80, without consuming any ZX81 CPU time.

The PEG accesses the AY chip registers directly and has up to three parallel execution threads, allowing several effects to play simultaneously.

| **ℹ** | *The PEG is primarily aimed at developers. To create PEG programs it is recommended to use the PEG assembler included in the project repository. For a complete description of the instruction set, see Appendix B.* |
|------|------------------------------------------------------------------|

**Loading a PEG Program in Memory:**

> LOAD \*PEG \<address\>,\"\<hexadecimal\>\"

Where \<address\> is the position in PEG memory (0--255) and \<hexadecimal\> is the instruction sequence in hexadecimal format. Each instruction occupies 4 hexadecimal characters (2 bytes).

**Start a PEG thread:**

> LOAD \*PEG THEN RUN \<thread\>,\<address\>

Where \<thread\> is the thread number (0, 1 or 2) and \<address\> is the start address of the PEG program.

**Stop, pause, and resume a thread:**

> LOAD \*PEG THEN STOP \<thread\>
>
> LOAD \*PEG THEN PAUSE \<thread\>
>
> LOAD \*PEG THEN CONT \<thread\>

**Loading a PEG program from the SD (LOAD \*PEB):**

In addition to inline loading with a hexadecimal string, it is possible to load a compiled PEG program directly from a binary file on the SD:

> LOAD \*PEB \<address\>,\"\<name\>\"

Where \<address\> is the start position in PEG memory (0--255) and \<name\> is the filename on the SD. The file must have the .PEB extension (PEG binary); if no extension is specified, the interface adds it automatically.

**Example:**

> LOAD \*PEB 0,\"EFECT\"
>
> LOAD \*PEG THEN RUN 0,0

This example loads the file EFECT.PEB into PEG memory from position 0 and starts thread 0 from that same address.

| **ℹ** | *The .PEB file is the result of assembling a .PEG source with the peg.py assembler, a Python script included in the EXAMPLES/PEG/ folder of the project repository. To compile: python peg.py efect.peg* |
|------|------------------------------------------------------------------|

## 10.4 Speech Synthesis --- SAY Command

The SD81 Booster incorporates a speech synthesiser that can reproduce English phrases directly from BASIC. The synthesiser is based on the phonemes of the SP0256 chip, a speech synthesiser widely used at the time in classic interfaces such as the Currah MicroSpeech for the ZX Spectrum or The Voice for the Videopac G7000/Odyssey 2.

**Syntax:**

> LOAD \*SAY \"STRING\"

The synthesiser analyses the text string and attempts to build the pronunciation by combining English phonemes. The text should be written in English, in uppercase.

**Examples:**

> LOAD \*SAY \"HELLO WORLD\"
>
> LOAD \*SAY \"ZX81 COMPUTER\"
>
> LOAD \*SAY \"ERROR 5\"

Numbers are read in English automatically, from zero to billions.

### Background Playback

By default, the CPU waits for the voice to finish before continuing program execution. If a \* is prepended to the start of the string, playback continues in the background and the ZX81 keeps executing BASIC.

> LOAD \*SAY \"\*HELLO WORLD\"

| **ℹ** | *It is not possible to add custom sounds or phonemes. The available phoneme set is fixed in the microcontroller\'s internal memory.* |
|------|------------------------------------------------------------------|

### How the synthesiser works

The synthesiser decomposes text from left to right, always looking for the longest possible match in its internal dictionary. Recognised words sound better than letter-by-letter phonetic decomposition.

Spaces and punctuation produce pauses of increasing length: a space produces a short pause, a comma a medium pause, and a semicolon or colon a long pause.

| **💡** | *To improve the pronunciation of unrecognised words, write them phonetically in English. For example, SINCLAIR may sound better as SINCLER. See Appendix C for the complete word dictionary and available phonemes.* |
|------|------------------------------------------------------------------|

# 11. Memory Management

The SD81 Booster incorporates up to 512 KB of RAM, far above the original 1 KB of the ZX81 or conventional memory expansions. This memory is managed via a memory mapper that divides both the Z80 address space and the interface RAM into 8 KB blocks and pages.

For most BASIC programs it is not necessary to manage memory manually: the interface configures it automatically on boot and the ZX81 BASIC can use the available RAM directly.

## 11.1 Basic Concepts: Blocks and Pages

The Z80 address space (64 KB) is divided into 8 blocks of 8 KB each:

| **Block** | **Address range** | **Typical use**                        |
|-----------|-------------------|----------------------------------------|
| **0**     | 0000--1FFF        | ZX81 ROM (read-only)                   |
| **1**     | 2000--3FFF        | Interface extension ROM                |
| **2**     | 4000--5FFF        | Main RAM / screen                      |
| **3**     | 6000--7FFF        | Main RAM                               |
| **4**     | 8000--9FFF        | Extended RAM                           |
| **5**     | A000--BFFF        | Extended RAM                           |
| **6**     | C000--DFFF        | Mirror of block 2 (required for video) |
| **7**     | E000--FFFF        | Mirror of block 3 (required for video) |

The 512 KB of RAM are divided into 64 pages of 8 KB. Each block can point to any of these 64 pages, allowing access to all the memory simply by changing which page is assigned to each block.

| **⚠** | *Blocks 6 and 7 must be kept as mirrors of blocks 2 and 3 respectively for the ZX81 video system to work correctly. Modifying them without bearing this in mind may cause screen corruption or system instability.* |
|------|------------------------------------------------------------------|

## 11.2 MAP Command --- Assign pages to blocks

To assign a memory page to a given block:

> LOAD \*MAP \<block\>,\<page\>

Where \<block\> is a number from 0 to 7 and \<page\> is a number from 0 to 63.

To read which page is currently assigned to a block:

> LOAD \*MAP \<block\> TO \<variable\>

**Example --- map block 4 to page 10:**

> LOAD \*MAP 4,10

From that point on, any read or write to addresses 32768--40959 will access page 10 of the RAM.

**Example --- read the page assigned to a block:**

> 10 LOAD \*MAP 4 TO A
>
> 20 PRINT \"Block 4 points to page \";A

| **ℹ** | *For a complete reference of the paging system, including 512 KB full paging mode and use from machine code, see Appendix D of this manual.* |
|------|------------------------------------------------------------------|

## 11.3 MC45 Mode --- Machine Code in Blocks 4 and 5

By design of the ZX81 hardware, machine code instructions located in blocks 4 and 5 (addresses 32768--49151) are executed incorrectly: opcodes in the ranges 00h--3Fh and 80h--BFh are replaced by NOP, preventing normal code execution in that region.

MC45 mode (Machine Code 4 and 5) deactivates this limitation, allowing any Z80 instruction to be executed in that memory area.

**Activate MC45 mode:**

> LOAD \*MC45

**Deactivate it:**

> LOAD \*MC45 STOP

| **⚠** | *MC45 mode is implemented by forcing the Z80\'s M1 pin to zero intermittently and during very brief time intervals, keeping the load on that pin at low levels and making it safe in practice. The interface already includes an internal 680Ω resistor for protection.* |
|------|------------------------------------------------------------------|

## 11.4 Boot with Alternative ROM

The SD81 Booster allows booting with a different ROM than the standard ZX81 ROM without needing to use any command. Simply place one or more ROM files in the /SYS/ folder on the SD card with names /SYS/0.ROM through /SYS/9.ROM.

**To boot with an alternative ROM:**

8.  Hold down the number corresponding to the desired ROM file while powering on the ZX81.

9.  Release the key once the computer has booted.

| **💡** | *This feature is very useful for testing alternative or modified ROMs without reprogramming any chip. The standard ZX81 ROM always loads if no key is pressed during boot.* |
|------|------------------------------------------------------------------|

## 11.5 User-Definable Characters (128C / 64C / 256C)

By default, the ZX81 has 64 characters defined by the ROM. The SD81 Booster allows you to expand this set to 128 characters, all of which are completely redefinable, by writing to the memory area between addresses 15360 and 16383 (3C00h--3FFFh).

**Activate 128-character mode:**

> LOAD \*128C

**Return to standard 64-character mode:**

> LOAD \*64C

**Activate 256-character mode:**

> LOAD \*256C

This mode only works in Superfast text mode. It extends the set to 256 characters, all redefinable, in the memory area between addresses 14336 and 16383 (3800h--3FFFh) --- twice the space of 128-character mode, aligned to 2K instead of 1K. As with 128C, the whole block is preloaded with the ROM character set at boot, so enabling the mode without having redefined anything does not change what is shown on screen. Activating \"128C\" or \"64C\" disables \"256C\".

With 256-character mode active, the Chroma mode 0 colour table (indexed by character code) also grows from 1K to 2K (14336--14463, \$C000-\$C7FF), so each of the 256 codes gets its own colour entry. Mode 1 (attribute file) is unaffected \-- it is indexed by screen position and already works the same with 128 or 256 characters.

| **ℹ** | *In 128-character mode, the upper 64 characters are automatically shown in inverse video by the hardware. To display them in normal video you must store the inverted graphic in the corresponding position.* |
|------|------------------------------------------------------------------|

If you need to restore the original character set after modifying it:

> LOAD \*LDIR 7680,15360,512
>
> LOAD \*LDIR 7680,15872,512

| **ℹ** | *Activating 128-character mode is incompatible with the HRG internal character generator (see section 10.6). Both modes cannot be active simultaneously.* |
|------|------------------------------------------------------------------|

### Example: Spectrum-style start screen

The following program illustrates the use of 128-character mode to load an alternative character set from the SD and display a screen in ZX Spectrum style:

> 5 LOAD \*128C
>
> 20 LOAD FAST \"SPEC-81-128.BIN\" CODE 15360
>
> 30 CLS
>
> 35 POKE 16418,0
>
> 40 PRINT AT 23,1;CHR\$ 8;\" 1982 S\[INCLAIR\] R\[ESEARCH\] L\[TD\].\"
>
> 45 POKE 16418,2
>
> 50 IF INKEY\$=\"\" THEN GOTO 50

| **ℹ** | *Texts in brackets indicate inverse video: S\[INCLAIR\] means the S is normal and INCLAIR is in inverse video; R\[ESEARCH\] means R is normal and ESEARCH is in inverse video; L\[TD\] means L is normal and TD is in inverse video. To enter inverse video characters press SHIFT + 9 before each character. CHR\$ 8 corresponds to the ZX81 grid graphic character (code 8, the © symbol in the Spectrum character set).* |
|------|------------------------------------------------------------------|

**Line by line explanation:**

- Line 5: activates 128-character mode.

- Line 20: loads the file SPEC-81-128.BIN from the SD at address 15360 (3C00h), the user-definable character area. This file contains the ZX Spectrum character set.

- Line 30: clears the screen.

- Line 35: activates Superfast mode (POKE 16418,0) to free the CPU from video control.

- Line 40: prints on line 23 the Spectrum copyright message, using CHR\$ 8 as the © symbol and the signature words in inverse video.

- Line 45: restores standard video mode (POKE 16418,2).

- Line 50: waits indefinitely for any key press.

# 12. Advanced BASIC Extensions

This section covers additional BASIC extension commands of the SD81 Booster oriented primarily towards programming: string manipulation, memory access, input/output port communication, and directory access from a program.

## 12.1 String Manipulation

**Invert characters in a string (\*INV):**

Inverts bit 7 of all characters in a string variable, turning normal characters into inverse and vice versa:

> LOAD \*INV A\$

String slices are also supported:

> LOAD \*INV A\$(2 TO 7)

**Force inverse video on a string (\*BOLD):**

Forces bit 7 of all characters in a string variable to 1, putting all characters in inverse video:

> LOAD \*BOLD A\$

| **💡** | *To put characters in normal video from an inverse string, first apply \*BOLD and then \*INV to invert the result.* |
|------|------------------------------------------------------------------|

## 12.2 Memory Block Copy and Fill

**Copy a memory block in ascending order (\*LDIR):**

> LOAD \*LDIR \<source\>,\<dest\>,\<length\>

Copies \<count\> bytes from \<source\> to \<dest\> in ascending order. Equivalent to the Z80 LDIR instruction.

**Copy in descending order (\*LDDR):**

> LOAD \*LDDR \<source\>,\<dest\>,\<length\>

Same as \*LDIR but in descending order. Equivalent to the Z80 LDDR instruction.

| **ℹ** | *When source and destination overlap, the copy order matters: use \*LDIR if the destination is before the source in memory, and \*LDDR if it is after, to avoid data being overwritten during the copy.* |
|------|------------------------------------------------------------------|

**Loading Hexadecimal Data into Memory (\*HEX):**

> LOAD \*HEX \<address\>,\"\<hexadecimal\>\"

For example, LOAD \*HEX 30000,\"0A014020\" loads the values 10 (0Ah), 1, 64 (40h) and 32 (20h) starting at address 30000.

## 12.3 Machine Code Execution

**Execute a machine code routine (LOAD USR):**

> LOAD USR \<address\>

Equivalent to RAND USR but with three important advantages:

- Does not modify the random number generator.

- The routine is called from the top level of BASIC, leaving the alternate registers BC\', DE\' and HL\' available and the stacks clean.

- The rest of the line is not parsed syntactically, allowing the routine to perform its own parameter parsing via RST 18h and RST 20h.

Upon entry to the routine, the BC register contains the address called.

| **ℹ** | *Limitation with numeric literals: if the routine uses the ROM\'s SCANNING routine to parse parameters, expressions with direct numeric literals (e.g. USR 40000) will not work correctly. Use VAL \"number\" or CODE \"character\" as a workaround.* |
|------|------------------------------------------------------------------|

## 12.4 Input/Output Port Access

**Write to a port (\*OUT):**

> LOAD \*OUT \<port\>,\<value\>

**Read from a port (\*IN):**

> LOAD \*IN \<port\> TO \<variable\>

| **ℹ** | *TO is the BASIC token (SHIFT + 4), not typed letter by letter.* |
|-------|------------------------------------------------------------------|

## 12.5 16-bit Memory Access

**Read a 16-bit memory value (LOAD PEEK):**

> LOAD PEEK \<address\> TO \<variable\>

Reads two consecutive bytes from memory starting at \<address\> and stores them as a 16-bit value in \<variable\>.

**Writing a 16-bit value to memory (LOAD THEN POKE):**

> LOAD THEN POKE \<address\>,\<value16\>

**Set the upper memory limit for BASIC (LOAD THEN CLEAR):**

> LOAD THEN CLEAR \<address\>

Sets the last RAM address available for BASIC. Unlike the standard CLEAR command, this does not clear variables; it only clears the GOSUB stack. Use a separate CLEAR if you also want to clear variables.

## 12.6 Directory Access from a Program

These commands allow reading the content of an SD directory from within a BASIC program, useful for building file selection menus.

**Open a directory for reading (\*OPENDIR):**

> LOAD \*OPENDIR \<string\>

The string can include a full path and wildcards. If wildcards are used, the maximum number of retrievable entries is 512. This command leaves the directory ready to use with \*ROW.

**Read a directory entry (\*ROW):**

> LOAD \*ROW \<number\> TO \<variable\$\>

Reads the directory entry with the given number (starting from 1) and stores it in the string variable. If the number is out of range, returns an empty string.

**Example --- file selection menu in BASIC:**

> 10 LOAD \*OPENDIR \"JUEGOS/\*.P\"
>
> 20 FOR I=1 TO 10
>
> 30 LOAD \*ROW I TO A\$
>
> 40 IF A\$=\"\" THEN GOTO 70
>
> 50 PRINT I;\". \";A\$
>
> 60 NEXT I
>
> 70 INPUT \"Select: \";N
>
> 80 LOAD \*ROW N TO A\$
>
> 90 LOAD FAST A\$

# 13. Program Examples

This section contains example programs that illustrate the use of the SD81 Booster\'s main features. All are written in standard ZX81 BASIC with the interface extensions.

| **ℹ** | *Texts in brackets (e.g. \[SINCLAIR\]) are entered in inverse video on the ZX81 by pressing SHIFT + 9 before each character. CHR\$ 8 corresponds to the ZX81 grid graphic character (code 8).* |
|------|------------------------------------------------------------------|

## 13.1 Real Time Clock (RTC)

Demonstrates three clock adjustment formats: full date and time, date only and time only.

> 10 LET A\$=\"2025-11-10 13:48:00.00\"
>
> 15 LOAD \*RTC
>
> 16 PRINT
>
> 20 LOAD \*RTC=A\$
>
> 25 LOAD \*RTC
>
> 26 PRINT
>
> 30 LOAD \*RTC=\"2026-11-10\"
>
> 35 LOAD \*RTC
>
> 36 PRINT
>
> 40 LOAD \*RTC=\"12:20\"
>
> 45 LOAD \*RTC
>
> 46 PRINT
>
> 50 LOAD \*RTC=\"13:20:35\"
>
> 55 LOAD \*RTC
>
> 56 PRINT

Displays the current time (line 15), then adjusts it via a string variable (line 20), then changes only the date (line 30), then only the time in short format (line 40) and finally with hours, minutes and seconds (line 50). After each adjustment it prints the result to verify it.

## 13.2 RTC Battery Status (BAT)

> 10 LOAD \*BAT TO A\$
>
> 20 PRINT A\$

Reads the real time clock battery status and displays it on screen. If the battery is low, the displayed value will indicate it.

## 13.3 MC45 Mode Check

Checks whether machine code mode in blocks 4 and 5 is active by loading a small assembly routine and executing it:

> 10 LOAD \*HEX 40000,\"010203C9\"
>
> 20 IF USR 40000=770 THEN GOTO 100
>
> 30 PRINT \"MC45 INACTIVE\"
>
> 40 STOP
>
> 100 PRINT \"MC45 ACTIVE\"

Loads a Z80 routine at address 40000 (blocks 4-5) that returns the value of BC on exit (C9=RET). If MC45 is not active, the instructions in that area will not execute correctly and the value returned will not be 770. If MC45 is active, the routine executes correctly and jumps to line 100.

## 13.4 Superfast Mode --- Speed Demonstration

Visually compares the refresh speed in standard ZX81 SLOW mode versus the SD81 Booster Superfast mode:

> 4 SLOW
>
> 5 PRINT \"ZX81 SLOW MODE\"
>
> 6 PAUSE 250
>
> 7 POKE 2045,85
>
> 8 POKE 16418,0
>
> 9 CLS
>
> 10 GOSUB 1000
>
> 15 CLS
>
> 20 POKE 2045,170
>
> 30 PRINT \"SD81-BOOSTER SUPER FAST MODE\"
>
> 31 PAUSE 250
>
> 35 CLS
>
> 36 FAST
>
> 37 GOSUB 1000
>
> 40 GOTO 40
>
> 75 POKE 1024,2
>
> 76 POKE 1024,3
>
> 77 POKE 1024,4
>
> 1000 PRINT \"\[CHR\$ 0\]123456789ABCDEFGHIJKLMNOPQRSTUV\";
>
> 1010 FOR N=1 TO 22
>
> 1020 PRINT \"0123456789ABCDEFGHIJKLMNOPQRSTUV\";
>
> 1030 NEXT N
>
> 1040 PRINT \"0123456789ABCDEFGHIJKLMNOQRSTUV\[CHR\$ 0\]\";
>
> 1050 RETURN
>
> 2000 PRINT \"\[CHR\$ 0\]123456789ABCDEFGHIJKLMNOPQRSTUV\";
>
> 2010 FOR N=1 TO 22
>
> 2020 PRINT \"0123456789ABCDEFGHIJKLMNOPQRSTUV\";
>
> 2030 NEXT N
>
> 2040 PRINT \"0123456789ABCDEFGHIJKLMNOQRSTUV\[CHR\$ 0\]\";
>
> 2050 RETURN

| **ℹ** | *\[CHR\$ 0\] in lines 1000, 1040, 2000 and 2040 represents character 0 in inverse video as seen in the original listing.* |
|------|------------------------------------------------------------------|

The program first shows a full screen of text in standard ZX81 SLOW mode, where the CPU dedicates time to video refresh. Then it activates Superfast mode (POKE 2045,170) and repeats the same screen fill with FAST, showing the speed difference.

## 13.5 Spectrum Image Loading

Loads a Spectrum format image (.SCR) from the /SCR/ folder on the SD and displays it using the interface\'s HiRes Spectrum mode:

> 5 LOAD \*CD \"/SCR\"
>
> 15 LET HFILE=32768
>
> 20 POKE 2044,HFILE/256
>
> 25 POKE 2045,172
>
> 30 LOAD \*OUT 32751,39
>
> 40 LOAD \*OUT 251,6
>
> 50 LOAD FAST \"Z.SCR\" CODE HFILE
>
> 190 IF INKEY\$=\"\" THEN GOTO 15
>
> 200 POKE 2045,85
>
> 210 LOAD \*OUT 32751,0

**Line by line explanation:**

- Line 5: changes to the /SCR directory on the SD.

- Line 15: defines HFILE=32768 (8000h), start of block 4.

- Line 20: writes the high byte of the screen file address to the interface register.

- Line 25: activates Superfast HiRes Spectrum mode (POKE 2045,172).

- Lines 30-40: configures border colour registers via the interface output port.

- Line 50: loads the file Z.SCR at address HFILE (32768).

- Line 190: waits for any key press; on press returns to line 15 to reload.

- Line 200: deactivates Superfast mode.

- Line 210: restores the output register to 0.

# 14. Error Codes

When an error occurs, the ZX81 displays a code at the bottom of the screen followed by the line number where it occurred. The error codes related to the SD81 Booster are:

| **Code** | **Meaning** |
|--------|----------------------------------------------------------------|
| **A** | Invalid argument (incorrect filename, parameter out of range, or hexadecimal string with incorrect length in \*PEG or \*HEX). |
| **D** | The user pressed BREAK to interrupt an operation (e.g. during a directory listing). |
| **G** | File not found on the SD card. |
| **H** | Error accessing the SD card. Check it is correctly inserted and formatted in FAT32. |
| **I** | I/O error on the SD card during a read or write operation. |
| **J** | Disk full: not enough space on the SD card to save the file. |
| **K** | File or directory already exists with that name. |
| **L** | File name that is too long or with characters that are not allowed. |
| **M** | The directory is not empty (when trying to delete with \*RD). |
| **N** | Permission denied or write-protected file. |

| **💡** | *If you get error H repeatedly, remove the SD card, check it is formatted as FAT32, and reinsert it. If the error persists, try a different card.* |
|------|------------------------------------------------------------------|

# 15. For Programmers

This section is aimed at developers who wish to take advantage of the SD81 Booster\'s advanced capabilities from Z80 machine code, including the expansion ROM routines and the MCU communication system.

## 15.1 Expansion ROM Map

The expansion ROM occupies block 1, starting at address 8192 (2000h):

| **Address** | **Content / Routine** |
|---------|---------------------------------------------------------------|
| 2000h | String \'SD81\' in ZX81 character encoding |
| 2004h | ROM version byte (high nibble = major, low nibble = minor). PEEK 8196. |
| 2005h | Routine: returns in HL the return address of the caller |
| 2006h | Routine: Runs JP (HL) --- emulates CALL (HL) |
| 2007h | GetMCUVersion --- MCU version in BC (B=0, C=version). USR 8199. |
| 200Ah | WaitClkDiff --- wait for a different clock bit than C bit 7 |
| 200Dh | WaitClkEq --- wait clock bit equal to C bit 7 |
| 2010h | OutWaitDiff --- sends A to the data port and waits for different clock |
| 2013h | OutWaitEq --- sends A to the data port and waits for equal clock |
| 2016h | WaitDiffBrk --- like WaitClkDiff but allows BREAK (does not return if pressed) |
| 2019h | WaitEqBrk --- like WaitDiffBrk but expect equality |
| 201Ch | SendString --- sends string to MCU prefixed by length. B=length, DE=address |
| 201Fh | SendStrLoop --- sends B bytes to MCU from DE without waiting for clock changes |
| 2022h | ReportStatus --- reads a byte from the MCU; if ≠0, raises BASIC error (1=G, 2=H, \...) |
| 2025h | PrintBPaged --- prints character with paging. B=character (0--63 or 128--191) |
| 2028h | Cmd64C --- Activate 64 character mode (= LOAD \*64C) |
| 202Bh | Cmd128C --- Activate 128 character mode (= LOAD \*128C) |
| 202Eh | GetPhase --- clock status in C bit 7 bit |
| 2031h | GetData --- reads the MCU\'s data port. Result in A. Does not modify flags. |
| 2034h | SD81_RESET --- RESET entry point. Requires NMI and DI disabled; HL=continuation address. |
| 2037h | SD81LOADCMD --- entry point for the extended LOAD command |
| 203Ah | SD81SAVECMD --- entry point for extended SAVE command |
| 203Dh | SD81RUNCMD --- entry point for extended RUN command |

**Get version from BASIC:**

> 10 LET V=USR 8199
>
> 20 LET MAJ=INT(V/16)
>
> 30 LET MIN=V-16\*MAJ
>
> 40 PRINT \"VERSION MCU: \";CHR\$(MAJ+28);\".\";CHR\$(MIN+28)

## 15.2 I/O Ports and MCU Protocol

| **Port** | **Function** |
|---------|---------------------------------------------------------------|
| **E7h** | Memory Mapper |
| **A7h** | MCU data port (read and write). |
| **AFh** | MCU control port (write=reset MCU; bit 7 on read=clock bit, bits 6..1 are the VSYNC counter since the last read and bit 0 on read indicates the instantaneous state of VSYNC) |

### VSYNC Synchronisation

Bit 0 of port AFh reflects the status of the vertical sync interrupt (VSYNC). This allows the CPU to wait for the start of screen refresh precisely, without needing interrupts or the HALT command.

Bits 6..1 are the VSYNC counter since the last read and bit 0 on read indicates the instantaneous state of VSYNC)

On the ZX Spectrum, many games used HALT or an IM1/IM2 interrupt routine to synchronise with the vertical scan. On the SD81 Booster this mechanism replaces that functionality and is especially useful when porting Spectrum games.

**Example VSYNC wait loop in Z80 assembly:**

> WAIT_VSYNC:
>
> in a,(0AFh) ; Read MCU Data Port
>
> and 01h ; isolate bit 0 (VSYNC)
>
> jr nz,WAIT_VSYNC ; wait until VSYNC = 0
>
> WAIT_VSYNC2:
>
> in a,(0AFh)
>
> and 01h
>
> jr z,WAIT_VSYNC2 ; wait edge (VSYNC = 1)
>
> ; Synchronized with the start of the frame

| **⚠** | *Writing any value to port AFh causes a software reset of the MCU. This should not be done while the MCU is saving or copying a file.* |
|------|------------------------------------------------------------------|

The communication is synchronised via the clock bit (bit 7 of port AFh), which is toggled with each read or write on A7h. The Z80 must wait for the bit to change before the next operation.

**Example --- GETBYTE command (index 16 of MCU internal memory):**

> in a,(0AFh) ; Read Start Clock Bit
>
> ld c,a
>
> ld a,20h ; GETBYTE code
>
> out (0A7h),a ; Send command
>
> WAIT1: in a,(0AFh)
>
> xor c
>
> jp p,WAIT1 ; Wait for a different clock
>
> ld a,16 ; index
>
> out (0A7h),a ; send parameter
>
> WAIT2: in a,(0AFh)
>
> xor c
>
> jp m,WAIT2 ; wait for equal clock
>
> in a,(0A7h) ; read response --- result in A

## 15.3 Memory Mapper (port E7h)

In simple paging mode (up to 256 KB), the 8 bits written to port E7h are interpreted as follows:

| **Bits**               | **Function**        |
|------------------------|---------------------|
| **D2, D1, D0**         | Block number (0--7) |
| **D7, D6, D5, D4, D3** | Page number (0--31) |

To access all 64 pages of full mode (512 KB), use the OUT (C),r instruction with the page number in B and the block number in another register. If A contains the block number (0--7) and B the page (0--63):

> ld c,0E7h ; mapper port
>
> out (c),a ; select page B in block A

The change between simple and full mode is done via an MCU command (FULLPAGING and HALFPAGING commands, see section 15.5).

## 15.4 Debug Console (USB-C Port)

The USB-C port on the interface also works as a serial debug port. When connected to a computer, the operating system detects a virtual serial port associated with the CH340G chip. On some systems the CH340 drivers may need to be installed.

**Connection parameters:**

| **Parameter**    | **Value**   |
|------------------|-------------|
| **Speed**        | 115200 baud |
| **Data bits**    | 8           |
| **Parity**       | None        |
| **Stop bits**    | 1           |
| **Flow control** | None        |

With any serial terminal program (PuTTY on Windows, minicom on Linux, CoolTerm on macOS, or the Arduino IDE\'s own Serial Monitor) it is possible to monitor in real time the MCU messages, including: boot progress, SD access errors, firmware update progress, and debug messages from the filesystem, VGM, PEG and speech synthesis.

| **ℹ** | *The production firmware emits basic status messages via the serial port. Recompiling the firmware with the DEBUG macro active produces much more detailed output, useful for advanced diagnostics and development.* |
|------|------------------------------------------------------------------|

## 15.5 Complete MCU Command Table

Commands are sent to the MCU by writing their code to data port A7h, following the clock bit synchronisation protocol described in section 15.2. All string parameters are preceded by a byte with the length.

### Error codes returned by commands

| **Code** | **Meaning**                                      |
|----------|--------------------------------------------------|
| **0**    | Success                                          |
| **1**    | File or directory not found                      |
| **2**    | Not a directory                                  |
| **3**    | Operation error (could not create/delete/rename) |
| **4**    | File or directory already exists                 |
| **5**    | File too large                                   |
| **6**    | Could not create destination file                |
| **7**    | Write error                                      |
| **8**    | Partial read error                               |
| **12**   | No VGM file open                                 |
| **13**   | Operation not permitted in T81 directory         |
| **14**   | Invalid joystick parameter                       |

### System commands

| **Code** | **Name** | **Parameters** | **Response** | **Description** |
|------|---------|-------------|----------|-----------------------------------|
| **0** | **NOP** | **---** | **---** | **No operation. Only synchronises the clock.** |
| 1 | VERSION | --- | 1 byte: version | Returns the MCU version. Same format as byte at 2004h. |
| 32 | GETBYTE | 1 byte: index (0--255) | 1 byte: value | Reads a byte from MCU internal memory. Indices 0--127: volatile system variables. Indices 128--255: EEPROM (persistent). |
| 33 | SETBYTE | 1 byte: index + 1 byte: value | --- | Writes a byte to internal memory. |

### Filesystem commands

| **Code** | **Name** | **Parameters** | **Response** | **Description** |
|------|------------|----------------|--------------|--------------------------|
| **2** | **PWD** | **---** | **String + EOT + status** | **Returns the current directory in ZX81 encoding.** |
| 3 | CD | String: path | Status | Changes the current directory. Accepts absolute (/) and relative paths. |
| 4 | DEL | String: filename | Status | Deletes a file from the current directory. No wildcards. |
| 5 | MKDIR | String: filename | Status | Creates a subdirectory. |
| 6 | RMDIR | String: filename | Status | Removes an empty directory. |
| 7 | MOVE | String: source + String: destination | Status | Renames or moves a file. |
| 8 | COPY | String: source + String: destination | Status | Copies a file. Date/time is not preserved. |
| 9 | LOAD | String: filename | 2B length + N bytes + Status | Loads a file. .P/.81: calculates real size. .ROM: loads at address 0 and resets. .WAV: plays audio. |
| 10 | SAVE | String: filename + 2B length + N bytes | Status | Saves a data block as a file on the SD. |
| 11 | TYPE | String: filename | String char by char + EOT + Status | Sends the content of a text file. With \* searches in /MAN/ with .TXT extension. |
| 12 | DIR | String: path/wildcard | String char by char + EOT + Status | Lists the directory, including file sizes. |
| 14 | FREE_TXT | --- | String + EOT + Status | Returns SD total and free space as text. |
| 15 | FREE | --- | 4B total + 4B free + Status | Space total and free in KB as 32-bit little-endian values. |
| 16 | OPENDIR | String: path/wildcard | Status | Opens a directory and builds an internal array (max. 512 entries). |
| 17 | GETROWLEN | 2B: index | 1B: length + Status | Length of the name of entry index in the array opened with OPENDIR. |
| 18 | GETROW | 2B: index | 1B: length + N bytes + Status | Name of entry index in ZX81 encoding. Index 0 = current directory. Directories between \< and \>. |
| 53 | F_OPEN | Handle(0..3)+name in ASCII | 1B: status | Open a big file (32 bits size) in a specified file handle (0..3). Name is a ASCII Pascal string. |
| 58 | F_OPEN_ZX81 | Handle(0..3)+name in ZX81 | 1B: status | Open a big file (32 bits size) in a specified file handle (0..3). Name is a ZX81 Pascal string. |
| 54 | F_SEEK | Handle(0..3)+Offset (4 bytes Little endian) | 1B: status | Move read/write pointer to the specified position by offset |
| 55 | F_READ | Handle(0..3)+Count(2B Little Endian) | count bytes + 1B:status | Read count bytes. It Will send count bytes allways padding with zeroes if needed |
| 56 | F_WRITE | Handle(0..3)+Count(2B Little Endian)+info to write (count bytes) | 1B:status | Write count bytes. |
| 57 | F_CLOSE | Handle(0..3) | 1B:status | Close file |
| 59 | F_STAT | Handle(0..3) | 4B:size+2B:date+2B:time+1B:status | Returns the size (32 bits) and creation date/time (FAT format) of a file already opened with F_OPEN. |

### Hardware control commands

| **Code** | **Name** | **Parameters** | **Response** | **Description** |
|------|-------------|----------------|---------|------------------------------|
| **19** | **ENABLE_MC45** | **---** | **---** | **Activates MC45 mode (machine code in blocks 4 and 5).** |
| 20 | DISABLE_MC45 | --- | --- | Deactivates MC45 mode. |
| 21 | JOY | String: 5 bytes of ZX81 keys | Status | Configures the joystick mapping: left, right, up, down, fire. |
| 27 | SEL_128CHARS | --- | --- | Activates 128-character mode. Equivalent to LOAD \*128C. |
| 28 | SEL_64CHARS | --- | --- | Activates standard 64-character mode. Equivalent to LOAD \*64C. |
| 29 | FULLPAGING | --- | --- | Activates full paging mode (512 KB, 64 pages). |
| 30 | HALFPAGING | --- | --- | Activates simple paging mode (256 KB, 32 pages). |
| 48 | ENABLE_48K | --- | --- | Activates 48 KB extended RAM mode. Equivalent to LOAD \*RAM48.. |
| 49 | DISABLE_48K | --- | --- | Deactivates extended RAM mode. Equivalent to LOAD \*RAM48 STOP. |
| 65 | SEL_256CHARS | --- | --- | Activates 256-character mode (Superfast text only). Equivalent to LOAD \*256C. |

### Speech synthesis commands

| **Code** | **Name** | **Parameters** | **Response** | **Description** |
|------|-----------|--------------|---------|---------------------------------|
| **22** | **BINARY_SAY** | **String: allophone bytes** | **Status** | **Plays allophones in binary format. Synchronous (blocks until finished).** |
| 23 | SAY | String: ASCII text | Status | Converts text to phonemes and plays it. With \* as first character: background. Equivalent to LOAD \*SAY. |

### AY / sound commands

| **Code** | **Name** | **Parameters** | **Response** | **Description** |
|------|-----------|-----------------|---------|------------------------------|
| **24** | **AY_SET_REG** | **1B: register (0-15) + 1B: value** | **---** | **Writes a value to an AY emulator register.** |
| 25 | AY_GET_REG | 1B: register (0-15) | 1B: value | Reads the current value of an AY emulator register. |
| 26 | AY_PLAY | String: channel A + String: B + String: C | Status | Plays up to three simultaneous PLAY strings. With \* in ch A: background. Equivalent to LOAD \*PLAY. |

### VGM commands

| **Code** | **Name** | **Parameters** | **Response** | **Description** |
|------|-----------|---------------|---------|--------------------------------|
| **34** | **PLAY_VGM** | **String: filename** | **Status** | **Opens and starts playing a VGM file in background. Adds .vgm if no extension.** |
| 35 | STOP_VGM | --- | --- | Stops VGM playback and resets the AY emulator. |
| 36 | PAUSE_VGM | --- | --- | Pauses VGM playback. |
| 37 | CONT_VGM | --- | --- | Resumes paused VGM playback. |
| 38 | LOOP_VGM | 1B: mode (0=no loop, 1=loop) | --- | Sets the VGM player loop mode. |

### PEG commands

| **Code** | **Name** | **Parameters** | **Response** | **Description** |
|------|------------|---------------|---------|-------------------------------|
| **40** | **LOAD_PEG** | **1B: address + String: hex data** | **---** | **Loads PEG instructions into generator memory. 2 bytes per instruction in little-endian.** |
| 41 | PLAY_PEG | 1B: thread (0--2) + 1B: address | --- | Starts execution of a PEG program on the indicated thread. |
| 42 | STOP_PEG | 1B: thread (0-2) | --- | Stops and resets the indicated PEG thread. |
| 43 | PAUSE_PEG | 1B: thread (0-2) | --- | Pauses the indicated PEG thread. |
| 44 | CONT_PEG | 1B: thread (0-2) | --- | Resumes the indicated PEG thread. |
| 45 | SDLOAD_PEG | String: name + 1B: address | Status | Loads a .PEB file from SD into PEG memory. Maximum size: 512 bytes. |

### RTC and battery commands

| **Code** | **Name** | **Parameters** | **Response** | **Description** |
|------|--------|-------------|---------------|--------------------------------|
| **50** | **RTC** | **String: date/time (or empty to read)** | **If read: ZX81 String + Status. If write: Status** | **Without params: returns date/time. With params: sets the clock. Formats: YYYY-MM-DD HH:MM:SS.CC / YYYY-MM-DD HH:MM:SS / YYYY-MM-DD / HH:MM:SS.CC / HH:MM:SS / HH:MM.** |
| 52 | BAT | --- | 5 bytes ASCII + Status | Returns RTC battery level as a 5-character string in format V.mmm (ZX81 encoding). |

# 16. Troubleshooting

## 16.1 Interface Does Not Boot or ZX81 Freezes

| **Symptom** | **Solution** |
|-------------------------|-----------------------------------------------|
| **ZX81 shows nothing on power on** | Check the interface is correctly inserted in the expansion port. Disconnect and reconnect with the ZX81 powered off. |
| **STAT LED does not light up** | Check the SD card is inserted and contains the SYS folder with its complete contents. Without it the interface will not boot. |
| **STAT LED blinks Blue/Red during boot** | Error initialising the SD card. Check it is correctly inserted and formatted in FAT32. |
| **STAT LED blinks Orange/Red during boot** | Error writing ROM to RAM. Check the SYS folder contains the necessary ROM files. |
| **STAT LED blinks Orange during boot** | MCU is waiting for FPGA response. If the blink does not stop, it may indicate a hardware problem. |
| **Blank screen or noise** | Make sure the expansion connector pins are not bent or dirty. |
| **ZX81 boots but interface commands do not work** | Check the firmware version with LOAD \*VER. To update, copy firmware.bin to the SD root and power on the ZX81. |

## 16.2 microSD Card Problems

| **Symptom** | **Solution** |
|-------------------|-----------------------------------------------------|
| **Error H when trying to load** | Check the SD is correctly inserted. Remove and reinsert it. If the error persists, reformat the card in FAT32. If it continues failing, the SD card may be defective. |
| **SD not recognised** | Check it is formatted in FAT32 (not exFAT or NTFS). |
| **Error G when loading a program** | The file does not exist with that name. Use LOAD \*DIR to see exact names. |
| **Error J when saving** | The SD card is full. Use LOAD \*FREE to check available space. |

## 16.3 Clock Loses Time on Power Off

The real time clock is powered by the CR2032 button battery. If the clock systematically loses time when the ZX81 is powered off, the battery probably needs replacing.

Check the charge level with:

> LOAD \*BAT

If the voltage is below approximately 2.5V, replace the battery with a new CR2032. The battery is located on the interface board and can be removed with a thin flat tool.

## 16.4 Joystick Does Not Respond

| **Symptom** | **Solution** |
|------------------|------------------------------------------------------|
| **Joystick does nothing** | Configure the mapping with LOAD \*JOY \"QAOP \" (up/down/left/right/fire) or another mapping depending on the game, before launching the program. |
| **Only some directions work** | Check the configuration string has exactly 5 characters. |
| **Joystick moves but does not fire** | Check the fifth character of the JOY string corresponds to the game\'s fire key. |

## 16.5 Sound Does Not Work

| **Symptom** | **Solution** |
|------------------|------------------------------------------------------|
| **No sound with LOAD \*PLAY** | Check the television or monitor is connected to the interface\'s SCART connector and that the SCART audio channel is not muted. |
| **Speech is not intelligible** | Try short phrases in English. Spell words phonetically if the result is not satisfactory. |
| **VGM does not play** | Check the file is a VGM with AY chip data only. VGMs with other chips are not compatible. |

## 16.6 Firmware Update Errors

| **Symptom** | **Solution** |
|--------------------------|----------------------------------------------|
| **STAT LED blinks Blue/Red on boot with firmware.bin on SD** | Error initialising SD card before update. Remove the SD, check FAT32 format and try again. |
| **STAT LED blinks White/Red after trying to update** | Error during update. The firmware.bin file remains on the SD. Check the file is not corrupted and power on the ZX81 again to retry. |
| **STAT LED stays solid yellow indefinitely** | Update is in progress. Wait at least 2 minutes before considering there is a problem. Do not power off the ZX81. |
| **After update the interface does not respond** | Check with LOAD \*VER that the version is correct. If the interface does not boot, follow the emergency USB recovery procedure described in section 17. |

## 16.7 Using the Debug Console as a Diagnostic Tool

When the STAT LED shows an error but the cause is not clear, the USB-C debug console can provide very valuable additional information.

**How to connect:**

10. Connect a USB-C cable between the interface and the computer.

11. Open a serial terminal program (PuTTY, Tera Term, minicom, the Arduino IDE\'s Serial Monitor\...) and connect to the CH340 COM/serial port with the parameters: 115200 baud, 8N1, no flow control.

12. Power on the ZX81 with the interface connected.

13. Observe the messages that appear in the terminal during boot and normal operation.

The production firmware emits basic status messages that allow identifying which boot phase fails, whether the SD card is recognised correctly, the result of firmware updates and other relevant events.

| **💡** | *For developers: recompiling the firmware with the DEBUG macro active produces much more detailed output, including flash operation progress, AY register status, and the details of each command received from the Z80.* |
|------|------------------------------------------------------------------|

# 17. Firmware Update

The SD81 Booster has two independently updatable firmware components: the microcontroller (MCU) and the FPGA.

| **⚠** | *Do not interrupt the update process once started. An incomplete update may leave the interface in a non-operational state.* |
|------|------------------------------------------------------------------|

## 17.1 Microcontroller (MCU) Update

The SD81 Booster incorporates a bootloader that allows updating the firmware without external tools or special cables.

**Requirements:**

- Interface microSD card.

- firmware.bin firmware file, available in the project repository.

**Process:**

14. Download firmware.bin from the project repository.

15. Copy firmware.bin to the root of the microSD card (not in any subfolder).

16. Insert the card in the interface with the ZX81 powered off.

17. Turn on the ZX81. The bootloader will detect the file, perform the update and delete it from the SD when finished.

| **⚠** | *Do not power off the ZX81 or remove the SD card during the update.* |
|------|------------------------------------------------------------------|

Verify the installed version with:

> LOAD \*VER

**RTC initialization:**

Only needed **the first time the MCU is ever programmed** (e.g. a factory-fresh chip), or if the RTC configuration becomes corrupted \-\-- for example due to a problem with the backup battery. **A normal user updating an already-working interface does not need to do this.**

**Caution:** Make sure the CR2032 backup battery is installed and has charge before running this procedure.

1\. Power off the interface.

2\. Press and hold the QuickSilva button (left side of the interface --- the second button from the top, i.e. the one closest to the user).

3\. Power on the interface while keeping the button held, for at least 5 seconds.

4\. Release the button. The STAT LED lights solid yellow for half a second, then solid blue for half a second, and --- if everything goes well --- the interface continues the normal boot process until the STAT LED turns solid green.

The date and time are set to a fixed value in 2025, which you can then update with **LOAD \*RTC** from the ZX81.

**Emergency USB recovery:**

If the interface becomes inoperative, there is a USB-C recovery procedure for advanced users that requires opening the enclosure and manipulating jumper JP7. Detailed instructions are available in the project repository:

https://codeberg.org/Retrostuff/SD81-Booster

## 17.2 FPGA Update

The FPGA (Xilinx Spartan-6 XC6SLX9) loads its configuration on each boot from an auxiliary SPI flash memory (W25Q128). Just like the MCU, the SD81 Booster can reprogram this flash automatically from the microSD card, with no special tools or cables required --- a Xilinx Platform Cable USB and the iMPACT tool are no longer needed.

**Requirements:**

- Interface microSD card.

- SD81.MCS file, available in the project repository.

**Process:**

18. Download SD81.MCS from the project repository.

19. Copy SD81.MCS to the root of the microSD card (not in any subfolder).

20. Insert the card in the interface with the ZX81 powered off.

21. Turn on the ZX81. The system will detect the file, reprogram the FPGA flash, and delete it from the SD when finished.

|  |  |
|:----:|------------------------------------------------------------------|
| **⚠** | *Do not power off the ZX81 or remove the SD card during the update. If the process is interrupted, the system will detect this and automatically retry on the next boot --- the SD81.MCS file is not deleted from the SD until the update has been confirmed to complete successfully.* |

Verify the installed version with:

LOAD \*FPGA

# 18. Glossary

| **Term** | **Definition** |
|--------------|----------------------------------------------------------|
| **Allophone** | Minimal speech sound unit used by the speech synthesiser. The SD81 Booster uses the SP0256 chip allophones. |
| **Block** | 8 KB division of the Z80 address space. The SD81 Booster divides the Z80\'s 64 KB into 8 blocks (0-7). |
| **FPGA** | Programmable logic circuit (Xilinx Spartan-6 XC6SLX9) that implements in hardware the video logic, memory mapper and other interface functions. |
| **FAT32** | Filesystem required by the interface\'s microSD card. Incompatible with exFAT and NTFS. |
| **FAST** | ZX81 BASIC token (SHIFT+F). On the SD81 Booster, activates SD loading/saving mode. |
| **Screen file (HFILE)** | Memory block containing the screen data in Superfast modes. |
| **HRG** | High Resolution Graphics. ZX81 high resolution mode. |
| **MCU** | Microcontroller. The chip that manages the SD, sound, RTC and communication with the Z80. |
| **Page** | 8 KB division of the interface RAM. The 512 KB are divided into 64 pages (0--63) that can be mapped to any block. |
| **PEG** | Programmable Effects Generator. Virtual machine for playing sound effects in the background without using the ZX81 CPU. |
| **RTC** | Real Time Clock. Real time clock incorporated in the SD81 Booster, powered by a CR2032 battery. |
| **SLOW** | ZX81 BASIC token (SHIFT+D). On the SD81 Booster, activates tape (audio) loading/saving mode. |
| **SP0256** | General Instrument speech synthesiser chip, base of the SD81 Booster synthesiser. Also used in the Currah MicroSpeech and The Voice. |
| **Superfast** | Mode where the SD81 Booster hardware manages screen refresh, freeing the ZX81 CPU for other tasks. |
| **T81** | Container file format that groups multiple ZX81 programs. The interface can navigate its content as if it were a directory. (alpha stage) |
| **Token** | In ZX81 BASIC, each reserved word is stored as a single byte. Entered with SHIFT key combinations. |
| **VGM** | Video Game Music. File format for music that the SD81 Booster can play in the background using the AY emulator. |
| **Inverse video** | ZX81 display mode where background and character swap colours. Activated with SHIFT+9 before the character. |

# 19. Firmware Version History

| **Version** | **Date** | **Main changes**      |
|-------------|----------|-----------------------|
| **1.0**     | 2025     | First public release. |

| **ℹ** | *This history will be updated with each new firmware version. Check the project repository for the complete changelog.* |
|------|------------------------------------------------------------------|

# 20. References

## The SD81 Booster Project

**Official repository** --- source code, firmware, schematics and technical documentation:

https://codeberg.org/Retrostuff/SD81-Booster

**Va de Retro** --- Spanish retrocomputing forum where a previous version of the interface was announced:

https://www.va-de-retro.com/foros/portal

## Tools

**STM32CubeProgrammer** --- STM32 MCU programming tool, needed for USB recovery in emergencies:

https://www.st.com/en/development-tools/stm32cubeprog.html

## Technical Reference Documentation

**SP0256-AL2 speech synthesiser chip** (General Instrument) --- datasheet for the chip on which the SD81 Booster speech synthesiser is based:

https://rarewaves.net/wp-content/uploads/2018/09/SP0256-AL2.pdf

**Chroma81 interface** --- documentation for the ZX81 colour interface whose functionality the SD81 Booster implements. The original link is no longer available; it can be found in web archives:

http://www.fruitcake.plus.com/Sinclair/ZX81/Chroma/ChromaInterface_Documentation.htm

**.P and .P81 file format** --- technical specification of the ZX81 program formats:

https://k1.spdns.de/Develop/Projects/zasm/Info/O80%20and%20P81%20Format.txt

**VGM format specification** (Video Game Music) --- file format for music used by the interface VGM player:

https://vgmrips.net/wiki/VGM_Specification

## ZX81 ROM

The SD81 Booster includes a modified ROM based on the original ZX81 disassembly. Credits:

**Geoff Wearmouth** --- commented ZX81 ROM disassembly (preserved at archive.org):

https://web.archive.org/web/20150815035607/http://www.wearmouth.demon.co.uk/zx81.htm

**Tomaž Šolc** --- preservation of the disassembly:

https://www.tablix.org/\~avian/spectrum/rom/

## Project Credits

- Hardware design and MCU firmware: Alejandro Valero (wilco2009)

- Z80 code / Modified ROM: Pedro Gimeno (pgimeno)

# Appendix A --- Complete PLAY Command Reference

## Duration Table

| **Value** | **Name**                  | **Duration at 60 bpm** |
|-----------|---------------------------|------------------------|
| **1**     | Semiquaver (1/4 crotchet) | 0.25 s                 |
| **2**     | Dotted semiquaver         | 0.375 s                |
| **3**     | Quaver (1/2 crotchet)     | 0.5 s                  |
| **4**     | Dotted quaver             | 0.75 s                 |
| **5**     | Crotchet                  | 1 s                    |
| **6**     | Dotted crotchet           | 1.5 s                  |
| **7**     | Minim (2 crotchets)       | 2 s                    |
| **8**     | Dotted minim              | 3 s                    |
| **9**     | Semibreve (4 crotchets)   | 4 s                    |
| **10**    | Semiquaver triplet        | 0.1667 s               |
| **11**    | Quaver triplet            | 0.3333 s               |
| **12**    | Crotchet triplet          | 0.6667 s               |

## Envelope Table (W)

| **Code** | **Shape**        | **Description**         |
|----------|------------------|-------------------------|
| **W0**   | \\\|\_\_\_\_\_\_ | Decay then stay off     |
| **W1**   | /\|\_\_\_\_\_\_  | Rise then stay off      |
| **W2**   | \\\|‾‾‾‾‾        | Decay then stay on      |
| **W3**   | /‾‾‾‾‾‾          | Rise then stay on       |
| **W4**   | \\\|\\\|\\\|\\\| | Repeated decay          |
| **W5**   | /\|/\|/\|/\|     | Repeated rise           |
| **W6**   | /\\/\\/\\/       | Rise then decay, repeat |
| **W7**   | \\/\\/\\/\\      | Decay then rise, repeat |

## Complete Command Table

| **Parameter** | **Description**                                       |
|---------------|-------------------------------------------------------|
| C..B          | Note in current octave                                |
| Inv(C..B)     | Note in next octave (inverse video)                   |
| =             | Sharpens the next note                                |
| £             | Plays a rest                                          |
| 1..12         | Duration from this point                              |
| \-            | Duration ligature                                     |
| N / space     | Number separator                                      |
| O\<n\>        | Octave (0--8, default 4)                              |
| T\<n\>        | Tempo in bpm (60-240, default 120) --- channel A only |
| V\<n\>        | Volume (0--15)                                        |
| W\<n\>        | Selects envelope effect (0--7).                       |
| U             | Enables envelope for the channel.                     |
| X\<n\>        | Sets envelope ramp time (0--65535; 6927 ≈ 1 second).  |
| M\<n\>        | Active channel and mode selection (0--63)             |
| ( )           | Repeat enclosed section once more                     |
| )             | Repeat from start indefinitely                        |
| H             | Forces termination of the entire PLAY command.        |
| \*            | (Channel A only) Background playback                  |

# Appendix B --- PEG Effects Generator Reference

The PEG is a 16-bit virtual machine with access to the 16 AY chip registers (R0--R15) and 16 general purpose variables (V0--V15). It supports up to 3 parallel execution threads and up to 256 16-bit words of program memory.

| **Instruction** | **Encoding** | **Description** |
|----------------|--------------|-------------------------------------------|
| LD R,XX | 0R XX | Load register with 8-bit value |
| ADD R,XX | 1R XX | Add 8-bit value to register |
| LD V,XX | 2R XX | Load variable with 8-bit value (rest zeros) |
| ADD V,XX | 3R XX | Add 8-bit value to variable |
| LD R,R | 40 RR | Load register from another register |
| LD R,V | 41 RV | Load register from variable |
| LD V,R | 42 VR | Load variable from register |
| LD V,V | 43 VV | Load variable from another variable |
| ADD V,V | 44 VV | Add second variable to first |
| SUB V,V | 45 VV | Subtract second variable from first |
| ADC V,V | 46 VV | Add with carry |
| SBC V,V | 47 VV | Subtract with carry |
| NOT V,V | 48 VV | First var = bitwise NOT of second |
| AND V,V | 49 VV | Bitwise AND |
| OR V,V | 4A VV | Bitwise OR |
| XOR V,V | 4B VV | Bitwise XOR |
| MUL V,V | 4C VV | Multiply (32-bit result split across two vars) |
| DIV V,V | 4D VV | Divide (quotient + remainder in next var) |
| SHR V,X | 4E VX | Shift right |
| SHL V,X | 4F VX | Shift left |
| MUL V,XX | 5V XX | Multiply variable by constant |
| DIV V,XX | 6V XX | Divide variable by constant |
| SUB V,XX | 7V XX | Subtract constant from variable |
| DJNZ V,XX | 8V XX | Decrement and jump by offset if not zero |
| WAIT XXX | 9X XX | Wait given milliseconds |
| WAIT V | A0 0V | Wait time specified in variable |
| HALT | A0 10 | Stop effect |
| JR XX | A1 XX | Jump by offset |

| **ℹ** | *Jump offsets are relative to the next instruction. The PEG assembler included in the repository handles this automatically.* |
|------|------------------------------------------------------------------|

# Appendix C --- Speech Synthesiser Dictionary

The synthesiser is based on the SP0256 phonemes from General Instrument (Currah MicroSpeech, The Voice). Text is analysed left to right looking for the longest possible match in the dictionary. Recognised words sound better than letter-by-letter phonetic decomposition.

## Recognised Words (selection by length)

13-character words: INVESTIGATORS, IRRESPONSIBLE

12-character words: INVESTIGATOR

11-character words: INVESTIGATE

10-character words: CORRECTING

9-character words: COGNITIVE, CORRECTED, SEPTEMBER, SINCERELY, SINCERITY, INTERFACE

8-character words: CHECKERS, CHECKING, COMPUTER, CORRECTS, DAUGHTER, DECEMBER, EIGHTEEN, FEBRUARY, FREEZERS, FREEZING, NINETEEN, NOVEMBER, PLEDGING, SATURDAY

7-character words: BOOSTER, CHECKED, CHECKER, CORRECT, FREEZER, JANUARY, MINUTES, OCTOBER, PLASTIC, SIXTEEN, TUESDAY, COLLIDE

6-character words: AUGUST, COOKIE, EQUALS, EXTENT, FRIDAY, FROZEN, MONDAY, SUNDAY, TALKED, TALKER, TWENTY

5-character words: APRIL, CHECK, CROWN, EIGHT, EQUAL, ERROR, FIFTY, HELLO, MARCH, MONTH, SIXTY, TALKS, THREE, WORLD

4-character words: DATE, FIVE, FOUR, HAVE, JUNE, NINE, RAYS, TALK, THIS, TIME, WHAT, WHOA, WILL, ZX81

3-character words: ACK, ACT, ADD, AMP, ASH, ASK, BAD, BED, BIG, BOX, BUT, CAR, END, EST, GET, HAS, HIM, ICK, IMP, ING, INK, JOB, KEY, MAY, NOT, NOW, OLD, OUR, OUT, RAY, RED, SIX, SUN, TEN, THE, TOP, TWO, USE, WAY, WHO, WHY, YES, YET, YOU, and many more.

## Punctuation and Pauses

| **Character** | **Effect**   |
|---------------|--------------|
| Space         | Short pause  |
| ,             | Medium pause |
| ; :           | Long Pause   |

## Direct Allophones (advanced use)

Pauses: PA1, PA2, PA3, PA4, PA5

Vowels: AA, AE, AH, AO, AW, AX, AY, EH, ER1, ER2, EY, IH, IY, OW, OY, UH, UW1, UW2, XR, YR

Consonants: BB1, BB2, CH, DD1, DD2, DH1, DH2, EL, FF, GG1, GG2, GG3, HH1, HH2, JH, KK1, KK2, KK3, LL, MM, NG, NN1, NN2, OR, PP, RR1, RR2, SH, SS, TH, TT1, TT2, VV, WH, WW, YY1, YY2, ZH, ZZ

# Appendix D --- Memory Paging System

## Initial Page Assignment

| **Block** | **Default page** | **Range / Use**             |
|-----------|------------------|-----------------------------|
| **0**     | 0                | 0000--1FFF (ROM, read only) |
| **1**     | 1                | 2000--3FFF (ROM Extension)  |
| **2**     | 2                | 4000--5FFF (Main RAM)       |
| **3**     | 3                | 6000--7FFF (Main RAM)       |
| **4**     | 4                | 8000--9FFF (Extra RAM)      |
| **5**     | 5                | A000--BFFF (Extra RAM)      |
| **6**     | 2                | C000--DFFF (Block 2 mirror) |
| **7**     | 3                | E000--FFFF (Block 3 mirror) |

## Use Rules

- Block 0 is always read only. Blocks 1--7 are read/write.

- The same page can be mapped to more than one block simultaneously.

- Blocks 6 and 7 must mirror blocks 2 and 3 respectively for the video system to work. Exception: if the display file is entirely in block 2, block 7 can be freely mapped; if entirely in block 3, block 6 can be freely mapped.

- Blocks 4 and 5 can always be mapped to any page.

- Due to ZX81 hardware quirks, blocks 4--7 can only be used for data, not to execute code (except with MC45 mode active for blocks 4 and 5).

## ROM Modification

It is possible to modify the ROM by mapping page 0 to any write-enabled block and making changes there. The ROM can also be completely replaced by loading new content into a page and mapping it to block 0.\
\
**Machine Code Execution Restriction**

By default, any instruction in blocks 4--7 whose opcode has bit 6 equal to 0 (opcodes 0--63 and 128--191) is replaced by a NOP by the ZX81 hardware. MC45 mode (section 11.3) removes this restriction for blocks 4 and 5.

**Simple vs. Full Paging Mode**

**Simple mode (up to 256 KB):** uses bits D2--D0 of port E7h for the block and bits D7--D3 for the page (0--31).

**Full mode (up to 512 KB):** uses the instruction OUT (C),r with the page in B (0--63). Changed via the FULLPAGING/HALFPAGING MCU commands.

# Appendix E --- Chroma81 Interface Port (7FEFh)

The SD81 Booster implements the Chroma81 colour interface through port 7FEFh (01111111 11101111 in binary). This port can be read and written.

## Writing (OUT 7FEFh)

Allows you to set the color mode and border color:

| **Bits** | **Function**                                         |
|----------|------------------------------------------------------|
| **7--6** | Reserved for future use. Always at 0.                |
| **5**    | Activate color mode: 1 = color on.                   |
| **4**    | Colour mode: 0 = character code, 1 = attribute file. |
| **3**    | Border color brightness bit.                         |
| **2--0** | Border color in GRB (Green-Red-Blue) format.         |

### Border colour format (bits 2-0, GRB format)

| **Value (GRB)** | **Color** |
|-----------------|-----------|
| **000**         | Black     |
| **001**         | Blue      |
| **010**         | Red       |
| **011**         | Magenta   |
| **100**         | Green     |
| **101**         | Cyan      |
| **110**         | Yellow    |
| **111**         | White     |

## Reading (IN 7FEFh)

Allows detecting whether colour mode is available and reading VSync status:

| **Bit** | **Function** |
|------|------------------------------------------------------------------|
| **7--6** | Not used (reserved). |
| **5** | 0 = colour modes available. Always 0, indicating colour mode is always active. |
| **4--1** | Not used (reserved). |
| **0** | VSync interrupt status. 1 = the electron beam is in the vertical blanking period (screen painted). |

| **ℹ** | *Reading bit 5 as 0 allows programs to automatically detect the presence of the Chroma81 interface and activate colour modes if available.* |
|------|------------------------------------------------------------------|

Equivalent command:

LOAD \*COLOR : REM enable colour, border white (7) by default

LOAD \*COLOR \<colour\> : REM enable colour, border = \<colour\> (0-15)

LOAD \*COLOR \<colour\>,\<mode\> : REM colour = \<colour\> (0-15), Chroma mode = \<mode\> (0-1)

LOAD \*COLOR STOP : REM disable colour

(equivalent to the OUT 7FEFh above; sets bit 5 and the character-code mode automatically. **\<colour\>** is the port's low 4 bits, brightness + GRB combined into 0-15. **\<mode\>** is optional (0 = character-code colour table, 1 = attribute file; 0 if omitted). Mode 1 is only implemented in hardware for the Superfast submodes: in native mode (Superfast off) it has no real effect.

**VSync synchronisation:**

Bit 0 allows the CPU to wait until the screen has finished painting before updating its content, avoiding flicker and visual artefacts. Many ZX Spectrum games used the HALT instruction or an interrupt routine to synchronise with VSync; on the SD81 Booster this mechanism is the direct equivalent for that functionality:

> ; Wait for start of VSync
>
> WAIT: in a,(\$AF)
>
> rrca ; bit 0 to carry
>
> jr nc,WAIT ; if carry=0, screen still painting
>
> ; screen refresh complete, safe to update video

# Appendix F --- Superfast and Spectrum Modes

## The Video Problem in the Original ZX81

The ZX81 in SLOW mode manages video by software: during screen refresh, the CPU must execute NOP instructions while the hardware generates the video signal, consuming a significant fraction of processor time.

## Superfast Mode

In Superfast mode, the interface hardware takes control of the data bus during screen refresh, placing video data directly without CPU intervention. This frees the processor completely for other tasks.

Before activating the mode it is necessary to indicate the screen file address (HFILE):

> POKE 2043, HFILE_low : REM low byte of screen file address
>
> POKE 2044, HFILE_high : REM high byte of screen file address

The three Superfast mode variants are selected by POKEing address 2045:

| **POKE** | **Mode** |
|------------------|------------------------------------------------------|
| POKE 2045, 170 | Superfast text (standard text mode, accelerated) |
| POKE 2045, 171 | Superfast HiRes native (high resolution in ZX81 format) |
| POKE 2045, 172 | Superfast HiRes Spectrum (high resolution in Spectrum format) |
| POKE 2045, 85 | Deactivate Superfast mode |

Equivalent commands:

LOAD \*SFAST : REM Superfast text mode

LOAD \*SFHR \<address\> : REM Superfast native HiRes, HFILE = \<address\>

LOAD \*SFSP \<address\> : REM Superfast Spectrum HiRes, HFILE = \<address\>

LOAD \*SFAST STOP : REM disable (equivalent to \*SFHR STOP / \*SFSP STOP)

(equivalent to the POKEs above, for machine-code use). **LOAD \*SFHR** and **LOAD \*SFSP** write HFILE (2043/2044) and enable the mode in a single step; HFILE is not needed for **LOAD \*SFAST** since text mode does not use extended RAM.

## Fine Horizontal Scroll (Superfast)

All three Superfast submodes support a fine horizontal scroll offset of 0 to 7 pixels, which advances the character/attribute/pixel fetch sequence by that many pixels on every row:

POKE 2090, \<offset\> : REM fine scroll offset, 0-7 pixels

Equivalent command:

LOAD \*SCROLL \<offset\> : REM equivalent to POKE 2090

The offset applies equally to all three Superfast submodes (text, native HiRes and Spectrum HiRes); it has no effect in native mode. In Superfast text mode, the pixel gap left at the right edge of each row is filled by the DFILE's NEWLINE byte (position 32 of each row, unused in this mode): software is responsible for writing whatever content should appear there as the scroll advances. This happens whether or not colour is enabled (the character fetch itself does not depend on Chroma); the only thing that can vary with the Chroma mode is the colour this extra column is shown in, and only in mode 1 (attribute file), which does not compute this column's colour correctly. In the other submodes (native HiRes and Spectrum HiRes) that gap does not have a defined content yet.

The scroll can be turned on or off per row, to keep markers or a score line fixed while the rest of the screen scrolls. Each text row (0-23) has one bit in one of three 8-bit registers:

POKE 2091, \<map\> : REM rows 0-7 (bit0=row 0)

POKE 2092, \<map\> : REM rows 8-15

POKE 2093, \<map\> : REM rows 16-23

Bit=1 means that row is shifted by the **LOAD \*SCROLL** offset; bit=0 keeps that row fixed. By default (after a reset) all three registers are 255 (every row scrolls), matching the behaviour before this register existed. Equivalent command:

LOAD \*SCROWS \<lo\>,\<mid\>,\<hi\> : REM equivalent to POKE 2091/2092/2093

(equivalent to the three POKEs above in a single step; each argument is a plain byte, 0-255).

## Spectrum Mode

Spectrum mode (POKE 2045,172) reorders screen lines to match the ZX Spectrum screen organisation, facilitating the conversion of programs between both platforms and enabling the loading of .SCR files directly.

| **⚠** | *In Spectrum mode, the Spectrum beeper and edge port (ULA write port = FBh) is emulated, where bits 2--0 control the edge color and bits 4--3 control the beeper. In this mode, the FBh port is occupied and compatibility with the ZX Printer is lost.* |
|------|------------------------------------------------------------------|

## Border control

**Change border attributes:**

> POKE 2046, \<attr\>

**Define and activate a border pattern:**

It is possible to define an 8-byte pattern that will be repeated along the screen border, allowing it to be completely filled with a custom design:

> POKE 2048, byte0 : REM first byte of pattern
>
> POKE 2049, byte1
>
> \...
>
> POKE 2055, byte7 : REM last byte of pattern
>
> POKE 2047, 170 : REM activate border pattern
>
> POKE 2047, 85 : REM deactivate border pattern

**ULA port in Spectrum mode (FBh):**

| **Bits** | **Function**   |
|----------|----------------|
| **4--3** | Beeper control |
| **2--0** | Border color   |

Equivalent command (pattern ink + Spectrum-mode border):

LOAD \*BORDER \<colour\> : REM pattern ink (0-15) and Spectrum-mode border (bits 2-0)

(equivalent to **POKE 2046,\<colour\>** plus the ULA port FBh above in a single step; it does not enable the pattern by itself, **POKE 2047,170** is still needed. The native-mode border background colour is set with **LOAD \*COLOR**, not this command.)

## VSYNC Synchronisation

On the ZX Spectrum, many games used the HALT instruction or an IM1/IM2 interrupt routine to synchronise with the vertical screen scan and achieve smooth animations without flickering.

On the SD81 Booster this functionality is replaced by reading bit 0 of port AFh, which reflects the status of the vertical sync interrupt (VSYNC). The CPU can wait for this bit to synchronise with the start of screen refresh without needing interrupts or HALT.\
Bits 6..1 are the VSYNC counter since the last read and bit 0 on read indicates the instantaneous state of VSYNC)

## Double buffering (present-blit)

### How the interface generates the image (without double buffering)

All of the system\'s RAM (up to 512 KB) lives in a single external memory chip (the \*\*SRAM\*\*), organised in 8 KB pages that get assigned to the Z80\'s 8 memory blocks through the mapper (E7h). The video circuit, however, does \*\*not\*\* read that SRAM directly --- doing so would make it compete with the CPU for the same memory chip on every cycle. To avoid that, the interface keeps an \*\*internal video memory\*\* (64 KB) inside the FPGA itself, acting as a \*\*mirror\*\* of the 8 blocks: every time the CPU writes a byte to the SRAM, that same byte is automatically copied into the mirror. The video circuit always generates the image by reading this internal mirror, never the SRAM.

The mirror is faithful at all times --- too faithful, in fact: if at some instant the CPU has just erased a sprite, the mirror already reflects the empty gap, and if the video beam happens to scan that area right then, it draws that gap. Because the CPU and the video circuit share the same \"live\" data, the beam can catch intermediate drawing states --- hence the flicker and tearing that double buffering is meant to fix.

### What double buffering changes

POKE 2057, 168+B : REM enable double buffer; front buffer = block B (0-7)

POKE 2057, 85 : REM disable double buffer

When enabled, the interface stops keeping the real-time mirror **only for block \`B\`** of the internal video memory (every other block of the mirror keeps working as before). From that point on:

\- The video circuit stops looking at the HFILE block\'s mirror (the usual screen) and starts looking at block \`B\`\'s mirror instead.

\- Block \`B\`\'s mirror is **no longer updated write by write**. On every vertical blanking (VSYNC), the hardware copies, byte by byte, the entire current content of the HFILE block\'s mirror into block \`B\`\'s mirror in one go. Between one VSYNC and the next, block \`B\`\'s mirror stays **frozen**: it is a fixed snapshot, not a live reflection.

\> **The part that trips people up --- read it twice:** logical block \`B\` still exists as normal SRAM too, just as accessible and writable by the CPU as any other block. But while double buffering is active, **that SRAM has no relation whatsoever to what you see on screen** --- it is that block\'s internal mirror that stops reflecting it. Whatever you write to block \`B\`\'s SRAM will not show up in the image, and what you see on screen is not that SRAM but the snapshot the FPGA refreshed at the last VSYNC, copied from the HFILE block. That is why block \`B\` should be treated as \"reserved for the hardware\" while double buffering is active, even though you can technically still use it as RAM for unrelated things (variables, code) that have nothing to do with the screen.

Put differently, three different things coexist under the same block number, and they should not be confused:

| Layer | What it contains | Who updates it |
|------------------------|------------------|-------------------------------|
| Physical SRAM of block \`B\` | The system\'s real memory (8 KB) | The CPU, on every normal read/write |
| Internal mirror of block \`B\` --- double buffer OFF | A live copy of that SRAM | Automatically, byte by byte, on every CPU write |
| Internal mirror of block \`B\` --- double buffer ON | A frozen snapshot of the HFILE block | Only the FPGA, all at once, once per VSYNC |

The practical result:

\- The screen always shows a **complete snapshot** taken at the last VSYNC: erases or half-drawn sprites are never visible.

\- The program always draws on **a single surface** (the usual HFILE page): no page swapping, no \"redraw what changed two frames ago\" bookkeeping.

\- Reading back from the screen (the HFILE page, not block \`B\`) always returns the last value written --- coherent read-modify-write.

**Correct usage:** wait for the rising edge of VSYNC (bit 0 of port AFh) and do all erasing/drawing right after. From that moment you have about **16 ms** before the hardware takes the next snapshot (the copy starts right after the visible area ends). If drawing takes longer, the snapshot may capture an intermediate state --- the same behaviour as a classic double buffer.

**Choosing the front block (\`B\`):** since that block\'s mirror stops reflecting its SRAM, **no video element** (HFILE, DFILE, Chroma81 attributes) must point to it while double buffering is active --- its mirror would no longer be able to display them. Recommended blocks: **4 or 5** (\$8000-\$9FFF / \$A000-\$BFFF, whichever does not hold your HFILE). Avoid: 0 (text-mode ROM glyphs), 1 (chr RAM), 2-3 (DFILE), 6-7 (Chroma81 attributes). If the front block matches HFILE\'s block, the display freezes permanently: the blit just copies the block onto itself every VSYNC (no effect), and the write mask permanently blocks your new drawing from ever reaching the mirror \-\-- the SRAM does get updated, but none of it ever becomes visible.

\> **Note:** in native HiRes mode only the bitmap is double-buffered; attributes (the \$C000 area) are read live. In Spectrum mode the whole block is double-buffered (bitmap + attributes). Text mode does not use the double buffer.

**Typical setup** (HFILE at \$8000 = block 4, front at block 5):

POKE 2043,0 : REM HFILE low

POKE 2044,128 : REM HFILE high (\$8000)

POKE 2045,172 : REM Superfast Spectrum HiRes

POKE 2057,173 : REM double buffer ON, front = block 5 (168+5)

Equivalent command:

LOAD \*DBUF 5 : REM double buffer ON, front = block 5

LOAD \*DBUF STOP : REM double buffer OFF

**I/O port control (pseudo-block 8):** \`POKE 2057\` stops working once a program has disabled the control-POKE window by writing to 2056 (\"flat RAM\" mode, e.g. CP/M). For those cases the double buffer can also be controlled through the **mapper port (E7h)** using the fictitious block 8:

asm

ld a,08h ; pseudo-block 8

ld b,32+5 ; value: bit5=enable, bits2:0=front block (here 5)

ld c,0e7h

out (c),a ; double buffer ON, front = block 5

ld b,0 ; value 0 = disable

out (c),a

This path is available when **full paging** mode is active or after writing to 2056. Restriction: in half paging after 2056, do not assign an odd page to block 0 through the mapper port (that data pattern, x8h, matches pseudo-block 8).

**MANUAL mode: double buffering without an automatic copy**

In addition to the AUTO mode described above (with an automatic blit on every VSYNC), there is a **MANUAL mode** where the FPGA copies nothing: the Z80 itself draws the whole frame directly into whichever block is not currently the front, and only switches which of the two blocks is shown. This saves the cost of the automatic copy (\~630 µs per frame), at the price of the Z80 having to redraw the whole frame itself (workable in Superfast modes, where the CPU is free from video refresh). Unlike AUTO mode, there is no single HFILE that the FPGA copies here: you need two real blocks (for example 4 and 5).

POKE 2057, 200+B : REM double buffer ON, MANUAL mode; front = block B (0-7)

The values 168+B (AUTO mode) and 200+B (MANUAL mode) share the same POKE 2057; use 85 to disable in either case.

**Typical use:**

1\. Pick two real blocks for the two buffers (e.g. 4 and 5). HFILE is irrelevant to this mechanism while double buffering is active.

2\. Enable it with the starting block as front (POKE 2057, 200+4) and draw the first frame into the other block (5).

3\. Every frame: draw the whole frame into whichever block is not the current front; wait for VSYNC (bit 0 of port AFh); switch the front with POKE 2057, 200+B (B = the block you just drew into).

4\. On the next frame, draw into the block that has just been freed (the former front).

The FPGA\'s write mask still automatically protects the current front block from CPU writes, in both modes.

**I/O port control (pseudo-block 8):** same as in AUTO mode, but with an extra bit (bit4 of the B value) to select MANUAL:

ld a,08h ; pseudo-block 8

ld b,32+16+5 ; bit5=enable, bit4=MANUAL mode, bits2:0=front block (here 5)

ld c,0e7h

out (c),a ; double buffer ON, MANUAL mode, front = block 5

See the complete machine-code example in \`EXAMPLES/DBUF/\` (bouncing ball with real-time double-buffer toggling).

## WRX with the 8-16K RAM

Some WRX high-resolution programs place their graphics in the **8-16K** area (\$2000-\$3FFF) and point the I register there (for example, Psion\'s **Hi-res Chess**). On a real ZX81 this requires a RAM expansion in that region; on the SD81 Booster that region exists, but by default the interface treats it as a **RAM character generator** (the LOAD \*128C feature and games with user-defined characters), which needs exactly the opposite behaviour during the video refresh. Both are legitimate uses of the same region and cannot be told apart automatically, so it is selected with the command:

LOAD \*WRX : REM enable WRX mode in 8-16K

LOAD \*WRX STOP : REM disable (character generator mode, default)

(equivalent to POKE 2058,170 / POKE 2058,85, for machine-code use). Enable WRX mode **before loading** the program (for example LOAD \*WRX and then LOAD \"HRCHESS\"). After a reset it returns to the default mode. It does not affect WRX programs that place their graphics at \$4000 or above, which always work without this command.

## Control POKEs summary

| **Address** | **Value** | **Function** |
|--------------|-----------|------------------------------------------------|
| 2043 | \<low\> | Low byte of screen file address |
| 2044 | \<hi\> | High byte of screen file address |
| 2045 | 170 | Activate Superfast text mode |
| 2045 | 171 | Activate Superfast native HiRes |
| 2045 | 172 | Activate Superfast Spectrum HiRes |
| 2045 | 85 | Deactivate Superfast |
| 2046 | \<attr\> | Change border attributes |
| 2047 | 170 | Activate border pattern |
| 2047 | 85 | Deactivate border pattern |
| 2048--2055 | \<data\> | Define border pattern (8 bytes) |
| 2056 | xxxx | Disable control pokes and enable writing to block 0 |
| 2057 | 168+B | Enable double buffer (front buffer = block B, 0-7) |
| 2057 | 200+B | Enable double buffer, MANUAL mode (front buffer = block B, 0-7) |
| 2057 | 85 | Disable double buffer |
| 2058 | 170 | Enable WRX in the 8-16K RAM |
| 2058 | 85 | Disable WRX (character generator mode) |
| 2090 | 0-7 | Fine horizontal scroll, Superfast (0-7 pixels) |
| 2091 | \<map\> | Rows 0-7 with fine scroll active (bit0=row 0) |
| 2092 | \<map\> | Rows 8-15 with fine scroll active |
| 2093 | \<map\> | Rows 16-23 with fine scroll active |

# Appendix G --- Audio Technical Reference: AY chip, VGM and allophones

## AY-3-8910/12 Chip Registers

The SD81 Booster AY emulator is register-level compatible with the original chip. It supports three independent voices, envelope and noise.

| **Reg** | **Description** | **B7** | **B6** | **B5** | **B4** | **B3** | **B2** | **B1** | **B0** |
|-----|---------------------|:--:|:--:|:-----:|:-----:|:-----:|:-----:|:-----:|:-----:|
| **R0** | Channel A --- tone period (low) | B7 | B6 | B5 | B4 | B3 | B2 | B1 | B0 |
| **R1** | Channel A --- tone period (high) | --- | --- | --- | --- | B3 | B2 | B1 | B0 |
| **R2** | Channel B --- tone period (low) | B7 | B6 | B5 | B4 | B3 | B2 | B1 | B0 |
| **R3** | Channel B --- tone period (high) | --- | --- | --- | --- | B3 | B2 | B1 | B0 |
| **R4** | Channel C --- tone period (low) | B7 | B6 | B5 | B4 | B3 | B2 | B1 | B0 |
| **R5** | Channel C --- tone period (high) | --- | --- | --- | --- | B3 | B2 | B1 | B0 |
| **R6** | Noise period | --- | --- | --- | B4 | B3 | B2 | B1 | B0 |
| **R7** | Channel enable | --- | --- | Noise C | Noise B | Noise A | Tone C | Tone B | Tone A |
| **R8** | Channel A amplitude | --- | --- | --- | Env. | L3 | L2 | L1 | L0 |
| **R9** | Channel B amplitude | --- | --- | --- | Env. | L3 | L2 | L1 | L0 |
| **R10** | Channel C amplitude | --- | --- | --- | Env. | L3 | L2 | L1 | L0 |
| **R11** | Envelope period (low) | B7 | B6 | B5 | B4 | B3 | B2 | B1 | B0 |
| **R12** | Envelope period (high) | B7 | B6 | B5 | B4 | B3 | B2 | B1 | B0 |
| **R13** | Envelope shape | --- | --- | --- | --- | --- | B2 | B1 | B0 |

| **ℹ** | *In R7, a bit at 0 enables the channel; at 1 it disables it. In R8--R10, if the Env. bit is active, amplitude is controlled by the envelope (R11--R13) instead of L3--L0.* |
|------|------------------------------------------------------------------|

## I/O Ports --- Two ZonX-81 Compatible AY Chips

The SD81 Booster implements two physical AY chips, compatible with the standard ZonX-81 interface, using partial address decoding: only bits A1, A2, A3, A5 and A7 are checked; bits A0, A4 and A6 are don\'t-care.

| **Chip** | **Register select (latch)** | **Data write** |
|--------------------------------|---------------------|------------|
| Chip A (standard ZonX-81, A3=1) | \$CFh / \$DFh | \$0Fh / \$1Fh |
| Chip B (SD81 Booster extension, A3=0) | \$C6h | \$06h |

Bit A7 acts as the AY\'s BC1 line: during a write, A7=1 selects the register (address latch) and A7=0 writes the data into the already-selected register. PSG status read-back (BC1=1, BDIR=0) is implemented in the FPGA but not currently exposed via firmware/BASIC.

## VGM Player Opcodes

The interface VGM player only interprets opcodes referring to the AY chip. The rest are ignored without producing an error.

| **Opcode** | **Parameters** | **Description** |
|---------|------------|---------------------------------------------------|
| **61h** | nn nn | Wait n samples (little-endian, 0--65535; ≈ 1.49 s maximum). Long pauses are represented with several consecutive commands. |
| **62h** | --- | Wait 735 samples (1/60 of a second). Equivalent to 61h DFh 02h. |
| **63h** | --- | Wait 882 samples (1/50 of a second). Equivalent to 61h 72h 03h. |
| **A0h** | aa dd | Write value dd to AY register number aa. |

## SP0256-AL2 Allophone Table

Allophones are used with the MCU BINARY SAY command (16h) for precise phonetic synthesis. High-level text access (LOAD \*SAY \"text\") does not require knowing these codes.

| **Code** | **Allophone** | **Example** | **Code** | **Allophone** | **Example** |
|--------|------------|-----------------|--------|------------|-----------------|
| **\$00** | PA1 | pause 10 ms | **\$20** | AW | out |
| **\$01** | PA2 | pause 30 ms | **\$21** | DD2 | do |
| **\$02** | PA3 | pause 50 ms | **\$22** | GG3 | wig |
| **\$03** | PA4 | pause 100 ms | **\$23** | VV | vest |
| **\$04** | PA5 | pause 200 ms | **\$24** | GG1 | got |
| **\$05** | OY | boy | **\$25** | SH | ship |
| **\$06** | AY | sky | **\$26** | ZH | azure |
| **\$07** | EH | end | **\$27** | RR2 | brain |
| **\$08** | KK3 | comb | **\$28** | FF | food |
| **\$09** | PP | pow | **\$29** | KK2 | sky |
| **\$0A** | JH | dodge | **\$2A** | KK1 | can\'t |
| **\$0B** | NN1 | thin | **\$2B** | ZZ | zoo |
| **\$0C** | IH | sit | **\$2C** | NG | anchor |
| **\$0D** | TT2 | to | **\$2D** | LL | lake |
| **\$0E** | RR1 | rural | **\$2E** | WW | wool |
| **\$0F** | AX | succeed | **\$2F** | XR | repair |
| **\$10** | MM | milk | **\$30** | WH | whig |
| **\$11** | TT1 | part | **\$31** | YY1 | yes (short) |
| **\$12** | DH1 | they | **\$32** | CH | church |
| **\$13** | IY | see | **\$33** | ER1 | fir (short) |
| **\$14** | EY | beige | **\$34** | ER2 | fir (long) |
| **\$15** | DD1 | could | **\$35** | OW | beau |
| **\$16** | UW1 | too | **\$36** | DH2 | they |
| **\$17** | AO | aught | **\$37** | SS | vest |
| **\$18** | AA | hot | **\$38** | NN2 | no |
| **\$19** | YY2 | yes (long) | **\$39** | HH2 | hoe |
| **\$1A** | AE | hat | **\$3A** | OR | store |
| **\$1B** | HH1 | he | **\$3B** | AR | alarm |
| **\$1C** | BB1 | business (short) | **\$3C** | YR | clear |
| **\$1D** | TH | thin | **\$3D** | GG2 | guest |
| **\$1E** | UH | book | **\$3E** | EL | saddle |
| **\$1F** | UW2 | food | **\$3F** | BB2 | business (long) |

# Appendix H --- Hardware sprites

The SD81 Booster includes a sprite system handled entirely by hardware: 32 sprites of 8×8 pixels, each with its own transparency mask and its own colour per row (several colours within the same sprite), automatically composited over the background image at no CPU cost. They are controlled with POKEs to a fixed address block, or more conveniently with four dedicated LOAD \* commands.

## Coordinate system

A sprite\'s position is expressed in a coordinate system offset by 32 pixels from the screen, so a sprite can enter and leave through any edge with a clean clip instead of abruptly reappearing on the opposite side:

sprite coordinate 32 = screen pixel 0

X: 0-318 (32 off-screen left, 256 visible, 32 off-screen right)

Y: 0-255 (32 off-screen above, 192 visible, 32 off-screen below)

Any sprite pixel that falls outside the real visible screen area (0,0)-(255,191) simply isn\'t drawn. That\'s why X needs 9 bits (two POKEs: low byte and high byte) while Y fits in a single byte.

## Address map (POKE 2100-2128)

Just like HFILE or the border pattern, these addresses fall within the first 8 KB of the Z80 memory map (ROM area): the interface intercepts these specific writes instead of letting them fall through, so no real memory is needed there. Before writing a sprite\'s colour, pixel data or mask, it must be selected first with POKE 2100,n.

| **Address** | **Value** | **Function** |
|------------|----------|--------------------------------------------------|
| 2100 | 0-31 | Selects the active sprite. Subsequent writes affect this sprite. |
| 2101 | 0/1 | Enables (1) or disables (0) the selected sprite. |
| 2102 | 0-255 | X coordinate, low 8 bits. |
| 2103 | 0/1 | X coordinate, bit 8 (full range 0-318). |
| 2104 | 0-255 | Y coordinate (32 = first visible line). |
| 2105-2112 | byte | Colour of each of the 8 rows: ink×16+paper (0-15 each). |
| 2113-2120 | byte | Pixel pattern of each of the 8 rows. |
| 2121-2128 | byte | Transparency mask of each of the 8 rows. |

## How each pixel is drawn

Three bits combine for each sprite pixel, one from each array at the same row and column:

**1. Mask ---** if the bit is 0, that point is transparent and whatever is underneath shows through (background, text, another lower-priority sprite). If it\'s 1, the sprite draws something there.

**2. Pixel data ---** only matters if the mask is 1: bit=1 uses that row\'s ink colour, bit=0 uses that row\'s paper colour.

Since colour is defined row by row (not a single colour per sprite), one sprite can mix several colours --- for example, a ball with different coloured bands on each line.

## Colour format

Each row\'s colour byte is ink×16+paper, with ink and paper each on the 0-15 scale (the same one used by LOAD \*COLOR and LOAD \*BORDER): bit 3 = bright, bit 2 = green, bit 1 = red, bit 0 = blue. This format is the same in both CHROMA and SPECTRUM mode --- the sprite\'s colour is applied after the hardware has already resolved which of the two modes is active, so there\'s no need to worry about the mode when defining a sprite\'s colour.

## Priority between overlapping sprites

If more than one sprite is active at the same pixel, the highest-numbered one wins (sprite 31 covers sprite 0). There is no other drawing-order control besides the sprite number.

## BASIC commands

**LOAD \*SPRITE \<n\>,\<x\>,\<y\>** --- selects sprite \<n\> (0-31), sets its position and enables it, in a single step.

LOAD \*SPRITE 0,150,88

**LOAD \*SPRITE \<n\> STOP** --- hides sprite \<n\> without touching its position or graphic data.

LOAD \*SPRITE 0 STOP

**LOAD \*SPRCOL / LOAD \*SPRPIX / LOAD \*SPRMASK \<n\>,\"\<16 hex chars\>\"** --- load, respectively, the colour, pixel data or mask of sprite \<n\>: 16 hexadecimal characters = 8 bytes = one row per byte. Each command automatically selects the given sprite.

LOAD \*SPRCOL 0,\"0202020606060606\"

LOAD \*SPRPIX 0,\"070810182626181C\"

LOAD \*SPRMASK 0,\"070F1F1F3F3F1F1F\"

16 hex characters are used instead of a single 48-character block (colour+pixel+mask together) precisely to reduce the risk of a mistake when typing such a long string by hand.

## Capacity

The current firmware supports 32 simultaneous sprites, measured on real hardware at 65% of the FPGA\'s LUTs (with free per-pixel positioning and per-row colour). This is a fixed figure for the end user ---it depends on the specific FPGA bitstream installed on the interface--- not something configurable from BASIC.

*User Manual SD81 Booster v1.0 --- Open hardware and open source software*
