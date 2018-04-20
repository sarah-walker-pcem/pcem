PCem v13.1

PCem is licensed under the GPL, see COPYING for more details.

Changes since v13:

- Minor recompiler tweak, fixed slowdown in some situations (mainly seen on Windows 9x just after booting)
- Fixed issues with PCJr/Tandy sound on some Sierra games
- Fixed plasma display on Toshiba 3100e
- Fixed handling of configurations with full stops in the name
- Fixed sound output gain when using OpenAL Soft
- Switched to using OpenAL Soft by default

Changes since v12:

- New machines added - Atari PC3, Epson PC AX, Epson PC AX2e, GW-286CT GEAR, IBM PS/2 Model 30-286,
  IBM PS/2 Model 50, IBM PS/2 Model 55SX, IBM PS/2 Model 80, IBM XT Model 286, KMX-C-02,
  Samsung SPC-4200P, Samsung SPC-4216P, Toshiba 3100e,
- New graphics cards - MDSI Genius, Plantronics ColorPlus
- New sound cards added - Disney Sound Source, Ensoniq AudioPCI (ES1371), LPT DAC, Sound Blaster PCI 128
- New hard drive controllers added - AT Fixed Disk Adapter, DTC 5150X, Fixed Disk Adapter (Xebec),
  IBM ESDI Fixed Disk Controller, Western Digital WD1007V-SE1
- New SCSI adapters added - Adaptec AHA-1542C, BusLogic BT-545S, Longshine LCS-6821N, Rancho RT1000B,
  Trantor T130B
- New network adapters added - NE2000 compatible
- New cross-platform GUI
- Voodoo SLI emulation
- Improvements to Sound Blaster emulation
- Improvements to Pentium timing
- Various bug fixes
- Minor optimisations

Thanks to AmatCoder, basic2004, bit, dns2k, ecksemess, Greatpsycho, hOMER247, James-F, John Elliott,
JosepMa, leilei, neozeed, ruben_balea, SA1988 and tomaszkam for contributions towards this release.


PCem emulates the following machines:

8088 based :

AMI XT clone (1989)

8088 at 8+ MHz, 64-640kb RAM.

ROM files needed:
 amixt\ami_8088_bios_31jan89.bin


Atari PC3 (19xx)

8088 at 8 MHz, 640kb RAM.

ROM files needed:
 ataripc3/AWARD_ATARI_PC_BIOS_3.08.BIN


Compaq Portable Plus (1983)

8088 at 4.77 MHz, 128-640kb RAM.

ROM files needed:
 compaq_pip/Compaq Portable Plus 100666-001 Rev C.bin


DTK Clone XT (1986)

8088 at 8/10 MHz, 64kb-640kb RAM.

ROM files needed:
 dtk/dtk_erso_2.42_2764.bin


(c)Anonymous Generic Turbo XT BIOS (1988?)

8088 at 8+ MHz, 64-640kb RAM.

ROM files needed:
 genxt\pcxt.rom


IBM PC (1981)

8088 at 4.77 MHz, 16-640kb RAM (PCem emulates minimum 64kb)

ROM files needed:
 ibmpc\pc102782.bin
 ibmpc\basicc11.f6
 ibmpc\basicc11.f8
 ibmpc\basicc11.fa
 ibmpc\basicc11.fc


IBM PCjr (1984)

8088 at 4.77 MHz, 64-640kb RAM (PCem emulates minimum 128kb), built-in 16 colour graphics, 3 voice sound.
Not generally PC compatible.

ROM files needed:
 ibmpcjr\bios.rom


IBM XT (1983)

8088 at 4.77 MHz, 64-640kb RAM

ROM files needed:
 ibmxt\5000027.u19
 ibmxt\1501512.u18


Juko XT clone (1988)

ROM files needed:

jukopc\000o001.bin


NCR PC4i (1985)

8088 at 4.77 MHz, 256-640kb RAM

ROM files needed:
 ncr_pc4i/NCR_PC4i_BIOSROM_1985.BIN


Phoenix XT clone (1986)

8088 at 8/10 MHz, 64kb-640kb RAM.

