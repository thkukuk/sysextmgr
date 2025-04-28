/* SPDX-License-Identifier: LGPL-2.1-or-later */

// based on systemd v258

#pragma once

#include "sysextmgr.h"

/* Given an image name (for logging purposes), a set of os-release values from the host and a key-value pair
 * vector of extension-release variables, check that the distro and (system extension level or distro
 * version) match and return 1, and 0 otherwise. */
extern int extension_release_validate(
                const char *name,
                const char *host_os_release_id,
                const char *host_os_release_version_id,
                const char *host_os_release_sysext_level,
                const char *host_extension_scope,
		const struct image_deps *extension, 
		bool verbose);
