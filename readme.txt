PCem v12

PCem is licensed under the GPL, see COPYING for more details.

Changes since v11:

- New machines added - AMI 386DX, MR 386DX
- New graphics cards - Plantronics ColorPlus, Wyse WY-700, Obsidian SB50, Voodoo 2
- CPU optimisations - up to 50% speedup seen
- 3DFX optimisations
- Improved joystick emulation - analogue joystick up to 8 buttons, CH Flightstick Pro, ThrustMaster FCS, SideWinder pad(s)
- Mouse can be selected between serial, PS/2, and IntelliMouse
- Basic 286/386 prefetch emulation - 286 & 386 performance much closer to real systems
- Improved CGA/PCjr/Tandy composite emulation
- Various bug fixes


PCem emulates the following machines:

IBM 5150 PC (1981) 
The original PC. This shipped in 1981 with a 4.77mhz 8088, 64k of RAM, and a cassette port.
Disc drives quickly became standard, along with more memory.

ROM files needed:

ibmpc\pc102782.bin
ibmpc\basicc11.f6
ibmpc\basicc11.f8
ibmpc\basicc11.fa
ibmpc\basicc11.fc


IBM 5160 XT (1983)
From a hardware perspective, this is a minor tweak of the original PC. It originally shipped
with 128k of RAM and a 10mb hard disc, both of which could be easily fitted to the 1981 machine.
However, this was targetted as businesses and was more successful than the original.

ROM files needed:

ibmxt\5000027.u19
ibmxt\1501512.u18


IBM PCjr (1984)
A home machine, which had better graphics and sound than most XTs but was not hardware compatible
with the PC.

ROM files needed:

ibmpcjr\bios.rom


IBM AT (1984)
This was the 'next generation' PC, fully 16-bit with an 80286. The original model came with a 6mhz
286, which ran three times as fast as the XT. This model also introduced EGA.

ROM files needed:

ibmat\at111585.0
ibmat\at111585.1


Olivetti M24 (1984)
An enhanced XT clone, also known as the AT&T PC 6300. Has an 8086 CPU, and an unusual 'double-res'
CGA display.

ROM files needed:

olivetti_m24\olivetti_m24_version_1.43_low.bin
olivetti_m24\olivetti_m24_version_1.43_high.bin


Tandy 1000 (1984)
This is a clone of the unsuccessful IBM PCjr, which added better graphics and sound to the XT,
but removed much expandability plus some other hardware (such as the DMA controller). The Tandy
puts back the DMA controller and ISA slots, making it a much more useful machine. Many games
from the late 80s support the Tandy.

ROM files needed:

tandy\tandy1t1.020


Tandy 1000HX (1987)
A Tandy 1000 with a faster 7.16 MHz 8088, and DOS in ROM.

ROM files needed:

tandy1000hx\v020000.u12


Tandy 1000SL/2 (1989)
An enhanced Tandy 1000 with a 9.54 MHz 8086, and enhanced graphics and sound.

ROM files needed:

tandy1000sl2\8079047.hu1
tandy1000sl2\8079048.hu2


DTK Clone XT (1986)
A generic clone XT board.

ROM files needed:

dtk\DTK_ERSO_2.42_2764.bin


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

pc1512\40043.v1
pc1512\40044.v2
pc1512\40078.ic127


Amstrad PC1640 (1987)
Amstrad's followup to the PC1512, the PC1640 had 640k of RAM and onboard EGA, but was otherwise
mostly the same.

ROM files needed:

pc1640\40043.v3
pc1640\40044.v3
pc1640\40100


Sinclair PC200/Amstrad PC20 (1988)
This was Amstrad's entry to the 16-bit home computer market, intended to compete with the Atari
ST and Commodore Amiga. It's similar to the PC1512, but is based on Amstrad's portable PPC512
system. With stock CGA and PC speaker, it couldn't compare with the ST or Amiga.