ROM files needed:
 pxxt\000p001.bin


Schneider Euro PC (1988)

8088 at 9.54 MHz, 512-640kb RAM.

ROM files needed:
 europc\50145
 europc\50146


Tandy 1000 (1984)

8088 at 4.77 MHz, 128kb-640kb RAM, built-in 16 colour graphics + 3 voice sound

ROM files needed:
 tandy\tandy1t1.020


Tandy 1000HX (1987)

8088 at 7.16 MHz, 256-640kb RAM, built-in 16 colour graphics + 3 voice sound.
Has DOS 2.11 in ROM.
A Tandy 1000 with a faster 7.16 MHz 8088, and DOS in ROM.

ROM files needed:
 tandy1000hx\v020000.u12


Thomson TO16 PC (1987)

8088 at 9.54 MHz, 512-640kb RAM.

ROM files needed:
 to16_pc/TO16_103.bin


Toshiba T1000 (1987)

8088 at 4.77 MHz, 512-1024 kb RAM, CGA on built-in LCD.

PCem maps [Fn] to right-Ctrl and right-Alt. The following functions are supported :
	Fn + Num Lock  - toggle numpad
	Fn + Home      - Internal LCD display
	Fn + Page Down - Turbo on
	Fn + Right     - Toggle LCD font
	Fn + End       - External CRT display
	Fn + SysRQ     - Toggle window

ROM files needed:
 t1000/t1000.rom
 t1000/t1000font.rom


VTech Laser Turbo XT (1987)

8088 at 10 MHz, 640kb RAM

ROM files needed:

ltxt\27c64.bin


Xi8088 (2015)

8088 at 4.77 - 13.33 MHz, 640kb RAM

ROM files needed:
 xi8088/bios-xi8088.bin


8086 based :


Amstrad PC1512 (1986)

8086 at 8 MHz, 512kb-640kb RAM, enhanced CGA (supports 640x200x16), custom mouse port.

ROM files needed:
 pc1512\40043.v1
 pc1512\40044.v2
 pc1512\40078.ic127


Amstrad PC1640 (1987)

8086 at 8 MHz, 640kb RAM, built-in Paradise EGA, custom mouse port.

ROM files needed:
 pc1640\40043.v3
 pc1640\40044.v3
 pc1640\40100


Amstrad PC2086 (1989)

8086 at 8 MHz, 640kb, built-in VGA.

ROM files needed:
 pc2086\40179.ic129
 pc2086\40180.ic132
 pc2086\40186.ic171


Amstrad PC3086 (1990)

8086 at 8 MHz, 640kb, built-in VGA.

ROM files needed:
 pc3086\fc00.bin
 pc3086\c000.bin


Olivetti M24 (1984)

8086 at 8 MHz, 128kb-640kb RAM, built-in enhanced CGA (supports 640x400x2).

ROM files needed:
 olivetti_m24\olivetti_m24_version_1.43_low.bin
 olivetti_m24\olivetti_m24_version_1.43_high.bin


Sinclair PC200/Amstrad PC20 (1988)

8086 at 8 MHz, 512-640kb RAM, built-in CGA.

ROM files needed:
 pc200\pc20v2.0
 pc200\pc20v2.1
 pc200\40109.bin


Tandy 1000SL/2 (1989)

8086 at 9.54 MHz, 512-768kb RAM, built-in 16 colour graphics + 4 voice sound.

ROM files needed:
 tandy1000sl2\8079047.hu1
 tandy1000sl2\8079048.hu2


Toshiba T1200 (1987)

8086 at 9.54 MHz, 1-2 MB RAM, CGA on built-in LCD.

PCem maps [Fn] to right-Ctrl and right-Alt. The following functions are supported :
	Fn + Num Lock  - toggle numpad
	Fn + Home      - Internal LCD display
	Fn + Page Down - Turbo on
	Fn + Right     - Toggle LCD font
	Fn + End       - External CRT display
	Fn + SysRQ     - Toggle window

ROM files needed:
 t1200/t1200_019e.ic15.bin
 t1200/t1000font.rom


VTech Laser XT3 (1988)

