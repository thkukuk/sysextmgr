/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "basics.h"

#include <errno.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include "download.h"
#include "extract.h"

#define SYSTEMD_DISSECT_PATH "/usr/bin/systemd-dissect"

/* XXX see sysupdate-resource.c/download_manifest */
int
extract(const char *path, const char *name, int outfd)
{
  _cleanup_free_ char *fn = NULL, *erf = NULL;
  pid_t pid;
  int r;

  if (!endswith(name, ".raw") && !endswith(name, ".img"))
    return -EINVAL;

  r = join_path(path, name, &fn);
  if (r < 0)
    return r;

  if (asprintf(&erf, "/usr/lib/extension-release.d/extension-release.%s", name) < 0)
    return -ENOMEM;

  /* remove .raw/.img */
  erf[strlen(erf) - 4] = '\0';

  /* XXX safe_fork_full() */
  pid = fork();
  if (pid < 0)
    return -errno;

  if (pid == 0)
    {
      const char *const cmdline[] = {
	SYSTEMD_DISSECT_PATH,
	"--copy-from",
	fn,
	erf,
	"-",
	NULL
      };
      close(1);
      r = dup2(outfd, 1);
      if (r < 0)
	return r;

      /* XXX (void) close_all_fds(NULL, 0); */
      /* XXX r = invoke_callout_binary(SYSTEMD_PULL_PATH, (char *const*) cmdline); */

      execv (SYSTEMD_DISSECT_PATH, (char *const *)cmdline);
      fprintf(stderr, "execl(): %s", strerror (errno));
      exit (0);
    }

  /* r = wait_for_terminate_and_check("(sd-pull)", pid, WAIT_LOG); */
  waitpid (pid, NULL, 0);
  return 0;
}
