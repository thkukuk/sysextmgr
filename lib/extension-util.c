/* SPDX-License-Identifier: LGPL-2.1-or-later */

// based on systemd v258

#include <assert.h>
#include "basics.h"
#include "osrelease.h"
#include "image-deps.h"
#include "extension-util.h"
#include "architecture.h"
#include "strv.h"
#include "log_msg.h"

int extention_architecture_compatible(const char *architecture)
{
   if(!isempty(architecture) && !streq(architecture, "_any") &&
      !streq(architecture_to_string(uname_architecture()), architecture))
     return 0;
   else
     return 1;
}

int
extension_release_validate(const char *name,
			   const struct osrelease *host_os_release,
			   const char *host_extension_scope,
			   const struct image_deps *extension)
{
  _cleanup_strv_free_ char **id_like_l = NULL;

  assert(extension);

  if (extension->sysext_scope && host_extension_scope)
    {
      if (strstr(extension->sysext_scope, host_extension_scope) == NULL)
	{
	  log_msg(LOG_INFO, "Extension '%s' is not suitable for scope %s, ignoring.\n", name, host_extension_scope);
	  return 0;
	}
    }

  /* When the architecture field is present and not '_any' it must match the host - for now just look at uname but in
   * the future we could check if the kernel also supports 32 bit or binfmt has a translator set up for the architecture */
  if (!extention_architecture_compatible(extension->architecture))
    {
      log_msg(LOG_INFO, "Extension '%s' is for architecture '%s', but deployed on top of '%s'.\n",
	      name, extension->architecture, architecture_to_string(uname_architecture()));
      return 0;
    }

  if (isempty(extension->id))
    {
      log_msg(LOG_INFO, "Extension '%s' does not contain ID in release file but requested to match '%s' or be '_any'\n",
	      name, host_os_release->id);
      return 0;
    }

  /* A sysext(or confext) with no host OS dependency (static binaries or scripts) can match
   * '_any' host OS, and VERSION_ID or SYSEXT_LEVEL(or CONFEXT_LEVEL) are not required anywhere */
  if (streq(extension->id, "_any"))
    {
      log_msg(LOG_INFO, "Extension '%s' matches '_any' OS.\n", name);
      return 1;
    }

  /* Match extension OS ID against host OS ID or ID_LIKE */
  /* This is currently not supported. It seems that is too much general. */
  /* E.g. "openSUSE MicroOS" has ID_LIKE: "suse opensuse opensuse-tumbleweed microos sl-micro" */
  if (host_os_release->id_like)
    {
#if 0
      id_like_l = strv_split(host_os_release->id_like, WHITESPACE);
      if (!id_like_l)
      {
        log_msg(LOG_ERR, "Out of Memory");
	return 0;
      }
#else
      id_like_l = NULL;
#endif
    }

  if (!streq(host_os_release->id, extension->id) && !strv_contains(id_like_l, extension->id))
    {
      log_msg(LOG_INFO, "Extension '%s' is for OS '%s', but deployed on top of '%s'%s%s%s.\n",
	      name, extension->id, host_os_release->id,
	      host_os_release->id_like ? " (like '" : "",
	      strempty(host_os_release->id_like),
	      host_os_release->id_like ? "')" : "");
      return 0;
    }

  /* Rolling releases do not typically set VERSION_ID (eg: ArchLinux) */
  if (isempty(host_os_release->version_id) && isempty(host_os_release->sysext_level))
    {
      log_msg(LOG_INFO, "No version info on the host (rolling release?), but ID in %s matched.\n", name);
      return 1;
    }

  /* If the extension has a sysext API level declared, then it must match the host API
   * level. Otherwise, compare OS version as a whole */
  if (!isempty(host_os_release->sysext_level) && !isempty(extension->sysext_level))
    {
      if (!streq(host_os_release->sysext_level, extension->sysext_level))
	{
          log_msg(LOG_INFO, "Extension '%s' is for API level '%s', but running on API level '%s'\n",
		  name, extension->sysext_level, host_os_release->sysext_level);
	  return 0;
	}
    }
  else if (!isempty(host_os_release->version_id))
    {
      if (isempty(extension->version_id))
	{
          log_msg(LOG_INFO, "Extension '%s' does not contain VERSION_ID in release file but requested to match '%s'\n",
		  name, host_os_release->version_id);
	  return 0;
	}

      if (!streq(host_os_release->version_id, extension->version_id))
	{
          log_msg(LOG_INFO, "Extension '%s' is for version '%s', but deployed on top of '%s'.\n",
		  name, extension->version_id, host_os_release->version_id);
	  return 0;
	}
    }

  log_msg(LOG_INFO, "Version info of extension '%s' matches host.\n", name);
  return 1;
}
