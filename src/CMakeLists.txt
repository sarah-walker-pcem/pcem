if(FORCE_X11)
        set(PCEM_DEFINES ${PCEM_DEFINES} FORCE_X11)
endif()

set(PCEM_PRIVATE_API ${PCEM_PRIVATE_API}
        ${CMAKE_SOURCE_DIR}/includes/private/bswap.h
        ${CMAKE_SOURCE_DIR}/includes/private/fdi2raw.h
        ${CMAKE_SOURCE_DIR}/includes/private/filters.h
        ${CMAKE_SOURCE_DIR}/includes/private/ibm.h
        ${CMAKE_SOURCE_DIR}/includes/private/io.h
        ${CMAKE_SOURCE_DIR}/includes/private/pch.h
        ${CMAKE_SOURCE_DIR}/includes/private/pgc_palettes.h
        ${CMAKE_SOURCE_DIR}/includes/private/plat-dinput.h
        ${CMAKE_SOURCE_DIR}/includes/private/plat-joystick.h
        ${CMAKE_SOURCE_DIR}/includes/private/plat-keyboard.h
        ${CMAKE_SOURCE_DIR}/includes/private/plat-midi.h
        ${CMAKE_SOURCE_DIR}/includes/private/plat-mouse.h
        ${CMAKE_SOURCE_DIR}/includes/private/pzx.h
        ${CMAKE_SOURCE_DIR}/includes/private/resources.h
        ${CMAKE_SOURCE_DIR}/includes/private/rtc.h
        ${CMAKE_SOURCE_DIR}/includes/private/rtc_tc8521.h
        ${CMAKE_SOURCE_DIR}/includes/private/scamp.h
        ${CMAKE_SOURCE_DIR}/includes/private/thread.h
        ${CMAKE_SOURCE_DIR}/includes/private/timer.h
        )

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

if(${PCEM_DISPLAY_ENGINE} STREQUAL "wxWidgets")
        include(${CMAKE_CURRENT_SOURCE_DIR}/wx-ui/wx-ui.cmake)
        include(${CMAKE_CURRENT_SOURCE_DIR}/wx-ui/viewers/viewers.cmake)
endif()
if(${PCEM_DISPLAY_ENGINE} STREQUAL "Qt")
        message(FATAL_ERROR "Qt Mode is not yet implemented.")
endif()

set(PCEM_SRC ${PCEM_SRC}
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

set(PCEM_LIBRARIES ${DISPLAY_ENGINE_LIBRARIES} ${SDL2_LIBRARIES} ${OPENAL_LIBRARY} ${OPENGL_LIBRARIES} ${PCEM_ADDITIONAL_LIBS})

include(${CMAKE_CURRENT_SOURCE_DIR}/plugin-api/plugin-api.cmake)

set(PCEM_LIBRARIES ${PCEM_LIBRARIES} PARENT_SCOPE)

add_executable(pcem ${PCEM_SRC} ${PCEM_PRIVATE_API} ${PCEM_EMBEDDED_PLUGIN_API})
target_compile_definitions(pcem PUBLIC ${PCEM_DEFINES})
target_compile_options(pcem PUBLIC $<$<COMPILE_LANGUAGE:CXX>:-fcommon> $<$<COMPILE_LANGUAGE:C>:-fcommon>)
if(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
        target_link_options(pcem PRIVATE "-Wl,--subsystem,windows")
endif()
target_link_libraries(pcem ${PCEM_LIBRARIES})
