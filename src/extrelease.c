/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <assert.h>
#include <unistd.h>
#include <libeconf.h>

#include "basics.h"
#include "sysext-cli.h"
#include "extrelease.h"

int
load_ext_release(const char *name, const char *fn, struct image_deps **res)
{
  _cleanup_(econf_freeFilep) econf_file *key_file = NULL;
  _cleanup_(free_image_depsp) struct image_deps *e = NULL;
  econf_err error;

  assert(name);
  assert(fn);
  assert(res);

  e = calloc(1, sizeof(struct image_deps));
  if (e == NULL)
    oom();

  e->image_name = strdup(name);
  if (e->image_name == NULL)
    oom();

  if ((error = econf_readFile(&key_file, fn, "=", "#")))
    {
      fprintf(stderr, "ERROR: couldn't read %s: %s\n", fn, econf_errString(error));
      return -1;
    }

  if ((error = econf_getStringValue(key_file, NULL, "ID", &(e->id))))
    {
      fprintf(stderr, "ERROR: couldn't get key 'ID' from %s: %s\n", fn, econf_errString(error));
      return -1;
    }

  if ((error = econf_getStringValue(key_file, NULL, "VERSION_ID", &(e->version_id))))
    {
      fprintf(stderr, "ERROR: couldn't get key 'VERSION_ID' from %s: %s\n", fn, econf_errString(error));
      return -1;
    }

  if ((error = econf_getStringValue(key_file, NULL, "SYSEXT_LEVEL", &(e->sysext_level)))
      && error != ECONF_NOKEY)
    {
      fprintf(stderr, "ERROR: couldn't get key 'SYSEXT_LEVEL' from %s: %s\n",
	      fn, econf_errString(error));
      return -1;
    }

  if ((error = econf_getStringValue(key_file, NULL, "SYSEXT_VERSION_ID", &(e->sysext_version_id)))
      && error != ECONF_NOKEY)
    {
      fprintf(stderr, "ERROR: couldn't get key 'SYSEXT_VERSION_ID' from %s: %s\n",
	      fn, econf_errString(error));
      return -1;
    }

  if ((error = econf_getStringValue(key_file, NULL, "SYSEXT_SCOPE", &(e->sysext_scope)))
      && error != ECONF_NOKEY)
    {
      fprintf(stderr, "ERROR: couldn't get key 'SYSEXT_SCOPE' from %s: %s\n",
	      fn, econf_errString(error));
      return -1;
    }

  if ((error = econf_getStringValue(key_file, NULL, "ARCHITECTURE", &(e->architecture)))
      && error != ECONF_NOKEY)
    {
      fprintf(stderr, "ERROR: couldn't get key 'ARCHITECTURE' from %s: %s\n",
	      fn, econf_errString(error));
      return -1;
    }

  *res = TAKE_PTR(e);

  return 0;
}
