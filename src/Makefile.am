# Makefile.am for PCem

bin_PROGRAMS = pcem
noinst_SCRIPTS = ../pcem
CLEANFILES = $(noinst_SCRIPTS)

../pcem: pcem
	cp pcem ..

amrefresh:	


pcem_SOURCES = 386.c 386_dynarec.c 386_dynarec_ops.c 808x.c acer386sx.c ali1429.c  amstrad.c cdrom-ioctl-linux.c cdrom-image.cc cdrom-null.c \
codegen.c codegen_ops.c codegen_timing_486.c codegen_timing_686.c codegen_timing_common.c codegen_timing_pentium.c codegen_timing_winchip.c compaq.c config.c cpu.c \
dac.c dells200.c device.c disc.c disc_fdi.c disc_img.c disc_sector.c dma.c esdi_at.c fdc.c fdc37c665.c fdd.c fdi2raw.c gameport.c \
hdd.c hdd_esdi.c hdd_file.c headland.c i430lx.c i430fx.c i430vx.c ide.c intel.c intel_flash.c io.c jim.c joystick_ch_flightstick_pro.c joystick_standard.c joystick_sw_pad.c \
joystick_tm_fcs.c keyboard.c keyboard_amstrad.c keyboard_at.c keyboard_olim24.c keyboard_pcjr.c keyboard_xt.c \
laserxt.c lpt.c mca.c mcr.c mem.c mfm_at.c mfm_xebec.c model.c mouse.c mouse_ps2.c mouse_serial.c neat.c nmi.c nvr.c olivetti_m24.c \
opti495.c paths.c pc.c pci.c pic.c piix.c pit.c ppi.c ps1.c ps2.c ps2_mca.c ps2_nvr.c rom.c rtc.c scat.c serial.c sio.c sis496.c sound.c \
sound_ad1848.c sound_adlib.c sound_adlibgold.c sound_cms.c sound_emu8k.c sound_gus.c \
sound_mpu401_uart.c sound_opl.c sound_pas16.c sound_ps1.c sound_pssj.c sound_sb.c sound_sb_dsp.c sound_sn76489.c \
sound_speaker.c sound_ssi2001.c sound_wss.c sound_ym7128.c soundopenal.c tandy_eeprom.c tandy_rom.c \
timer.c um8669f.c um8881f.c vid_ati_eeprom.c vid_ati_mach64.c vid_ati18800.c vid_ati28800.c \
vid_ati68860_ramdac.c vid_cga.c vid_cl5429.c vid_colorplus.c vid_ega.c vid_et4000.c vid_et4000w32.c vid_genius.c vid_hercules.c \
vid_icd2061.c vid_ics2595.c vid_incolor.c vid_mda.c vid_olivetti_m24.c vid_oti067.c vid_paradise.c vid_pc200.c \
vid_pc1512.c vid_pc1640.c vid_pcjr.c vid_ps1_svga.c vid_s3.c vid_s3_virge.c vid_sdac_ramdac.c \
vid_stg_ramdac.c vid_svga.c vid_svga_render.c vid_tandy.c vid_tandysl.c vid_tgui9440.c \
vid_tkd8001_ramdac.c vid_tvga.c vid_unk_ramdac.c vid_vga.c vid_voodoo.c video.c wd76c10.c vid_wy700.c \
x86seg.c x87.c xtide.c sound_dbopl.cc sound_resid.cc dosbox/cdrom_image.cpp dosbox/dbopl.cpp \
dosbox/nukedopl.cpp dosbox/vid_cga_comp.c resid-fp/convolve.cc resid-fp/convolve-sse.cc resid-fp/envelope.cc \
resid-fp/extfilt.cc resid-fp/filter.cc resid-fp/pot.cc resid-fp/sid.cc \
resid-fp/voice.cc resid-fp/wave6581_PS_.cc resid-fp/wave6581_PST.cc \
resid-fp/wave6581_P_T.cc resid-fp/wave6581__ST.cc resid-fp/wave8580_PS_.cc \
resid-fp/wave8580_PST.cc resid-fp/wave8580_P_T.cc resid-fp/wave8580__ST.cc \
resid-fp/wave.cc

if USE_WX
pcem_CFLAGS = $(shell wx-config --cxxflags) $(shell sdl2-config --cflags)
pcem_CXXFLAGS = $(shell wx-config --cxxflags) $(shell sdl2-config --cflags)
pcem_LDADD = @LIBS@
pcem_SOURCES += wx-main.cc wx-config_sel.c wx-dialogbox.cc wx-utils.cc wx-app.cc \
 wx-sdl2-joystick.c wx-sdl2-mouse.c wx-sdl2-keyboard.c wx-sdl2-video.c wx-sdl2-midi.c \
 wx-sdl2.c wx-config.c wx-deviceconfig.cc wx-status.cc wx-sdl2-status.c \
 wx-resources.cpp wx-thread.c wx-common.c wx-sdl2-display.c wx-sdl2-video-renderer.c wx-sdl2-video-gl3.c \
 wx-glslp-parser.c wx-shader_man.c wx-shaderconfig.cc
else
pcem_CFLAGS = $(allegro_CFLAGS) -DPCEM_ALLEGRO
pcem_LDADD = $(allegro_LIBS) -lopenal
pcem_SOURCES += allegro-gui.c allegro-gui-config-sel.c allegro-gui-configure.c \
 allegro-gui-deviceconfig.c allegro-gui-hdconf.c allegro-joystick.c \
 allegro-keyboard.c allegro-main.c allegro-midi.c allegro-mouse.c allegro-video.c \
 thread-pthread.c
endif

if CPU_I386
pcem_SOURCES += codegen_x86.c
pcem_CFLAGS += -msse2
endif

if CPU_X86_64
pcem_SOURCES += codegen_x86-64.c
endif

