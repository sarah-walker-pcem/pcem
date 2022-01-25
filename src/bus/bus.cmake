set(PCEM_PRIVATE_API ${PCEM_PRIVATE_API}
        ${CMAKE_SOURCE_DIR}/includes/private/bus/mca.h
        ${CMAKE_SOURCE_DIR}/includes/private/bus/pci.h
        )

set(PCEM_SRC ${PCEM_SRC}
        bus/mca.c
        bus/pci.c
        )