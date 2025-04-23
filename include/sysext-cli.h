//SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

/* json-common.c */

#include <systemd/sd-json.h>

#include "image-deps.h"

extern int parse_image_deps(sd_json_variant *json, struct image_deps **e);
extern int load_image_json(int fd, const char *path, struct image_deps ***images);

extern int get_latest_version(struct image_entry *curr, struct image_entry **new, const char *url);
/* main.c */
extern void oom(void);
extern void usage(int retval);

/* main-list.c */
extern int main_list(int argc, char **argv);

/* main-check.c */
extern int main_check(int argc, char **argv);

/* main-update.c */
extern int main_update(int argc, char **argv);

/* main-install.c */
extern int main_install(int argc, char **argv);
