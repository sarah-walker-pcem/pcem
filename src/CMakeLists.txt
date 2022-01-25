set(PCEM_DEFINES ${PCEM_DEFINES} INST_PREFIX="${CMAKE_INSTALL_PREFIX}")

if(PLUGIN_ENGINE)
        set(PCEM_DEFINES ${PCEM_DEFINES} PLUGIN_ENGINE)
endif()

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

include(${CMAKE_CURRENT_SOURCE_DIR}/bus/bus.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cdrom/cdrom.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/codegen/codegen.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cpu/cpu.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/devices/devices.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/disc/disc.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/dosbox/dosbox.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/flash/flash.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/floppy/floppy.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/hdd/hdd.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/ide/ide.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/joystick/joystick.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/keyboard/keyboard.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/lpt/lpt.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/memory/memory.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/mfm/mfm.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/models/models.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/mouse/mouse.cmake)
if(USE_NETWORKING)
        include(${CMAKE_CURRENT_SOURCE_DIR}/networking/networking.cmake)
endif()
include(${CMAKE_CURRENT_SOURCE_DIR}/scsi/scsi.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/sound/sound.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/video/video.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/wx-ui/wx-ui.cmake)

set(PCEM_SRC ${PCEM_SRC_BUS} ${PCEM_SRC_CDROM} ${PCEM_SRC_CODEGEN} ${PCEM_SRC_CPU} ${PCEM_SRC_DEVICES} ${PCEM_SRC_DISC} ${PCEM_SRC_FLASH}
${PCEM_SRC_FLOPPY} ${PCEM_SRC_HDD} ${PCEM_SRC_IDE} ${PCEM_SRC_JOYSTICK} ${PCEM_SRC_KEYBOARD} ${PCEM_SRC_LPT} ${PCEM_SRC_MEMORY} ${PCEM_SRC_NETWORKING}
${PCEM_SRC_MFM} ${PCEM_SRC_DOSBOX} ${PCEM_SRC_WXUI} ${PCEM_SRC_MODELS} ${PCEM_SRC_MOUSE} ${PCEM_SRC_SCSI} ${PCEM_SRC_SOUND} ${PCEM_SRC_VIDEO})

set(PCEM_SRC
        ${PCEM_SRC}
        fdi2raw.c
        io.c
        mcr.c
        pc.c
        ppi.c
        pzx.c
        rtc.c
        rtc_tc8521.c
        timer.c
        )

set(PCEM_LIBRARIES ${wxWidgets_LIBRARIES} ${SDL2_LIBRARIES} ${OPENAL_LIBRARY} ${OPENGL_LIBRARIES} ${PCEM_ADDITIONAL_LIBS})

include(${CMAKE_CURRENT_SOURCE_DIR}/plugin-api/plugin-api.cmake)

if(PLUGIN_ENGINE)
        add_library(pcem-plugin-api SHARED ${PCEM_SRC_PLUGINAPI} ${PCEM_PUBLIC_API})
        target_compile_definitions(pcem-plugin-api PUBLIC ${PCEM_DEFINES})
        install(TARGETS pcem-plugin-api RUNTIME DESTINATION bin LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
        set(PCEM_LIBRARIES ${PCEM_LIBRARIES} pcem-plugin-api)
else()
        set(PCEM_EMBEDDED_PLUGIN_API ${PCEM_SRC_PLUGINAPI} ${PCEM_PUBLIC_API})
endif()

add_executable(pcem ${PCEM_SRC} ${PCEM_PRIVATE_API} ${PCEM_EMBEDDED_PLUGIN_API})
target_compile_definitions(pcem PUBLIC ${PCEM_DEFINES})
target_compile_options(pcem PUBLIC $<$<COMPILE_LANGUAGE:CXX>:-fcommon> $<$<COMPILE_LANGUAGE:C>:-fcommon>)
target_link_libraries(pcem ${PCEM_LIBRARIES})
install(TARGETS pcem RUNTIME DESTINATION bin)

install(DIRECTORY ${CMAKE_SOURCE_DIR}/nvr/ DESTINATION share/pcem/nvr/default)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/roms DESTINATION share/pcem)
install(FILES ${CMAKE_SOURCE_DIR}/COPYING DESTINATION share/licenses/pcem)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/includes/public/ DESTINATION include FILES_MATCHING PATTERN *.h)
