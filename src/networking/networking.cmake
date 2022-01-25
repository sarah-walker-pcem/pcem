set(PCEM_PRIVATE_API ${PCEM_PRIVATE_API}
        ${CMAKE_SOURCE_DIR}/includes/private/networking/slirp/bootp.h
        ${CMAKE_SOURCE_DIR}/includes/private/networking/slirp/config.h
        ${CMAKE_SOURCE_DIR}/includes/private/networking/slirp/config-host.h
        ${CMAKE_SOURCE_DIR}/includes/private/networking/slirp/ctl.h
        ${CMAKE_SOURCE_DIR}/includes/private/networking/slirp/debug.h
        ${CMAKE_SOURCE_DIR}/includes/private/networking/slirp/icmp_var.h
        ${CMAKE_SOURCE_DIR}/includes/private/networking/slirp/if.h
        ${CMAKE_SOURCE_DIR}/includes/private/networking/slirp/ip.h
        ${CMAKE_SOURCE_DIR}/includes/private/networking/slirp/ip_icmp.h
        ${CMAKE_SOURCE_DIR}/includes/private/networking/slirp/libslirp.h
        ${CMAKE_SOURCE_DIR}/includes/private/networking/slirp/main.h
        ${CMAKE_SOURCE_DIR}/includes/private/networking/slirp/mbuf.h
        ${CMAKE_SOURCE_DIR}/includes/private/networking/slirp/misc.h
        ${CMAKE_SOURCE_DIR}/includes/private/networking/slirp/queue.h
        ${CMAKE_SOURCE_DIR}/includes/private/networking/slirp/sbuf.h
        ${CMAKE_SOURCE_DIR}/includes/private/networking/slirp/slirp_config.h
        ${CMAKE_SOURCE_DIR}/includes/private/networking/slirp/slirp.h
        ${CMAKE_SOURCE_DIR}/includes/private/networking/slirp/socket.h
        ${CMAKE_SOURCE_DIR}/includes/private/networking/slirp/tcp.h
        ${CMAKE_SOURCE_DIR}/includes/private/networking/slirp/tcpip.h
        ${CMAKE_SOURCE_DIR}/includes/private/networking/slirp/tcp_timer.h
        ${CMAKE_SOURCE_DIR}/includes/private/networking/slirp/tcp_var.h
        ${CMAKE_SOURCE_DIR}/includes/private/networking/slirp/tftp.h
        ${CMAKE_SOURCE_DIR}/includes/private/networking/slirp/udp.h
        ${CMAKE_SOURCE_DIR}/includes/private/networking/ne2000.h
        ${CMAKE_SOURCE_DIR}/includes/private/networking/nethandler.h
        )

set(PCEM_DEFINES ${PCEM_DEFINES} USE_NETWORKING)

if(USE_PCAP_NETWORKING)
        set(PCEM_DEFINES ${PCEM_DEFINES} USE_PCAP_NETWORKING)
        set(PCEM_ADDITIONAL_LIBS ${PCEM_ADDITIONAL_LIBS} ${PCAP_LIBRARY})
endif()

set(PCEM_SRC ${PCEM_SRC}
        networking/ne2000.c
        networking/nethandler.c
        )

set(PCEM_SRC ${PCEM_SRC}
        networking/slirp/bootp.c
        networking/slirp/cksum.c
        networking/slirp/debug.c
        networking/slirp/if.c
        networking/slirp/ip_icmp.c
        networking/slirp/ip_input.c
        networking/slirp/ip_output.c
        networking/slirp/mbuf.c
        networking/slirp/misc.c
        networking/slirp/queue.c
        networking/slirp/sbuf.c
        networking/slirp/slirp.c
        networking/slirp/socket.c
        networking/slirp/tcp_input.c
        networking/slirp/tcp_output.c
        networking/slirp/tcp_subr.c
        networking/slirp/tcp_timer.c
        networking/slirp/tftp.c
        networking/slirp/udp.c
        )

if(WIN32)
        set(PCEM_ADDITIONAL_LIBS ${PCEM_ADDITIONAL_LIBS} wsock32 iphlpapi)
endif()