set(PCEM_PRIVATE_API ${PCEM_PRIVATE_API}
        ${CMAKE_SOURCE_DIR}/includes/private/keyboard/keyboard_amstrad.h
        ${CMAKE_SOURCE_DIR}/includes/private/keyboard/keyboard_at.h
        ${CMAKE_SOURCE_DIR}/includes/private/keyboard/keyboard.h
        ${CMAKE_SOURCE_DIR}/includes/private/keyboard/keyboard_olim24.h
        ${CMAKE_SOURCE_DIR}/includes/private/keyboard/keyboard_pcjr.h
        ${CMAKE_SOURCE_DIR}/includes/private/keyboard/keyboard_xt.h
        )

set(PCEM_SRC ${PCEM_SRC}
        keyboard/keyboard.c
        keyboard/keyboard_amstrad.c
        keyboard/keyboard_at.c
        keyboard/keyboard_olim24.c
        keyboard/keyboard_pcjr.c
        keyboard/keyboard_xt.c
        )