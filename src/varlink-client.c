// SPDX-License-Identifier: GPL-2.0-or-later

#include "basics.h"
#include "varlink-client.h"

int
connect_to_sysextmgrd(sd_varlink **ret, const char *socket)
{
  _cleanup_(sd_varlink_unrefp) sd_varlink *link = NULL;
  int r;

  r = sd_varlink_connect_address(&link, socket);
  if (r < 0)
    {
      fprintf(stderr, "Failed to connect to %s: %s\n", socket, strerror(-r));
      return r;
    }

  *ret = TAKE_PTR(link);
  return 0;
}
