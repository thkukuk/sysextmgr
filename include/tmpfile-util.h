/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

extern int mkostemp_safe(char *pattern);
extern void unlink_tempfilep(char (*p)[]);
extern int mkdtemp_malloc(const char *template, char **ret);
