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
target_compile_options(pcem PUBLIC $<$<COMPILE_LANGUAGE:CXX>:-fno-common> $<$<COMPILE_LANGUAGE:C>:-fno-common>)
if(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
        target_link_options(pcem PRIVATE "-Wl,--subsystem,windows")
endif()
target_link_libraries(pcem PRIVATE ${PCEM_LIBRARIES})

if(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
        set_target_properties(pcem PROPERTIES
                MACOSX_BUNDLE TRUE
                BUILD_WITH_INSTALL_RPATH TRUE
                INSTALL_RPATH "@executable_path/../Frameworks"
                OUTPUT_NAME PCem
                MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/wx-ui/macos/Info.plist.in
                MACOSX_BUNDLE_BUNDLE_NAME PCem
                MACOSX_BUNDLE_GUI_IDENTIFIER uk.co.pcem-emulator.PCem
                XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER uk.co.pcem-emulator.PCem
                MACOSX_BUNDLE_SHORT_VERSION_STRING "${PCEM_VERSION_STRING}"
                MACOSX_BUNDLE_BUNDLE_VERSION "${PCEM_VERSION_STRING}")

        find_library(PCEM_SDL3_LIBRARY NAMES SDL3)
        set(PCEM_BUNDLE_LIB_DIRS "")
        foreach(lib ${PCEM_LIBRARIES})
                if(TARGET ${lib})
                        set(PCEM_BUNDLE_LIB_DIRS "${PCEM_BUNDLE_LIB_DIRS}|$<TARGET_FILE_DIR:${lib}>")
                elseif(lib MATCHES "^-L")
                        string(SUBSTRING "${lib}" 2 -1 lib_dir)
                        set(PCEM_BUNDLE_LIB_DIRS "${PCEM_BUNDLE_LIB_DIRS}|${lib_dir}")
                elseif(IS_ABSOLUTE "${lib}")
                        get_filename_component(lib_dir "${lib}" DIRECTORY)
                        set(PCEM_BUNDLE_LIB_DIRS "${PCEM_BUNDLE_LIB_DIRS}|${lib_dir}")
                endif()
        endforeach()

        add_custom_command(TARGET pcem POST_BUILD
                COMMAND ${CMAKE_COMMAND} "-DAPP=$<TARGET_BUNDLE_DIR:pcem>" "-DLIB_DIRS=${PCEM_BUNDLE_LIB_DIRS}"
                        "-DSDL3_LIBRARY=${PCEM_SDL3_LIBRARY}" -P ${CMAKE_SOURCE_DIR}/cmake/macos-bundle.cmake
                VERBATIM)
endif()
