//SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

/* json-common.c */

#include <systemd/sd-json.h>

struct image_entry {
  char *image_name;
  char *sysext_version_id;
  char *sysext_scope;
  char *id;
  char *sysext_level;
  char *version_id;
  char *architecture;
  sd_json_variant *sysext;
};

extern void image_entry_free(struct image_entry *e);
extern void free_images_list(struct image_entry ***images);
extern void dump_image_entry(struct image_entry *e);
extern int parse_image_entry(sd_json_variant *json, struct image_entry *e);
extern int load_image_entries(const char *path, struct image_entry ***images);

/* main.c */
extern void oom(void);
extern void usage(int retval);

/* images-list.c */
extern int main_list(int argc, char **argv);
