project(
  'sysextmgr',
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

prefixdir = get_option('prefix')
if not prefixdir.startswith('/')
        error('Prefix is not absolute: "@0@"'.format(prefixdir))
endif
libexecdir = join_paths(prefixdir, get_option('libexecdir'))
datadir = join_paths(prefixdir, get_option('datadir'))
conf.set_quoted('DATADIR', datadir)
systemunitdir = prefixdir / 'lib/systemd/system'

sysextstoredir = get_option('sysextstoredir')
conf.set_quoted('SYSEXT_STORE_DIR', sysextstoredir)

extensionsdir = get_option('extensionsdir')
conf.set_quoted('EXTENSIONS_DIR', extensionsdir)

tukitplugindir = get_option('tukitplugindir')
conf.set_quoted('TUKITPLUGIN_DIR', tukitplugindir)

libeconf = dependency('libeconf', version : '>=0.7.5', required : true)
libsystemd = dependency('libsystemd', version: '>= 257', required : true)
libz = dependency('zlib', required : true)
#libzio = dependency('libzio', required : true)
libzio = declare_dependency(dependencies : cc.find_library('zio'))

inc = include_directories(['include'])

sysextmgrcli_c = ['src/sysextmgrcli.c', 'src/json-common.c',
  'src/main-check.c', 'src/main-list.c', 'src/main-install.c',
  'src/main-update.c', 'src/main-cleanup.c', 'src/image-deps.c',
  'src/main-tukit-plugin.c', 'src/mkosi-manifest.c', 'src/varlink-client.c']
sysextmgrd_c = ['src/sysextmgrd.c', 'src/varlink-org.openSUSE.sysextmgr.c',
  'src/mkdir_p.c', 'src/osrelease.c', 'src/images-list.c', 'src/image-deps.c',
  'src/extrelease.c', 'src/extract.c', 'src/download.c', 'src/log_msg.c',
  'src/config.c', 'src/json-common.c', 'src/newversion.c', 
  'lib/extension-util.c', 'lib/string-util-fundamental.c', 'lib/tmpfile-util.c',
  'lib/strv.c', 'lib/architecture.c']

executable('sysextmgrcli',
           sysextmgrcli_c,
           include_directories : inc,
           dependencies : [libeconf, libsystemd, libzio, libz],
           install : true)

if get_option('tukit-plugin')
  install_symlink('50-sysext-update',
                   pointing_to: '../../../bin/sysextmgrcli',
                   install_dir: tukitplugindir)
endif

executable('sysextmgrd',
           sysextmgrd_c,
           include_directories : inc,
           dependencies : [libeconf, libsystemd],
           install_dir : libexecdir,
           install : true)

# Unit tests
subdir('tests')

# systemd units
subdir('units')

# Manual pages
#subdir('man')

config_h = configure_file(
  output : 'config.h',
  configuration : conf)
