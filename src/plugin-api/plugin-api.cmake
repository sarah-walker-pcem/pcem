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
        ${CMAKE_SOURCE_DIR}/includes/public/pcem/unsafe/config.h
        ${CMAKE_SOURCE_DIR}/includes/public/pcem/unsafe/devices.h
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