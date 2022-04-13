pcem_private_api += files(
        '../../includes/private/plugin-api/config.h',
        '../../includes/private/plugin-api/paths.h',
        '../../includes/private/plugin-api/plugin.h',
        '../../includes/private/plugin-api/tinydir.h',
        '../../includes/private/plugin-api/device.h'
        )

pcem_public_api += files(
        '../../includes/public/pcem/cpu.h',
        '../../includes/public/pcem/defines.h',
        '../../includes/public/pcem/devices.h',
        '../../includes/public/pcem/logging.h',
        '../../includes/public/pcem/plugin.h',
        '../../includes/public/pcem/unsafe/config.h',
        '../../includes/public/pcem/unsafe/devices.h'
        )

pcem_pluginapi_src = files(
        'config.c',
        'paths.c',
        'logging.c',
        'device.c',
        'plugin.c',
        'wx-utils.cc'
        )

if get_option('use-plugin-engine')
        pcem_plugin_api = shared_library('pcem', sources: pcem_pluginapi_src, dependencies: pcem_dependencies, include_directories: pcem_include_dirs, install: true)
        pcem_project_libraries += pcem_plugin_api
else
        pcem_src += pcem_pluginapi_src
endif