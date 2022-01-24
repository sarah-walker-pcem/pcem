set(PCEM_DEFINES ${PCEM_DEFINES} USE_NETWORKING)

if(USE_PCAP_NETWORKING)
        set(PCEM_DEFINES ${PCEM_DEFINES} USE_PCAP_NETWORKING)
        set(PCEM_ADDITIONAL_LIBS ${PCEM_ADDITIONAL_LIBS} ${PCAP_LIBRARY})
endif()

set(PCEM_SRC_NETWORKING
        networking/ne2000.c
        networking/nethandler.c
        )

set(PCEM_SRC_NETWORKING
        ${PCEM_SRC_NETWORKING}
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