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
		  '-Werror=return-type',
		  '-Wbad-function-cast',
		  '-Wcast-align',
		  '-Wcast-qual',
                  '-Wformat-security',
		  '-Winline',
		  '-Wmissing-declarations',
		  '-Wmissing-prototypes',
		  '-Wnested-externs',
		  '-Wshadow',
		  '-Wstrict-prototypes',
		  '-Wundef',
		  ]
add_project_arguments(cc.get_supported_arguments(possible_cc_flags), language : 'c')

libeconf = dependency('libeconf', version : '>=0.7.5', required : true)
libsystemd = dependency('libsystemd', version: '>= 257', required : true)

inc = include_directories(['include'])

sysext_cli_c = ['src/sysext-cli.c', 'src/json-common.c', 'src/images-list.c',
  'lib/extension-util.c', 'lib/architecture.c', 'lib/string-util-fundamental.c']

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
