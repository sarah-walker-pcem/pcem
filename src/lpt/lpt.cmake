set(PCEM_PRIVATE_API ${PCEM_PRIVATE_API}
        ${CMAKE_SOURCE_DIR}/includes/private/lpt/lpt_dac.h
        ${CMAKE_SOURCE_DIR}/includes/private/lpt/lpt_dss.h
        ${CMAKE_SOURCE_DIR}/includes/private/lpt/lpt.h
        )

set(PCEM_SRC ${PCEM_SRC}
        lpt/lpt.c
        lpt/lpt_dac.c
        lpt/lpt_dss.c
        )
