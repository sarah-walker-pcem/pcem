# Makefile.am for PCem

AUTOMAKE_OPTIONS = subdir-objects
bin_PROGRAMS = pcem
noinst_SCRIPTS =
CLEANFILES =

if !OS_WINDOWS
CLEANFILES += $(noinst_SCRIPTS)
noinst_SCRIPTS += ../pcem
../pcem: pcem
	cp pcem ..
endif

amrefresh:	

# Special wxWidgets files
WINDRES = $(shell $(WX_CONFIG_PATH) --rescomp)
wx.res: wx.rc
	$(WINDRES) -i wx.rc --input-format=rc -o wx.res -O coff
 
wx-resources.cpp : pc.xrc
	-wxrc -c pc.xrc -o wx-resources.cpp

# PCem
pcem_SOURCES = 386.c 386_common.c 386_dynarec.c 386_dynarec_ops.c 808x.c 82091aa.c acc2036.c acc2168.c acc3221.c acer386sx.c \
ali1429.c amstrad.c cassette.c cbm_io.c cdrom-image.cc cdrom-null.c cmd640.c codegen.c codegen_accumulate.c \
codegen_allocator.c codegen_block.c codegen_ir.c codegen_ops.c codegen_ops_3dnow.c codegen_ops_arith.c \
codegen_ops_branch.c codegen_ops_fpu_arith.c codegen_ops_fpu_constant.c codegen_ops_fpu_loadstore.c \
codegen_ops_fpu_misc.c codegen_ops_mmx_arith.c codegen_ops_mmx_cmp.c codegen_ops_mmx_loadstore.c \
codegen_ops_mmx_logic.c codegen_ops_mmx_pack.c codegen_ops_mmx_shift.c codegen_ops_helpers.c codegen_ops_jump.c \
codegen_ops_logic.c codegen_ops_shift.c codegen_ops_misc.c codegen_ops_mov.c codegen_ops_stack.c codegen_reg.c \
codegen_timing_486.c codegen_timing_686.c codegen_timing_common.c codegen_timing_k6.c codegen_timing_pentium.c \
codegen_timing_winchip.c codegen_timing_winchip2.c compaq.c config.c cpu.c cpu_tables.c cs8230.c dells200.c device.c disc.c \
disc_fdi.c disc_img.c disc_sector.c dma.c esdi_at.c f82c710_upc.c fdc.c fdc37c665.c fdc37c93x.c fdd.c fdi2raw.c gameport.c hdd.c hdd_esdi.c \
hdd_file.c headland.c i430lx.c i430fx.c i430hx.c i430vx.c ide.c ide_atapi.c ide_sff8038i.c intel.c intel_flash.c io.c \
jim.c joystick_ch_flightstick_pro.c joystick_standard.c joystick_sw_pad.c joystick_tm_fcs.c keyboard.c \
keyboard_amstrad.c keyboard_at.c keyboard_olim24.c keyboard_pcjr.c keyboard_xt.c laserxt.c lpt.c lpt_dac.c lpt_dss.c \
mca.c mcr.c mem.c mem_bios.c mfm_at.c mfm_xebec.c model.c mouse.c mouse_msystems.c mouse_ps2.c mouse_serial.c mvp3.c \
neat.c nmi.c nvr.c olivetti_m24.c opti495.c paths.c pc.c pc87306.c pci.c pic.c piix.c pit.c ppi.c ps1.c ps2.c ps2_mca.c \
ps2_nvr.c nvr_tc8521.c pzx.c rom.c rtc.c rtc_tc8521.c scamp.c scat.c scsi.c scsi_53c400.c scsi_aha1540.c scsi_cd.c scsi_hd.c \
scsi_ibm.c scsi_zip.c serial.c sio.c sis496.c sl82c460.c sound.c sound_ad1848.c sound_adlib.c sound_adlibgold.c sound_audiopci.c \
sound_azt2316a.c sound_cms.c sound_emu8k.c sound_gus.c sound_mpu401_uart.c sound_opl.c sound_pas16.c sound_ps1.c sound_pssj.c \
sound_sb.c sound_sb_dsp.c sound_sn76489.c sound_speaker.c sound_ssi2001.c sound_wss.c sound_ym7128.c soundopenal.c \
sst39sf010.c tandy_eeprom.c tandy_rom.c t1000.c t3100e.c timer.c um8669f.c um8881f.c vid_ati_eeprom.c vid_ati_mach64.c \
vid_ati18800.c vid_ati28800.c vid_ati68860_ramdac.c vid_cga.c vid_cl5429.c vid_colorplus.c vid_compaq_cga.c vid_ega.c \
vid_et4000.c vid_et4000w32.c vid_genius.c vid_hercules.c vid_ht216.c vid_icd2061.c vid_ics2595.c vid_im1024.c \
vid_incolor.c vid_mda.c vid_mga.c vid_olivetti_m24.c vid_oti037.c vid_oti067.c vid_paradise.c vid_pc200.c vid_pc1512.c \
vid_pc1640.c vid_pcjr.c vid_pgc.c vid_ps1_svga.c vid_s3.c vid_s3_virge.c vid_sdac_ramdac.c vid_sigma.c \
vid_stg_ramdac.c vid_svga.c vid_svga_render.c vid_t1000.c vid_t3100e.c vid_tandy.c vid_tandysl.c vid_tgui9440.c \
vid_tkd8001_ramdac.c vid_tvga.c vid_unk_ramdac.c vid_vga.c vid_voodoo.c video.c wd76c10.c vid_wy700.c vt82c586b.c \
vl82c480.c w83877tf.c x86seg.c x87.c x87_timings.c xi8088.c xtide.c sound_dbopl.cc sound_resid.cc

