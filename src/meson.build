if get_option('use-plugin-engine')
        add_project_arguments('-DPLUGIN_ENGINE', language: ['c', 'cpp'])
endif

subdir('bus')
subdir('cdrom')
subdir('codegen')
subdir('cpu')
subdir('devices')
subdir('disc')
subdir('dosbox')
subdir('flash')
subdir('floppy')
subdir('hdd')
subdir('ide')
subdir('joystick')
subdir('keyboard')
subdir('lpt')
subdir('memory')
subdir('mfm')
subdir('models')
subdir('mouse')
if get_option('use-networking')
        subdir('networking')
endif
subdir('scsi')
subdir('sound')
subdir('video')
subdir('wx-ui')

pcem_src += files(
        'fdi2raw.c',
        'io.c',
        'mcr.c',
        'pc.c',
        'ppi.c',
        'pzx.c',
        'rtc.c',
        'rtc_tc8521.c',
        'timer.c'
        )

subdir('plugin-api')

executable('pcem', pcem_src, dependencies: pcem_dependencies, link_with: pcem_project_libraries, include_directories: pcem_include_dirs, install: true, win_subsystem: 'windows')
