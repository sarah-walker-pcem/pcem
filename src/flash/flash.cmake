set(PCEM_PRIVATE_API ${PCEM_PRIVATE_API}
        ${CMAKE_SOURCE_DIR}/includes/private/flash/intel_flash.h
        ${CMAKE_SOURCE_DIR}/includes/private/flash/rom.h
        ${CMAKE_SOURCE_DIR}/includes/private/flash/sst39sf010.h
        ${CMAKE_SOURCE_DIR}/includes/private/flash/tandy_eeprom.h
        ${CMAKE_SOURCE_DIR}/includes/private/flash/tandy_rom.h
        )

set(PCEM_SRC ${PCEM_SRC}
        flash/intel_flash.c
        flash/rom.c
        flash/sst39sf010.c
        flash/tandy_eeprom.c
        flash/tandy_rom.c
        )