# PCem v18
## Changes in v18
- PCAP Support is now both Windows and Linux
- Numerous bugfixes in this build
- Changed some GUI elements.
- Plugin interface added. Allows you to create your own machines or features to extend PCem.
- Added RAM disk preloaded with RAW/VHD images (*.rdimg;*.rdvhd)
  - Load up to 2GB disks to temporary disk (doesn't modify the image file)
  - Mounts as read-only if memory allocation fails (mind the 32bit PCem memory limits)
- 32-bit Windows builds will now be deprecated due to technical issues. v17 will be the last 32-bit PCem.

## Added the following machines to v18
- Hyundai SUPER-16T
- Hyundai SUPER-16TE
- Commodore PC10C

## Added the following CD-Drives to v18
- Sony CDU311 (8X)
- Toshiba XM-5702B (12X)
- GoldStar CRD-8160B (16X)
- Creative CR-587-B (24X)
- Creative CR-588-B (32X)
- BTC BCD36XH (36X)
- Philips PCA403CD (40X)
- Mitsumi CRMC-FX4820T (48X)

## Added the following Video Cards to v18
- Matrox Millennium
- Quadram Quadcolor I / I+II
- Yamaha V6335D Video Chipset

## Added the following Sound Cards to v18
- Mindscape Music Board

## Developer Changes to v18
- First release to switch from autotools/make to CMake/Ninja
- Legacy autotools and mingw makefiles are removed
- Windows and Linux builds now will be built in Clang, and are required to be built with Clang

# PCem v17
- New machines added - Amstrad PC5086, Compaq Deskpro, Samsung SPC-6033P, Samsung SPC-6000A, Intel VS440FX, Gigabyte GA-686BX
- New graphics cards added - 3DFX Voodoo Banshee, 3DFX Voodoo 3 2000, 3DFX Voodoo 3 3000, Creative 3D Blaster Banshee, Kasan Hangulmadang-16, Trident TVGA9000B
- New CPUs - Pentium Pro, Pentium II, Celeron, Cyrix III
- VHD disc image support
- Numerous bug fixes
- A few other bits and pieces 

# PCem v16
- New machines added - Commodore SL386SX-25, ECS 386/32, Goldstar GDC-212M, Hyundai Super-286TR, IBM PS/1 Model 2133 (EMEA 451), Itautec Infoway Multimidia, Samsung SPC-4620P, Leading Edge Model M
- New graphics cards added - ATI EGA Wonder 800+, AVGA2, Cirrus Logic GD5428, IBM 1MB SVGA Adapter/A
- New sound card added - Aztech Sound Galaxy Pro 16 AB (Washington)
- New SCSI card added - IBM SCSI Adapter with Cache
- Support FPU emulation on pre-486 machines
- Numerous bug fixes
- A few other bits and pieces 

# PCem v15
- New machines added - Zenith Data SupersPort, Bull Micral 45, Tulip AT Compact, Amstrad PPC512/640, Packard Bell PB410A, ASUS P/I-P55TVP4, ASUS P/I-P55T2P4, Epox P55-VA, FIC VA-503+
- New graphics cards added - Image Manager 1024, Sigma Designs Color 400, Trigem Korean VGA
- Added emulation of AMD K6 family and IDT Winchip 2
- New CPU recompiler. This provides several optimisations, and the new design allows for greater portability and more scope for optimisation in the future
- Experimental ARM and ARM64 host support
- Read-only cassette emulation for IBM PC and PCjr
- Numerous bug fixes

# PCem v14
- New machines added - Compaq Portable Plus, Compaq Portable II, Elonex PC-425X, IBM PS/2 Model 70 (types 3 & 4), Intel Advanced/ZP, NCR PC4i, Packard Bell Legend 300SX, Packard Bell PB520R, Packard Bell PB570, Thomson TO16 PC, Toshiba T1000, Toshiba T1200, Xi8088
- New graphics cards added - ATI Korean VGA, Cirrus Logic CL-GD5429, Cirrus Logic CL-GD5430, Cirrus Logic CL-GD5435, OAK OTI-037, Trident TGUI9400CXi
- New network adapters added - Realtek RTL8029AS
- Iomega Zip drive emulation
- Added option for default video timing
- Added dynamic low-pass filter for SB16/AWE32 DSP playback
- Can select external video card on some systems with built-in video
- Can use IDE hard drives up to 127 GB
- Can now use 7 SCSI devices
- Implemented CMPXCHG8B on Winchip. Can now boot Windows XP on Winchip processors
- CD-ROM emulation on OS X
- Tweaks to Pentium and 6x86 timing
- Numerous bug fixes 

# PCem v13.1
- Minor recompiler tweak, fixed slowdown in some situations (mainly seen on Windows 9x just after booting)
- Fixed issues with PCJr/Tandy sound on some Sierra games
- Fixed plasma display on Toshiba 3100e
- Fixed handling of configurations with full stops in the name
- Fixed sound output gain when using OpenAL Soft
- Switched to using OpenAL Soft by default 

# PCem v13
- New machines added - Atari PC3, Epson PC AX, Epson PC AX2e, GW-286CT GEAR, IBM PS/2 Model 30-286, IBM PS/2 Model 50, IBM PS/2 Model 55SX, IBM PS/2 Model 80, IBM XT Model 286, KMX-C-02, Samsung SPC-4200P, Samsung SPC-4216P, Toshiba 3100e
- New graphics cards - ATI Video Xpression, MDSI Genius
- New sound cards added - Disney Sound Source, Ensoniq AudioPCI (ES1371), LPT DAC, Sound Blaster PCI 128
- New hard drive controllers added - AT Fixed Disk Adapter, DTC 5150X, Fixed Disk Adapter (Xebec), IBM ESDI Fixed Disk Controller, Western Digital WD1007V-SE1
- New SCSI adapters added - Adaptec AHA-1542C, BusLogic BT-545S, Longshine LCS-6821N, Rancho RT1000B, Trantor T130B
- New network adapters added - NE2000 compatible
- New cross-platform GUI
- Voodoo SLI emulation
- Improvements to Sound Blaster emulation
- Improvements to Pentium timing
- Various bug fixes
- Minor optimisations 

# PCem v12
- New machines added - AMI 386DX, MR 386DX
- New graphics cards - Plantronics ColorPlus, Wyse WY-700, Obsidian SB50, Voodoo 2
- CPU optimisations - up to 50% speedup seen
- 3DFX optimisations
- Improved joystick emulation - analogue joystick up to 8 buttons, CH Flightstick Pro, ThrustMaster FCS, SideWinder pad(s)
- Mouse can be selected between serial, PS/2, and IntelliMouse
- Basic 286/386 prefetch emulation - 286 & 386 performance much closer to real systems
- Improved CGA/PCjr/Tandy composite emulation
- Various bug fixes 

# PCem v11
- New machines added - Tandy 1000HX, Tandy 1000SL/2, Award 286 clone, IBM PS/1 model 2121
- New graphics card - Hercules InColor
- 3DFX recompiler - 2-4x speedup over previous emulation
- Added Cyrix 6x86 emulation
- Some optimisations to dynamic recompiler - typically around 10-15% improvement over v10, more when MMX used
- Fixed broken 8088/8086 timing
- Fixes to Mach64 and ViRGE 2D blitters
- XT machines can now have less than 640kb RAM
- Added IBM PS/1 audio card emulation
- Added Adlib Gold surround module emulation
- Fixes to PCjr/Tandy PSG emulation
- GUS now in stereo
- Numerous FDC changes - more drive types, FIFO emulation, better support of XDF images, better FDI support
- CD-ROM changes - CD-ROM IDE channel now configurable, improved disc change handling, better volume control support
- Now directly supports .ISO format for CD-ROM emulation
- Fixed crash when using Direct3D output on Intel HD graphics
- Various other fixes 

# PCem v10.1
- Fixed buffer overruns in PIIX and ET4000/W32p emulation
- Add command line options to start in fullscreen and to specify config file
- Emulator doesn't die when the CPU jumps to an unexecutable address
- Removed Voodoo memory dump on exit

# PCem v10
- New machines - AMI XT clone, VTech Laser Turbo XT, VTech Laser XT3, Phoenix XT clone, Juko XT clone, IBM PS/1 model 2011, Compaq Deskpro 386, DTK 386SX clone, Phoenix 386 clone, Intel Premiere/PCI, Intel Advanced/EV
- New graphics cards - IBM VGA, 3DFX Voodoo Graphics
- Experimental dynamic recompiler - up to 3x speedup
- Pentium and Pentium MMX emulation
- CPU fixes - fixed issues in Unreal, Half-Life, Final Fantasy VII, Little Big Adventure 2, Windows 9x setup, Coherent, BeOS and others
- Improved FDC emulation - more accurate, supports FDI images, supports 1.2MB 5.25" floppy drive emulation, supports write protect correctly
- Internal timer improvements, fixes sound in some games (eg Lion King)
- Added support for up to 4 IDE hard drives
- MIDI OUT code now handles sysex commands correctly
- CD-ROM code now no longer crashes Windows 9x when CD-ROM drive empty
- Fixes to ViRGE, S3 Vision series, ATI Mach64 and OAK OTI-067 cards
- Various other fixes/changes

# PCem v9
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

# PCem v8.1
- This fixes a number of issues in v8.

# PCem v8
- New machines - SiS496/497, 430VX
- WinChip emulation (including MMX emulation)
- New graphics cards - S3 Trio64, Trident TGUI9440AGi, ATI VGA Edge-16, ATI VGA Charger, OAK OTI-067, ATI Mach64
- New sound cards - Adlib Gold, Windows Sound System, SB AWE32
- Improved GUS emulation
- MPU-401 emulation (UART mode only) on SB16 and AWE32
- Fixed DMA bug, floppy drives work properly in Windows 3.x
- Fixed bug in FXAM - fixes Wolf 3D, Dogz, some other stuff as well
- Other FPU fixes
- Fixed serial bugs, mouse no longer disappears in Windows 9x hardware detection
- Major reorganisation of CPU emulation
- Direct3D output mode
- Fullscreen mode
- Various internal changes

# PCem v0.7
- Windows 98 now works, Win95 more stable, more machines + graphics cards, and a huge number of fixes.

# PCem v0.6
- Windows 95 now works, FPU emulation, and load of other stuff.

# PCem v0.5
- Loads of fixes + new features in this version.

# PCem v0.41a
- This fixes a disc corruption bug, and re-adds (poor) composite colour emulation.

# PCem v0.41
- This fixes some embarassing bugs in v0.4, as well as a few games.

# PCem v0.4
- 386/486 emulation (buggy), GUS emulation, accurate 8088/8086 timings, and lots of other changes.

# PCem v0.3
- This adds more machines, SB Pro emulation, SVGA emulation, and some other stuff.

# PCem v0.2a
- This is a bugfix release over v0.2.

# PCem v0.2
- This adds PC1640 and AT emulation, 286 emulation, EGA/VGA emulation, Soundblaster emulation, hard disc emulation, and some bugfixes.

# PCem v0.1
- First Release

