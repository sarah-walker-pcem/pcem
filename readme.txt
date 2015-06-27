PCem v9

PCem is licensed under the GPL, see COPYING for more details.

Changes since v8.1:

- New machines - IBM PCjr
- New graphics cards - Diamond Stealth 3D 2000 (S3 ViRGE/325), S3 ViRGE/DX
- New sound cards - Innovation SSI-2001 (using ReSID-FP)
- CPU fixes - Windows NT now works, OS/2 2.0+ works better
- Fixed issue with port 3DA when in blanking, DOS 6.2/V now works
- Re-written PIT emulation
- IRQs 8-15 now handled correctly, Civilization no longer hangs
- Fixed vertical axis on Amstrad mouse
- Serial fixes - fixes mouse issues on Win 3.x and OS/2
- New Windows keyboard code - should work better with international keyboards
- Changes to keyboard emulation - should fix stuck keys
- Some CD-ROM fixes
- Joystick emulation
- Preliminary Linux port

Thanks to HalfMinute, SA1988 and Battler for contributions towards this release.


PCem emulates the following machines:

IBM 5150 PC (1981) 
The original PC. This shipped in 1981 with a 4.77mhz 8088, 64k of RAM, and a cassette port.
Disc drives quickly became standard, along with more memory.

ROM files needed:

pc102782.bin
basicc11.f6
basicc11.f8
basicc11.fa
basicc11.fc


IBM 5160 XT (1983)
From a hardware perspective, this is a minor tweak of the original PC. It originally shipped
with 128k of RAM and a 10mb hard disc, both of which could be easily fitted to the 1981 machine.
However, this was targetted as businesses and was more successful than the original.

ROM files needed:

5000027.u19
1501512.u18


IBM PCjr (1984)
A home machine, which had better graphics and sound than most XTs but was not hardware compatible
with the PC.

ROM files needed:

bios.rom


IBM AT (1984)
This was the 'next generation' PC, fully 16-bit with an 80286. The original model came with a 6mhz
286, which ran three times as fast as the XT. This model also introduced EGA.

ROM files needed:

at111585.0
at111585.1


Olivetti M24 (1984)
An enhanced XT clone, also known as the AT&T PC 6300. Has an 8086 CPU, and an unusual 'double-res'
CGA display.

ROM files needed:

olivetti_m24_version_1.43_low.bin
olivetti_m24_version_1.43_high.bin


Tandy 1000 (1985)
This is a clone of the unsuccessful IBM PCjr, which added better graphics and sound to the XT,
but removed much expandability plus some other hardware (such as the DMA controller). The Tandy
puts back the DMA controller and ISA slots, making it a much more useful machine. Many games
from the late 80s support the Tandy.

ROM files needed:

tandy1t1.020


DTK Clone XT (1986)
A generic clone XT board.

ROM files needed:

DTK_ERSO_2.42_2764.bin


Amstrad PC1512 (1986)
This was Amstrad's first entry into the PC clone market (after the CPC and PCW machines), and
was the first cheap PC available in the UK, selling for only £500. It was a 'turbo' clone, 
having an 8mhz 8086, as opposed to an 8088, and had 512k RAM as standard. It also had a 
perculiar modification to its onboard CGA controller - the 640x200 mode had 16 colours instead
of the usual 2. This was put to good use by GEM, which shipped with the machine.

Amstrad's CGA implementation has a few oddities, these are emulated as best as possible. This
mainly affects games defining unusual video modes, though 160x100x16 still works (as on the real
machine).

ROM files needed:

40043.v1
40044.v2
40078.ic127


Amstrad PC1640 (1987)
Amstrad's followup to the PC1512, the PC1640 had 640k of RAM and onboard EGA, but was otherwise
mostly the same.

ROM files needed:

40043.v3
40044.v3
40100


Sinclair PC200/Amstrad PC20 (1988)
This was Amstrad's entry to the 16-bit home computer market, intended to compete with the Atari
ST and Commodore Amiga. It's similar to the PC1512, but is based on Amstrad's portable PPC512
system. With stock CGA and PC speaker, it couldn't compare with the ST or Amiga.

ROM files needed:

pc20v2.0
pc20v2.1
40109.bin


Schneider Euro PC (1988)
A German XT clone. An 'all-in-one' system like the Sinclair PC200. I don't know much about this
machine to be honest! This doesn't appear to work with the XTIDE BIOS, so therefore this is the
only model that does not support hard discs.

ROM files needed:

50145
50146


