set(PCEM_PRIVATE_API ${PCEM_PRIVATE_API}
        ${CMAKE_SOURCE_DIR}/includes/private/memory/mem_bios.h
        ${CMAKE_SOURCE_DIR}/includes/private/memory/mem.h
        )

set(PCEM_SRC ${PCEM_SRC}
        memory/mem.c
        memory/mem_bios.c
        )