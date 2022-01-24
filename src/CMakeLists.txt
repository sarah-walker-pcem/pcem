include(${CMAKE_SOURCE_DIR}/cmake/pcem_includes.cmake)
set(PCEM_DEFINES ${PCEM_DEFINES} INST_PREFIX="${CMAKE_INSTALL_PREFIX}")

if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        set(PCEM_DEFINES ${PCEM_DEFINES} DEBUG)

        if(PCEM_SLIRP_DEBUG)
                set(PCEM_DEFINES ${PCEM_DEFINES} SLIRP_DEBUG)
        endif()
        if(PCEM_RECOMPILER_DEBUG)
                set(PCEM_DEFINES ${PCEM_DEFINES} RECOMPILER_DEBUG)
        endif()
        if(PCEM_NE2000_DEBUG)
                set(PCEM_DEFINES ${PCEM_DEFINES} NE2000_DEBUG)
        endif()
        if(PCEM_EMU8K_DEBUG_REGISTERS)
                set(PCEM_DEFINES ${PCEM_DEFINES} EMU8K_DEBUG_REGISTERS)
        endif()
        if(PCEM_SB_DSP_RECORD_DEBUG)
                set(PCEM_DEFINES ${PCEM_DEFINES} SB_DSP_RECORD_DEBUG)
        endif()
        if(PCEM_MACH64_DEBUG)
                set(PCEM_DEFINES ${PCEM_DEFINES} MACH64_DEBUG)
        endif()
        if(PCEM_NE2000_DEBUG)
                set(PCEM_DEFINES ${PCEM_DEFINES} NE2000_DEBUG)
        endif()
        if(PCEM_DEBUG_EXTRA)
                set(PCEM_DEFINES ${PCEM_DEFINES} DEBUG_EXTRA)
        endif()
        if(PCEM_PRINTER_DEBUG)
                set(PCEM_DEFINES ${PCEM_DEFINES} PRINTER_DEBUG)
        endif()
else()
        set(PCEM_DEFINES ${PCEM_DEFINES} RELEASE_BUILD)
endif()

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/wx-ui)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/wx-ui/wx-resources.cpp
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/wx-ui/pc.xrc
        COMMAND wxrc
        ARGS -c ${CMAKE_CURRENT_SOURCE_DIR}/wx-ui/pc.xrc -o ${CMAKE_CURRENT_BINARY_DIR}/wx-ui/wx-resources.cpp)

