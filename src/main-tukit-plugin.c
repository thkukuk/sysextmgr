/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "config.h"

#include <getopt.h>
#include <stdbool.h>

#include "basics.h"
#include "sysextmgr.h"
#include "varlink-client.h"

int
main_tukit_plugin(int argc, char **argv)
{
  int r;
  const char *action = argv[0];
  const char *path = argv[1];

  if (argc == 0)
    {
      fprintf(stderr, "sysextmgr tukit plugin called without arguments!\n");
      return EINVAL;
    }

  if (!streq(action, "finalize-pre"))
    return 0;

  if (argc != 3)
    {
      fprintf(stderr, "sysextmgr tukit plugin called with wrong number of arguments. Expected 3, got %i\n", argc);
      return EINVAL;
    }

  printf("Checking for sysext image updates...\n");

  r = varlink_check(NULL, path);
  if (r < 0)
    {
      /* sysextmgrd not running, do nothing */
      if (VARLINK_IS_NOT_RUNNING(r))
	{
	  fprintf(stderr, "sysextmgrd not running!\n");
	  return 0;
	}
    }

  /* Return ENOMEDIUM if current image is incompatible and there is no update */
  if (r == -ENOMEDIUM)
    {
      fprintf(stderr, "At least one installed sysext image is incompatible and no update exists.\n");
      return ENOMEDIUM;
    }

  /* No update available */
  if (r == -ENODATA)
    {
      printf("No updates found\n");
      return 0;
    }

  printf("Updating the sysext images, be patient...\n");
  r = varlink_update(NULL, path);
  if (r < 0)
    return -r;

  return 0;
}
