/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <assert.h>
#include "basics.h"
#include "sysext-cli.h"
#include "extension-util.h"
#include "architecture.h"

int
extension_release_validate(const char *name,
			   const char *host_os_release_id,
			   const char *host_os_release_version_id,
			   const char *host_os_release_sysext_level,
			   const char *host_extension_scope,
			   const struct image_deps *extension,
			   bool verbose)
{
  assert(extension);

  if (extension->sysext_scope && host_extension_scope)
    {
      if (strstr(extension->sysext_scope, host_extension_scope) == NULL)
	{
	  if (verbose)
	    printf("Extension '%s' is not suitable for scope %s, ignoring.\n", name, host_extension_scope);
	  return 0;
	}
    }

  /* When the architecture field is present and not '_any' it must match the host - for now just look at uname but in
   * the future we could check if the kernel also supports 32 bit or binfmt has a translator set up for the architecture */
  if (!isempty(extension->architecture) && !streq(extension->architecture, "_any") &&
        !streq(architecture_to_string(uname_architecture()), extension->architecture))
    {
      if (verbose)
	printf("Extension '%s' is for architecture '%s', but deployed on top of '%s'.\n",
	       name, extension->architecture, architecture_to_string(uname_architecture()));
      return 0;
    }

  if (isempty(extension->id))
    {
      if (verbose)
	printf("Extension '%s' does not contain ID in release file but requested to match '%s' or be '_any'",
	       name, host_os_release_id);
      return 0;
    }

  /* A sysext(or confext) with no host OS dependency (static binaries or scripts) can match
   * '_any' host OS, and VERSION_ID or SYSEXT_LEVEL(or CONFEXT_LEVEL) are not required anywhere */
  if (streq(extension->id, "_any"))
    {
      if (verbose)
	printf("Extension '%s' matches '_any' OS.\n", name);
      return 1;
    }

  if (!streq(host_os_release_id, extension->id))
    {
      if (verbose)
	printf("Extension '%s' is for OS '%s', but deployed on top of '%s'.\n",
	       name, extension->id, host_os_release_id);
      return 0;
    }

  /* Rolling releases do not typically set VERSION_ID (eg: ArchLinux) */
  if (isempty(host_os_release_version_id) && isempty(host_os_release_sysext_level))
    {
      if (verbose)
	printf("No version info on the host (rolling release?), but ID in %s matched.\n", name);
      return 1;
    }

  /* If the extension has a sysext API level declared, then it must match the host API
   * level. Otherwise, compare OS version as a whole */
  if (!isempty(host_os_release_sysext_level) && !isempty(extension->sysext_level))
    {
      if (!streq(host_os_release_sysext_level, extension->sysext_level))
	{
	  if (verbose)
	    printf("Extension '%s' is for API level '%s', but running on API level '%s'",
		   name, extension->sysext_level, host_os_release_sysext_level);
	  return 0;
	}
    }
  else if (!isempty(host_os_release_version_id))
    {
      if (isempty(extension->version_id))
	{
	  if (verbose)
	    printf("Extension '%s' does not contain VERSION_ID in release file but requested to match '%s'",
		   name, host_os_release_version_id);
	  return 0;
	}

      if (!streq(host_os_release_version_id, extension->version_id))
	{
	  if (verbose)
	    printf("Extension '%s' is for OS '%s', but deployed on top of '%s'.\n",
		   name, extension->version_id, host_os_release_version_id);
	  return 0;
	}
    }

  if (verbose)
    printf ("Version info of extension '%s' matches host.\n", name);
  return 1;
}