set(PCEM_SRC
        bus/mca.c
        bus/pci.c
        cdrom/cdrom-image.cc
        cdrom/cdrom-null.c
        codegen/codegen.c
        codegen/codegen_accumulate.c
        codegen/codegen_allocator.c
        codegen/codegen_block.c
        codegen/codegen_ir.c
        codegen/codegen_ops.c
        codegen/codegen_ops_3dnow.c
        codegen/codegen_ops_arith.c
        codegen/codegen_ops_branch.c
        codegen/codegen_ops_fpu_arith.c
        codegen/codegen_ops_fpu_constant.c
        codegen/codegen_ops_fpu_loadstore.c
        codegen/codegen_ops_fpu_misc.c
        codegen/codegen_ops_helpers.c
        codegen/codegen_ops_jump.c
        codegen/codegen_ops_logic.c
        codegen/codegen_ops_misc.c
        codegen/codegen_ops_mmx_arith.c
        codegen/codegen_ops_mmx_cmp.c
        codegen/codegen_ops_mmx_loadstore.c
        codegen/codegen_ops_mmx_logic.c
        codegen/codegen_ops_mmx_pack.c
        codegen/codegen_ops_mmx_shift.c
        codegen/codegen_ops_mov.c
        codegen/codegen_ops_shift.c
        codegen/codegen_ops_stack.c
        codegen/codegen_reg.c
        codegen/codegen_timing_486.c
        codegen/codegen_timing_686.c
        codegen/codegen_timing_common.c
        codegen/codegen_timing_cyrixiii.c
        codegen/codegen_timing_k6.c
        codegen/codegen_timing_p6.c
        codegen/codegen_timing_pentium.c
        codegen/codegen_timing_winchip.c
        codegen/codegen_timing_winchip2.c
        cpu/386.c
        cpu/386_common.c
        cpu/386_dynarec.c
        cpu/386_dynarec_ops.c
        cpu/808x.c
        cpu/cpu.c
        cpu/cpu_tables.c
        cpu/x86seg.c
        cpu/x87.c
        cpu/x87_timings.c
        devices/cassette.c
        devices/device.c
        devices/esdi_at.c
        devices/f82c710_upc.c
        devices/nvr.c
        devices/ps2_nvr.c
        devices/sis496.c
        disc/disc.c
        disc/disc_fdi.c
        disc/disc_img.c
        disc/disc_sector.c
        flash/intel_flash.c
        flash/rom.c
        flash/sst39sf010.c
        flash/tandy_eeprom.c
        flash/tandy_rom.c
        floppy/fdc.c
        floppy/fdc37c665.c
        floppy/fdc37c93x.c
        floppy/fdd.c
        hdd/hdd.c
        hdd/hdd_esdi.c
        hdd/hdd_file.c
        ide/ide.c
        ide/ide_atapi.c
        ide/ide_sff8038i.c
        ide/xtide.c
        joystick/gameport.c
        joystick/joystick_ch_flightstick_pro.c
        joystick/joystick_standard.c
        joystick/joystick_sw_pad.c
        joystick/joystick_tm_fcs.c
        keyboard/keyboard.c
        keyboard/keyboard_amstrad.c
        keyboard/keyboard_at.c
        keyboard/keyboard_olim24.c
        keyboard/keyboard_pcjr.c
        keyboard/keyboard_xt.c
        lpt/lpt.c
        lpt/lpt_dac.c
        lpt/lpt_dss.c
        memory/mem.c
        memory/mem_bios.c
        mfm/mfm_at.c
        mfm/mfm_xebec.c
        models/82091aa.c
        models/acc2036.c
        models/acc2168.c
        models/acc3221.c
        models/acer386sx.c
        models/ali1429.c
        models/amstrad.c
        models/cbm_io.c
        models/cmd640.c
        models/compaq.c
        models/cs8230.c
        models/dells200.c
        models/dma.c
        models/headland.c
        models/i430fx.c
        models/i430hx.c
        models/i430lx.c
        models/i430vx.c
        models/i440bx.c
        models/i440fx.c
        models/intel.c
        models/jim.c
        models/laserxt.c
        models/model.c
        models/mvp3.c
        models/neat.c
        models/nmi.c
        models/nvr_tc8521.c
        models/olivetti_m24.c
        models/opti495.c
        models/pc87306.c
        models/pc87307.c
        models/pic.c
        models/piix.c
        models/piix_pm.c
        models/pit.c
        models/ps1.c
        models/ps2.c
        models/ps2_mca.c
        models/scamp.c
        models/scat.c
        models/serial.c
        models/sio.c
        models/sl82c460.c
        models/superxt.c
        models/t1000.c
        models/t3100e.c
        models/um8669f.c
        models/um8881f.c
        models/vl82c480.c
        models/vt82c586b.c
        models/w83877tf.c
        models/w83977tf.c
        models/wd76c10.c
        models/xi8088.c
        models/zenith.c
        mouse/mouse.c
        mouse/mouse_msystems.c
        mouse/mouse_ps2.c
        mouse/mouse_serial.c
        scsi/scsi.c
        scsi/scsi_53c400.c
        scsi/scsi_aha1540.c
        scsi/scsi_cd.c
        scsi/scsi_hd.c
        scsi/scsi_ibm.c
        scsi/scsi_zip.c
        sound/sound.c
        sound/sound_ad1848.c
        sound/sound_adlib.c
        sound/sound_adlibgold.c
        sound/sound_audiopci.c
        sound/sound_azt2316a.c
        sound/sound_cms.c
        sound/sound_dbopl.cc
        sound/sound_emu8k.c
        sound/sound_gus.c
        sound/sound_mpu401_uart.c
        sound/sound_opl.c
        sound/sound_pas16.c
        sound/sound_ps1.c
        sound/sound_pssj.c
        sound/sound_resid.cc
        sound/sound_sb.c
        sound/sound_sb_dsp.c
        sound/sound_sn76489.c
        sound/sound_speaker.c
        sound/sound_ssi2001.c
        sound/sound_wss.c
        sound/sound_ym7128.c
        sound/soundopenal.c
        video/vid_ati18800.c
        video/vid_ati28800.c
        video/vid_ati68860_ramdac.c
        video/vid_ati_eeprom.c
        video/vid_ati_mach64.c
        video/vid_cga.c
        video/vid_cl5429.c
        video/vid_colorplus.c
        video/vid_compaq_cga.c
        video/vid_ddc.c
        video/vid_ega.c
        video/vid_et4000.c
        video/vid_et4000w32.c
        video/vid_genius.c
        video/vid_hercules.c
        video/vid_ht216.c
        video/vid_icd2061.c
        video/vid_ics2595.c
        video/vid_im1024.c
        video/vid_incolor.c
        video/vid_mda.c
        video/vid_mga.c
        video/vid_olivetti_m24.c
        video/vid_oti037.c
        video/vid_oti067.c
        video/vid_paradise.c
        video/vid_pc1512.c
        video/vid_pc1640.c
        video/vid_pc200.c
        video/vid_pcjr.c
        video/vid_pgc.c
        video/vid_ps1_svga.c
        video/vid_s3.c
        video/vid_s3_virge.c
        video/vid_sdac_ramdac.c
        video/vid_sigma.c
        video/vid_stg_ramdac.c
        video/vid_svga.c
        video/vid_svga_render.c
        video/vid_t1000.c
        video/vid_t3100e.c
        video/vid_tandy.c
        video/vid_tandysl.c
        video/vid_tgui9440.c
        video/vid_tkd8001_ramdac.c
        video/vid_tvga.c
        video/vid_tvp3026_ramdac.c
        video/vid_unk_ramdac.c
        video/vid_vga.c
        video/vid_voodoo.c
        video/vid_voodoo_banshee.c
        video/vid_voodoo_banshee_blitter.c
        video/vid_voodoo_blitter.c
        video/vid_voodoo_display.c
        video/vid_voodoo_fb.c
        video/vid_voodoo_fifo.c
        video/vid_voodoo_reg.c
        video/vid_voodoo_render.c
        video/vid_voodoo_setup.c
        video/vid_voodoo_texture.c
        video/vid_wy700.c
        video/video.c
        config.c
        fdi2raw.c
        io.c
        mcr.c
        paths.c
        pc.c
        plugin.c
        ppi.c
        pzx.c
        rtc.c
        rtc_tc8521.c
        timer.c
        )

