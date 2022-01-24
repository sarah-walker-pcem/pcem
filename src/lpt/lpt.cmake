set(PCEM_SRC_LPT
        lpt/lpt.c
        lpt/lpt_dac.c
        lpt/lpt_dss.c
        )

if(USE_EXPERIMENTAL AND USE_EXPERIMENTAL_PRINTER)
        set(PCEM_DEFINES ${PCEM_DEFINES} USE_EXPERIMENTAL_PRINTER)
        set(PCEM_SRC_LPT ${PCEM_SRC_LPT}
                lpt/lpt_epsonlx810.c
                )
        set(PCEM_ADDITIONAL_LIBS ${PCEM_ADDITIONAL_LIBS} Freetype::Freetype)
endif()