//SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

/* json-common.c */

#include <systemd/sd-json.h>

struct image_deps {
  char *image_name;
  char *sysext_version_id;
  char *sysext_scope;
  char *id;
  char *sysext_level;
  char *version_id;
  char *architecture;
  sd_json_variant *sysext;
};

extern void free_image_deps(struct image_deps *e);
extern void free_image_depsp(struct image_deps **e);
extern void free_image_deps_list(struct image_deps ***images);
extern void dump_image_deps(struct image_deps *e);
extern int parse_image_deps(sd_json_variant *json, struct image_deps **e);
extern int load_image_json(int fd, const char *path, struct image_deps ***images);

/* main.c */
extern void oom(void);
extern void usage(int retval);

/* images-list.c */
extern int main_list(int argc, char **argv);
