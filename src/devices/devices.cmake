set(PCEM_PRIVATE_API ${PCEM_PRIVATE_API}
        ${CMAKE_SOURCE_DIR}/includes/private/devices/cassette.h
        ${CMAKE_SOURCE_DIR}/includes/private/devices/esdi_at.h
        ${CMAKE_SOURCE_DIR}/includes/private/devices/f82c710_upc.h
        ${CMAKE_SOURCE_DIR}/includes/private/devices/nvr.h
        ${CMAKE_SOURCE_DIR}/includes/private/devices/nvr_tc8521.h
        ${CMAKE_SOURCE_DIR}/includes/private/devices/ps2_nvr.h
        ${CMAKE_SOURCE_DIR}/includes/private/devices/sis496.h
        )

set(PCEM_SRC ${PCEM_SRC}
        devices/cassette.c
        devices/esdi_at.c
        devices/f82c710_upc.c
        devices/nvr.c
        devices/ps2_nvr.c
        devices/sis496.c
        )