//SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "osrelease.h"

extern int discover_images(const char *path, char ***result);
extern int image_remote_metadata(const char *url, struct image_entry ***res,
		size_t *nr, const char *filter, bool verify_signature,
		struct osrelease *osrelease, bool verbose);
extern int image_local_metadata(const char *store, struct image_entry ***res,
		size_t *nr, const char *filter, struct osrelease *osrelease,
		bool verbose);
