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

  if (argc == 0)
    {
      fprintf(stderr, "sysextmgr tukit plugin called without arguments!\n");
      return EINVAL;
    }

  if (!streq(argv[0], "finalize-pre"))
    return 0;

  if (argc < 3)
    {
      fprintf(stderr, "sysextmgr tukit plugin called with wrong number of arguments. Expected >= 3, got %i\n", argc);
      return EINVAL;
    }

  r = varlink_check(NULL, argv[0]);
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
    return ENOMEDIUM;

  /* No update available */
  if (r == -ENODATA)
    return 0;

  r = varlink_update(NULL, argv[0]);
  if (r < 0)
    return -r;

  return 0;
}
