project(
  'wxgui',
  'c',
  version: '0.1.0',
  license: 'MIT',
  default_options: [
    'c_std=c99',
    'warning_level=3',
    'werror=true',
  ],
)

version = '"@0@"'.format(meson.project_version())
add_project_arguments([
  '-DVERSION=@0@'.format(version),
], language: 'c')

sdl2_dep = dependency('sdl2')

executable(
  'wxgui',
  [
    'main.c',
  ],
  dependencies: [sdl2_dep],
  install: true,
)
