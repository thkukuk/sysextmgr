/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "basics.h"

#include <spawn.h>
#include <sys/wait.h>
#include <errno.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include "download.h"
#include "log_msg.h"
#include "extract.h"

#define SYSTEMD_DISSECT_PATH "/usr/bin/systemd-dissect"

// External environment array
extern char **environ;

int
extract(const char *path, const char *name, int outfd)
{
  _cleanup_free_ char *fn = NULL, *erf = NULL;
  pid_t pid;
  posix_spawn_file_actions_t actions;
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

  const char *const cmdline[] = {
	  SYSTEMD_DISSECT_PATH,
	  "--copy-from",
	  fn,
	  erf,
	  "-",
	  NULL
  };

  posix_spawn_file_actions_init(&actions);

  /* Copy 'outfd' to FD 1 (stdout) of the new process. */
  /* The parent process does not touch the original 'outfd'. */
  r = posix_spawn_file_actions_adddup2(&actions, outfd, STDOUT_FILENO);
  if (r == 0)
    {
      r = posix_spawn(&pid, SYSTEMD_DISSECT_PATH, &actions, NULL, (char *const *)cmdline, environ);

      if (r != 0)
	{
          log_msg(LOG_ERR, "Cannot start extract: %s\n", strerror(r));
          posix_spawn_file_actions_destroy(&actions);
          return r;
	}
      else
	{
          /* waiting for child process */
          int status;

          r =waitpid(pid, &status, 0);
          if (r == -1)
            {
              posix_spawn_file_actions_destroy(&actions);
	      return -errno;
	    }

	  // Use WIFEXITED to check the result
          if (!WIFEXITED(status))
            {
              posix_spawn_file_actions_destroy(&actions);
              return status;
	    }
        }
    }
  else
    {
      log_msg(LOG_ERR, "Cannot set stdout: %s\n", strerror(r));
      posix_spawn_file_actions_destroy(&actions);
      return r;
    }

  posix_spawn_file_actions_destroy(&actions);
  return r;
}