# DOSBox
set(PCEM_SRC ${PCEM_SRC}
        dosbox/cdrom_image.cpp
        dosbox/dbopl.cpp
        dosbox/nukedopl.cpp
        dosbox/vid_cga_comp.c
        )

# RESID-FP
set(PCEM_SRC ${PCEM_SRC}
        sound/resid-fp/convolve.cc
        sound/resid-fp/convolve-sse.cc
        sound/resid-fp/envelope.cc
        sound/resid-fp/extfilt.cc
        sound/resid-fp/filter.cc
        sound/resid-fp/pot.cc
        sound/resid-fp/sid.cc
        sound/resid-fp/voice.cc
        sound/resid-fp/wave6581_PS_.cc
        sound/resid-fp/wave6581_PST.cc
        sound/resid-fp/wave6581_P_T.cc
        sound/resid-fp/wave6581__ST.cc
        sound/resid-fp/wave8580_PS_.cc
        sound/resid-fp/wave8580_PST.cc
        sound/resid-fp/wave8580_P_T.cc
        sound/resid-fp/wave8580__ST.cc
        sound/resid-fp/wave.cc
        )

# MiniVHD
set(PCEM_SRC ${PCEM_SRC}
        hdd/minivhd/cwalk.c
        hdd/minivhd/libxml2_encoding.c
        hdd/minivhd/minivhd_convert.c
        hdd/minivhd/minivhd_create.c
        hdd/minivhd/minivhd_io.c
        hdd/minivhd/minivhd_manage.c
        hdd/minivhd/minivhd_struct_rw.c
        hdd/minivhd/minivhd_util.c
        )

