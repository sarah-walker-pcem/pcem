pcem_private_api += files(
        '../../includes/private/cdrom/cdrom-image.h',
        '../../includes/private/cdrom/cdrom-ioctl.h',
        '../../includes/private/cdrom/cdrom-null.h'
        )

pcem_src += files(
        'cdrom-image.cc',
        'cdrom-null.c'
        )

if target_machine.system() == 'linux'
        pcem_src += files(
                'cdrom-ioctl-linux.c'
                )
endif

if target_machine.system() == 'windows'
        pcem_src += files(
                'cdrom-ioctl.c'
                )
endif