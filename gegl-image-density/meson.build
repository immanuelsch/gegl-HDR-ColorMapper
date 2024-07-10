project('image-density', 'c',
  version : '0.1',
  license : 'GPL-3.0-or-later')

# These arguments are only used to build the shared library
# not the executables that use the library.
lib_args = ['-DBUILDING_GEGLACTIONLINES']
#
pkgconfig = import('pkgconfig')
i18n      = import('i18n')
gnome     = import('gnome')
gegl_prefix     = get_option('prefix')
gegl_libdir     = get_option('libdir')
project_build_root = meson.current_build_dir()
project_source_root = meson.current_source_dir()

dep_ver = {
  'babl'            : '>=0.1.78',
  'glib'            : '>=2.44.0',
  'gegl'            : '>=0.3'
}

cc = meson.get_compiler('c')
m_dep = cc.find_library('m', required : true)
#executable(..., dependencies : m_dep)

gegl = dependency('gegl-0.4', required : false)
if not gegl.found()
    gegl = dependency('gegl-0.3')
endif


shlib = shared_library('image-density', 'image-density.c', 'config.h',
  c_args : lib_args,
  dependencies : [gegl, m_dep, ],
  name_prefix : '',
)


# Make this library usable as a Meson subproject.
stroke_dep = declare_dependency(
  include_directories: include_directories('.'),
  link_with : shlib)