ROM files needed:

pc200\pc20v2.0
pc200\pc20v2.1
pc200\40109.bin


Schneider Euro PC (1988)
A German XT clone. An 'all-in-one' system like the Sinclair PC200. I don't know much about this
machine to be honest! This doesn't appear to work with the XTIDE BIOS, so therefore this is the
only model that does not support hard discs.

ROM files needed:

europc\50145
europc\50146


(c)Anonymous Generic Turbo XT BIOS (1988?)
This is a BIOS whose source code was made available on Usenet in 1988. It appears to be an 
anonymous BIOS from an XT clone board. It was then heavily modified to fix bugs. The history of
this BIOS (and the source code) is at http://dizzie.narod.ru/bios.txt

ROM files needed:

genxt\pcxt.rom


AMI XT clone (1989)

ROM files needed:

amixt\AMI_8088_BIOS_31JAN89.BIN


DTK XT clone (1988)

ROM files needed:

dtk\DTK_ERSO_2.42_2764.bin


VTech Laser Turbo XT (1987)

ROM files needed:

ltxt\27C64.bin


VTech Laser XT3 (1989)

ROM files needed:

lxt3\27C64D.bin


Phoenix XT clone (1986)

ROM files needed:

pxxt\000p001.bin


Juko XT clone (1988)

ROM files needed:

jukopc\000o001.bin


Commodore PC30-III (1988)
A fairly generic 286 clone.

ROM files needed:

cmdpc30\commodore pc 30 iii even.bin
cmdpc30\commodore pc 30 iii odd.bin


Amstrad PC2086 (1989)
The PC2086 is essentially a PC1640 with VGA and 3.5" floppy drives.

ROM files needed:

pc2086\40179.ic129
pc2086\40180.ic132
pc2086\40186.ic171


Amstrad PC3086 (1990)
The PC3086 is a version of the PC2086 with a more standard case.

ROM files needed:

pc3086\fc00.bin
pc3086\c000.bin


Dell System 200 (1990?)
This is a pretty generic 286 clone with a Phoenix BIOS.

ROM files needed:

dells200\dell0.bin
dells200\dell1.bin


AMI 286 clone (1990)
This is a generic 286 clone with an AMI BIOS.

ROM files needed:

ami286\amic206.bin


Award 286 clone (1990)
This is a generic 286 clone with an Award BIOS.

ROM files needed:

award286\award.bin


IBM PS/1 Model 2011 (1990)
This is a 286 with integrated VGA and a basic GUI and DOS 4.01 in ROM.

ROM files needed:

ibmps1\f80000.bin


Compaq Deskpro 386 (1989)
An early 386 system. I don't think this BIOS is from the original 1986 version
(the very first 386 system), but from a 1989 refresh.

ROM files needed:

deskpro386\109592-005.U11.bin
deskpro386\109591-005.U13.bin


IBM PS/1 Model 2121 (1990)
This is a 386SX with integrated VGA.

ROM files needed:

ibmps1_2121\fc0000.bin


Acermate 386SX/25N (1992?)
An integrated 386SX clone, with onboard Oak SVGA and IO.

ROM files needed:
acer386\acer386.bin
acer386\oti067.bin


DTK 386SX clone (1990)

ROM files needed:

dtk386\3cto001.bin


Amstrad MegaPC (1992)
A 386SX clone (otherwise known as the PC7386SX) with a built-in Sega Megadrive. Only the PC section
is emulated, obv.

ROM files needed:
megapc\41651-bios lo.u18
megapc\211253-bios hi.u19


AMI 386SX clone (1994)
This is a generic 386SX clone with an AMI BIOS.

ROM files needed:

ami386\ami386.bin


MR 386DX clone (1994)
This is a generic 386DX clone with an MR BIOS.

ROM files needed:

mr386dx\OPT495SX.MR


