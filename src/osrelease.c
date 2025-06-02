/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <libeconf.h>

#include "basics.h"
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
  _cleanup_(econf_freeFilep) econf_file *key_file = NULL;
  econf_err error;

  assert(res);

  *res = calloc(1, sizeof(struct osrelease));
  if (res == NULL)
    return -ENOMEM;

  /* XXX add prefix to look in snapshots */
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
