/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "config.h"

#include <assert.h>
#include <getopt.h>
#include <unistd.h>
#include <dirent.h>
#include <stdlib.h>

#include "basics.h"
#include "download.h"
#include "sysextmgr.h"
#include "images-list.h"
#include "tmpfile-util.h"

/* XXX merge with main-update.c */
static void
unlink_and_free_tempfilep(char **p)
{
  if (p == NULL || *p == NULL)
    return;

  /* If the file is created with mkstemp(), it will (almost always) change the suffix.
   * Treat this as a sign that the file was successfully created. We ignore both the rare case
   * where the original suffix is used and unlink failures. */
  if (!endswith(*p, ".XXXXXX"))
    (void) unlink(*p);

  *p = mfree(*p);
}


int
main_install(int argc, char **argv)
{
  struct option const longopts[] = {
    {"url", required_argument, NULL, 'u'},
    {"quiet", no_argument, NULL, 'q'},
    {NULL, 0, NULL, '\0'}
  };
  char *url = NULL;
  bool arg_quiet = false;
  int c, r;

  while ((c = getopt_long(argc, argv, "qu:", longopts, NULL)) != -1)
    {
      switch (c)
        {
        case 'u':
          url = optarg;
          break;
	case 'q':
	  arg_quiet = true;
	  break;
        default:
          usage(EXIT_FAILURE);
          break;
        }
    }

  if (argc <= optind)
    {
      fprintf(stderr, "No images to install specified!\n");
      usage(EXIT_FAILURE);
    }

  for (int i = optind; i < argc; i++)
    {
      _cleanup_(free_image_entryp) struct image_entry *new = NULL;
      struct image_deps wanted_deps = {
	.architecture = "x86-64" /* XXX */
      };
      struct image_entry wanted = {
	.name = argv[i],
	.deps = &wanted_deps
      };

      r = get_latest_version(&wanted, &new, url, true /* verify_signature XXX */);
      if (r < 0)
	return r;

      if (new)
	{
	  _cleanup_free_ char *fn = NULL;
	  _cleanup_free_ char *linkfn = NULL;

	  if (!arg_quiet)
	    printf("Installing %s\n", new->deps->image_name);

	  r = join_path(SYSEXT_STORE_DIR, new->deps->image_name, &fn);
	  if (r < 0)
	    return r;

	  if (asprintf(&linkfn, "%s/%s.raw", EXTENSIONS_DIR, new->name) < 0)
	    return -ENOMEM;

	  /* XXX merge with main-update.c */
	  if (!new->local && new->remote)
	    {
	      _cleanup_(unlink_and_free_tempfilep) char *tmpfn = NULL;
	      _cleanup_close_ int fd = -EBADF;

	      assert(url);

	      if (asprintf(&tmpfn, "%s/.%s.XXXXXX", SYSEXT_STORE_DIR, new->deps->image_name) < 0)
		return -ENOMEM;

	      fd = mkostemp_safe(tmpfn);

	      r = download(url, new->deps->image_name, tmpfn, false /*XXX*/);
	      if (r < 0)
		{
		  fprintf(stderr, "Failed to download '%s' from '%s': %s\n",
			 new->deps->image_name, url, strerror(-r));
		  return r;
		}

	      if (rename(tmpfn, fn) < 0)
		{
		  fprintf(stderr, "Error to rename '%s' to '%s': %m\n",
			  tmpfn, fn);
		  return -errno;
		}
	    }

	  if (symlink(fn, linkfn) < 0)
	    {
	      fprintf(stderr, "Error to symlink '%s' to '%s': %m\n",
		      fn, linkfn);
	      return -errno;
	    }
	}
    }

  return EXIT_SUCCESS;
}