# DOSBox
pcem_SOURCES += dosbox/cdrom_image.cpp dosbox/dbopl.cpp dosbox/nukedopl.cpp dosbox/vid_cga_comp.c

# resid-fp
pcem_SOURCES += resid-fp/convolve.cc resid-fp/convolve-sse.cc resid-fp/envelope.cc \
resid-fp/extfilt.cc resid-fp/filter.cc resid-fp/pot.cc resid-fp/sid.cc \
resid-fp/voice.cc resid-fp/wave6581_PS_.cc resid-fp/wave6581_PST.cc \
resid-fp/wave6581_P_T.cc resid-fp/wave6581__ST.cc resid-fp/wave8580_PS_.cc \
resid-fp/wave8580_PST.cc resid-fp/wave8580_P_T.cc resid-fp/wave8580__ST.cc \
resid-fp/wave.cc

pcem_CFLAGS = $(subst -fpermissive,,$(shell $(WX_CONFIG_PATH) --cxxflags) $(shell sdl2-config --cflags))
pcem_CXXFLAGS = $(shell $(WX_CONFIG_PATH) --cxxflags) $(shell sdl2-config --cflags)
pcem_LDADD = @LIBS@
pcem_SOURCES += wx-main.cc wx-config_sel.c wx-dialogbox.cc wx-utils.cc wx-app.cc \
 wx-sdl2-joystick.c wx-sdl2-mouse.c wx-sdl2-keyboard.c wx-sdl2-video.c \
 wx-sdl2.c wx-config.c wx-deviceconfig.cc wx-status.cc wx-sdl2-status.c \
 wx-thread.c wx-common.c wx-sdl2-video-renderer.c wx-sdl2-video-gl3.c \
 wx-glslp-parser.c wx-shader_man.c wx-shaderconfig.cc wx-joystickconfig.cc wx-createdisc.cc \
 wx-resources.cpp
if USE_ALSA
pcem_LDADD += -lasound
pcem_SOURCES += midi_alsa.c
else
pcem_SOURCES += wx-sdl2-midi.c
endif

if CPU_I386
pcem_SOURCES += codegen_backend_x86.c codegen_backend_x86_ops.c codegen_backend_x86_ops_fpu.c codegen_backend_x86_ops_sse.c codegen_backend_x86_uops.c
pcem_CFLAGS += -msse2
endif

if CPU_X86_64
pcem_SOURCES += codegen_backend_x86-64.c codegen_backend_x86-64_ops.c codegen_backend_x86-64_ops_sse.c codegen_backend_x86-64_uops.c
endif

if CPU_ARM
pcem_SOURCES += codegen_backend_arm.c codegen_backend_arm_ops.c codegen_backend_arm_uops.c
endif

if CPU_ARM64
pcem_SOURCES += codegen_backend_arm64.c codegen_backend_arm64_imm.c codegen_backend_arm64_ops.c codegen_backend_arm64_uops.c
endif

if USE_NETWORKING
pcem_CFLAGS += -DUSE_NETWORKING
pcem_CXXFLAGS += -DUSE_NETWORKING
pcem_SOURCES += ne2000.c nethandler.c wx-hostconfig.c
# SLiRP
pcem_SOURCES += slirp/bootp.c slirp/cksum.c slirp/debug.c slirp/if.c slirp/ip_icmp.c \
slirp/ip_input.c slirp/ip_output.c slirp/mbuf.c slirp/misc.c slirp/queue.c \
slirp/sbuf.c slirp/slirp.c slirp/socket.c slirp/tcp_input.c slirp/tcp_output.c \
slirp/tcp_subr.c slirp/tcp_timer.c slirp/tftp.c slirp/udp.c

if OS_WINDOWS
pcem_LDADD += -lwsock32 -liphlpapi
endif
endif

if OS_LINUX
pcem_SOURCES += cdrom-ioctl-linux.c wx-sdl2-display.c
endif

if OS_OTHER
pcem_SOURCES += cdrom-ioctl-dummy.c wx-sdl2-display.c
endif

if OS_MACOSX
pcem_SOURCES += cdrom-ioctl-osx.c wx-sdl2-display.c
pcem_CFLAGS += -DPCEM_RENDER_WITH_TIMER -DPCEM_RENDER_TIMER_LOOP
pcem_CXXFLAGS += -DPCEM_RENDER_WITH_TIMER -DPCEM_RENDER_TIMER_LOOP
endif

if OS_WINDOWS
DEFAULT_INCLUDES = -iquote .
pcem_SOURCES += cdrom-ioctl.c wx-sdl2-display-win.c
pcem_LDADD += wx.res
endif

if !HAS_OFF64T
#pcem_CFLAGS += -Doff64_t=off_t -Dfopen64=fopen -Dfseeko64=fseek -Dftello64=ftell
endif

if RELEASE_BUILD
pcem_CFLAGS += -DRELEASE_BUILD
pcem_CXXFLAGS += -DRELEASE_BUILD
endif

#pcem_CFLAGS += -mtune=cortex-a53
#pcem_CXXFLAGS += -mtune=cortex-a53
#pcem_LDFLAGS = -flto -O3 -mtune=cortex-a15

# pcem_CFLAGS += -fprofile-use
# pcem_CXXFLAGS += -fprofile-use
# pcem_LDFLAGS = -fprofile-use
