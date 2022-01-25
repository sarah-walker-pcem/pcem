set(PCEM_PRIVATE_API ${PCEM_PRIVATE_API}
        ${CMAKE_SOURCE_DIR}/includes/private/floppy/fdc37c665.h
        ${CMAKE_SOURCE_DIR}/includes/private/floppy/fdc37c93x.h
        ${CMAKE_SOURCE_DIR}/includes/private/floppy/fdc.h
        ${CMAKE_SOURCE_DIR}/includes/private/floppy/fdd.h
        )

set(PCEM_SRC ${PCEM_SRC}
        floppy/fdc.c
        floppy/fdc37c665.c
        floppy/fdc37c93x.c
        floppy/fdd.c
        )