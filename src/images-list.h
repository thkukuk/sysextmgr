//SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "osrelease.h"

extern int discover_images(const char *path, char ***result);
extern int image_remote_metadata(const char *url, struct image_entry ***res,
		size_t *nr, const char *filter, bool verify_signature,
		const struct osrelease *osrelease, bool verbose);
extern int image_local_metadata(const char *store, struct image_entry ***res,
		size_t *nr, const char *filter, const struct osrelease *osrelease,
		bool read_metadata, bool verbose);
extern int calc_refcount(struct image_entry **list, size_t n);

