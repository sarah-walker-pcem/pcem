set(PCEM_PUBLIC_API ${PCEM_PUBLIC_API}
        ${CMAKE_CURRENT_SOURCE_DIR}/includes/public/pcem/cpu.h
        ${CMAKE_CURRENT_SOURCE_DIR}/includes/public/pcem/defines.h
        ${CMAKE_CURRENT_SOURCE_DIR}/includes/public/pcem/devices.h
        ${CMAKE_CURRENT_SOURCE_DIR}/includes/public/pcem/logging.h
        ${CMAKE_CURRENT_SOURCE_DIR}/includes/public/pcem/plugin.h
        ${CMAKE_CURRENT_SOURCE_DIR}/includes/public/pcem/unsafe/config.h
        ${CMAKE_CURRENT_SOURCE_DIR}/includes/public/pcem/unsafe/devices.h
        )

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/includes/public)