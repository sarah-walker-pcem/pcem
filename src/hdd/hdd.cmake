set(PCEM_PRIVATE_API ${PCEM_PRIVATE_API}
        ${CMAKE_SOURCE_DIR}/includes/private/hdd/hdd_esdi.h
        ${CMAKE_SOURCE_DIR}/includes/private/hdd/hdd_file.h
        ${CMAKE_SOURCE_DIR}/includes/private/hdd/hdd.h
        ${CMAKE_SOURCE_DIR}/includes/private/hdd/minivhd/cwalk.h
        ${CMAKE_SOURCE_DIR}/includes/private/hdd/minivhd/libxml2_encoding.h
        ${CMAKE_SOURCE_DIR}/includes/private/hdd/minivhd/minivhd_create.h
        ${CMAKE_SOURCE_DIR}/includes/private/hdd/minivhd/minivhd.h
        ${CMAKE_SOURCE_DIR}/includes/private/hdd/minivhd/minivhd_internal.h
        ${CMAKE_SOURCE_DIR}/includes/private/hdd/minivhd/minivhd_io.h
        ${CMAKE_SOURCE_DIR}/includes/private/hdd/minivhd/minivhd_struct_rw.h
        ${CMAKE_SOURCE_DIR}/includes/private/hdd/minivhd/minivhd_util.h
        )

set(PCEM_SRC ${PCEM_SRC}
        hdd/hdd.c
        hdd/hdd_esdi.c
        hdd/hdd_file.c
        )

# MiniVHD
set(PCEM_SRC ${PCEM_SRC}
        hdd/minivhd/cwalk.c
        hdd/minivhd/libxml2_encoding.c
        hdd/minivhd/minivhd_convert.c
        hdd/minivhd/minivhd_create.c
        hdd/minivhd/minivhd_io.c
        hdd/minivhd/minivhd_manage.c
        hdd/minivhd/minivhd_struct_rw.c
        hdd/minivhd/minivhd_util.c
        )
