set(PCEM_PRIVATE_API ${PCEM_PRIVATE_API}
        ${CMAKE_SOURCE_DIR}/includes/private/wx-ui/wx-app.h
        ${CMAKE_SOURCE_DIR}/includes/private/wx-ui/wx-common.h
        ${CMAKE_SOURCE_DIR}/includes/private/wx-ui/wx-createdisc.h
        ${CMAKE_SOURCE_DIR}/includes/private/wx-ui/wx-deviceconfig.h
        ${CMAKE_SOURCE_DIR}/includes/private/wx-ui/wx-dialogbox.h
        ${CMAKE_SOURCE_DIR}/includes/private/wx-ui/wx-display.h
        ${CMAKE_SOURCE_DIR}/includes/private/wx-ui/wx-glsl.h
        ${CMAKE_SOURCE_DIR}/includes/private/wx-ui/wx-glslp-parser.h
        ${CMAKE_SOURCE_DIR}/includes/private/wx-ui/wx-hostconfig.h
        ${CMAKE_SOURCE_DIR}/includes/private/wx-ui/wx-joystickconfig.h
        ${CMAKE_SOURCE_DIR}/includes/private/wx-ui/wx-sdl2-glw.h
        ${CMAKE_SOURCE_DIR}/includes/private/wx-ui/wx-sdl2.h
        ${CMAKE_SOURCE_DIR}/includes/private/wx-ui/wx-sdl2-video-gl3.h
        ${CMAKE_SOURCE_DIR}/includes/private/wx-ui/wx-sdl2-video.h
        ${CMAKE_SOURCE_DIR}/includes/private/wx-ui/wx-sdl2-video-renderer.h
        ${CMAKE_SOURCE_DIR}/includes/private/wx-ui/wx-shaderconfig.h
        ${CMAKE_SOURCE_DIR}/includes/private/wx-ui/wx-status.h
        ${CMAKE_SOURCE_DIR}/includes/private/wx-ui/wx-utils.h
        )

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/wx-ui)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/wx-ui/wx-resources.cpp
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/wx-ui/pc.xrc
        COMMAND wxrc
        ARGS -c ${CMAKE_CURRENT_SOURCE_DIR}/wx-ui/pc.xrc -o ${CMAKE_CURRENT_BINARY_DIR}/wx-ui/wx-resources.cpp)

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
        set(PCEM_SRC ${PCEM_SRC}
                wx-ui/wx-hostconfig.c
                )
endif()

if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
        set(PCEM_SRC ${PCEM_SRC}
                wx-ui/wx-sdl2-display.c
                )
endif()

if(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
        set(PCEM_SRC ${PCEM_SRC}
                wx-ui/wx-sdl2-display-win.c
                wx-ui/wx.rc
                )
endif()


if(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
        set(PCEM_SRC ${PCEM_SRC}
                wx-ui/wx-sdl2-display.c
                )

        add_compile_definitions(PCEM_RENDER_WITH_TIMER PCEM_RENDER_TIMER_LOOP)

endif()
