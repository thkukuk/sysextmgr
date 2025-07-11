/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "config.h"

#include <assert.h>

#include "basics.h"
#include "image-deps.h"
#include "images-list.h"
#include "sysextmgr.h"

static int
check_if_newer(struct image_entry *old, struct image_entry *new,
	       struct image_entry **update)
{
  assert(update);

  /* new image is not compatible */
  if (!new->compatible)
    return 0;

  if (!streq(old->name, new->name))
      return 0;

  if (!streq(old->deps->architecture,
	    new->deps->architecture))
    return 0;

  /* both images are identical, make sure flags are identical, too */
  if (*update &&
      streq((*update)->image_name, new->image_name) &&
      streq((*update)->deps->sysext_version_id,
	    new->deps->sysext_version_id))
    {
      if (new->local)
	(*update)->local = new->local;
      if (new->remote)
	(*update)->remote = new->remote;
      if (new->installed)
	(*update)->installed = new->installed;
      if (new->compatible)
	(*update)->compatible = new->compatible;
    }
  /* old->deps->sysext_version_id is not set if this is image is not installed */
  else if (old->deps->sysext_version_id == NULL ||
	   strverscmp(old->deps->sysext_version_id,
		      new->deps->sysext_version_id) < 0)
    {
      /* don't update with older version */
      if (*update)
	{
	  if (strverscmp((*update)->deps->sysext_version_id,
			 new->deps->sysext_version_id) >= 0)
	    return 0;
	  free_image_entryp(update);
	}

      *update = calloc(1, sizeof(struct image_entry));
      if (*update == NULL)
	return -ENOMEM;
      (*update)->name = strdup(new->name);
      (*update)->image_name = strdup(new->image_name);
      (*update)->deps = TAKE_PTR(new->deps);
      (*update)->local = new->local;
      (*update)->remote = new->remote;
      (*update)->installed = new->installed;
      (*update)->compatible = new->compatible;
      (*update)->refcount = new->refcount;
    }

  return 0;
}

int
get_latest_version(struct image_entry *curr, struct image_entry **new,
		   const char *url, bool verify_signature,
		   const struct osrelease *osrelease, bool verbose)
{
  _cleanup_(free_image_entryp) struct image_entry *update = NULL;
  _cleanup_(free_image_entry_list) struct image_entry **images_remote = NULL;
  _cleanup_(free_image_entry_list) struct image_entry **images_local = NULL;
  size_t n_remote = 0, n_local = 0;
  int r;

  if (url)
    {
      r = image_remote_metadata(url, &images_remote, &n_remote, curr->name,
				verify_signature, osrelease, verbose);
      if (r < 0)
	{
	  fprintf(stderr, "Fetching image data from '%s' failed: %s\n",
		  url, strerror(-r));
	  return r;
	}
    }

  for (size_t i = 0; i < n_remote; i++)
    {
      r = check_if_newer(curr, images_remote[i], &update);
      if (r < 0)
	{
	  fprintf(stderr, "Image check failed: %s\n", strerror(-r));
	  return r;
	}
    }

  /* now do the same with local images */
  r = image_local_metadata(SYSEXT_STORE_DIR, &images_local, &n_local, curr->name, osrelease, true, verbose);
  if (r < 0)
    {
      fprintf(stderr, "Searching for images in '%s' failed: %s\n",
	      SYSEXT_STORE_DIR, strerror(-r));
      return r;
    }

  for (size_t i = 0; i < n_local; i++)
    {
      r = check_if_newer(curr, images_local[i], &update);
      if (r < 0)
	{
	  fprintf(stderr, "Image check failed: %s\n", strerror(-r));
	  return r;
	}
    }

  if (update)
    *new = TAKE_PTR(update);

  return 0;
}
