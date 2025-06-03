/* SPDX-License-Identifier: LGPL-2.1-or-later */

// based on systemd v258

#include <assert.h>
#include "basics.h"
#include "osrelease.h"
#include "image-deps.h"
#include "extension-util.h"
#include "architecture.h"
#include "strv.h"

int
extension_release_validate(const char *name,
			   const struct osrelease *host_os_release,
			   const char *host_extension_scope,
			   const struct image_deps *extension,
			   bool verbose)
{
  _cleanup_strv_free_ char **id_like_l = NULL;

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
	       name, host_os_release->id);
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

  /* Match extension OS ID against host OS ID or ID_LIKE */
  if (host_os_release->id_like)
    {
#if 0 /* XXX */
      id_like_l = strv_split(host_os_release->id_like, WHITESPACE);
#else
      id_like_l = NULL;
#endif
      /* XXX if (!id_like_l)
	 return log_oom(); */
    }

  if (!streq(host_os_release->id, extension->id) && !strv_contains(id_like_l, extension->id))
    {
      if (verbose)
	printf("Extension '%s' is for OS '%s', but deployed on top of '%s'%s%s%s.\n",
	       name, extension->id, host_os_release->id,
	       host_os_release->id_like ? " (like '" : "",
	       strempty(host_os_release->id_like),
	       host_os_release->id_like ? "')" : "");
      return 0;
    }

  /* Rolling releases do not typically set VERSION_ID (eg: ArchLinux) */
  if (isempty(host_os_release->version_id) && isempty(host_os_release->sysext_level))
    {
      if (verbose)
	printf("No version info on the host (rolling release?), but ID in %s matched.\n", name);
      return 1;
    }

  /* If the extension has a sysext API level declared, then it must match the host API
   * level. Otherwise, compare OS version as a whole */
  if (!isempty(host_os_release->sysext_level) && !isempty(extension->sysext_level))
    {
      if (!streq(host_os_release->sysext_level, extension->sysext_level))
	{
	  if (verbose)
	    printf("Extension '%s' is for API level '%s', but running on API level '%s'",
		   name, extension->sysext_level, host_os_release->sysext_level);
	  return 0;
	}
    }
  else if (!isempty(host_os_release->version_id))
    {
      if (isempty(extension->version_id))
	{
	  if (verbose)
	    printf("Extension '%s' does not contain VERSION_ID in release file but requested to match '%s'",
		   name, host_os_release->version_id);
	  return 0;
	}

      if (!streq(host_os_release->version_id, extension->version_id))
	{
	  if (verbose)
	    printf("Extension '%s' is for version '%s', but deployed on top of '%s'.\n",
		   name, extension->version_id, host_os_release->version_id);
	  return 0;
	}
    }

  if (verbose)
    printf ("Version info of extension '%s' matches host.\n", name);
  return 1;
}
