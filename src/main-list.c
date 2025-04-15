/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "config.h"

#include <assert.h>
#include <getopt.h>
#include <unistd.h>
#include <dirent.h>

#include <systemd/sd-json.h>

#include "basics.h"
#include "extension-util.h"
#include "sysext-cli.h"
#include "osrelease.h"
#include "extrelease.h"
#include "tmpfile-util.h"
#include "strv.h"
#include "images-list.h"

static bool arg_verbose = false;

/* compare the image names, but move NULL to the end
   of the list */
static int
image_cmp(const void *a, const void *b)
{
  const struct image_entry *const *i_a = a;
  const struct image_entry *const *i_b = b;

  if (a == NULL && b == NULL)
    return 0;

  if (a == NULL)
    return 1;

  if (b == NULL)
    return -1;

  return strcmp((*i_a)->deps->image_name, (*i_b)->deps->image_name);
}

int
main_list(int argc, char **argv)
{
  struct option const longopts[] = {
    {"url", required_argument, NULL, 'u'},
    {"verbose", no_argument, NULL, 'v'},
    {NULL, 0, NULL, '\0'}
  };
  _cleanup_(sd_json_variant_unrefp) sd_json_variant *json = NULL;
  _cleanup_(freep) char *osrelease_id = NULL;
  _cleanup_(freep) char *osrelease_sysext_level = NULL;
  _cleanup_(freep) char *osrelease_version_id = NULL;
  _cleanup_(free_image_entry_list) struct image_entry **images = NULL;
  _cleanup_(free_image_entry_list) struct image_entry **images_remote = NULL;
  _cleanup_(free_image_entry_list) struct image_entry **images_local = NULL;
  size_t n_remote = 0, n_local = 0;
  char *url = NULL;
  int c, r;

  while ((c = getopt_long(argc, argv, "u:v", longopts, NULL)) != -1)
    {
      switch (c)
        {
        case 'u':
          url = optarg;
          break;
	case 'v':
	  arg_verbose = true;
	  break;
        default:
          usage(EXIT_FAILURE);
          break;
        }
    }

  if (argc > optind)
    {
      fprintf(stderr, "Unexpected argument: %s\n", argv[optind]);
      usage(EXIT_FAILURE);
    }

  r = load_os_release(&osrelease_id, &osrelease_version_id, &osrelease_sysext_level);
  if (r < 0)
    return EXIT_FAILURE;

  if (url)
    {
      r = image_remote_metadata(url, &images_remote, &n_remote);
      if (r < 0)
	{
	  fprintf(stderr, "Fetching image data from '%s' failed: %s\n",
		  url, strerror(-r));
	  return r;
	}

      if (n_remote > 0)
	{
	  for (size_t i = 0; i < n_remote; i++)
	    if (images_remote[i]->deps)
	      images_remote[i]->compatible = extension_release_validate(images_remote[i]->deps->image_name,
									osrelease_id, osrelease_version_id,
									osrelease_sysext_level, "system",
									images_remote[i]->deps, arg_verbose);
	}
    }

  r = image_local_metadata(SYSEXT_STORE_DIR, &images_local, &n_local);
  if (r < 0)
    {
      fprintf(stderr, "Searching for images in '%s' failed: %s\n",
	      SYSEXT_STORE_DIR, strerror(-r));
      return r;
    }

  if ((n_local + n_remote) == 0)
    {
      printf("No images found\n");
      return EXIT_SUCCESS;
    }

  /* merge remote and local images */
  images = calloc((n_remote + n_local + 1), sizeof(struct image_entry *));
  if (images == NULL)
    oom();

  size_t n = 0;

  for (size_t i = 0; i < n_remote; i++)
    {
      images[n] = TAKE_PTR(images_remote[i]);

      if (images[n]->deps)
	images[n]->compatible = extension_release_validate(images[n]->deps->image_name,
							   osrelease_id, osrelease_version_id,
							   osrelease_sysext_level, "system",
							   images[n]->deps, arg_verbose);
      n++;
    }

  for (size_t i = 0; i < n_local; i++)
    {
      bool found = false;

      /* check if we know already the image */
      for (size_t j = 0; j < n_remote; j++)
	{
	  if (streq(images_local[i]->deps->image_name, images[j]->deps->image_name))
	    {
	      images[j]->installed = true;
	      found = true;
	      break;
	    }
	}

      if (!found)
	{
	  images[n] = TAKE_PTR(images_local[i]);

	  if (images[n]->deps)
	    images[n]->compatible = extension_release_validate(images[n]->deps->image_name,
							       osrelease_id, osrelease_version_id,
							       osrelease_sysext_level, "system",
							       images[n]->deps, arg_verbose);
	  n++;
	}
    }

  /* sort list */
  qsort(images, n, sizeof(struct image_entry *), image_cmp);

  /* XXX Use table_print_with_pager */
  printf (" A I C Name\n");
  for (size_t i = 0; images[i] != NULL; i++)
    {
      if (images[i]->remote)
	printf(" x");
      else
	printf("  ");
      if (images[i]->installed)
	printf(" x");
      else
	printf("  ");
      if (images[i]->compatible)
	printf(" x");
      else
	printf("  ");
      printf(" %s\n", images[i]->deps->image_name);
    }
  printf("A = available, I = installed, C = commpatible\n");

  return EXIT_SUCCESS;
}
