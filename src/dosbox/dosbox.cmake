set(PCEM_PRIVATE_API ${PCEM_PRIVATE_API}
        ${CMAKE_SOURCE_DIR}/includes/private/dosbox/cdrom.h
        ${CMAKE_SOURCE_DIR}/includes/private/dosbox/dbopl.h
        ${CMAKE_SOURCE_DIR}/includes/private/dosbox/nukedopl.h
        ${CMAKE_SOURCE_DIR}/includes/private/dosbox/vid_cga_comp.h
        )

set(PCEM_SRC ${PCEM_SRC}
        dosbox/cdrom_image.cpp
        dosbox/dbopl.cpp
        dosbox/nukedopl.cpp
        dosbox/vid_cga_comp.c
        )