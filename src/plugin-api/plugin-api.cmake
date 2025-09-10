if(PLUGIN_ENGINE)
        set(PCEM_DEFINES ${PCEM_DEFINES} PLUGIN_ENGINE)
endif()

set(PCEM_PRIVATE_API ${PCEM_PRIVATE_API}
        ${CMAKE_SOURCE_DIR}/includes/private/plugin-api/config.h
        ${CMAKE_SOURCE_DIR}/includes/private/plugin-api/paths.h
        ${CMAKE_SOURCE_DIR}/includes/private/plugin-api/plugin.h
        ${CMAKE_SOURCE_DIR}/includes/private/plugin-api/tinydir.h
        ${CMAKE_SOURCE_DIR}/includes/private/plugin-api/device.h
        )

set(PCEM_PUBLIC_API ${PCEM_PUBLIC_API}
        ${CMAKE_SOURCE_DIR}/includes/public/pcem/cpu.h
        ${CMAKE_SOURCE_DIR}/includes/public/pcem/defines.h
        ${CMAKE_SOURCE_DIR}/includes/public/pcem/devices.h
        ${CMAKE_SOURCE_DIR}/includes/public/pcem/logging.h
        ${CMAKE_SOURCE_DIR}/includes/public/pcem/plugin.h
        ${CMAKE_SOURCE_DIR}/includes/public/pcem/config.h
        )

set(PCEM_SRC_PLUGINAPI
        ${PCEM_SRC_PLUGINAPI}
        plugin-api/config.c
        plugin-api/paths.c
        plugin-api/logging.c
        plugin-api/device.c
        plugin-api/plugin.c
        plugin-api/wx-utils.cc
        )

if(PLUGIN_ENGINE)
        add_library(pcem-plugin-api SHARED ${PCEM_SRC_PLUGINAPI} ${PCEM_PUBLIC_API})
        target_link_libraries(pcem-plugin-api ${SDL2_LIBRARIES} ${DISPLAY_ENGINE_LIBRARIES})
        target_compile_definitions(pcem-plugin-api PUBLIC ${PCEM_DEFINES})
        install(TARGETS pcem-plugin-api RUNTIME DESTINATION ${PCEM_BIN_DIR} LIBRARY DESTINATION ${PCEM_LIB_DIR} ARCHIVE DESTINATION ${PCEM_LIB_DIR})
        set(PCEM_LIBRARIES ${PCEM_LIBRARIES} pcem-plugin-api)
else()
        set(PCEM_EMBEDDED_PLUGIN_API ${PCEM_SRC_PLUGINAPI} ${PCEM_PUBLIC_API})
endif()

