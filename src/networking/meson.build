pcem_private_api += files(
        '../../includes/private/networking/slirp/bootp.h',
        '../../includes/private/networking/slirp/config.h',
        '../../includes/private/networking/slirp/config-host.h',
        '../../includes/private/networking/slirp/ctl.h',
        '../../includes/private/networking/slirp/debug.h',
        '../../includes/private/networking/slirp/icmp_var.h',
        '../../includes/private/networking/slirp/if.h',
        '../../includes/private/networking/slirp/ip.h',
        '../../includes/private/networking/slirp/ip_icmp.h',
        '../../includes/private/networking/slirp/libslirp.h',
        '../../includes/private/networking/slirp/main.h',
        '../../includes/private/networking/slirp/mbuf.h',
        '../../includes/private/networking/slirp/misc.h',
        '../../includes/private/networking/slirp/queue.h',
        '../../includes/private/networking/slirp/sbuf.h',
        '../../includes/private/networking/slirp/slirp_config.h',
        '../../includes/private/networking/slirp/slirp.h',
        '../../includes/private/networking/slirp/socket.h',
        '../../includes/private/networking/slirp/tcp.h',
        '../../includes/private/networking/slirp/tcpip.h',
        '../../includes/private/networking/slirp/tcp_timer.h',
        '../../includes/private/networking/slirp/tcp_var.h',
        '../../includes/private/networking/slirp/tftp.h',
        '../../includes/private/networking/slirp/udp.h',
        '../../includes/private/networking/ne2000.h',
        '../../includes/private/networking/nethandler.h',
        )

add_project_arguments('-DUSE_NETWORKING', language: ['c', 'cpp'])

if get_option('use-pcap-networking')
        add_project_arguments('-DUSE_PCAP_NETWORKING', language: ['c', 'cpp'])
        pcap = dependency('libpcap')
        pcem_dependencies += pcap
endif

pcem_src += files(
        'ne2000.c',
        'nethandler.c'
        )

pcem_src += files(
        'slirp/bootp.c',
        'slirp/cksum.c',
        'slirp/debug.c',
        'slirp/if.c',
        'slirp/ip_icmp.c',
        'slirp/ip_input.c',
        'slirp/ip_output.c',
        'slirp/mbuf.c',
        'slirp/misc.c',
        'slirp/queue.c',
        'slirp/sbuf.c',
        'slirp/slirp.c',
        'slirp/socket.c',
        'slirp/tcp_input.c',
        'slirp/tcp_output.c',
        'slirp/tcp_subr.c',
        'slirp/tcp_timer.c',
        'slirp/tftp.c',
        'slirp/udp.c'
        )

if target_machine.system() == 'windows'
        wsock32 = compiler.find_library('wsock32', required: true)
        iphlpapi = compiler.find_library('iphlpapi', required: true)
        pcem_dependencies += [wsock32, iphlpapi]
endif