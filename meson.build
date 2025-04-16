project(
  'sysext-cli',
  'c',
  meson_version : '>= 0.61.0',
  default_options : [
                  'prefix=/usr',
                  'sysconfdir=/etc',
                  'localstatedir=/var',
                  'buildtype=debugoptimized',
  		  'default_library=shared',
		  'b_pie=true',
                  'b_lto=true',
                  'c_std=gnu17',
		  'warning_level=2'],
  license : ['GPL-2.0-or-later', 'LGPL-2.1-or-later'],
  version : '0.0.0',
)

conf = configuration_data()
conf.set_quoted('VERSION', meson.project_version())
conf.set_quoted('PACKAGE', meson.project_name())

cc = meson.get_compiler('c')
pkg = import('pkgconfig')

add_project_arguments(['-D_GNU_SOURCE=1',
		       '-DXTSTRINGDEFINES',
		       '-D_FORTIFY_SOURCE=2',
                       '-D_FILE_OFFSET_BITS=64',
                       '-D_TIME_BITS=64'], language : 'c')

possible_cc_flags = [
		  '-fstack-protector-strong',
		  '-funwind-tables',
		  '-fasynchronous-unwind-tables',
		  '-fstack-clash-protection',
		  '-Wbad-function-cast',
		  '-Wcast-align',
                  '-Wformat-security',
		  '-Wmissing-declarations',
		  '-Wmissing-prototypes',
		  '-Wnested-externs',
		  '-Wshadow',
		  '-Wstrict-prototypes',
		  '-Wundef',
                  '-Wuninitialized',
		  ]
add_project_arguments(cc.get_supported_arguments(possible_cc_flags), language : 'c')

sysextstoredir = get_option('sysextstoredir')
conf.set_quoted('SYSEXT_STORE_DIR', sysextstoredir)

extensionsdir = get_option('extensionsdir')
conf.set_quoted('EXTENSIONS_DIR', extensionsdir)

libeconf = dependency('libeconf', version : '>=0.7.5', required : true)
libsystemd = dependency('libsystemd', version: '>= 257', required : true)

inc = include_directories(['include'])

sysext_cli_c = ['src/sysext-cli.c', 'src/json-common.c', 'src/images-list.c',
  'src/download.c', 'src/osrelease.c', 'src/extrelease.c', 'src/extract.c',
  'src/image-deps.c', 'src/main-list.c', 'src/main-check.c', 'src/newversion.c',
  'lib/tmpfile-util.c', 'lib/string-util-fundamental.c', 'lib/strv.c',
  'lib/extension-util.c', 'lib/architecture.c']

executable('sysext-cli',
           sysext_cli_c,
           include_directories : inc,
           dependencies : [libeconf, libsystemd],
           install : true)

# Unit tests
subdir('tests')

# Manual pages
#subdir('man')

config_h = configure_file(
  output : 'config.h',
  configuration : conf)
