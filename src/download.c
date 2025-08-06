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

#define SYSTEMD_PULL_PATH "/usr/lib/systemd/systemd-pull"

const char *
wstatus2str(int wstatus)
{
  static char buf[100];

  if (WIFEXITED(wstatus))
    snprintf(buf, sizeof(buf), "exit status %d", WEXITSTATUS(wstatus));
  else if (WIFSIGNALED(wstatus))
    snprintf(buf, sizeof(buf), "killed by signal %d", WTERMSIG(wstatus));
  else if (WIFSTOPPED(wstatus))
    snprintf(buf, sizeof(buf), "stopped by signal %d", WSTOPSIG(wstatus));
  else
    snprintf(buf, sizeof(buf), "unknown wstatus %d", wstatus);

  buf[sizeof(buf)-1] = '\0';

  return buf;
}

int
join_path(const char *url, const char *suffix, char **ret)
{
  char *s, *p;

  assert(url);
  assert(ret);

  s = malloc (strlen(url) + 1 + strlen(suffix) + 1);
  if (!s)
    return -ENOMEM;

  p = stpcpy(s, url);

  if (p[-1] != '/')
    p = stpcpy(p, "/");

  strcpy(p, suffix);

  *ret = s;
  return 0;
}


/* XXX see sysupdate-resource.c/download_manifest */
/* return value:
   < 0 : -errno (error)
   = 0 : success
   > 0 : status of waitpid (error)
*/
int
download(const char *url, const char *fn, const char *destfn, bool verify_signature)
{
  _cleanup_(freep) char *fullurl = NULL;
  pid_t pid;
  int status, r;

  r = join_path(url, fn, &fullurl);
  if (r < 0)
    return r;

  /* XXX safe_fork_full() */
  pid = fork();
  if (pid < 0)
    return -errno;

  if (pid == 0)
    {
      const char *const cmdline[] = {
	SYSTEMD_PULL_PATH,
	"raw",
	"--direct",                        /* just download the specified URL, don't download anything else */
	"--verify", verify_signature ? "signature" : "no", /* verify the manifest file */
	fullurl,
	destfn,
	NULL
      };
      /* XXX (void) close_all_fds(NULL, 0); */
      /* XXX r = invoke_callout_binary(SYSTEMD_PULL_PATH, (char *const*) cmdline); */

      execv (SYSTEMD_PULL_PATH, (char *const *)cmdline);
      fprintf(stderr, "execl(): %s", strerror (errno));
      exit (0);
    }

  /* r = wait_for_terminate_and_check("(sd-pull)", pid, WAIT_LOG); */
  r = waitpid (pid, &status, 0);
  if (r == -1)
    return -errno;

  if (status)
    return status;

  return 0;
}