(c)Anonymous Generic Turbo XT BIOS (1988?)
This is a BIOS whose source code was made available on Usenet in 1988. It appears to be an 
anonymous BIOS from an XT clone board. It was then heavily modified to fix bugs. The history of
this BIOS (and the source code) is at http://dizzie.narod.ru/bios.txt

ROM files needed:

pcxt.rom


Commodore PC30-III (1988)
A fairly generic 286 clone.

ROM files needed:

commodore pc 30 iii even.bin
commodore pc 30 iii odd.bin


Amstrad PC2086 (1989)
The PC2086 is essentially a PC1640 with VGA and 3.5" floppy drives.

ROM files needed:

40179.ic129
40180.ic132
40186.ic171


Amstrad PC3086 (1990)
The PC3086 is a version of the PC2086 with a more standard case.

ROM files needed:

fc00.bin
c000.bin


Dell System 200 (1990?)
This is a pretty generic 286 clone with a Phoenix BIOS.

HIMEM.SYS doesn't appear to work on this one, for some reason.

ROM files needed:

dell0.bin
dell1.bin


AMI 286 clone (1990)
This is a generic 286 clone with an AMI BIOS.

ROM files needed:

amic206.bin


IBM PS/1 Model 2011 (1990)
This is a 286 with integrated VGA and a basic GUI and DOS 4.01 in ROM.

ROM files needed:

f80000.bin


Acermate 386SX/25N (1992?)
An integrated 386SX clone, with onboard Oak SVGA and IO.

ROM files needed:
acer386.bin
oti067.bin


Amstrad MegaPC (1992)
A 386SX clone (otherwise known as the PC7386SX) with a built-in Sega Megadrive. Only the PC section
is emulated, obv.

ROM files needed:
41651-bios lo.u18
211253-bios hi.u19


AMI 386 clone (1994)
This is a generic 386 clone with an AMI BIOS. The BIOS came from my 386DX/40, the motherboard is
dated June 1994.

ROM files needed:

ami495.bin


AMI 486 clone (1993)
This is a generic 486 clone with an AMI BIOS. The BIOS came from my 486SX/25, bought in December
1993.

ROM files needed:

ami1429.bin


AMI WinBIOS 486 clone (1994)
A 486 clone with a newer AMI BIOS.

ROM files needed:

ali1429g.amw


Award SiS 496/497 (1995)
A 486 clone using the SiS 496/497 chipset, with PCI bus and Award BIOS.

ROM files needed:

SIS496-1.AWA