8086 at 10 MHz, 512-1152kb RAM.

ROM files needed:
 lxt3\27c64d.bin



286 based :


AMI 286 clone (1990)

286 at 8+ MHz, 512kb - 16MB RAM.

ROM files needed:
 ami286\amic206.bin


Award 286 clone (1990)

286 at 8+ MHz, 512kb - 16MB RAM.

ROM files needed:
 award286\award.bin


Commodore PC30-III (1988)

286 at 12 MHz, 512kb - 16MB RAM.

ROM files needed:
 cmdpc30\commodore pc 30 iii even.bin
 cmdpc30\commodore pc 30 iii odd.bin


Compaq Portable II (1986)

286 at 8 MHz, 256kb-15MB RAM.

ROM files needed:
 compaq_pii/109739-001.rom
 compaq_pii/109740-001.rom


Dell System 200 (1990?)

286 at 12 MHz, 640kb-16MB RAM.

ROM files needed:
 dells200\dell0.bin
 dells200\dell1.bin


Epson PC AX (1989)

286, 256kb - 16MB RAM.

ROM files needed :
 epson_pcax\EVAX
 epson_pcax\ODAX


Epson PC AX2e (1989)

286 at 12 MHz, 256kb - 16MB RAM.

ROM files needed :
 epson_pcax2e\EVAX
 epson_pcax2e\ODAX


GW-286CT GEAR

286 at 8+ MHz, 512kb - 16MB RAM.

ROM files needed:

 gw286ct/2ctc001.bin


IBM AT (1984)

286 at 6 or 8 MHz, 256kb-16mb RAM

ROM files needed:
 ibmat\at111585.0
 ibmat\at111585.1


IBM PS/1 Model 2011 (1990)

286 at 10 MHz, 512kb-16MB RAM, built-in VGA, DOS 4.01 + GUI menu system in ROM.

ROM files needed :
 ibmps1\f80000.bin


IBM PS/2 Model 30-286 (1988)

286 at 10 MHz, 1MB-16MB RAM, built-in VGA, MCA bus.

ROM files needed :
 ibmps2_m30_286\33f5381a.bin


IBM PS/2 Model 50 (1987)

286 at 10 MHz, 1MB-16MB RAM, built-in VGA, MCA bus.

ROM files needed :
 i8550021\90x7420.zm13
 i8550021\90x7423.zm14
 i8550021\90x7426.zm16
 i8550021\90x7429.zm18


IBM XT Model 286 (1986)

286 at 6 MHz, 256kb-16mb RAM

ROM files needed :
 ibmxt286\BIOS_5162_21APR86_U34_78X7460_27256.BIN
 ibmxt286\BIOS_5162_21APR86_U35_78X7461_27256.BIN


Samsung SPC-4200P

286 at 12 MHz, 512kb - 2MB RAM

ROM files needed :
spc4200p\u8.01


Samsung SPC-4216P

286 at 12 MHz, 1MB - 5MB RAM

ROM files needed :
 spc4216p\phoenix.bin
or
 spc4216p\7101.u8
 spc4216p\ac64.u10


Toshiba T3100e (1986)

286 at 12 MHz, 1MB - 5MB RAM, CGA on gas-plasma display

PCem maps [Fn] to right-Ctrl and right-Alt.

ROM files needed :
 t3100e\t3100e_font.bin
 t3100e\t3100e.rom



386SX based :


Acermate 386SX/25N (1992?)

386SX at 25 MHz, 2-16MB RAM, built-in Oak SVGA.

ROM files needed :
 acer386\acer386.bin
 acer386\oti067.bin


AMI 386SX clone (1994)

386SX at 25 MHz, 1-16MB RAM.

ROM files needed :
 ami386\ami386.bin


Amstrad MegaPC (1992)

386SX at 25 MHz, 1-16MB RAM, built-in VGA.

The original machine had a built-in Sega MegaDrive. This is not emulated in PCem.

ROM files needed :
 megapc\41651-bios lo.u18
 megapc\211253-bios hi.u19


DTK 386SX clone (1990)

386SX, 512kb - 16MB RAM.