AMI 386DX clone (1994)
This is a generic 386DX clone with an AMI BIOS. This is the only currently emulated 386DX system
to support an external cache.

ROM files needed:

ami386dx\OPT495SX.AMI


AMI 486 clone (1993)
This is a generic 486 clone with an AMI BIOS. The BIOS came from my 486SX/25, bought in December
1993.

ROM files needed:

ami486\ami486.bin


AMI WinBIOS 486 clone (1994)
A 486 clone with a newer AMI BIOS.

ROM files needed:

win486\ali1429g.amw


Award SiS 496/497 (1995)
A 486 clone using the SiS 496/497 chipset, with PCI bus and Award BIOS.

ROM files needed:

sis496\SIS496-1.AWA


Intel Premiere/PCI (Batman's Revenge) (1994)
A Socket 4 based board with 430LX chipset.

ROM files needed:

revenge\1009AF2_.BI0
revenge\1009AF2_.BI1


Intel Advanced/EV (Endeavor) (1995)
A Socket 5/7 based board with 430FX chipset. The real board has a Sound Blaster Vibra 16 on board,
which is not emulated - use a discrete card instead. Some Advanced/EVs also had a Trio64 on board,
the emulated board does not have this either.

ROM files needed:

endeavor\1006CB0_.BI0
endeavor\1006CB0_.BI1


Award 430VX PCI (1996)
A generic Socket 5/7 board with 430VX chipset.

ROM files needed:

430vx\55XWUQ0E.BIN



PCem emulates the following graphics adapters :

MDA
The original PC adapter. This displays 80x25 text in monochrome.


Hercules
A clone of MDA, with the addition of a high-resolution 720x348 graphics mode.


CGA
The most common of the original adapters, supporting 40x25 and 80x25 text, and 
320x200 in 4 colours, 640x200 in 2 colours, and a composite mode giving 160x200
in 16 colours.


Plantronics ColorPlus
An enhanced CGA board, with support for 320x200x16 and 640x200x4.


Wyse WY-700
A CGA-compatible board, with support for a 1280x800 mode


IBM EGA
The original 1984 IBM EGA card, with 256k VRAM.

ROM files needed:
ibm_6277356_ega_card_u44_27128.bin


Hercules InColor
An enhanced Hercules with a custom 720x350 16 colour mode.


IBM VGA
The original VGA card.

ROM files needed:
ibm_vga.bin


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
A Mach64GX based board.

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


3DFX Voodoo Graphics
3D accelerator. Widely supported in late 90s games.

PCem emulates this in software. The emulation is a lot faster than in v10 (thanks to a new
dynamic recompiler) and should be capable of hitting Voodoo 1 performance on most machines
when two render threads are used. As before, the emulated CPU is the bottleneck for most games.

PCem can emulate 6 and 8 MB configurations, but defaults to 4 MB for compatibility. It can also
emulate the screen filter present on the original card, though this does at present have a
noticeable performance hit.

Almost everything I've tried works okay, with a very few exceptions - Screamer 2 and Rally have
serious issues.


Obsidian SB50
Voodoo Graphics with 2 TMUs. Drivers for this are a bit limited - the official drivers don't
support 2 TMUs in Direct3D.


Voodoo 2
Improved Voodoo Graphics - higher clocks, 2 TMUs, triangle setup etc.

PCem can emulate both 8 and 12 MB configurations. It can also emulate 6 and 10 MB configurations
(with 2MB framebuffer memory), which were never sold into the PC market but do exist for arcade
systems.


Some models have fixed graphics adapters :

IBM PCjr
CGA with various new modes - 160x200x16, 320x200x16, 640x200x4.

Olivetti M24
CGA with double-res text modes and a 640x400 mode. I haven't seen a dump of the font
ROM for this yet, so if one is not provided the MDA font will be used - which looks slightly odd
as it is 14-line instead of 16-line.

Tandy 1000
Clone of PCjr video. Widely supported in 80s games.

Tandy 1000 SL/2
Improvement of Tandy 1000, with support for 640x200x16.

Amstrad PC1512
CGA with a new mode (640x200x16). Only supported in GEM to my knowledge.

Amstrad PC1640
Paradise EGA.

Amstrad PC2086/PC3086
Paradise PVGA1. An early SVGA clone with 256kb VRAM.

IBM PS/1 Model 2011
Stock VGA with 256kb VRAM.

IBM PS/1 Model 2121
Basic (and unknown) SVGA with 256kb VRAM.

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
v11 now emulates the differences between the SN76496 (PCjr and Tandy 1000), and the NCR8496
(currently assigned to the Tandy 1000HX). Maniac Mansion and Zak McKraken will only sound
correct on the latter.

Tandy PSSJ
Used on the Tandy 1000SL/2, this clones the NCR8496, adding an addition frequency divider (did any
software actually use this?) and an 8-bit DAC.

PS/1 audio card
An SN76496 clone plus an 8-bit DAC. The SN76496 isn't at the same address as PCjr/Tandy, so most
software doesn't support it.

Gameblaster
The Creative Labs Gameblaster/Creative Music System, Creative's first sound card
introduced in 1987. Has two Philips SAA1099, giving 12 voices of square waves plus 4 noise
voices. In stereo!

Adlib
Has a Yamaha YM3812, giving 9 voices of 2 op FM, or 6 voices plus a rhythm section. PCem
uses the DOSBox dbopl emulator.

Adlib Gold
OPL3 with YM318Z 12-bit digital section. Possibly some bugs (not a lot of software to test).
The surround module is now emulated.

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
A standard 2 button PS/2 mouse. As with serial, compatible drivers are common.

Microsoft PS/2 Intellimouse
A PS/2 mouse with mouse wheel.

ATAPI CD-ROM
Works with OAKCDROM.SYS, VDD-IDE.SYS, and the internal drivers of every OS I've tried.


XTIDE :

The XTIDE board is emulated for machines that don't natively support IDE hard discs.

You will need to download the XTIDE BIOS seperately. Of the various versions, ide_at.bin and ide_xt.bin
should be placed in the ROMS directory. ide_xt is used on all XT models, and ide_at is used on the IBM AT
and Commodore PC30-III machines.

The BIOS is available at :

http://code.google.com/p/xtideuniversalbios/

v2.0.0 beta 1 is the version I've mostly tested. v2.0.0 beta 3 is known to have some issues.

For the PS/1, you will need v1.1.5. The PS/1 is a bit fussy with XTIDE, and I've found that it works best
when the XTIDE configuration has 'Full Operating Mode' disabled. This version must be called
ide_at_1_1_5.bin and should also be placed in the ROMS directory.


Notes :

- The time on the PC1512 clock is wrong. The date is correct, though since the PC1512's bios isn't
  Y2k compliant, it thinks it's 1988.

- The envelope system on the Gameblaster isn't emulated. The noise may not be right either.

- Some of the more unusual VGA features are not emulated. I haven't found anything that uses them yet.

- On some versions of Windows the AWE32 is not set up correctly, claiming a resource conflict. To correct
  this open the relevant item in Device Manager, choose 'Set Configuration Manually' and accept the
  options presented.


Software tested:

MS-DOS 1.25
MS-DOS 2.11
MS-DOS 3.31
Compaq DOS 3.31
MS-DOS 5.0
MS-DOS 6.22

DR-DOS 6.0

Windows 1.03
Windows 2.03
Windows/386 2.11
Windows 3.0
Windows 3.1
Windows for Workgroups 3.11
Windows 95
Windows 95 OSR 2
Windows 98
Windows 98 SE
Windows ME

Windows NT 3.51
Windows NT 4
Windows 2000
Windows XP

OS/2 v1.1
OS/2 v1.2
OS/2 v1.3
OS/2 v2.1
OS/2 Warp 3
OS/2 Warp 4

GEM 3.11

Corel Linux 1.2
Ubuntu 10.04 (very slow, does not support serial mouse)

BeOS 5.0 Professional

Adobe Photoshop 3.0.4
CorelDRAW! 2.5 (OS/2)
CorelDRAW! 5.0
FastTracker v2.08
Lotus 1-2-3 2.1 (OS/2)
Microsoft Excel 3.0 (OS/2)
Microsoft Word 1.1 (OS/2)
Microsoft Word for Windows 6.1
Microsoft Works for Windows 3.0
Microsoft Visual Basic 3.0
Microsoft Visual C++ 1.0
Microsoft Visual C++ 6.0
StarOffice 3.1 (OS/2)

Age of Empires
Alien vs Predator (3DFX)
Blake Stone
Breakneck (3DFX)
Caesar III
Civilization
Civilization II
Colin McRae Rally (3DFX)
Colonization
Command & Conquer : Red Alert
Command & Conquer : Red Alert 2
Commander Keen : Goodbye Galaxy
Commander Keen : Invasion of the Vorticons
Croc (3DFX)
Curse of Monkey Island
Dawn Patrol
Deus Ex (3DFX, slow)
Discworld II
Doom
Doom II
Duke Nukem 3D
Dune
Dune II
Epic Pinball
Grand Theft Auto (3DFX)
GTA 2 (3DFX)
Half-Life (3DFX)
Heretic
House of the Dead 2 (3DFX)
Icon
Incoming (3DFX)
Indiana Jones and the Last Crusade
Jetpack
King's Quest (PCjr, Tandy)
King's Quest II (Tandy)
Klotz
Lemmings
Lemmings 2
Little Big Adventure 2
Lotus III
Maniac Mansion
Microsoft Arcade
Monkey Island 2
Mystic Towers
Need for Speed II SE (3DFX)
Need for Speed III (3DFX)
Network Q RAC Rally
Oddworld : Abe's Oddysee
Overlord
Populous : The Beginning (3DFX)
Pro Pinball : Big Race USA
Pro Pinball : The Web
Psycho Pinball
Resident Evil 2 (3DFX)
Rollercoaster Tycoon
Quake (3DFX)
Quake II (3DFX)
Quake III Arena (3DFX)
Scorched Earth
Screamer
Screamer Rally (not 3DFX)
Secret of Monkey Island
SimCity (DOS, OS/2)
SimCity 2000 (DOs, Windows, OS/2)
SimCity 3000
Spear of Destiny
Star Goose
Stargunner
Stunt Island
System Shock
System Shock 2 (3DFX)
Terminal Velocity
The Humans
Theme Hospital
Tomb Raider (3DFX)
Tomb Raider III (3DFX)
Tony Hawk's Pro Skater 2 (3DFX)
Total Annihilation
Transport Tycoon
Turok (3DFX)
Turok 2 (3DFX)
Tyrian
UFO : Enemy Unknown
Unreal (3DFX)
Unreal Tournament (3DFX)
Wacky Wheels
Wing Commander
Wolfenstein 3D
World Cup 98 (3DFX)
Worms
Worms 2
X-Com : Apocalypse
X-Com : Terror From The Deep
X-Wing
Zone 66

Complex - Cyboman 2
EMF - Verses
Future Crew - Second Reality
Gazebo - Cyboman!
KFMF - Trip (3DFX)
Logic Design - Fashion
Orange - X14
Renaissance - Amnesia
Skull - Putrefaction
Tran - Ambience (flickery with a host display at 60 Hz)
Tran - Timeless
Triton - Crystal Dream
Triton - Crystal Dream II (visual artifacts in titles)
Ultraforce - Vectdemo
Witan - Witan House

BeebInC v0.99f
Fellow v0.33
Kgen98 v0.4b
PacifiST v0.48
SNES9x v0.96
vMac v0.1.9.1