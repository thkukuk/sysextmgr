//SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

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

struct image_entry {
  char *name;
  struct image_deps *deps;
  bool remote;
  bool installed;
  bool compatible;
};

extern void free_image_deps(struct image_deps *e);
extern void free_image_depsp(struct image_deps **e);
extern void free_image_deps_list(struct image_deps ***images);
extern void dump_image_deps(struct image_deps *e);
extern void free_image_entry_list(struct image_entry ***list);
