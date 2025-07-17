// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <errno.h>
#include <systemd/sd-varlink.h>

#define VARLINK_IS_NOT_RUNNING(r) (r == -ECONNREFUSED || r == -ENOENT || r == -ECONNRESET || r == -EACCES)

extern int connect_to_sysextmgrd(sd_varlink **ret, const char *socket);
extern int varlink_list_images (const char *url);
extern int varlink_check (const char *url, const char *prefix);
extern int varlink_cleanup (void);
extern int varlink_update (const char *url, const char *prefix);
extern int varlink_install (const char *name, const char *url);

