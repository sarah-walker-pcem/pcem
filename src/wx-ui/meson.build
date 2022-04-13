xrcfile = join_paths(meson.current_build_dir(), 'wx-resources.cpp')
run_command('wxrc', '-c', files('pc.xrc'), '-o', xrcfile)
pcem_src += xrcfile

pcem_private_api += files(
        '../../includes/private/wx-ui/wx-app.h',
        '../../includes/private/wx-ui/wx-common.h',
        '../../includes/private/wx-ui/wx-createdisc.h',
        '../../includes/private/wx-ui/wx-deviceconfig.h',
        '../../includes/private/wx-ui/wx-dialogbox.h',
        '../../includes/private/wx-ui/wx-display.h',
        '../../includes/private/wx-ui/wx-glsl.h',
        '../../includes/private/wx-ui/wx-glslp-parser.h',
        '../../includes/private/wx-ui/wx-hostconfig.h',
        '../../includes/private/wx-ui/wx-joystickconfig.h',
        '../../includes/private/wx-ui/wx-sdl2-glw.h',
        '../../includes/private/wx-ui/wx-sdl2.h',
        '../../includes/private/wx-ui/wx-sdl2-video-gl3.h',
        '../../includes/private/wx-ui/wx-sdl2-video.h',
        '../../includes/private/wx-ui/wx-sdl2-video-renderer.h',
        '../../includes/private/wx-ui/wx-shaderconfig.h',
        '../../includes/private/wx-ui/wx-status.h',
        '../../includes/private/wx-ui/wx-utils.h'
        )

pcem_src += files(
        'wx-main.cc',
        'wx-config_sel.c',
        'wx-dialogbox.cc',
        'wx-utils.cc',
        'wx-app.cc',
        'wx-sdl2-joystick.c',
        'wx-sdl2-mouse.c',
        'wx-sdl2-keyboard.c',
        'wx-sdl2-video.c',
        'wx-sdl2.c',
        'wx-config.c',
        'wx-deviceconfig.cc',
        'wx-status.cc',
        'wx-sdl2-status.c',
        'wx-thread.c',
        'wx-common.c',
        'wx-sdl2-video-renderer.c',
        'wx-sdl2-video-gl3.c',
        'wx-glslp-parser.c',
        'wx-shader_man.c',
        'wx-shaderconfig.cc',
        'wx-joystickconfig.cc',
        'wx-config-eventbinder.cc',
        'wx-createdisc.cc'
        )

if get_option('use-networking')
        pcem_src += files(
                'wx-hostconfig.c'
                )
endif

if target_machine.system() == 'linux'
        pcem_src += files(
                'wx-sdl2-display.c'
                )
endif

if target_machine.system() == 'windows'
        pcem_src += files(
                'wx-sdl2-display-win.c',
                )

        raw_wx_windres_args = wxwidgets.get_variable(configtool : 'rescomp').split()
        wx_windres_args = []
        foreach a : raw_wx_windres_args
            if a != 'windres'
                wx_windres_args += a
            endif
        endforeach

        pcem_src += import('windows').compile_resources(files('wx.rc'), args: wx_windres_args)
endif
