xsltproc_exe = find_program('xsltproc', required : get_option('man'))
want_man = (get_option('man').enabled() or get_option('man').auto()) and xsltproc_exe.found()
xsltproc_flags = [
        '--nonet',
        '--xinclude',
        '--stringparam', 'version', '@0@'.format(meson.project_version()),
        '--path',
        '@0@:@1@'.format(meson.current_build_dir(), meson.current_source_dir())]

custom_man_xsl = files('custom-man.xsl')

xslt_cmd = [xsltproc_exe, '-o', '@OUTPUT0@'] + xsltproc_flags

mandir1 = get_option('mandir') / 'man1'
mandir5 = get_option('mandir') / 'man5'
mandir8 = get_option('mandir') / 'man8'

if xsltproc_exe.found()
custom_target('sysextmgr.conf.5',
              input : 'sysextmgr.conf.5.xml',
              output : 'sysextmgr.conf.5',
              command : xslt_cmd + [custom_man_xsl, '@INPUT@'],
              install : want_man,
              install_dir : mandir5)
endif
