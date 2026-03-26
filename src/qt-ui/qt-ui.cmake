set(PCEM_PRIVATE_API ${PCEM_PRIVATE_API}
        ${CMAKE_SOURCE_DIR}/includes/private/qt-ui/qt-app.h
        ${CMAKE_SOURCE_DIR}/includes/private/qt-ui/qt-common.h
        ${CMAKE_SOURCE_DIR}/includes/private/qt-ui/qt-createdisc.h
        ${CMAKE_SOURCE_DIR}/includes/private/qt-ui/qt-deviceconfig.h
        ${CMAKE_SOURCE_DIR}/includes/private/qt-ui/qt-dialogbox.h
        ${CMAKE_SOURCE_DIR}/includes/private/qt-ui/qt-display.h
        ${CMAKE_SOURCE_DIR}/includes/private/qt-ui/qt-glsl.h
        ${CMAKE_SOURCE_DIR}/includes/private/qt-ui/qt-glslp-parser.h
        ${CMAKE_SOURCE_DIR}/includes/private/qt-ui/qt-hostconfig.h
        ${CMAKE_SOURCE_DIR}/includes/private/qt-ui/qt-joystickconfig.h
        ${CMAKE_SOURCE_DIR}/includes/private/qt-ui/qt-sdl2.h
        ${CMAKE_SOURCE_DIR}/includes/private/qt-ui/qt-sdl2-glw.h
        ${CMAKE_SOURCE_DIR}/includes/private/qt-ui/qt-sdl2-video.h
        ${CMAKE_SOURCE_DIR}/includes/private/qt-ui/qt-sdl2-video-gl3.h
        ${CMAKE_SOURCE_DIR}/includes/private/qt-ui/qt-sdl2-video-renderer.h
        ${CMAKE_SOURCE_DIR}/includes/private/qt-ui/qt-shaderconfig.h
        ${CMAKE_SOURCE_DIR}/includes/private/qt-ui/qt-status.h
        ${CMAKE_SOURCE_DIR}/includes/private/qt-ui/qt-utils.h
        )

set(PCEM_SRC ${PCEM_SRC}
        qt-ui/qt-app.cc
        qt-ui/qt-common.c
        qt-ui/qt-config.c
        qt-ui/qt-config_sel.c
        qt-ui/qt-config-eventbinder.cc
        qt-ui/qt-createdisc.cc
        qt-ui/qt-deviceconfig.cc
        qt-ui/qt-dialogbox.cc
        qt-ui/qt-glslp-parser.c
        qt-ui/qt-joystickconfig.cc
        qt-ui/qt-main.cc
        qt-ui/qt-sdl2.c
        qt-ui/qt-sdl2-joystick.c
        qt-ui/qt-sdl2-keyboard.c
        qt-ui/qt-sdl2-mouse.c
        qt-ui/qt-sdl2-status.c
        qt-ui/qt-sdl2-video.c
        qt-ui/qt-sdl2-video-gl3.c
        qt-ui/qt-sdl2-video-renderer.c
        qt-ui/qt-shader_man.c
        qt-ui/qt-shaderconfig.cc
        qt-ui/qt-status.cc
        qt-ui/qt-thread.c
        qt-ui/qt-utils.cc
		qt-ui/AboutDlg.ui
		qt-ui/ConfigureDlg.ui
		qt-ui/ConfigureSelectionDlg.ui
		qt-ui/ConfirmRememberDlg.ui
		qt-ui/CustomResolutionDlg.ui
		qt-ui/HdNewDlg.ui
		qt-ui/HdSizeDlg.ui
		qt-ui/HostConfig.ui
		qt-ui/ShaderManagerDlg.ui
        )

if(USE_NETWORKING)
        set(PCEM_SRC ${PCEM_SRC}
                qt-ui/qt-hostconfig.c
                )
endif()

# SDL display - unified Qt-based approach for all platforms
set(PCEM_SRC ${PCEM_SRC}
        qt-ui/qt-sdl2-display.cc
        )

if(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
        # TODO: add Windows application icon resource if needed
endif()

qt6_add_resources(PCEM_SRC qt-ui/qt-resources.qrc)

if(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
        add_compile_definitions(PCEM_RENDER_WITH_TIMER PCEM_RENDER_TIMER_LOOP)
endif()
