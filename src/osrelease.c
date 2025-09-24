/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <libeconf.h>

#include "basics.h"
#include "download.h"
#include "osrelease.h"

void
free_os_release(struct osrelease *p)
{
  if (!p)
    return;

  p->id = mfree(p->id);
  p->id_like = mfree(p->id_like);
  p->version_id = mfree(p->version_id);
  p->sysext_level = mfree(p->sysext_level);
}

void
free_os_releasep(struct osrelease **p)
{
  if (!p || !*p)
    return;

  free_os_release(*p);
  *p = mfree(*p);
}

int
load_os_release(const char *prefix, struct osrelease **res)
{
  _cleanup_free_ char *osrelease = NULL;
  _cleanup_(econf_freeFilep) econf_file *key_file = NULL;
  econf_err error;
  int r;

  assert(res);

  *res = calloc(1, sizeof(struct osrelease));
  if (res == NULL)
    return -ENOMEM;


  if (!isempty(prefix))
    {
      r = join_path(prefix, "/etc/os-release", &osrelease);
      if (r < 0)
	return r;
    }
  else
    {
      osrelease = strdup("/etc/os-release");
      if (!osrelease)
	return -ENOMEM;
    }

  if (access(osrelease, F_OK) != 0)
    {
      osrelease = mfree(osrelease);
      if (!isempty(prefix))
	{
	  r = join_path(prefix, "/usr/lib/os-release", &osrelease);
	  if (r < 0)
	    return r;
	}
      else
	{
	  osrelease = strdup("/usr/lib/os-release");
	  if (!osrelease)
	    return -ENOMEM;
	}
  }

  if ((error = econf_readFile(&key_file, osrelease, "=", "#")))
    {
      fprintf(stderr, "ERROR: couldn't read %s: %s\n", osrelease, econf_errString(error));
      return -1;
    }

  if ((error = econf_getStringValue(key_file, NULL, "ID", &(*res)->id)))
    {
      fprintf(stderr, "ERROR: couldn't get key 'ID' from %s: %s\n",
	      osrelease, econf_errString(error));
      return -1;
    }

  if ((error = econf_getStringValue(key_file, NULL, "ID_LIKE", &(*res)->id_like)))
    {
      fprintf(stderr, "ERROR: couldn't get key 'ID_LIKE' from %s: %s\n",
	      osrelease, econf_errString(error));
      return -1;
    }

  if ((error = econf_getStringValue(key_file, NULL, "VERSION_ID", &(*res)->version_id)))
    {
      fprintf(stderr, "ERROR: couldn't get key 'VERSION_ID' from %s: %s\n", osrelease, econf_errString(error));
      return -1;
    }

  if ((error = econf_getStringValue(key_file, NULL, "SYSEXT_LEVEL", &(*res)->sysext_level))
      && error != ECONF_NOKEY)
    {
      fprintf(stderr, "ERROR: couldn't get key 'SYSEXT_LEVEL' from %s: %s\n",
	      osrelease, econf_errString(error));
      return -1;
    }

  return 0;
}
