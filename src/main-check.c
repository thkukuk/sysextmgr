/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "config.h"

#include <assert.h>
#include <getopt.h>
#include <unistd.h>
#include <dirent.h>
#include <stdlib.h>

#include "basics.h"
#include "sysext-cli.h"
#include "images-list.h"
#include "strv.h"

static bool arg_verbose = false;

static int
version_cmp(const char *s1, const char *s2)
{
  return strcmp(s1,s2); /* XXX this needs proper version number parsing */
}

static int
check_if_newer(struct image_entry *old, struct image_entry *new, struct image_entry **update)
{
  assert(update);

  if (streq(old->deps->architecture,
	    new->deps->architecture) &&
      version_cmp(old->deps->sysext_version_id,
		  new->deps->sysext_version_id) < 0)
    {
      /* don't update with older version */
      if (*update &&
	  version_cmp((*update)->deps->sysext_version_id,
		      new->deps->sysext_version_id) >= 0)
	return 0;

      *update = new;
    }
  return 0;
}

int
main_check(int argc, char **argv)
{
  struct option const longopts[] = {
    {"url", required_argument, NULL, 'u'},
    {"verbose", no_argument, NULL, 'v'},
    {"quiet", no_argument, NULL, 'q'},
    {NULL, 0, NULL, '\0'}
  };
  _cleanup_(free_image_entry_list) struct image_entry **images_etc = NULL;
  size_t n_etc = 0;
  char *url = NULL;
  bool update_available = false;
  bool arg_quiet = false;
  int c, r;

  while ((c = getopt_long(argc, argv, "qu:v", longopts, NULL)) != -1)
    {
      switch (c)
        {
        case 'u':
          url = optarg;
          break;
	case 'v':
	  arg_verbose = true;
	  break;
	case 'q':
	  arg_quiet = true;
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

  /* list of "installed" images visible to systemd-sysext */
  r = image_local_metadata(EXTENSIONS_DIR, &images_etc, &n_etc, NULL);
  if (r < 0)
    {
      fprintf(stderr, "Searching for images in '%s' failed: %s\n",
              EXTENSIONS_DIR, strerror(-r));
      return r;
    }
  if (n_etc == 0)
    {
      printf("No installed images found.\n");
      return EXIT_SUCCESS;
    }

  for (size_t n = 0; n < n_etc; n++)
    {
      struct image_entry *update = NULL;
      _cleanup_(free_image_entry_list) struct image_entry **images_remote = NULL;
      _cleanup_(free_image_entry_list) struct image_entry **images_local = NULL;
      size_t n_remote = 0, n_local = 0;

      if (url)
	{
	  r = image_remote_metadata(url, &images_remote, &n_remote, images_etc[n]->name);
	  if (r < 0)
	    {
	      fprintf(stderr, "Fetching image data from '%s' failed: %s\n",
		      url, strerror(-r));
	      return r;
	    }
	}

      for (size_t i = 0; i < n_remote; i++)
	check_if_newer(images_etc[n], images_remote[i], &update);

      /* now do the same with local images */
      r = image_local_metadata(SYSEXT_STORE_DIR, &images_local, &n_local, images_etc[n]->name);
      if (r < 0)
	{
	  fprintf(stderr, "Searching for images in '%s' failed: %s\n",
		  SYSEXT_STORE_DIR, strerror(-r));
	  return r;
	}

      for (size_t i = 0; i < n_local; i++)
	check_if_newer(images_etc[n], images_local[i], &update);

      if (update)
	{
	  /* XXX pretty print */
	  if (!arg_quiet)
	    printf("%s -> %s\n", images_etc[n]->deps->image_name, update->deps->image_name);
	  update_available = true;
	}
    }

  /* Return ENODATA if there is no update and we should not print anything */
  if (arg_quiet && !update_available)
    return ENODATA;

  return EXIT_SUCCESS;
}
