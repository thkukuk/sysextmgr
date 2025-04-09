/* SPDX-License-Identifier: LGPL-2.1-or-later */

// based on systemd v258

#pragma once

extern int mkostemp_safe(char *pattern);
extern void unlink_tempfilep(char (*p)[]);
extern int mkdtemp_malloc(const char *template, char **ret);
