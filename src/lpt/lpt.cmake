set(PCEM_PRIVATE_API ${PCEM_PRIVATE_API}
        ${CMAKE_SOURCE_DIR}/includes/private/lpt/lpt_dac.h
        ${CMAKE_SOURCE_DIR}/includes/private/lpt/lpt_dss.h
        ${CMAKE_SOURCE_DIR}/includes/private/lpt/lpt_epsonlx810.h
        ${CMAKE_SOURCE_DIR}/includes/private/lpt/lpt.h
        )

set(PCEM_SRC ${PCEM_SRC}
        lpt/lpt.c
        lpt/lpt_dac.c
        lpt/lpt_dss.c
        )

if(USE_EXPERIMENTAL AND USE_EXPERIMENTAL_PRINTER)
        set(PCEM_DEFINES ${PCEM_DEFINES} USE_EXPERIMENTAL_PRINTER)
        set(PCEM_SRC ${PCEM_SRC}
                lpt/lpt_epsonlx810.c
                )
        set(PCEM_ADDITIONAL_LIBS ${PCEM_ADDITIONAL_LIBS} Freetype::Freetype)
endif()