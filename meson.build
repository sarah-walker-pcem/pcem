project('PCem', ['c', 'cpp'], version: 'vNext', license: 'GPLv2')

add_project_arguments('-DPCEM_VERSION_STRING="' + meson.project_version() + '"', language: ['c', 'cpp'])

pcem_private_api = []
pcem_public_api = []
pcem_src = []

subdir('includes')

compiler = meson.get_compiler('c')
sdl2 = dependency('sdl2')
openal = dependency('openal')
opengl = dependency('opengl')
wxwidgets = dependency('wxwidgets')

pcem_dependencies = [sdl2, openal, opengl, wxwidgets]

if target_machine.cpu_family() == 'x86'
  add_project_arguments('-msse2', language: ['c', 'cpp'])
endif

add_project_arguments('-fcommon', language: ['c', 'cpp'])

subdir('docs')
subdir('src')