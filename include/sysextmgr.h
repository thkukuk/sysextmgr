//SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#define _VARLINK_SYSEXTMGR_SOCKET_DIR "/run/sysextmgr"
#define _VARLINK_SYSEXTMGR_SOCKET _VARLINK_SYSEXTMGR_SOCKET_DIR"/socket"

/* config.c */
struct config {
  bool verbose;
  bool verify_signature;
  char *url;
  char *sysext_store_dir;
  char *extensions_dir;
};

extern struct config config;

extern int load_config(const char *defgroup);

/* json-common.c */

#include <systemd/sd-json.h>

#include "osrelease.h"
#include "image-deps.h"

extern int parse_image_deps(sd_json_variant *json, struct image_deps **e);
extern int load_image_json(int fd, const char *path, struct image_deps ***images);

extern int get_latest_version(struct image_entry *curr, struct image_entry **new, const char *url, bool verify_signature, const struct osrelease *osrelease, bool verbose);
/* main.c */
extern void oom(void);
extern void usage(int retval);

/* main-list.c */
extern int main_list(int argc, char **argv);

/* main-check.c */
extern int main_check(int argc, char **argv);

/* main-cleanup.c */
extern int main_cleanup(int argc, char **argv);

/* main-update.c */
extern int main_update(int argc, char **argv);

/* main-install.c */
extern int main_install(int argc, char **argv);

/* main-tukit-plugin.c */
extern int main_tukit_plugin(int argc, char **argv);
