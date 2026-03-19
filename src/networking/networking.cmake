set(PCEM_PRIVATE_API ${PCEM_PRIVATE_API}
        ${CMAKE_SOURCE_DIR}/includes/private/networking/queue.h
        ${CMAKE_SOURCE_DIR}/includes/private/networking/ne2000.h
        ${CMAKE_SOURCE_DIR}/includes/private/networking/nethandler.h
        )

set(PCEM_DEFINES ${PCEM_DEFINES} USE_NETWORKING)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SLIRP REQUIRED slirp)
include_directories(${SLIRP_INCLUDE_DIRS})
set(PCEM_ADDITIONAL_LIBS ${PCEM_ADDITIONAL_LIBS} ${SLIRP_LIBRARIES})

if(USE_PCAP_NETWORKING)
        set(PCEM_DEFINES ${PCEM_DEFINES} USE_PCAP_NETWORKING)
        set(PCEM_ADDITIONAL_LIBS ${PCEM_ADDITIONAL_LIBS} ${PCAP_LIBRARY})
endif()

set(PCEM_SRC ${PCEM_SRC}
        networking/ne2000.c
        networking/nethandler.c
        networking/queue.c
        )

if(WIN32)
        set(PCEM_ADDITIONAL_LIBS ${PCEM_ADDITIONAL_LIBS} wsock32 iphlpapi ws2_32)
endif()
