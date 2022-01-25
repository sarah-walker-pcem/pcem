set(PCEM_PRIVATE_API ${PCEM_PRIVATE_API}
        ${CMAKE_SOURCE_DIR}/includes/private/disc/disc_fdi.h
        ${CMAKE_SOURCE_DIR}/includes/private/disc/disc.h
        ${CMAKE_SOURCE_DIR}/includes/private/disc/disc_img.h
        ${CMAKE_SOURCE_DIR}/includes/private/disc/disc_sector.h
        )

set(PCEM_SRC ${PCEM_SRC}
        disc/disc.c
        disc/disc_fdi.c
        disc/disc_img.c
        disc/disc_sector.c
        )