ROM files needed :
 dtk386\3cto001.bin


Epson PC AX3 (1989)

386SX at 16 MHz, 256kb-16MB RAM.

ROM files needed :
 epson_pcax3\EVAX3
 epson_pcax3\ODAX3


IBM PS/1 Model 2121 (1990)

386SX at 20 MHz, 1MB-16MB RAM, built-in VGA.

ROM files needed:
 ibmps1_2121\fc0000.bin


IBM PS/2 Model 55SX (1989)

386SX at 16 MHz, 1MB-8MB RAM, built-in VGA, MCA bus.

ROM files needed :
 i8555081\33f8146.zm41
 i8555081\33f8145.zm40


KMX-C-02

386SX, 512kb-16MB RAM.

ROM files needed :
 kmxc02\3ctm005.bin


Packard Bell Legend 300SX (1992)

386 SX at 16 MHz, 1-16MB RAM

ROM files needed :
 pb_l300sx/pb_l300sx.bin



386DX based :


AMI 386DX clone (1994)

386DX at 40 MHz, 1-32MB RAM.

ROM files needed:
 ami386dx\opt495sx.ami


Compaq Deskpro 386 (1989)

386DX at 20 MHz, 1MB-15MB RAM.

ROM files needed:
 deskpro386\109592-005.u11.bin
 deskpro386\109591-005.u13.bin


IBM PS/2 Model 70 (type 3) (1989)

386DX at 25 MHz, 2-16MB RAM, built-in VGA, MCA bus.

ROM files needed :
 ibmps2_m70_type3/70-a_even.bin
 ibmps2_m70_type3/70-a_odd.bin


IBM PS/2 Model 80 (1987)

386DX at 20 MHz, 1MB-12MB RAM, built-in VGA, MCA bus.

ROM files needed :
 i8580111\15f6637.bin
 i8580111\15f6639.bin


MR 386DX clone (1994)
This is a generic 386DX clone with an MR BIOS.

ROM files needed:

mr386dx\opt495sx.mr



486 based :


AMI 486 clone (1993)

486 at 16-66 MHz, 1-32MB RAM.

ROM files needed:
 ami486\ami486.bin


AMI WinBIOS 486 clone (1994)

486 at 16-66 MHz, 1-32MB RAM.

ROM files needed:
 win486\ali1429g.amw


Award SiS 496/497 (1995)

486 at 16-120 MHz, Am5x86 at 133-160 MHz, Cx5x86 at 100-133 MHz, Pentium Overdrive
at 63-83 MHz. 1-64MB RAM.

ROM files needed:
 sis496\sis496-1.awa


Elonex PC-425X (1993)

486SX at 25 MHz, 1-256MB RAM, built-in Trident TGUI9440CXi

ROM files needed:
 elx_pc425x/elx_pc425x.bin
or
 elx_pc425x/elx_pc425x_bios.bin
 elx_pc425x/elx_pc425x_vbios.bin


IBM PS/2 Model 70 (type 4) (1990)

486DX at 25 MHz, 2-16MB RAM, built-in VGA, MCA bus.

PCem's FPU emulation is not bit accurate and can not pass IBM's floating point tests. As a result,
this machine will always print 12903 and 162 errors on bootup. These can be ignored - F1 will boot
the machine.

ROM files needed :
 ibmps2_m70_type3/70-a_even.bin
 ibmps2_m70_type3/70-a_odd.bin


Pentium based :


Award 430VX PCI (1996)

Pentium at 75-200 MHz, Pentium MMX at 166-233 MHz, Mobile Pentium MMX at 120-300 MHz,
Cyrix 6x86 at PR90(80 MHz)-PR200(200 MHz), Cyrix 6x86MX/MII at PR166(133 MHz)-
PR400(285 MHz), IDT WinChip at 75-240 MHz. 1-128MB RAM (PCem allows up to 256).

ROM files needed:
 430vx\55xwuq0e.bin


Intel Advanced/EV (Endeavor) (1995)

Pentium at 75-133 MHz, Pentium Overdrive at 125-200 MHz, 1-128MB RAM.

