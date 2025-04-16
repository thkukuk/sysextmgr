//SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

extern int discover_images(const char *path, char ***result);
extern int image_remote_metadata(const char *url, struct image_entry ***res, size_t *nr);
extern int image_local_metadata(const char *store, struct image_entry ***res, size_t *nr);
