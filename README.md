# [PCem](https://pcem-emulator.co.uk/)
![GitHub Actions](https://github.com/sarah-walker-pcem/pcem/actions/workflows/workflow.yml/badge.svg)

## AI Policy
PCem is AI-free, and will not accept any AI generated code, AI generated/assisted code is not allowed in PCem. Pull Requests with 
AI generated code will be rejected. Code subsequently discovered to be AI generated/assisted will be removed without warning.

## License
PCem is licensed under GPL v2.0, see [COPYING](COPYING) for more details.

## Patch Submission
You can submit patches on our [forum](https://pcem-emulator.co.uk/phpBB3). Before you do, please note the [guidelines](https://pcem-emulator.co.uk/phpBB3/viewtopic.php?f=3&t=5) for submitting patches.

## Note about ROM Files
<b>NO COPYRIGHTED ROM FILES ARE INCLUDED NOR WILL THEY BE. PLEASE DO NOT ASK FOR THEM.</b>

## How to Build
You will need the following libraries and buildtools (and their dependencies):
- SDL2
- wxWidgets 3.x
- OpenAL
- CMake
- ALSA (Linux only)
- GTK3 Dev Package (Linux only)
- PCap
- LibSlirp
- Ninja (Recommended, but you can use a Makefile generator if you prefer)

Open a terminal window, navigate to the PCem directory, create a build directory, then enter in that build directory: 
```
cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Release ..
ninja
```

then `./src/pcem` to run on most platforms, and open `./src/PCem.app` on macOS

BIOS ROM images, configuration files, and other data are stored in `~/.pcem` on non-macOS platforms. You can also create a `.pcem` folder with
the Binary, and run it in a portable mode.

The configure options are specified below. They are in the format of -D`Option`=`Value`. `Value` under here is the
default value.
```
  -DCMAKE_BUILD_TYPE=Release : Generate release build. Recommended for regular use.
  -DCMAKE_BUILD_TYPE=Debug   : Compile with debugging enabled.
  -DPCEM_DISPLAY_ENGINE=wxWidgets : Compile with the WxWidgets Display Engine. Default
  -DPCEM_DISPLAY_ENGINE=SDL  : Compile with the SDL Display Engine. This is more of a sample for additional GUI's but is here for postarity.
  -DUSE_NETWORKING=ON        : Build with networking support.
  -DUSE_PCAP_NETWORKING=ON   : Build with pcap networking support. (Needs USE_NETWORKING to compile) Requires libpcap.
  -DFORCE_X11=ON             : Enables a hack to force X11 on Wayland systems. See #128 for details. (Linux Only)
  -DPLUGIN_ENGINE=ON         : Build with plugin support. Builds libpcem-plugin-api and links PCem with it.
  -DPCEM_MARCH=x86_64-v2     : Change the architecture used for generated instructions, by default we set it for
                               >= Nehalem for Intel, and >= Bulldozer for AMD. 
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

CD-ROM support currently only accesses `/dev/cdrom`. It has not been heavily tested.

On Windows, if you want to use PCAP networking, you will need to install [npcap](https://npcap.com) and replace the libpcap.dll in PCem dir using the wpcap.dll installed in C:\Windows\System32  

## Links

### [PCem emulates the following hardware](SYSTEMS.md):
Hardware | Links
--- | ---
Systems | [8088](SYSTEMS.md#8088-based)<br/>[8086](SYSTEMS.md#8086-based)<br/>[286](SYSTEMS.md#286-based)<br/>[386](SYSTEMS.md#386-based)<br/>[486](SYSTEMS.md#486-based)<br/>[Pentium](SYSTEMS.md#pentium-based)<br/>[Super Socket 7](SYSTEMS.md#super-socket-7-based)
Graphics | [Basic](SYSTEMS.md#basic-cards)<br/>[Unaccelerated (S)VGA cards](SYSTEMS.md#unaccelerated-svga-cards)<br/>[2D accelerated SVGA cards](SYSTEMS.md#2d-accelerated-svga-cards)<br/>[3D accelerated SVGA cards](SYSTEMS.md#3d-accelerated-svga-cards)<br/>[3D only cards](SYSTEMS.md#3d-only-cards)
Sound | [Cards](SYSTEMS.md#sound-cards)
HDD Controller | [Cards](SYSTEMS.md#hdd-controller-cards)
Misc | [Cards](SYSTEMS.md#misc-cards)

### [Software Tested](TESTED.md):
- [DOS](TESTED.md#dos)<br/>
- [Windows](TESTED.md#windows)<br/>
- [Windows NT](TESTED.md#windows-nt)<br/>
- [OS/2](TESTED.md#os2)<br/>
- [Linux](TESTED.md#linux)<br/>
- [Applications](TESTED.md#applications)<br/>
- [Games](TESTED.md#games)<br/>
- [Demos](TESTED.md#demos)<br/>
- [Emulators](TESTED.md#emulators)<br/>
