set(PCEM_PRIVATE_API ${PCEM_PRIVATE_API}
        ${CMAKE_SOURCE_DIR}/includes/private/sdl2/sdl2-common.h
        ${CMAKE_SOURCE_DIR}/includes/private/sdl2/sdl2-display.h
        ${CMAKE_SOURCE_DIR}/includes/private/sdl2/sdl2-emu.h
        ${CMAKE_SOURCE_DIR}/includes/private/sdl2/sdl2-glsl.h
        ${CMAKE_SOURCE_DIR}/includes/private/sdl2/sdl2-glslp-parser.h
        ${CMAKE_SOURCE_DIR}/includes/private/sdl2/sdl2-glw.h
        ${CMAKE_SOURCE_DIR}/includes/private/sdl2/sdl2-host.h
        ${CMAKE_SOURCE_DIR}/includes/private/sdl2/sdl2-video-gl3.h
        ${CMAKE_SOURCE_DIR}/includes/private/sdl2/sdl2-video.h
        ${CMAKE_SOURCE_DIR}/includes/private/sdl2/sdl2-video-renderer.h
        )

set(PCEM_SRC ${PCEM_SRC}
        sdl2/sdl2-display.c
        sdl2/sdl2-emu.c
        sdl2/sdl2-glslp-parser.c
        sdl2/sdl2-host.c
        sdl2/sdl2-joystick.c
        sdl2/sdl2-keyboard.c
        sdl2/sdl2-mouse.c
        sdl2/sdl2-status.c
        sdl2/sdl2-video.c
        sdl2/sdl2-video-gl3.c
        sdl2/sdl2-video-renderer.c
        )

if(${PCEM_DISPLAY_ENGINE} STREQUAL "SDL")
        set(PCEM_SRC ${PCEM_SRC}
                sdl2/sdl2-main.c
                sdl2/sdl2-viewer.c
                )
endif()

if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux" OR ${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD")
        find_package(X11)
        if(X11_FOUND)
                include_directories(${X11_INCLUDE_DIR})
                set(PCEM_DEFINES ${PCEM_DEFINES} PCEM_SDL_X11)
                set(PCEM_ADDITIONAL_LIBS ${PCEM_ADDITIONAL_LIBS} ${X11_LIBRARIES})
        endif()
endif()
