set(PCEM_SRC_CDROM
        cdrom/cdrom-image.cc
        cdrom/cdrom-null.c
        )

if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
        set(PCEM_SRC_CDROM ${PCEM_SRC_CDROM}
                cdrom/cdrom-ioctl-linux.c
                )
endif()

if(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
        set(PCEM_SRC_CDROM ${PCEM_SRC_CDROM}
                cdrom/cdrom-ioctl.c
                )
endif()