Intel Premiere/PCI (Batman's Revenge) (1994)
A Socket 4 based board with 430LX chipset.

ROM files needed:

1009AF2_.BI0
1009AF2_.BI1


Intel Advanced/EV (Endeavor) (1995)
A Socket 5/7 based board with 430FX chipset. The real board has a Sound Blaster Vibra 16 on board,
which is not emulated - use a discrete card instead. Some Advanced/EVs also had a Trio64 on board,
the emulated board does not have this either.

ROM files needed:

1006CB0_.BI0
1006CB0_.BI1


Award 430VX PCI (1996)
A generic Socket 5/7 board with 430VX chipset.

ROM files needed:

55XWUQ0E.BIN



PCem emulates the following graphics adapters :

MDA
The original PC adapter. This displays 80x25 text in monochrome.


Hercules
A clone of MDA, with the addition of a high-resolution 720x348 graphics mode.


CGA
The most common of the original adapters, supporting 40x25 and 80x25 text, and 
320x200 in 4 colours, 640x200 in 2 colours, and a composite mode giving 160x200
in 16 colours.


IBM EGA
The original 1984 IBM EGA card, with 256k VRAM.

ROM files needed:

ibm_6277356_ega_card_u44_27128.bin


Trident 8900D SVGA
A low cost SVGA board circa 1992/1993. Not the greatest board in it's day, but
it has a reasonable VESA driver and (buggy) 15/16/24-bit colour modes.

ROM files needed:

trident.bin


Trident TGUI9440
A later Trident board with GUI acceleration. Windows 9x doesn't include drivers
for this, so they have to be downloaded and installed separately.

ROM files needed:

9440.vbi


Tseng ET4000AX SVGA
A somewhat better SVGA board than the Trident, here you get better compatibility
and speed (on the real card, not the emulator) in exchange for being limited to
8-bit colour.

ROM files needed:

et4000.bin


Diamond Stealth 32 SVGA
An ET4000/W32p based board, has 15/16/24-bit colour modes, plus acceleration.

ROM files needed:

et4000w32.bin


Paradise Bahamas 64
An S3 Vision864 based board.

ROM files needed:

bahamas64.bin


Number Nine 9FX
An S3 Trio64 based board.

ROM files needed:

s3_764.bin


ATI VGA Edge-16
A basic SVGA clone.

ROM files needed:

vgaedge16.vbi


ATI VGA Charger
A basic SVGA clone, similar to the Edge-16.

ROM files needed:

bios.bin


ATI Graphics Pro Turbo
A Mach64GX based board. Probably the best of the emulated boards for use in
Windows.

ROM files needed:

mach64gx/bios.bin


OAK OTI-067
A basic SVGA clone.

ROM files needed:

oti067/bios.bin


Diamond Stealth 3D 2000
An S3 ViRGE/325 based board.

PCem emulates the ViRGE S3D engine in software. This works with most games I tried, but
there may be some issues. The Direct3D drivers for the /325 are fairly poor (often showing
as missing triangles), so use of the /DX instead is recommended.

The streams processor (video overlay) is also emulated, however many features are missing.

ROM files needed:

s3virge.bin


S3 ViRGE/DX
An S3 ViRGE/DX based board. The drivers that come with Windows are similar to those for the
/325, however better ones do exist (try the 8-21-1997 version). With the correct drivers,
many early Direct3D games work okay (if slowly).

ROM files needed:

86c375_1.bin


Some models have fixed graphics adapters :

Olivetti M24
CGA with double-res text modes and a 640x400 mode. I haven't seen a dump of the font
ROM for this yet, so if one is not provided the MDA font will be used - which looks slightly odd
as it is 14-line instead of 16-line.

Tandy 1000
CGA with various new modes - 160x200x16, 320x200x16, 640x200x4. Widely supported in 80s 
games.

Amstrad PC1512
CGA with a new mode (640x200x16). Only supported in GEM to my knowledge.

Amstrad PC1640
Paradise EGA.

Amstrad PC2086/PC3086
Paradise PVGA1. An early SVGA clone with 256kb VRAM.

Amstrad MegaPC
Paradise 90C11. A development of the PVGA1, with 512kb VRAM.

Acer 386SX/25N
Oak OTI-067. Another 512kb SVGA clone.



PCem emulates the following sound devices :

PC speaker
The standard beeper on all PCs. Supports samples/RealSound.

Tandy PSG
The Texas Instruments chip in the PCjr and Tandy 1000. Supports 3 voices plus
noise. I reused the emulator in B-em for this (slightly modified).

Gameblaster
The Creative Labs Gameblaster/Creative Music System, Creative's first sound card
introduced in 1987. Has two Philips SAA1099, giving 12 voices of square waves plus 4 noise
voices. In stereo!

Adlib
Has a Yamaha YM3812, giving 9 voices of 2 op FM, or 6 voices plus a rhythm section. PCem
uses the DOSBox dbopl emulator.

Adlib Gold
OPL3 with YM318Z 12-bit digital section. Possibly some bugs (not a lot of software to test).

Sound Blaster
Several Sound Blasters are emulated :
    SB v1.0 - The original. Limited to 22khz, and no auto-init DMA (can cause crackles sometimes).
    SB v1.5 - Adds auto-init DMA
    SB v2.0 - Upped to 41khz
    SB Pro v1.0 - Stereo, with twin OPL2 chips.
    SB Pro v2.0 - Stereo, with OPL 3 chip
    SB 16 - 16 bit stereo
    SB AWE32 - SB 16 + wavetable MIDI. This requires a ROM dump from a real AWE32.
All are set to Address 220, IRQ 7 and DMA 1 (and High DMA 5). IRQ and DMA can be changed for the 
SB16 & AWE32 in the drivers.
The relevant SET line for autoexec.bat is
  SET BLASTER = A220 I7 D1 Tx    - where Tx is T1 for SB v1.0, T3 for SB v2.0, T4 for SB Pro,
				   and T6 for SB16.

AWE32 requires a ROM dump called awe32.raw. AWE-DUMP is a utility which can get a dump from a real
card. Most EMU8000 functionality should work, however filters are not correct and reverb/chorus
effects are not currently emulated.


Gravis Ultrasound
32 voice sample playback. Port address is fixed to 240, IRQ and DMA can be changed from the drivers.
Emulation is improved significantly over previous versions.


Windows Sound System
16-bit digital + OPL3. Note that this only emulates WSS itself, and should not be used with drivers
from compatible boards with additional components (eg Turtle Beach Monte Carlo)


Innovation SSI-2001
SID6581. Emulated using resid-fp. Board is fixed to port 280.


Other stuff emulated :

Serial mouse
A Microsoft compatible serial mouse on COM1. Compatible drivers are all over the place for this.

M24 mouse
I haven't seen a DOS mouse driver for this yet, but the regular scancode mode works, as does the
Windows 1.x driver.

PC1512 mouse
The PC1512's perculiar quadrature mouse. You need Amstrad's actual driver for this one.

PS/2 mouse
A PS/2 mouse is emulated on the MegaPC and 386SX/25N model. As with serial, compatible drivers are common.

ATAPI CD-ROM
Works with OAKCDROM.SYS. It can only work with actual CD-ROM drives at the minute, so to use ISO images
you need a virtual CD drive.


XTIDE :

The XTIDE board is emulated for machines that don't natively support IDE hard discs.

You will need to download the XTIDE BIOS seperately. Of the various versions, ide_at.bin and ide_xt.bin
should be placed in the ROMS directory. ide_xt is used on all XT models, and ide_at is used on the IBM AT
and Commodore PC30-III machines.

The BIOS is available at :

http://code.google.com/p/xtideuniversalbios/

v2.0.0 beta 1 is the only version that has been tested.

For the PS/1, you will need v1.1.5. The PS/1 is a bit fussy with XTIDE, and I've found that it works best
when the XTIDE configuration has 'Full Operating Mode' disabled. This version must be called
ide_at_1_1_5.bin and should also be placed in the ROMS directory.


Notes :

- The AT and AMI 286 both fail part of their self test. This doesn't really affect anything, 
  just puts an irritating message on the screen.

- The time on the PC1512 clock is wrong. The date is correct, though since the PC1512's bios isn't
  Y2k compliant, it thinks it's 1988.

- The envelope system on the Gameblaster isn't emulated. The noise may not be right either.

- Some of the more unusual VGA features are not emulated. I haven't found anything that uses them yet.

- Windows 3.x should work okay in all modes now.

- Windows 95/98/ME run, with the following caveats :
  - Setup sometimes crashes in the first stage (during file copying). This appears to be a side effect of the 
    bugs fixed making OS/2 work. Unfortunately I haven't been able to eliminate this issue.
  - On some versions of Windows the AWE32 is not set up correctly, claiming a resource conflict. To correct
    this open the relevant item in Device Manager, choose 'Set Configuration Manually' and accept the
    options presented.

- OS/2 1.3 seems to work okay, but I haven't tested it very thoroughly.

- Linux appears to work. fdlinux runs okay, but is relatively useless. SuSE 6.3 seemed
  mostly okay. RedHat Seawolf also seems to work fine.

- Windows NT works okay now. I've tested NT 3.51, NT 4, and 2000. I've also been informed
  that NT 3.1 works. XP does not currently run as PCem doesn't emulate any processor with
  the required CMPXCHG8B instruction.


Software tested:


MS-DOS 3.30
PC-DOS 3.30
PC-DOS 5.02
MS-DOS 6.22
 - Most of the supplied software seems to work, eg Drivespace, Defrag, Scandisk, QBASIC
   etc

Windows/286
Windows/386
Windows 3.0
Windows 3.1
Windows 3.11 for Workgroups
Windows 95
Windows 95 OSR 2.5
Windows 98
Windows 98 SE
Windows ME
Windows NT 3.51
Windows NT 4
Windows 2000

All New World of Lemmings
Command and Conquer : Red Alert
Croc (demo, ViRGE)
Dawn Patrol
Doom
Duke Nukem 3D
Epic Pinball
Final Fantasy 7 (i430VX only)
Forsaken (ViRGE)
Grand Theft Auto
Grim Fandango (ViRGE)
Incoming (ViRGE, SLOW)
Jedi Knight (ViRGE)
Lemmings
Network Q RAC Rally
Pro Pinball : Big Race USA
Psycho Pinball
Quake
Quake 2 (SLOW - unsurprisingly)
Screamer Rally
Simcity 2000
Simcity 3000 (SLOW)
Syndicate
System Shock
Theme Park
Tomb Raider (ViRGE version has oversized display)
Tomb Raider II (ViRGE - has display artifacts that also occur on real hardware)
Transport Tycoon
Tyrian
Wolfenstein 3D
Worms
UFO : Enemy Unknown
X-Com : Apocalypse