if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux" AND USE_ALSA)
        set(PCEM_SRC ${PCEM_SRC}
                sound/midi_alsa.c
                )
        set(PCEM_ADDITIONAL_LIBS ${PCEM_ADDITIONAL_LIBS} ${ALSA_LIBRARIES})
else()
        set(PCEM_SRC ${PCEM_SRC}
                wx-ui/wx-sdl2-midi.c
                )
endif()

if(USE_EXPERIMENTAL AND USE_EXPERIMENTAL_PGC)
        set(PCEM_DEFINES ${PCEM_DEFINES} USE_EXPERIMENTAL_PGC)
endif()

if(USE_EXPERIMENTAL AND USE_EXPERIMENTAL_PRINTER)
        set(PCEM_DEFINES ${PCEM_DEFINES} USE_EXPERIMENTAL_PRINTER)
        set(PCEM_SRC ${PCEM_SRC}
                lpt/lpt_epsonlx810.c
                )
        set(PCEM_ADDITIONAL_LIBS ${PCEM_ADDITIONAL_LIBS} Freetype::Freetype)
endif()

if(${PCEM_CPU_TYPE} STREQUAL "x86_64")
        set(PCEM_SRC ${PCEM_SRC}
                codegen/x86-64/codegen_backend_x86-64.c
                codegen/x86-64/codegen_backend_x86-64_ops.c
                codegen/x86-64/codegen_backend_x86-64_ops_sse.c
                codegen/x86-64/codegen_backend_x86-64_uops.c
                )
endif()

if(${PCEM_CPU_TYPE} STREQUAL "i386")
        set(PCEM_SRC ${PCEM_SRC}
                codegen/x86/codegen_backend_x86.c
                codegen/x86/codegen_backend_x86_ops.c
                codegen/x86/codegen_backend_x86_ops_fpu.c
                codegen/x86/codegen_backend_x86_ops_sse.c
                codegen/x86/codegen_backend_x86_uops.c
                )
endif()

if(${PCEM_CPU_TYPE} STREQUAL "arm")
        set(PCEM_SRC ${PCEM_SRC}
                codegen/arm32/codegen_backend_arm.c
                codegen/arm32/codegen_backend_arm_ops.c
                codegen/arm32/codegen_backend_arm_uops.c
                )
endif()

if(${PCEM_CPU_TYPE} STREQUAL "arm64")
        set(PCEM_SRC ${PCEM_SRC}
                codegen/arm64/codegen_backend_arm64.c
                codegen/arm64/codegen_backend_arm64_imm.c
                codegen/arm64/codegen_backend_arm64_ops.c
                codegen/arm64/codegen_backend_arm64_uops.c
                )
endif()

if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
        set(PCEM_SRC ${PCEM_SRC}
                cdrom/cdrom-ioctl-linux.c
                wx-ui/wx-sdl2-display.c
                )
endif()

