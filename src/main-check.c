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
      _cleanup_(free_image_entryp) struct image_entry *update = NULL;

      r = get_latest_version(images_etc[n], &update, url);
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
