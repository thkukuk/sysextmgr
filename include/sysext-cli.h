//SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

/* json-common.c */

#include <systemd/sd-json.h>

#include "image-deps.h"

extern int parse_image_deps(sd_json_variant *json, struct image_deps **e);
extern int load_image_json(int fd, const char *path, struct image_deps ***images);

/* main.c */
extern void oom(void);
extern void usage(int retval);

/* images-list.c */
extern int main_list(int argc, char **argv);