if(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
        set(PCEM_SRC ${PCEM_SRC}
                cdrom/cdrom-ioctl.c
                wx-ui/wx-sdl2-display-win.c
                )

        set(PCEM_BIN_SRC ${PCEM_BIN_SRC}
                wx-ui/wx.rc
                )
endif()

set(PCEM_SRC ${PCEM_SRC}
        wx-ui/pc.xrc
        wx-ui/wx-main.cc
        wx-ui/wx-config_sel.c
        wx-ui/wx-dialogbox.cc
        wx-ui/wx-utils.cc
        wx-ui/wx-app.cc
        wx-ui/wx-sdl2-joystick.c
        wx-ui/wx-sdl2-mouse.c
        wx-ui/wx-sdl2-keyboard.c
        wx-ui/wx-sdl2-video.c
        wx-ui/wx-sdl2.c
        wx-ui/wx-config.c
        wx-ui/wx-deviceconfig.cc
        wx-ui/wx-status.cc
        wx-ui/wx-sdl2-status.c
        wx-ui/wx-thread.c
        wx-ui/wx-common.c
        wx-ui/wx-sdl2-video-renderer.c
        wx-ui/wx-sdl2-video-gl3.c
        wx-ui/wx-glslp-parser.c
        wx-ui/wx-shader_man.c
        wx-ui/wx-shaderconfig.cc
        wx-ui/wx-joystickconfig.cc
        wx-ui/wx-config-eventbinder.cc
        wx-ui/wx-createdisc.cc
        ${CMAKE_CURRENT_BINARY_DIR}/wx-ui/wx-resources.cpp
        )


if(USE_NETWORKING)
        set(PCEM_DEFINES ${PCEM_DEFINES} USE_NETWORKING)

        if(USE_PCAP_NETWORKING)
                set(PCEM_DEFINES ${PCEM_DEFINES} USE_PCAP_NETWORKING)
                set(PCEM_ADDITIONAL_LIBS ${PCEM_ADDITIONAL_LIBS} ${PCAP_LIBRARY})
        endif()

        set(PCEM_SRC ${PCEM_SRC}
                networking/ne2000.c
                networking/nethandler.c
                wx-ui/wx-hostconfig.c
                )

        set(PCEM_SRC ${PCEM_SRC}
                networking/slirp/bootp.c
                networking/slirp/cksum.c
                networking/slirp/debug.c
                networking/slirp/if.c
                networking/slirp/ip_icmp.c
                networking/slirp/ip_input.c
                networking/slirp/ip_output.c
                networking/slirp/mbuf.c
                networking/slirp/misc.c
                networking/slirp/queue.c
                networking/slirp/sbuf.c
                networking/slirp/slirp.c
                networking/slirp/socket.c
                networking/slirp/tcp_input.c
                networking/slirp/tcp_output.c
                networking/slirp/tcp_subr.c
                networking/slirp/tcp_timer.c
                networking/slirp/tftp.c
                networking/slirp/udp.c
                )

        if(WIN32)
                set(PCEM_ADDITIONAL_LIBS ${PCEM_ADDITIONAL_LIBS} wsock32 iphlpapi)
        endif()
endif()

set(PCEM_BIN_SRC ${PCEM_BIN_SRC}
        main.c
        )

set(PCEM_LIBRARIES ${wxWidgets_LIBRARIES} ${SDL2_LIBRARIES} ${OPENAL_LIBRARY} ${OPENGL_LIBRARIES} ${PCEM_ADDITIONAL_LIBS})

add_library(pcem SHARED ${PCEM_SRC} ${PCEM_PRIVATE_HEADERS} ${PCEM_PUBLIC_HEADERS})
target_compile_definitions(pcem PUBLIC ${PCEM_DEFINES})
target_compile_options(pcem PUBLIC $<$<COMPILE_LANGUAGE:CXX>:-fcommon> $<$<COMPILE_LANGUAGE:C>:-fcommon>)
target_link_libraries(pcem ${PCEM_LIBRARIES})

add_executable(pcem-bin ${PCEM_BIN_SRC})
target_link_libraries(pcem-bin pcem ${PCEM_LIBRARIES})
set_target_properties(pcem-bin PROPERTIES OUTPUT_NAME pcem)

install(TARGETS pcem pcem-bin RUNTIME DESTINATION bin LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/nvr/ DESTINATION share/pcem/nvr/default)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/roms DESTINATION share/pcem)
install(FILES ${CMAKE_SOURCE_DIR}/COPYING DESTINATION share/licenses/pcem)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/includes/public/ DESTINATION include FILES_MATCHING PATTERN *.h)
