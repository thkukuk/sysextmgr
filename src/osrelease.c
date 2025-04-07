/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <unistd.h>
#include <libeconf.h>

#include "basics.h"
#include "osrelease.h"

int
load_os_release(char **id, char **version_id, char **sysext_level)
{
  _cleanup_(econf_freeFilep) econf_file *key_file = NULL;
  econf_err error;

  const char *osrelease = NULL;
  if (access("/etc/os-release", F_OK) == 0)
    osrelease = "/etc/os-release";
  else
    osrelease = "/usr/lib/os-release";

  if ((error = econf_readFile(&key_file, osrelease, "=", "#")))
    {
      fprintf(stderr, "ERROR: couldn't read %s: %s\n", osrelease, econf_errString(error));
      return -1;
    }

  if ((error = econf_getStringValue(key_file, NULL, "ID", id)))
    {
      fprintf(stderr, "ERROR: couldn't get key 'ID' from %s: %s\n", osrelease, econf_errString(error));
      return -1;
    }

  if ((error = econf_getStringValue(key_file, NULL, "VERSION_ID", version_id)))
    {
      fprintf(stderr, "ERROR: couldn't get key 'VERSION_ID' from %s: %s\n", osrelease, econf_errString(error));
      return -1;
    }

  if ((error = econf_getStringValue(key_file, NULL, "SYSEXT_LEVEL", sysext_level))
      && error != ECONF_NOKEY)
    {
      fprintf(stderr, "ERROR: couldn't get key 'SYSEXT_LEVEL' from %s: %s\n",
	      osrelease, econf_errString(error));
      return -1;
    }

  return 0;
}
