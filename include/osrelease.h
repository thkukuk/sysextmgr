//SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

struct osrelease {
  char *id;
  char *id_like;
  char *version_id;
  char *sysext_level;
};

extern void free_os_release(struct osrelease *p);
extern void free_os_releasep(struct osrelease **p);
extern int load_os_release(const char *prefix, struct osrelease **res);