The real board has a Sound Blaster 16 onboard, and optionally an S3 Trio64V+. Neither
are emulated as onboard devices.

ROM files needed:
 endeavor\1006cb0_.bi0
 endeavor\1006cb0_.bi1


Intel Advanced/ZP (Zappa) (1995)

Pentium at 75-133 MHz, Pentium Overdrive at 125-200 MHz, 1-128MB RAM.

ROM files needed:
 zappa/1006bs0_.bio
 zappa/1006bs0_.bi1


Intel Premiere/PCI (Batman's Revenge) (1994)

Pentium at 60-66 MHz, Pentium OverDrive at 120-133 MHz.

ROM files needed:
 revenge\1009af2_.bi0
 revenge\1009af2_.bi1


Packard Bell PB520R (Robin LC) (1995)

Pentium at 60-66 MHz, Pentium OverDrive at 120-133 MHz, 1-128MB RAM, built-in Cirrus
Logic GD-5434.

ROM files needed:
 pb520r/1009bc0r.bio
 pb520r/1009bc0r.bi1
 pb520r/gd5434.bin


Packard Bell PB570 (Hillary) (1995)

Pentium at 75-133 MHz, Pentium Overdrive at 125-200 MHz, 1-128MB RAM, built-in Cirrus
Logic GD-5430.

ROM files needed:
 pb570/1007by0r.bio
 pb570/1007by0r.bi1
 pb570/gd5430.bin


PCem emulates the following graphics adapters :


MDA (1981)
80x25 monochrome text.


CGA (1981)
40x25 and 80x25 text, 320x200 in 4 colours, 620x200 in 2 colours. Supports composite
output for ~16 colours.


Hercules (1982)
80x25 monochrome text, 720x348 in monochrome.


Plantronics ColorPlus
An enhanced CGA board, with support for 320x200x16 and 640x200x4.


Wyse WY-700
A CGA-compatible board, with support for a 1280x800 mode


IBM EGA (1984)
Text up to 80x43, graphics up to 640x350 in 16 colours.

ROM files needed:
 ibm_6277356_ega_card_u44_27128.bin


Hercules InColor
An enhanced Hercules with a custom 720x350 16 colour mode.


Unaccelerated (S)VGA cards :


ATI Korean VGA
ATI-28800 based. 512kb VRAM, supports up to 8-bit colour. Korean font support.

ROM files needed:
 atikorvga.bin
 ati_ksc5601.rom


ATI VGA Edge-16
ATI-18800 based. 512kb VRAM, supports up to 8-bit colour.

ROM files needed:
 vgaedge16.vbi


ATI VGA Charger
ATI-28800 based. 512kb VRAM, supports up to 8-bit colour.

ROM files needed:
 bios.bin


IBM VGA (1987)
256kb VRAM, Text up to 80x50, graphics up to 320x200 in 256 colours or 640x480 in 16 colours.

ROM files needed:
 ibm_vga.bin


OAK OTI-037C
256kb VRAM, supports up to 8-bit colour.

ROM files needed:
 oti037\bios.bin


OAK OTI-067
256kb-512kb VRAM, supports up to 8-bit colour.

ROM files needed:
 oti067\bios.bin


Olivetti GO481 (Paradise PVGA1A)
256kb VRAM, supports up to 8-bit colour.

ROM files needed :
 oli_go481_lo.bin
 oli_go481_hi.bin


Trident 8900D SVGA
256kb-1MB VRAM, supports up to 24-bit colour.

ROM files needed:
 trident.bin


Trident TGUI9400CXi
1-2MB VRAM, supports up to 24-bit colour.

ROM files needed:
9440.vbi


Tseng ET4000AX SVGA
1MB VRAM, supports up to 8-bit colour.

ROM files needed:
 et4000.bin


2D accelerated SVGA cards :

ATI Graphics Pro Turbo
Mach64GX based. 1-4MB VRAM.

ROM files needed:
 mach64gx\bios.bin


ATI Video Xpression
Mach64VT2 based. 2-4MB VRAM. Has video acceleration.

ROM files needed :
 atimach64vt2pci.bin


Cirrus Logic GD-5429
1-2MB VRAM.

ROM files needed:
 5429.vbi


Cirrus Logic GD-5430
1-2MB VRAM.

ROM files needed:
 gd5430/pci.bin


Cirrus Logic GD-5434
2-4MB VRAM. Real chip also supports 1MB configurations, however this is not currently
supported in PCem.

ROM files needed:
 gd5434.bin


Diamond Stealth 32 SVGA
ET4000/W32p based. 1-2MB VRAM.

ROM files needed:
 et4000w32.bin


Number Nine 9FX
S3 Trio64 based. 1-2MB VRAM.

ROM files needed:
 s3_764.bin


Paradise Bahamas 64
S3 Vision864 based. 1-4MB VRAM.

ROM files needed:
 bahamas64.bin


Phoenix S3 Trio32
S3 Trio32 based. 512kb-2MB VRAM.

ROM files needed:
 86c732p.bin


Phoenix S3 Trio64
S3 Trio64 based. 1MB-4MB VRAM.

ROM files needed:
 86c764x1.bin


Trident TGUI9440
Trident TGUI9440 based. 1-2MB VRAM.

ROM files needed:
9440.vbi



3D accelerated SVGA cards :


Diamond Stealth 3D 2000
S3 ViRGE/325 based. 2-4MB VRAM.

PCem emulates the ViRGE S3D engine in software. This works with most games I tried, but
there may be some issues. The Direct3D drivers for the /325 are fairly poor (often showing
as missing triangles), so use of the /DX instead is recommended.

The streams processor (video overlay) is also emulated, however many features are missing.

ROM files needed:
 s3virge.bin


S3 ViRGE/DX
S3 ViRGE/DX based. 2-4MB VRAM.

The drivers that come with Windows are similar to those for the /325, however better ones
do exist (try the 8-21-1997 version). With the correct drivers, many early Direct3D games
work okay (if slowly).

ROM files needed:
86c375_1.bin


3D only cards :

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

Amstrad MegaPC
Paradise 90C11. A development of the PVGA1, with 512kb VRAM. Can use external video card.

Acer 386SX/25N
Oak OTI-067. Another 512kb SVGA clone. Can use external video card.

Amstrad PC1512
CGA with a new mode (640x200x16).

Amstrad PC1640
Paradise EGA. Can use external video card.

Amstrad PC2086/PC3086
Paradise PVGA1. An early SVGA clone with 256kb VRAM. Can use external video card.

Elonex PC-425X
Trident TGUI9400CXi with 512kb VRAM.

IBM PCjr
CGA with various new modes - 160x200x16, 320x200x16, 640x200x4.

IBM PS/1 Model 2011
Stock VGA with 256kb VRAM.

IBM PS/1 Model 2121
Basic (and unknown) SVGA with 256kb VRAM.

IBM PS/2 machines
Stock VGA with 256kb VRAM.

Olivetti M24
CGA with double-res text modes and a 640x400 mode. I haven't seen a dump of the font
ROM for this yet, so if one is not provided the MDA font will be used - which looks slightly odd
as it is 14-line instead of 16-line.

Packard Bell PB520R
Cirrus Logic GD-5434. Can use external video card.

Packard Bell PB570
Cirrus Logic GD-5430. Can use external video card.

Sinclair PC200
CGA. Can use external video card.

Tandy 1000
Clone of PCjr video. Widely supported in 80s games.

Tandy 1000 SL/2
Improvement of Tandy 1000, with support for 640x200x16.

Toshiba T-series
CGA on built-in LCD or plasma display.



PCem emulates the following sound devices :

PC speaker
The standard beeper on all PCs. Supports samples/RealSound.

Tandy PSG
The Texas Instruments chip in the PCjr and Tandy 1000. Supports 3 voices plus
noise. I reused the emulator in B-em for this (slightly modified).
PCem emulates the differences between the SN76496 (PCjr and Tandy 1000), and the NCR8496
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

Ensoniq AudioPCI (ES1371) / Sound Blaster PCI 128
Basic PCI sound card. Emulates Sound Blaster in software.


PCem emulates the following hard drive controllers :

[MFM] Fixed Disk Adapter (Xebec)
MFM controller originally included in the IBM XT. This controller only supports HD types 0, 2, 13
and 16.

ROM files needed :
ibm_xebec_62x0822_1985.bin


[MFM] DTC 5150X
8-bit MFM controller. To configure drive types, run DEBUG.EXE and enter 'g=c800:5'.

ROM files needed :
dtc_cxd21a.bin


[MFM] AT Fixed Disk Adapter
MFM controller originally included in the IBM AT. Supported by all AT-compatible BIOSes.


[ESDI] Western Digital WD1007V-SE1
16-bit ESDI controller.

ROM files needed :
62-000279-061.bin


[ESDI] IBM ESDI Fixed Disk Controller
MCA ESDI controller. Only supported on PS/2 models.

ROM files needed :
90x8969.bin
90x8970.bin


[IDE] Standard IDE
Standard IDE controller. Supported by all AT-compatible BIOSes. Use this if in any doubt!


[IDE] XTIDE
8-bit IDE controller. The BIOS is available at :

http://code.google.com/p/xtideuniversalbios/

v2.0.0 beta 1 is the version I've mostly tested. v2.0.0 beta 3 is known to have some issues.

ROM files needed :
ide_xt.bin


[IDE] XTIDE (AT)
16-bit IDE controller.

ROM files needed :
ide_at.bin


[IDE] XTIDE (PS/1)
IDE controller for the PS/1 model 2033. For this machine you will need BIOS version v1.1.5. The
PS/1 is a bit fussy with XTIDE, and I've found that it works best when the XTIDE configuration
has 'Full Operating Mode' disabled.

ROM files needed :
ide_at_1_1_5.bin


[SCSI] Longshine LCS-6821N
8-bit SCSI controller.

ROM files needed :
Longshine LCS-6821N - BIOS version 1.04.bin


[SCSI] Rancho RT1000B
8-bit SCSI controller.

ROM files needed :
Rancho_RT1000_RTBios_version_8.10R.bin


[SCSI] Trantor T130B
8-bit SCSI controller.

ROM files needed :
trantor_t130b_bios_v2.14.bin


[SCSI] Adaptec AHA-1542C
16-bit SCSI controller.

ROM files needed :
adaptec_aha1542c_bios_534201-00.bin


[SCSI] BusLogic BT-545S
16-bit SCSI controller.

ROM files needed :
BusLogic_BT-545S_U15_27128_5002026-4.50.bin


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





Software tested:

PC-DOS 1.0
PC-DOS 2.10
MS-DOS 3.30
MS-DOS 5.0
MS-DOS 6.0
MS-DOS 6.22

DR-DOS 6.0

Windows 1.03
Windows 2.03
Windows/286 2.11
Windows/386 2.11
Windows 3.0
Windows 3.1
Windows for Workgroups 3.11
Windows 95
Windows 95 OSR2
Windows 98
Windows 98 SE
Windows ME

Windows NT 3.1
Windows NT 3.51
Windows NT 4
Windows 2000
Windows XP

OS/2 v1.0
OS/2 v1.1
OS/2 v1.2
OS/2 v1.3
OS/2 v2.0
OS/2 Warp 3
OS/2 Warp 4

Red Hat Linux 7.1 (Seawolf)
Ubuntu 10.04

BeOS 5 Personal
BeOS 5 Professional

DESQview/X

GEM Desktop 3.11

Borland C++ v3.1
Fasttracker v2.08
Internet Explorer 5.0
Microsoft Office 95
Microsoft Visual Basic 3.0
Microsoft Word for OS/2 1.1
Microsoft Word for Windows 1.1
Microsoft Works for Windows 3.0
Micrografx Windows Draw 1.04
PC Paintbrush for Windows v1.05
Ventura Publisher v3.0

Age of Empires
Aladdin
Alien vs Predator (3DFX)
All New World of Lemmings
Alone in the Dark
American McGee's Alice (3DFX)
Arkanoid
Beasts and Bumpkins
Blake Stone
BreakNeck (3DFX)
Caesar III
Cannon Fodder 2
Civilization
Civilization II
Colin McRae Rally (3DFX)
Colonization
Command & Conquer : Red Alert
Command & Conquer : Red Alert 2
Commander Keen : Goodbye Galaxy
Commander Keen : Invasion of the Vorticons
Corridor 7
Croc (3DFX)
Curse of Monkey Island
Dawn Patrol
Descent Freespace (3DFX)
Deus Ex (3DFX)
Discworld II
Doom
Doom II
Duke Nukem 3D
Dune
Dune II
Ecstatica
Epic Pinball
Expendable (3DFX)
Final Fantasy VII (3DFX)
Formula 1 '97 (3DFX)
Freespace 2 (3DFX)
G-Police (3DFX)
Grim Fandango (3DFX)
Grand Theft Auto (3DFX)
GTA2 (3DFX)
Half-Life (3DFX)
Heartlight
Hitchhiker's Guide to the Galaxy
Hocus Pocus
House of the Dead 2 (3DFX)
Incoming (3DFX)
International Rally Championship (3DFX)
Interstate 76
Jazz Jackrabbit
Jazz Jackrabbit 2
Jedi Knight (3DFX)
Jill of the Jungle
LBA 2
Lode Runner : The Legend Returns
Kings Quest
Microsoft Arcade
MiG-29 Fulcrum (3DFX)
Monkey Island 2
Monster Bash
Mortal Kombat
Mystic Towers
Need for Speed II SE (3DFX)
Need for Speed III (3DFX)
Network Q RAC Rally
No One Lives Forever (3DFX)
Oddworld : Abe's Oddysee
Pinball Fantasies
Police Quest
Power Drive
Prince of Persia
Pro Pinball : Big Race USA
Pro Pinball : Timeshock!
Psycho Pinball
Quake (3DFX)
Quake II (3DFX)
Quake III Arena (3DFX)
Railroad Tycoon II
Rayman
Rebel Moon Rising
Resident Evil 2 (3DFX)
Return Fire II (3DFX)
Return to Castle Wolfenstein (3DFX)
Rise of the Triad
Rollercoaster Tycoon
Rollercoaster Tycoon 2
Sam & Max Hit the Road
Screamer
Screamer Rally (not 3DFX)
Secret of Monkey Island
Sensible World of Soccer        
Sid Meier's Alpha Centauri
SimCity 2000
SimCity 3000
SiN (3DFX)
Sonic & Knuckles Collection
Star Wars Episode 1 Racer (3DFX)
Stargunner
System Shock
System Shock 2 (3DFX - SLOW)
The Chaos Engine
The Humans
The Lion King
Theme Hospital
Tie Fighter
TOCA 2 (3DFX)
Tomb Raider (3DFX, ViRGE)
Tomb Raider II (3DFX)
Tomb Raider III (3DFX)
Tony Hawk's Pro Skater 2 (3DFX)
Total Annihilation
Transport Tycoon
Trespasser (3DFX)
Turok (3DFX)
Turok 2 (3DFX)
Turrican II
Tyrian
Unreal (3DFX)
Unreal Tournament (3DFX)
Wacky Wheels
Wing Commander
Wizkid
Wolfenstein 3D
World Cup 98 (3DFX)
Worms
Worms World Party
X-Com : Apocalypse
X-Com : Terror From The Deep
Xenon
Xenon 2
Zak McKracken
Zone 66

Cascada - Cronologia
Complex - Cyboman 2
EMF - Verses
Future Crew - Second Reality
Gazebo - Cyboman!
Hornet - Introjr
KFMF - Dance, Move, Shake
KFMF - Trip (3DFX)
Logic Design - Fashion
Orange - x14
Renaissance - Amnesia
Skull - Putre Faction
Tran - Ambience
Tran - Luminati
Tran - Timeless
Triton - Crystal Dream
Triton - Crystal Dream II
Ultraforce - Coldcut
Ultraforce - Vectdemo

BeebInC v0.99f
Fellow v0.33
KGen98 v0.4b
PaCifiST v0.45
ZSNES v0.800
SNES9x v0.96