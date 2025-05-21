# [PCem](https://pcem-emulator.co.uk/)
![Test Debug Builds](https://github.com/sarah-walker-pcem/pcem/actions/workflows/test-debug-builds.yml/badge.svg)
![Test Release Builds](https://github.com/sarah-walker-pcem/pcem/actions/workflows/test-release-builds.yml/badge.svg)
## Download: [Windows](https://pcem-emulator.co.uk/files/PCemV17Win.zip)/[Linux](https://pcem-emulator.co.uk/files/PCemV17Linux.tar.gz)/[vNext (Wise Global Solutions Mirror)](https://mirror.wiseglobalsolutions.com/pcem/)

Latest version: <b>v17</b> [Changelog](CHANGELOG.md)

PCem is licensed under GPL v2.0, see [COPYING](COPYING) for more details.

You can submit patches on our [forum](https://pcem-emulator.co.uk/phpBB3). Before you do, please note the [guidelines](https://pcem-emulator.co.uk/phpBB3/viewtopic.php?f=3&t=5) for submitting patches.

:exclamation: Note: <b>NO COPYRIGHTED ROM FILES ARE INCLUDED NOR WILL THEY BE. PLEASE DO NOT ASK FOR THEM.</b>

## BSD and Linux supplement

You will need the following libraries and buildtools (and their dependencies):
- SDL2
- wxWidgets 3.x
- OpenAL
- CMake
- Ninja (Recommended, but you can use a Makefile generator if you prefer)

Open a terminal window, navigate to the PCem directory then enter:
### Linux/BSD
```
cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Release .
ninja
```

### Windows (MSYS2)
```
cmake -G "Ninja" -DMSYS=TRUE -DCMAKE_BUILD_TYPE=Release .
ninja
```

then `./src/pcem` to run.

The Linux/BSD versions store BIOS ROM images, configuration files, and other data in `~/.pcem`

You can specify the Display Engine using `-DPCEM_DISPLAY_ENGINE=` The only valid option you have at this time is
wxWidgets 

The configure options are specified below. They are in the format of -D`Option`=`Value`. `Value` under here is the
default value.
```
  -DCMAKE_BUILD_TYPE=Release : Generate release build. Recommended for regular use.
  -DCMAKE_BUILD_TYPE=Debug   : Compile with debugging enabled.
  -DUSE_NETWORKING=ON        : Build with networking support.
  -DUSE_PCAP_NETWORKING=ON   : Build with pcap networking support. (Needs USE_NETWORKING to compile) Requires libpcap.
  -DUSE_ALSA=OFF             : Build with support for MIDI output through ALSA. Requires libasound. (Linux Only)
  -DFORCE_X11=ON             : Enables a hack to force X11 on Wayland systems. See #128 for details. (Linux Only)
  -DPLUGIN_ENGINE=ON         : Build with plugin support. Builds libpcem-plugin-api and links PCem with it. 
```

If you are using -DCMAKE_BUILD_TYPE=Debug, there are some more debug options you can enable if needed
```
  -DPCEM_SLIRP_DEBUG=OFF           : Build PCem with SLIRP_DEBUG debug output
  -DPCEM_RECOMPILER_DEBUG=OFF      : Build PCem with RECOMPILER_DEBUG debug output
  -DPCEM_NE2000_DEBUG=OFF          : Build PCem with NE2000_DEBUG debug output
  -DPCEM_EMU8K_DEBUG_REGISTERS=OFF : Build PCem with EMU8K_DEBUG_REGISTERS debug output
  -DPCEM_SB_DSP_RECORD_DEBUG=OFF   : Build PCem with SB_DSP_RECORD_DEBUG debug output
  -DPCEM_MACH64_DEBUG=OFF          : Build PCem with MACH64_DEBUG debug output
  -DPCEM_DEBUG_EXTRA=OFF           : Build PCem with DEBUG_EXTRA debug output
```

If you are using -DCMAKE_BUILD_TYPE=RelWithDebInfo, there are additional options you can do
```
  -DPCEM_RELDEB_AS_RELEASE=ON     : Builds RelWithDebInfo with debugging logging enabled when this is off
```

They are some extra modules you can add if you build with `-DUSE_EXPERIMENTAL=ON`. These modules are untested.
incomplete, and may or may not be in a future build of PCem. We do not provide builds with these enabled as
well. It is also possible they may not even build.
```
  -DUSE_EXPERIMENTAL_PGC=ON       : Build PCem with Professional Graphics Controller support.
  -DUSE_EXPERIMENTAL_PRINTER=ON   : Build PCem with Printer support. Requires freetype.
``` 
The menu is a pop-up menu in the Linux/BSD port. Right-click on the main window when mouse is not
captured.

CD-ROM support currently only accesses `/dev/cdrom`. It has not been heavily tested.

## Links

### PCem emulates the following hardware:

Hardware | Links
--- | ---
Systems | [8088](#8088-based)<br/>[8086](#8086-based)<br/>[286](#286-based)<br/>[386](#386-based)<br/>[486](#486-based)<br/>[Pentium](#pentium-based)<br/>[Super Socket 7](#super-socket-7-based)
Graphics | [Basic](#basic-cards)<br/>[Unaccelerated (S)VGA cards](#unaccelerated-svga-cards)<br/>[2D accelerated SVGA cards](#2d-accelerated-svga-cards)<br/>[3D accelerated SVGA cards](#3d-accelerated-svga-cards)<br/>[3D only cards](#3d-only-cards)
Sound | [Cards](#sound-cards)
HDD Controller | [Cards](#hdd-controller-cards)
Misc | [Cards](#misc-cards)

### [Software Tested](TESTED.md) (list)
- [DOS](TESTED.md#dos)<br/>
- [Windows](TESTED.md#windows)<br/>
- [Windows NT](TESTED.md#windows-nt)<br/>
- [OS/2](TESTED.md#os2)<br/>
- [Linux](TESTED.md#linux)<br/>
- [Applications](TESTED.md#applications)<br/>
- [Games](TESTED.md#games)<br/>
- [Demos](TESTED.md#demos)<br/>
- [Emulators](TESTED.md#emulators)<br/>

<hr>

## Systems

### 8088 based
Release | Machine | ROM file needed<br/>(within ./roms/ folder)
:-: | --- | ---
1981 | <b>IBM PC</b><br/>8088 at 4.77 MHz<br/>16KB - 640KB RAM (min. 64KB) | ibmpc/pc102782.bin<br/>ibmpc/basicc11.f6<br/>ibmpc/basicc11.f8<br/>ibmpc/basicc11.fa<br/>ibmpc/basicc11.fc
1983 | <b>Compaq Portable Plus</b><br/>8088 at 4.77 MHz<br/>128KB - 640KB RAM | compaq_pip/Compaq Portable Plus 100666-001 Rev C.bin
1983 | <b>IBM XT</b><br/>8088 at 4.77 MHz<br/>64KB - 640KB RAM | ibmxt/5000027.u19<br/>ibmxt/1501512.u18
1983 | <b>Leading Edge Model M</b><br/>8088 at 7.16 MHz<br/>128KB - 704KB RAM | leadingedge_modelm/Leading Edge - Model M - BIOS ROM - Version 4.71.bin
1984 | <b>IBM PCjr</b> <i>[[5]](#system-note-5)</i><br/>8088 at 4.77 MHz<br/>64KB - 640KB RAM (min. 128KB)<br/>Built-in 16 colour graphics<br/>3 voice sound<br/>Not generally PC compatible. | ibmpcjr/bios.rom
1984 | <b>Tandy 1000</b> <i>[[5]](#system-note-5)</i><br/>8088 at 4.77 MHz<br/>128KB - 640KB RAM<br/>Built-in 16 colour graphics<br/>3 voice sound | tandy/tandy1t1.020
1985 | <b>Commodore PC10</b><br/>8088 at 4.77 MHz<br/>640KB RAM CGA/Monochrome | cbm_pc10/cbm-pc10c-bios-v4.41-318085-08.bin
1985 | <b>NCR PC4i</b><br/>8088 at 4.77 MHz<br/>256KB - 640KB RAM | ncr_pc4i/NCR_PC4i_BIOSROM_1985.BIN
1986 | <b>DTK Clone XT</b><br/>8088 at 8/10 MHz<br/>64KB - 640KB RAM | dtk/dtk_erso_2.42_2764.bin
1986 | <b>Phoenix XT clone</b><br/>8088 at 8/10 MHz<br/>64KB - 640KB RAM | pxxt/000p001.bin
1987 | <b>Hyundai Super 16T</b><br/>8088/8087 at 4.77/8.0 MHz<br/>640KB RAM<br/>EGA 16 colour graphics<br/> | super16t/hyundai-super-16t-system-bios-hea-v1.12ta.bin
1987 | <b>Tandy 1000HX</b><br/>8088 at 7.16 MHz<br/>256KB - 640KB RAM<br/>Built-in 16 colour graphics<br/>3 voice sound<br/>Has DOS 2.11 in ROM | tandy1000hx/v020000.u12
1987 | <b>Thomson TO16 PC</b><br/>8088 at 9.54 MHz<br/>512KB - 640KB RAM | to16_pc/TO16_103.bin
1987 | <b>Toshiba T1000</b> <i>[[1]](#system-note-1)</i> <i>[[5]](#system-note-5)</i><br/>8088 at 4.77 MHz<br/>512KB - 1024KB RAM<br/>CGA on built-in LCD | t1000/t1000.rom<br/>t1000/t1000font.rom
1987 | <b>VTech Laser Turbo XT</b><br/>8088 at 10 MHz<br/>640KB RAM | ltxt/27c64.bin
1987 | <b>Zenith Data SupersPort</b><br/>8088 at 8 MHz<br/>128KB - 640KB RAM<br/>Built-in LCD video is not currently emulated | zdsupers/z184m v3.1d.10d
1988? | <b>&#169;Anonymous Generic Turbo XT BIOS</b><br/>8088 at 8+ MHz<br/>64KB - 640KB RAM | genxt/pcxt.rom
1988 | <b>Atari PC3</b><br/>8088 at 8 MHz<br/>640KB RAM | ataripc3/AWARD_ATARI_PC_BIOS_3.08.BIN
1988 | <b>Juko XT clone</b> | jukopc/000o001.bin
1988 | <b>Schneider Euro PC</b><br/>8088 at 9.54 MHz<br/>512KB - 640KB RAM | europc/50145<br/>europc/50146
1989 | <b>AMI XT clone</b><br/>8088 at 8+ MHz<br/>64KB - 640KB RAM | amixt/ami_8088_bios_31jan89.bin
1989 | <b>Hyundai Super 16TE</b><br/>8088/8087 at 10 MHz<br/>640KB RAM<br/>EGA 16 colour graphics<br/> | super16te/hyundai-super-16te-system-bios-v2.00id.bin
2015 | <b>Xi8088</b><br/>8088 at 4.77-13.33 MHz<br/>640KB RAM | xi8088/bios-xi8088.bin

### 8086 based
Release | Machine | ROM file needed<br/>(within ./roms/ folder)
:-: | --- | ---
1984 | <b>Compaq Deskpro</b><br/>8086 at 8 MHz<br/>128KB - 640KB RAM | deskpro/Compaq - BIOS - Revision J - 106265-002.bin
1984 | <b>Olivetti M24</b> <i>[[5]](#system-note-5)</i><br/>8086 at 8 MHz<br/>128KB - 640KB RAM<br/>Built-in enhanced CGA (supports 640x400x2) | olivetti_m24/olivetti_m24_version_1.43_low.bin<br/>olivetti_m24/olivetti_m24_version_1.43_high.bin
1986 | <b>Amstrad PC1512</b> <i>[[5]](#system-note-5)</i><br/>8086 at 8 MHz<br/>512KB - 640KB RAM<br/>Enhanced CGA (supports 640x200x16)<br/>Custom mouse port | pc1512/40043.v1<br/>pc1512/40044.v2<br/>pc1512/40078.ic127
1987 | <b>Amstrad PC1640</b> <i>[[5]](#system-note-5)</i><br/>8086 at 8 MHz<br/>640KB RAM<br/>Built-in Paradise EGA<br/>Custom mouse port | pc1640/40043.v3<br/>pc1640/40044.v3<br/>pc1640/40100
1987 | <b>Toshiba T1200</b> <i>[[1]](#system-note-1)</i> <i>[[5]](#system-note-5)</i><br/>8086 at 9.54 MHz<br/>1MB - 2MB RAM<br/>CGA on built-in LCD | t1200/t1200_019e.ic15.bin<br/>t1200/t1000font.rom
1988 | <b>Amstrad PPC512/640</b> <i>[[5]](#system-note-5)</i><br/>8086 at 8 MHz<br/>512KB - 640KB RAM<br/>Built-in CGA w/ plasma display | ppc512/40107.v2<br/>ppc512/40108.v2<br/>ppc512/40109.bin
1988 | <b>Sinclair PC200/Amstrad PC20</b> <i>[[5]](#system-note-5)</i><br/>8086 at 8 MHz<br/>512KB - 640KB RAM<br/>Built-in CGA (supports TV-out 50hz PAL) | pc200/pc20v2.0<br/>pc200/pc20v2.1<br/>pc200/40109.bin
1988 | <b>VTech Laser XT3</b><br/>8086 at 10 MHz<br/>512KB - 1152KB RAM | lxt3/27c64d.bin
1989 | <b>Amstrad PC2086</b> <i>[[5]](#system-note-5)</i><br/>8086 at 8 MHz<br/>640KB RAM<br/>Built-in VGA | pc2086/40179.ic129<br/>pc2086/40180.ic132<br/>pc2086/40186.ic171
1989 | <b>Tandy 1000SL/2</b> <i>[[5]](#system-note-5)</i><br/>8086 at 9.54 MHz<br/>512KB - 768KB RAM<br/>Built-in 16 colour graphics<br/>4 voice sound | tandy1000sl2/8079047.hu1<br/>tandy1000sl2/8079048.hu2
1990 | <b>Amstrad PC3086</b> <i>[[5]](#system-note-5)</i><br/>8086 at 8 MHz<br/>640KB RAM<br/>Built-in VGA | pc3086/fc00.bin<br/>pc3086/c000.bin
1991 | <b>Amstrad PC5086</b><br/>8086 at 8 MHz<br/>640KB RAM | pc5086/sys_rom.bin

### 286 based
Release | Machine | ROM file needed<br/>(within ./roms/ folder)
:-: | --- | ---
1984 | <b>IBM AT</b><br/>286 at 6 or 8 MHz<br/>256KB - 16MB RAM | ibmat/at111585.0<br/>ibmat/at111585.1
1986 | <b>Compaq Portable II</b><br/>286 at 8 MHz<br/>256KB - 15MB RAM | compaq_pii/109739-001.rom<br/>compaq_pii/109740-001.rom
1986 | <b>IBM XT Model 286</b><br/>286 at 6 MHz<br/>256KB - 16MB RAM | ibmxt286/BIOS_5162_21APR86_U34_78X7460_27256.BIN<br/>ibmxt286/BIOS_5162_21APR86_U35_78X7461_27256.BIN
1986 | <b>Toshiba T3100e</b> <i>[[1]](#system-note-1)</i> <i>[[5]](#system-note-5)</i><br/>286 at 12 MHz<br/>1MB - 5MB RAM<br/>CGA on gas-plasma display | t3100e/t3100e_font.bin<br/>t3100e/t3100e.rom
1987 | <b>IBM PS/2 Model 50</b> <i>[[5]](#system-note-5)</i><br/>286 at 10 MHz<br/>1MB - 16MB RAM<br/>Built-in VGA<br/>MCA bus | i8550021/90x7420.zm13<br/>i8550021/90x7423.zm14<br/>i8550021/90x7426.zm16<br/>i8550021/90x7429.zm18
1988 | <b>Bull Micral 45</b><br/>286 at 12 MHz<br/>1MB - 6MB RAM | bull_micral_45/even.fil<br/>bull_micral_45/odd.fil
1988 | <b>Commodore PC30-III</b><br/>286 at 12 MHz<br/>512KB - 16MB RAM | cmdpc30/commodore pc 30 iii even.bin<br/>cmdpc30/commodore pc 30 iii odd.bin
1988 | <b>IBM PS/2 Model 30-286</b><br/>286 at 10 MHz<br/>1MB - 16MB RAM<br/>Built-in VGA<br/>MCA bus | ibmps2_m30_286/33f5381a.bin
1989 | <b>Epson PC AX</b><br/>286<br/>256KB - 16MB RAM | epson_pcax/EVAX<br/>epson_pcax/ODAX
1989 | <b>Epson PC AX2e</b><br/>286 at 12 MHz<br/>256KB - 16MB RAM | epson_pcax2e/EVAX<br/>epson_pcax2e/ODAX
1990 | <b>AMI 286 clone</b><br/>286 at 8+ MHz<br/>512KB - 16MB RAM | ami286/amic206.bin
1990 | <b>Award 286 clone</b><br/>286 at 8+ MHz<br/>512KB - 16MB RAM | award286/award.bin
1990 | <b>Dell System 200</b><br/>286 at 12 MHz<br/>640KB - 16MB RAM | dells200/dell0.bin<br/>dells200/dell1.bin
1990 | <b>IBM PS/1 Model 2011</b> <i>[[5]](#system-note-5)</i><br/>286 at 10 MHz<br/>512KB - 16MB RAM<br/>Built-in VGA<br/>DOS 4.01 + GUI menu system in ROM | ibmps1/f80000.bin
? | <b>Goldstar GDC-212M</b><br/>286 at 12 MHz<br/>512KB - 4MB RAM | gdc212m/gdc212m_72h.bin
? | <b>GW-286CT GEAR</b><br/>286 at 8+ MHz<br/>512KB - 16MB RAM | gw286ct/2ctc001.bin
? | <b>Hyundai Super-286TR</b><br/>286 at 12 MHz<br/>1MB - 4MB RAM | super286tr/award.bin
? | <b>Samsung SPC-4200P</b><br/>286 at 12 MHz<br/>512KB - 2MB RAM | spc4200p/u8.01
? | <b>Samsung SPC-4216P</b><br/>286 at 12 MHz<br/>1MB - 5MB RAM | spc4216p/phoenix.bin<br/>&nbsp;&nbsp;&nbsp;<i>or</i><br/>spc4216p/7101.u8<br/>spc4216p/ac64.u10
? | <b>Samsung SPC-4620P</b><br/>286 at 12 MHz<br/>1MB - 5MB RAM<br/>Built-in Korean ATI-28800 | spc4620p/31005h.u8<br/>spc4620p/31005h.u10<br/>spc4620p/svb6120a_font.rom<br/>spc4620p/31005h.u8<br/>spc4620p/31005h.u10
? | <b>Tulip AT Compact</b><br/>286<br/>640KB - 16MB RAM | tulip_tc7/tc7be.bin<br/>tulip_tc7/tc7bo.bin

### 386 based
Release | Machine | ROM file needed<br/>(within ./roms/ folder)
:-: | --- | ---
1987 | <b>IBM PS/2 Model 80</b> <i>[[5]](#system-note-5)</i><br/>386DX at 20 MHz<br/>1MB - 12MB RAM<br/>Built-in VGA<br/>MCA bus | i8580111/15f6637.bin<br/>i8580111/15f6639.bin
1988 | <b>ECS 386/32</b><br/>386DX at 20 MHz<br/>1MB - 16MB RAM | ecs386_32/386_32_even.bin<br/>ecs386_32/386_32_odd.bin
1989 | <b>IBM PS/2 Model 70 (type 3)</b> <i>[[5]](#system-note-5)</i><br/>386DX at 25 MHz<br/>2MB - 16MB RAM<br/>Built-in VGA<br/>MCA bus | ibmps2_m70_type3/70-a_even.bin<br/>ibmps2_m70_type3/70-a_odd.bin
1989 | <b>Compaq Deskpro 386</b><br/>386DX at 20 MHz<br/>1MB - 15MB RAM | deskpro386/109592-005.u11.bin<br/>deskpro386/109591-005.u13.bin
1989 | <b>Epson PC AX3</b><br/>386SX at 16 MHz<br/>256KB - 16MB RAM | epson_pcax3/EVAX3<br/>epson_pcax3/ODAX3
1989 | <b>IBM PS/2 Model 55SX</b><br/>386SX at 16 MHz<br/>1MB - 8MB RAM<br/>Built-in VGA<br/>MCA bus | i8555081/33f8146.zm41<br/>i8555081/33f8145.zm40
1990 | <b>DTK 386SX clone</b><br/>386SX<br/>512KB - 16MB RAM | dtk386/3cto001.bin
1990 | <b>IBM PS/1 Model 2121</b> <i>[[5]](#system-note-5)</i><br/>386SX at 20 MHz<br/>1MB - 16MB RAM<br/>Built-in VGA | ibmps1_2121/fc0000.bin
1990 | <b>Samsung SPC-6000A</b><br/>386DX<br/>1MB - 32 MB RAM | spc6000a/3c80.u27<br/>spc6000a/9f80.u26
1992 | <b>Acermate 386SX/25N</b> <i>[[5]](#system-note-5)</i><br/>386SX at 25 MHz<br/>2MB - 16MB RAM<br/>Built-in Oak SVGA | acer386/acer386.bin<br/>acer386/oti067.bin
1992 | <b>Amstrad MegaPC</b> <i>[[2]](#system-note-2)</i> <i>[[5]](#system-note-5)</i><br/>386SX at 25 MHz<br/>1MB - 16MB RAM<br/>Built-in VGA<br/> | megapc/41651-bios lo.u18<br/>megapc/211253-bios hi.u19
1992 | <b>Commodore SL386SX-25</b> <i>[[5]](#system-note-5)</i><br/>386SX at 25 MHz<br/>1MB - 16MB RAM<br/>Built-in AVGA2 | cbm_sl386sx25/f000.bin<br/>cbm_sl386sx25/c000.bin
1992 | <b>Packard Bell Legend 300SX</b><br/>386SX at 16 MHz<br/>1MB - 16MB RAM | pb_l300sx/pb_l300sx.bin
1992 | <b>Samsung SPC-6033P</b><br/>386SX at 33 MHz<br/>2MB - 12 MB RAM | spc6033p/phoenix.bin<br/>spc6033p/svb6120a_font.rom
1994 | <b>AMI 386DX clone</b><br/>386DX at 40 MHz<br/>1MB - 32MB RAM | ami386dx/opt495sx.ami
1994 | <b>AMI 386SX clone</b><br/>386SX at 25 MHz<br/>1MB - 16MB RAM | ami386/ami386.bin
1994 | <b>MR 386DX clone</b><br/>This is a generic 386DX clone with an MR BIOS | mr386dx/opt495sx.mr
? | <b>KMX-C-02</b><br/>386SX<br/>512KB - 16MB RAM | kmxc02/3ctm005.bin

### 486 based
Release | Machine<br/>(+ addl. hardware) | CPU(s) Supported | ROM file needed<br/>(within ./roms/ folder)
:-: | --- | --- | ---
1990 | <b>IBM PS/2 Model 70 (type 4)</b> <i>[[3]](#system-note-3)</i><br/>2MB - 16MB RAM<br/>Built-in VGA<br/>MCA bus | <b>486DX</b> at 25 MHz | ibmps2_m70_type3/70-a_even.bin<br/>ibmps2_m70_type3/70-a_odd.bin
1993 | <b>AMI 486 clone</b><br/>1MB - 32MB RAM | <b>486</b> at 16-66 MHz | ami486/ami486.bin
1993 | <b>Elonex PC-425X</b> <i>[[5]](#system-note-5)</i><br/>1MB - 256MB RAM<br/>Built-in Trident TGUI9440CXi | <b>486SX</b> at 25 MHz | elx_pc425x/elx_pc425x.bin<br/>&nbsp;&nbsp;&nbsp;<i>or</i><br/>elx_pc425x/elx_pc425x_bios.bin<br/>elx_pc425x/elx_pc425x_vbios.bin
1993 | <b>IBM PS/1 Model 2133 (EMEA 451)</b><br/>2MB - 64MB RAM<br/>Built-in Cirrus Logic GD5426 | <b>486SX</b> at 25 MHz | ibmps1_2133/PS1_2133_52G2974_ROM.bin
1993 | <b>Packard Bell PB410A</b> <i>[[5]](#system-note-5)</i><br/>1MB - 64MB RAM<br/>Built-in HT-216 video | <b>486</b> at 25-120 MHz<br/><b>Am5x86</b> at 133-160 MHz<br/><b>Cx5x86</b> at 100-133 MHz<br/><b>Pentium Overdrive</b> at 63-83 MHz | pb410a/PB410A.080337.4ABF.U25.bin
1994 | <b>AMI WinBIOS 486 clone</b><br/>1MB - 32MB RAM | <b>486</b> at 16-66 MHz | win486/ali1429g.amw
1995 | <b>Award SiS 496/497</b><br/>1MB - 64MB RAM | <b>486</b> at 16-120 MHz<br/><b>Am5x86</b> at 133-160 MHz<br/><b>Cx5x86</b> at 100-133 MHz<br/><b>Pentium Overdrive</b> at 63-83 MHz | sis496/sis496-1.awa

### Pentium based
Release | Machine<br/>(+ addl. hardware) | CPU(s) Supported | ROM file needed<br/>(within ./roms/ folder)
:-: | --- | --- | ---
1994 | <b>Intel Premiere/PCI (Batman's Revenge)</b><br/>1MB - 128MB RAM | <b>Pentium</b> at 60-66 MHz<br/><b>Pentium Overdrive</b> at 120-133 MHz | revenge/1009af2_.bi0<br/>revenge/1009af2_.bi1
1995 | <b>Intel Advanced/EV (Endeavor)</b> <i>[[4]](#system-note-4)</i><br/>1MB - 128MB RAM | <b>Pentium</b> at 75-133 MHz<br/><b>Pentium Overdrive</b> at 125-200 MHz | endeavor/1006cb0_.bi0<br/>endeavor/1006cb0_.bi1
1995 | <b>Intel Advanced/ZP (Zappa)</b><br/>1MB - 128MB RAM | <b>Pentium</b> at 75-133 MHz<br/><b>Pentium Overdrive</b> at 125-200 MHz | zappa/1006bs0_.bio<br/>zappa/1006bs0_.bi1
1995 | <b>Packard Bell PB520R (Robin LC)</b> <i>[[5]](#system-note-5)</i><br/>1MB - 128MB RAM<br/>Built-in Cirrus Logic GD-5434 | <b>Pentium</b> at 60-66 MHz<br/><b>Pentium Overdrive</b> at 120-133 MHz | pb520r/1009bc0r.bio<br/>pb520r/1009bc0r.bi1<br/>pb520r/gd5434.bin
1995 | <b>Packard Bell PB570 (Hillary)</b> <i>[[5]](#system-note-5)</i><br/>1MB - 128MB RAM<br/>Built-in Cirrus Logic GD-5430 | <b>Pentium</b> at 75-133 MHz<br/><b>Pentium Overdrive</b> at 125-200 MHz | pb570/1007by0r.bio<br/>pb570/1007by0r.bi1<br/>pb570/gd5430.bin
1996 | <b>ASUS P/I-P55TVP4</b><br/>1MB - 128MB RAM | <b>Pentium</b> at 75-200 MHz<br/><b>Pentium MMX</b> at 166-233 MHz<br/><b>Mobile Pentium MMX</b> at 120-300 MHz<br/><b>Cyrix 6x86</b> at PR90<i>(80 MHz)</i>-PR200<i>(200 MHz)</i><br/><b>Cyrix 6x86MX/MII</b> at PR166<i>(133 MHz)</i>-PR400<i>(285 MHz)</i><br/><b>IDT WinChip</b> at 75-240 MHz<br/><b>IDT Winchip 2</b> at 200-240 MHz<br/><b>IDT Winchip 2A</b> at 200-233 MHz<br/><b>AMD K6</b> at 166-300 MHz<br/><b>AMD K6-2</b> <i>(AFR-66)</i> at 233-300 MHz | p55tvp4/tv5i0204.awd
1996 | <b>ASUS P/I-P55T2P4</b><br/>1MB - 512MB RAM | <b>Pentium</b> at 75-200 MHz<br/><b>Pentium MMX</b> at 166-233 MHz<br/><b>Mobile Pentium MMX</b> at 120-300 MHz<br/><b>Cyrix 6x86</b> at PR90<i>(80 MHz)</i>-PR200<i>(200 MHz)</i><br/><b>Cyrix 6x86MX/MII</b> at PR166<i>(133 MHz)</i>-PR400<i>(285 MHz)</i><br/><b>IDT WinChip</b> at 75-240 MHz<br/><b>IDT Winchip 2</b> at 200-240 MHz<br/><b>IDT Winchip 2A</b> at 200-233 MHz<br/><b>AMD K6</b> at 166-300 MHz<br/><b>AMD K6-2</b> <i>(AFR-66)</i> at 233-300 MHz | p55t2p4/0207_j2.bin
1996 | <b>Award 430VX PCI</b><br/>1MB - 128MB RAM | <b>Pentium</b> at 75-200 MHz<br/><b>Pentium MMX</b> at 166-233 MHz<br/><b>Mobile Pentium MMX</b> at 120-300 MHz<br/><b>Cyrix 6x86</b> at PR90<i>(80 MHz)</i>-PR200<i>(200 MHz)</i><br/><b>Cyrix 6x86MX/MII</b> at PR166<i>(133 MHz)</i>-PR400<i>(285 MHz)</i><br/><b>IDT WinChip</b> at 75-240 MHz<br/><b>IDT Winchip 2</b> at 200-240 MHz<br/><b>IDT Winchip 2A</b> at 200-233 MHz<br/><b>AMD K6</b> at 166-300 MHz<br/><b>AMD K6-2</b> <i>(AFR-66)</i> at 233-300 MHz | 430vx/55xwuq0e.bin
1996 | <b>Itautec Infoway Multimidia</b><br/>8MB - 128MB RAM | <b>Pentium</b> at 75-133 MHz<br/><b>Pentium Overdrive</b> at 125-200 MHz | infowaym/1006bs0_.bio<br/>infowaym/1006bs0_.bi1
1997 | <b>Epox P55-VA</b><br/>1MB - 128MB RAM | <b>Pentium</b> at 75-200 MHz<br/><b>Pentium MMX</b> at 166-233 MHz<br/><b>Mobile Pentium MMX</b> at 120-300 MHz<br/><b>Cyrix 6x86</b> at PR90<i>(80 MHz)</i>-PR200<i>(200 MHz)</i><br/><b>Cyrix 6x86MX/MII</b> at PR166<i>(133 MHz)</i>-PR400<i>(285 MHz)</i><br/><b>IDT WinChip</b> at 75-240 MHz<br/><b>IDT Winchip 2</b> at 200-240 MHz<br/><b>IDT Winchip 2A</b> at 200-233 MHz<br/><b>AMD K6</b> at 166-300 MHz<br/><b>AMD K6-2</b> <i>(AFR-66)</i> at 233-300 MHz | p55va/va021297.bin

### Super Socket 7 based
Release | Machine<br/>(+ addl. hardware) | CPU(s) Supported | ROM file needed<br/>(within ./roms/ folder)
:-: | --- | --- | ---
1998 | <b>FIC VA-503+</b><br/>1MB - 512MB RAM | <b>AMD K6</b> at 166-300 MHz<br/><b>AMD K6-2</b> at 233-550 MHz<br/><b>AMD K6-2+</b> at 450-550 MHz<br/><b>AMD K6-III</b> at 400-450 MHz<br/><b>AMD K6-III+</b> at 400-500 MHz<br/><b>Pentium</b> at 75-200 MHz<br/><b>Pentium MMX</b> at 166-233 MHz<br/><b>Mobile Pentium MMX</b> at 120-300 MHz<br/><b>Cyrix 6x86</b> at PR90<i>(80 MHz)</i>-PR200<i>(200 MHz)</i><br/><b>Cyrix 6x86MX/MII</b> at PR166<i>(133 MHz)</i>-PR400<i>(285 MHz)</i><br/><b>IDT WinChip</b> at 75-240 MHz<br/><b>IDT WinChip2</b> at 200-250 MHz<br/><b>IDT Winchip 2A</b> at PR200<i>(200 MHz)</i>-PR300<i>(250 MHz)</i> | fic_va503p/je4333.bin

### Socket 8 based
Release | Machine<br/>(+ addl. hardware) | CPU(s) Supported | ROM file needed<br/>(within ./roms/ folder)
:-: | --- | --- | ---
1996 | <b>Intel VS440FX</b><br/>8MB - 256 MB RAM | <b>Pentium Pro</b> at 150-200 MHz<br/><b>Pentium II Overdrive</b> at 300-333 MHz | vs440fx/1018CS1_.BI1<br/>vs440fx/1018CS1_.BI2<br/>vs440fx/1018CS1_.BI3<br/>vs440fx/1018CS1_.BIO<br/>vs440fx/1018CS1_.RCV

### Slot 1 based
Release | Machine<br/>(+ addl. hardware) | CPU(s) Supported | ROM file needed<br/>(within ./roms/ folder)
:-: | --- | --- | ---
1998 | <b>Gigabyte GA-686BX</b><br/>8MB - 512MB RAM | <b>Pentium II</b> at 233-450 MHz<br/><b>Celeron</b> at 266-533 MHz<br/><b>Cyrix III</b>at 500 MHz | ga686bx/6BX.F2a

#### Additional Notes
<a name="system-note-1">`[1]`</a> <b>Toshiba Button Mapping</b>:
PCem maps [Fn] to `right-Ctrl` and `right-Alt`. The following functions are supported:
Key Combo | Function
---|---
Fn + Num Lock | toggle numpad
Fn + Home | Internal LCD display
Fn + Page Down | Turbo on
Fn + Right | Toggle LCD font
Fn + End | External CRT display
Fn + SysRQ | Toggle window

<a name="system-note-2">`[2]`</a> <b>Amstrad MegaPC</b> <i>(386SX)</i>: The original machine had a built-in Sega MegaDrive. This is not emulated in PCem.

<a name="system-note-3">`[3]`</a> <b>IBM PS/2 Model 70 (type 4)</b> <i>(486DX)</i>: PCem's FPU emulation is not bit accurate and can not pass IBM's floating point tests. As a result, this machine will always print 12903 and 162 errors on bootup. These can be ignored - F1 will boot the machine.

<a name="system-note-4">`[4]`</a> <b>Intel Advanced/EV (Endeavor)</b> <i>(Pentium)</i>: The real board has a Sound Blaster 16 onboard and optionally an S3 Trio64V+. Neither are emulated as onboard devices.

<a name="system-note-5">`[5]`</a> Some systems have fixed graphics adapters:<br/>
<i>** = Can use external video card.</i><br/>

System | Graphics | Addl. Info | **
--- | --- | --- | :-:
<b>Amstrad MegaPC</b> | Paradise 90C11 | A development of the PVGA1 with 512KB VRAM | &#10004;
<b>Acer 386SX/25N</b> | Oak OTI-067 | Another 512KB SVGA clone | &#10004;
<b>Amstrad PC1512</b> | CGA | Has a new mode (640x200x16) | X
<b>Amstrad PC1640</b> | Paradise EGA | &nbsp; | &#10004;
<b>Amstrad PC2086/PC3086</b> | Paradise PVGA1 | An early SVGA clone with 256KB VRAM | &#10004;
<b>Amstrad PPC512/640</b> | CGA/MDA | Outputs to 640x200 plasma display | &#10004;
<b>Commodore SL386SX-25</b> | AVGA2 | 256KB - 512KB VRAM | X
<b>Elonex PC-425X</b> | Trident TGUI9400CXi | 512KB VRAM | X
<b>IBM PCjr</b> | CGA | Has various new modes: <br/>160x200 x 16<br/>320x200 x 16<br/>640x200 x 4 | X
<b>IBM PS/1 Model 2011</b> | Stock VGA | 256KB VRAM | X
<b>IBM PS/1 Model 2121</b> | Basic (and unknown) SVGA | 256KB VRAM | X
<b>IBM PS/2 machines</b> | Stock VGA | 256KB VRAM | X
<b>Olivetti M24</b> <i>[[6]](#system-note-6)</i>| CGA | Has double-res text modes + 640x400 mode | X
<b>Packard Bell PB410A</b> | Headland HT-216 | &nbsp; | &#10004;
<b>Packard Bell PB520R</b> | Cirrus Logic GD-5434 | &nbsp; | &#10004;
<b>Packard Bell PB570</b> | Cirrus Logic GD-5430 | &nbsp; | &#10004;
<b>Sinclair PC200</b> | CGA | Can output to TV @ 50hz (UK) | &#10004;
<b>Tandy 1000</b> | Clone of PCjr video | Widely supported in 80s games | X
<b>Tandy 1000 SL/2</b> | Improved Tandy 1000 | Has support for 640x200x16 | X
<b>Toshiba T-series</b> | CGA | Outputs to built-in LCD or plasma display | X

<a name="system-note-6">`[6]`</a> <b>Olivetti M24 (display)</b>: I haven't seen a dump of the font ROM for this yet, so if one is not provided the MDA font will be used - which looks slightly odd as it is 14-line instead of 16-line.

<hr>

## Graphics Cards

### Basic cards
Hardware | Addl. Info | ROM file needed<br/>(within ./roms/ folder)
--- | --- | ---
<b>MDA</b> <i>(1981)</i> | 80x25 monochrome text | <i>(none)</i>
<b>CGA</b> <i>(1981)</i> | 40x25 and 80x25 text<br/>320x200 in 4 colours<br/>620x200 in 2 colours<br/>Supports composite output for ~16 colours. | <i>(none)</i>
<b>Hercules</b> <i>(1982)</i> | 80x25 monochrome text<br/>720x348 in monochrome | <i>(none)</i>
<b>Plantronics ColorPlus</b> | An enhanced CGA board with support for 320x200x16 and 640x200x4 | <i>(none)</i>
<b>Wyse WY-700</b> | A CGA-compatible board with support for a 1280x800 mode | <i>(none)</i>
<b>MDSI Genius</b> | Mono portrait board with support for a 728x1008 mode | 8x12.bin
<b>IBM EGA</b> <i>(1984)</i> | Text up to 80x43<br/>Graphics up to 640x350 in 16 colours | ibm_6277356_ega_card_u44_27128.bin
<b>ATI EGA Wonder 800+</b> | An enhanced EGA-compatible board with support for up to 800x600 in 16 colours | ATI EGA Wonder 800+ N1.00.BIN
<b>Hercules InColor</b> | An enhanced Hercules with a custom 720x350 16 colour mode | <i>(none)</i>

### Unaccelerated (S)VGA cards
Hardware | Addl. Info | ROM file needed<br/>(within ./roms/ folder)
--- | --- | ---
<b>ATI Korean VGA</b> | ATI-28800 based.<br/>512KB VRAM<br/>Supports up to 8-bit colour<br/>Korean font support | atikorvga.bin<br/>ati_ksc5601.rom
<b>ATI VGA Edge-16</b> | ATI-18800 based<br/>512KB VRAM<br/>Supports up to 8-bit colour | vgaedge16.vbi
<b>ATI VGA Charger</b> | ATI-28800 based<br/>512KB VRAM<br/>Supports up to 8-bit colour | bios.bin
<b>AVGA2</b> | Also known as Cirrus Logic GD5402<br/>256KB - 512KB VRAM<br/>Supports up to 8-bit colour | avga2vram.vbi
<b>IBM VGA</b> <i>(1987)</i> | 256KB VRAM<br/>Text up to 80x50<br/>Graphics up to 320x200 in 256 colours or 640x480 in 16 colours | ibm_vga.bin
<b>Kasan Hangulmadang-16</b> | ET4000AX based<br/>1MB VRAM<br/>Supports up to 8-bit colour<br/>Korean font support | et4000_kasan16.bin<br/>kasan_ksc5601.rom
<b>OAK OTI-037C</b> | 256KB VRAM<br/>Supports up to 8-bit colour | oti037/bios.bin
<b>OAK OTI-067</b> | 256KB - 512KB VRAM<br/>Supports up to 8-bit colour | oti067/bios.bin
<b>Olivetti GO481 (Paradise PVGA1A)</b> | 256KB VRAM<br/>Supports up to 8-bit colour | oli_go481_lo.bin<br/>oli_go481_hi.bin
<b>Trident 8900D SVGA</b> | 256KB - 1MB VRAM<br/>Supports up to 24-bit colour | trident.bin
<b>Trident 9000B SVGA</b> | 512KB VRAM<br/>Supports up to 8-bit colour | tvga9000b/BIOS.BIN
<b>Trident TGUI9400CXi</b> | 1MB - 2MB VRAM<br/>Supports up to 24-bit colour | 9440.vbi
<b>Trigem Korean VGA</b> | ET4000AX based<br/>1MB VRAM<br/>Supports up to 8-bit colour<br/>Korean font support | tgkorvga.bin<br/>tg_ksc5601.rom
<b>Tseng ET4000AX SVGA</b> | 1MB VRAM<br/>Supports up to 8-bit colour | et4000.bin

### 2D Accelerated SVGA cards
Hardware | Addl. Info | ROM file needed<br/>(within ./roms/ folder)
--- | --- | ---
<b>ATI Graphics Pro Turbo</b> | Mach64GX based<br/>1MB - 4MB VRAM | mach64gx/bios.bin
<b>ATI Video Xpression</b> | Mach64VT2 based<br/>2MB - 4MB VRAM<br/>Has video acceleration | atimach64vt2pci.bin
<b>Cirrus Logic GD-5428</b> | 1MB - 2MB VRAM | Machspeed_VGA_GUI_2100_VLB.vbi
<b>Cirrus Logic GD-5429</b> | 1MB - 2MB VRAM | 5429.vbi
<b>Cirrus Logic GD-5430</b> | 1MB - 2MB VRAM | gd5430/pci.bin
<b>Cirrus Logic GD-5434</b> <i>[[1]](#graphics-note-1)</i> | 2MB - 4MB VRAM | gd5434.bin
<b>Diamond Stealth 32 SVGA</b> | ET4000/W32p based<br/>1MB - 2MB VRAM | et4000w32.bin
<b>IBM 1MB SVGA Adapter/A</b> | Cirrus Logic GD5428 based<br/>1 MB VRAM<br/>Only supported on PS/2 models | SVGA141.ROM
<b>Number Nine 9FX</b> | S3 Trio64 based<br/>1MB - 2MB VRAM | s3_764.bin
<b>Paradise Bahamas 64</b> | S3 Vision864 based<br/>1MB - 4MB VRAM | bahamas64.bin
<b>Phoenix S3 Trio32</b> | S3 Trio32 based<br/>512KB - 2MB VRAM | 86c732p.bin
<b>Phoenix S3 Trio64</b> | S3 Trio64 based<br/>1MB - 4MB VRAM | 86c764x1.bin
<b>Trident TGUI9440</b> | 1MB - 2MB VRAM | 9440.vbi

### 3D Accelerated SVGA cards
Hardware | Addl. Info | ROM file needed<br/>(within ./roms/ folder)
--- | --- | ---
<b>3DFX Voodoo Banshee (reference)</b> | Voodoo Banshee based<br/>8MB - 16MB VRAM | pci_sg.rom
<b>3DFX Voodoo 3 2000</b> | Voodoo 3 based<br/>16MB VRAM | voodoo3_2000/2k11sd.rom
<b>3DFX Voodoo 3 3000</b> | Voodoo 3 based<br/>16MB VRAM | voodoo3_3000/3k12sd.rom
<b>Creative Labs 3D Blaster Banshee</b> | Voodoo Banshee based<br/>16MB VRAM | blasterpci.rom
<b>Diamond Stealth 3D 2000</b> <i>[[2]](#graphics-note-2)</i>| S3 ViRGE/325 based<br/>2MB - 4MB VRAM | s3virge.bin
<b>S3 ViRGE/DX</b> <i>[[3]](#graphics-note-3)</i>| S3 ViRGE/DX based<br/>2MB - 4MB VRAM | 86c375_1.bin

### 3D only cards
Hardware | Addl. Info 
--- | --- 
<b>3DFX Voodoo Graphics</b> <i>[[4]](#graphics-note-4)</i>| 3D accelerator. Widely supported in late 90s games.
<b>Obsidian SB50</b> <i>[[5]](#graphics-note-5)</i>| Voodoo with 2 TMUs
<b>3DFX Voodoo 2</b> <i>[[6]](#graphics-note-6)</i>| Improved Voodoo Graphics<br/>Higher clocks<br/>2 TMUs<br/>Triangle setup, etc.

#### Additional Notes
<a name="graphics-note-1">`[1]`</a> <b>Cirrus Logic GD-5434</b>: Real chip also supports 1MB configurations, however this is not currently supported in PCem.

<a name="graphics-note-2">`[2]`</a> <b>Diamond Stealth 3D 2000</b>: PCem emulates the ViRGE S3D engine in software. This works with most games I tried, but there may be some issues. The Direct3D drivers for the /325 are fairly poor (often showing as missing triangles), so use of the /DX instead is recommended.

<a name="graphics-note-3">`[3]`</a> <b>S3 ViRGE/DX</b>: The drivers that come with Windows are similar to those for the /325, however better ones do exist (try the 8-21-1997 version). With the correct drivers, many early Direct3D games work okay (if slowly).

<a name="graphics-note-4">`[4]`</a> <b>3DFX Voodoo Graphics</b>: PCem emulates this in software. The emulation is a lot faster than in v10 (thanks to a new dynamic recompiler) and should be capable of hitting Voodoo 1 performance on most machines when two render threads are used. As before, the emulated CPU is the bottleneck for most games. <br/><br/>PCem can emulate 6 and 8 MB configurations, but defaults to 4 MB for compatibility. It can also emulate the screen filter present on the original card, though this does at present have a noticeable performance hit.<br/><br/>Almost everything I've tried works okay, with a very few exceptions - Screamer 2 and Rally have serious issues.

<a name="graphics-note-5">`[5]`</a> <b>Obsidian SB50</b>: Drivers for this are a bit limited - the official drivers don't support 2 TMUs in Direct3D.

<a name="graphics-note-6">`[6]`</a> <b>3DFX Voodoo 2</b>: PCem can emulate both 8 and 12 MB configurations. It can also emulate 6 and 10 MB configurations (with 2MB framebuffer memory), which were never sold into the PC market but do exist for arcade systems.

<hr>

## Sound Cards

Hardware | Notes
--- | ---
<b>PC speaker</b> | The standard beeper on all PCs. Supports samples/RealSound.
<b>Tandy PSG</b> | The Texas Instruments chip in the PCjr and Tandy 1000. Supports 3 voices plus noise. I reused the emulator in B-em for this (slightly modified). PCem emulates the differences between the SN76496 (PCjr and Tandy 1000), and the NCR8496 (currently assigned to the Tandy 1000HX). Maniac Mansion and Zak McKraken will only sound correct on the latter.
<b>Tandy PSSJ</b> | Used on the Tandy 1000SL/2, this clones the NCR8496, adding an addition frequency divider (did any software actually use this?) and an 8-bit DAC.
<b>PS/1 audio card</b> | An SN76496 clone plus an 8-bit DAC. The SN76496 isn't at the same address as PCjr/Tandy, so most software doesn't support it.
<b>Gameblaster</b> | The Creative Labs Gameblaster/Creative Music System, Creative's first sound card introduced in 1987. Has two Philips SAA1099, giving 12 voices of square waves plus 4 noise voices. In stereo!
<b>Adlib</b> | Has a Yamaha YM3812, giving 9 voices of 2 op FM, or 6 voices plus a rhythm section. PCem uses the DOSBox dbopl emulator.
<b>Adlib Gold</b> | OPL3 with YM318Z 12-bit digital section. Possibly some bugs (not a lot of software to test). The surround module is now emulated.
<b>Sound Blaster</b> <i>[[1]](#sound-note-1)</i> | See linked note for more details.
<b>Gravis Ultrasound</b> | 32 voice sample playback. Port address is fixed to 240, IRQ and DMA can be changed from the drivers. Emulation is improved significantly over previous versions.
<b>Windows Sound System</b> | 16-bit digital + OPL3. Note that this only emulates WSS itself, and should not be used with drivers from compatible boards with additional components (eg Turtle Beach Monte Carlo)
<b>Aztech Sound Galaxy Pro 16 AB (Washington)</b> | SB compatible + WSS compatible
<b>Innovation SSI-2001</b> | SID6581. Emulated using resid-fp. Board is fixed to port 280.
<b>Ensoniq AudioPCI (ES1371)<br/>Sound Blaster PCI 128</b> | Basic PCI sound card. Emulates Sound Blaster in software.

### Additional Notes
<a name="sound-note-1">`[1]`</a> <b>Sound Blaster</b>: Several Sound Blasters are emulated.
* SB v1.0 - The original. Limited to 22khz, and no auto-init DMA (can cause crackles sometimes).
* SB v1.5 - Adds auto-init DMA
* SB v2.0 - Upped to 41khz
* SB Pro v1.0 - Stereo with twin OPL2 chips.
* SB Pro v2.0 - Stereo with OPL 3 chip
* SB 16 - 16 bit stereo
* SB AWE32 - SB 16 + wavetable MIDI. This requires a ROM dump from a real AWE32.

All cards are set to Address 220, IRQ 7 and DMA 1 (and High DMA 5). IRQ and DMA can be changed for the SB16 & AWE32 in the drivers. The relevant SET line for autoexec.bat is `SET BLASTER = A220 I7 D1 Tx` - where Tx is T1 for SB v1.0, T3 for SB v2.0, T4 for SB Pro, and T6 for SB16.

AWE32 requires a ROM dump called `awe32.raw`. AWE-DUMP is a utility which can get a dump from a real card. Most EMU8000 functionality should work, however filters are not correct and reverb/chorus effects are not currently emulated.

<hr>

## HDD Controller Cards

Int. | Hardware | Notes | ROM file needed<br/>(within ./roms/ folder)
:-: | --- | --- | ----
MFM | <b>Fixed Disk Adapter (Xebec)</b> | MFM controller originally included in the IBM XT. This controller only supports HD types 0, 2, 13, and 16. | ibm_xebec_62x0822_1985.bin
MFM | <b>DTC 5150X</b> | 8-bit MFM controller.<br/>To configure drive types, run `DEBUG.EXE` and enter `g=c800:5`. | dtc_cxd21a.bin
MFM | <b>AT Fixed Disk Adapter</b> | MFM controller originally included in the IBM AT. Supported by all AT-compatible BIOSes. | <i>(none)</i>
ESDI | <b>Western Digital WD1007V-SE1</b> | 16-bit ESDI controller | 62-000279-061.bin
ESDI | <b>IBM ESDI Fixed Disk Controller</b> | MCA ESDI controller. Only supported on PS/2 models. | 90x8969.bin<br/>90x8970.bin
IDE | <b>Standard IDE</b> | Standard IDE controller. Supported by all AT-compatible BIOSes. Use this if in any doubt! | <i>(none)</i>
IDE | <b>XTIDE</b> | 8-bit IDE controller. The BIOS is available [here](http://code.google.com/p/xtideuniversalbios/). <br/>v2.0.0 beta 1 is the version I've mostly tested. v2.0.0 beta 3 is known to have some issues. | ide_xt.bin
IDE | <b>XTIDE (AT)</b> | 16-bit IDE controller. | ide_at.bin
IDE | <b>XTIDE (PS/1)</b> | IDE controller for the PS/1 model 2033. For this machine you will need BIOS version v1.1.5. The PS/1 is a bit fussy with XTIDE, and I've found that it works best when the XTIDE configuration has 'Full Operating Mode' disabled. | ide_at_1_1_5.bin
SCSI | <b>Longshine LCS-6821N</b> | 8-bit SCSI controller. | Longshine LCS-6821N - BIOS version 1.04.bin
SCSI | <b>Rancho RT1000B</b> | 8-bit SCSI controller. | Rancho_RT1000_RTBios_version_8.10R.bin
SCSI | <b>Trantor T130B</b> | 8-bit SCSI controller. | trantor_t130b_bios_v2.14.bin
SCSI | <b>IBM SCSI Adapter with Cache</b> | MCA SCSI controller. Only supported on PS/2 models. | 92F2244.U68<br/>92F2245.U69
SCSI | <b>Adaptec AHA-1542C</b> | 16-bit SCSI controller. | adaptec_aha1542c_bios_534201-00.bin
SCSI | <b>BusLogic BT-545S</b> | 16-bit SCSI controller. | BusLogic_BT-545S_U15_27128_5002026-4.50.bin

<hr>

## Misc Cards

Hardware | Note
--- | ---
Serial mouse | A Microsoft compatible serial mouse on COM1. Compatible drivers are all over the place for this.
M24 mouse | I haven't seen a DOS mouse driver for this yet but the regular scancode mode works as does the Windows 1.x driver.
PC1512 mouse | The PC1512's perculiar quadrature mouse. You need Amstrad's actual driver for this one.
PS/2 mouse | A standard 2 button PS/2 mouse. As with serial, compatible drivers are common.
Microsoft PS/2 Intellimouse | A PS/2 mouse with mouse wheel.
ATAPI CD-ROM | Works with OAKCDROM.SYS, VDD-IDE.SYS, and the internal drivers of every OS I've tried.
