include_directories(${CMAKE_SOURCE_DIR}/includes/private/wx-ui)

set(PCEM_PRIVATE_API ${PCEM_PRIVATE_API}
        ${CMAKE_SOURCE_DIR}/includes/private/wx-ui/wx-app.h
        ${CMAKE_SOURCE_DIR}/includes/private/wx-ui/wx-common.h
        ${CMAKE_SOURCE_DIR}/includes/private/wx-ui/wx-config.h
        ${CMAKE_SOURCE_DIR}/includes/private/wx-ui/wx-config_sel.h
        ${CMAKE_SOURCE_DIR}/includes/private/wx-ui/wx-createdisc.h
        ${CMAKE_SOURCE_DIR}/includes/private/wx-ui/wx-customresolution.h
        ${CMAKE_SOURCE_DIR}/includes/private/wx-ui/wx-deviceconfig.h
        ${CMAKE_SOURCE_DIR}/includes/private/wx-ui/wx-display.h
        ${CMAKE_SOURCE_DIR}/includes/private/wx-ui/wx-frontend.h
        ${CMAKE_SOURCE_DIR}/includes/private/wx-ui/wx-hdgeometry.h
        ${CMAKE_SOURCE_DIR}/includes/private/wx-ui/wx-hdnew.h
        ${CMAKE_SOURCE_DIR}/includes/private/wx-ui/wx-hdsize.h
        ${CMAKE_SOURCE_DIR}/includes/private/wx-ui/wx-hostconfig.h
        ${CMAKE_SOURCE_DIR}/includes/private/wx-ui/wx-joystickconfig.h
        ${CMAKE_SOURCE_DIR}/includes/private/wx-ui/wx-shader_man.h
        ${CMAKE_SOURCE_DIR}/includes/private/wx-ui/wx-shaderconfig.h
        ${CMAKE_SOURCE_DIR}/includes/private/wx-ui/wx-status.h
        ${CMAKE_SOURCE_DIR}/includes/private/wx-ui/wx-utils.h
        )

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/wx-ui)

set(PCEM_WX_XRC
        ${CMAKE_CURRENT_SOURCE_DIR}/wx-ui/wx-app.xrc
        ${CMAKE_CURRENT_SOURCE_DIR}/wx-ui/wx-display.xrc
        ${CMAKE_CURRENT_SOURCE_DIR}/wx-ui/wx-status.xrc
        ${CMAKE_CURRENT_SOURCE_DIR}/wx-ui/wx-config.xrc
        ${CMAKE_CURRENT_SOURCE_DIR}/wx-ui/wx-config_sel.xrc
        ${CMAKE_CURRENT_SOURCE_DIR}/wx-ui/wx-hdnew.xrc
        ${CMAKE_CURRENT_SOURCE_DIR}/wx-ui/wx-hdsize.xrc
        ${CMAKE_CURRENT_SOURCE_DIR}/wx-ui/wx-hostconfig.xrc
        ${CMAKE_CURRENT_SOURCE_DIR}/wx-ui/wx-shader_man.xrc
        ${CMAKE_CURRENT_SOURCE_DIR}/wx-ui/wx-customresolution.xrc
        ${CMAKE_CURRENT_SOURCE_DIR}/wx-ui/viewers/viewer_vram.xrc
        )

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/wx-ui/wx-resources.cpp
        DEPENDS ${PCEM_WX_XRC}
        COMMAND ${wxWidgets_wxrc_EXECUTABLE}
        ARGS -c ${PCEM_WX_XRC} -o ${CMAKE_CURRENT_BINARY_DIR}/wx-ui/wx-resources.cpp)

set(PCEM_SRC ${PCEM_SRC}
        ${PCEM_WX_XRC}
        wx-ui/wx-main.cc
        wx-ui/wx-config_sel.cc
        wx-ui/wx-utils.cc
        wx-ui/wx-display.cc
        wx-ui/wx-app.cc
        wx-ui/wx-frontend.cc
        wx-ui/wx-config.cc
        wx-ui/wx-hdgeometry.cc
        wx-ui/wx-hdnew.cc
        wx-ui/wx-hdsize.cc
        wx-ui/wx-customresolution.cc
        wx-ui/wx-deviceconfig.cc
        wx-ui/wx-status.cc
        wx-ui/wx-shader_man.cc
        wx-ui/wx-shaderconfig.cc
        wx-ui/wx-joystickconfig.cc
        wx-ui/wx-createdisc.cc
        ${CMAKE_CURRENT_BINARY_DIR}/wx-ui/wx-resources.cpp
        )

if(USE_NETWORKING)
        set(PCEM_SRC ${PCEM_SRC}
                wx-ui/wx-hostconfig.cc
                )
endif()

if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
        find_package(X11 REQUIRED)
        include_directories(${GTK3_INCLUDE_DIRS} ${X11_INCLUDE_DIR})
        set(PCEM_ADDITIONAL_LIBS ${PCEM_ADDITIONAL_LIBS} ${GTK3_LIBRARIES} ${X11_LIBRARIES})
endif()

if(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
        set(PCEM_SRC ${PCEM_SRC}
                wx-ui/wx.rc
                )
endif()


if(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
        add_compile_definitions(PCEM_RENDER_WITH_TIMER PCEM_RENDER_TIMER_LOOP)

endif()
