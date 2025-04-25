// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdio.h>
#include <unistd.h>
#include <syslog.h>
#include <systemd/sd-journal.h>

#include "log_msg.h"

static int log_level = LOG_WARNING;

void
set_max_log_level(int level)
{
  log_level = level;
}

void
log_msg(int priority, const char *fmt, ...)
{
  static int is_tty = -1;

  if (priority > log_level)
    return;

  if (is_tty == -1)
    is_tty = isatty(STDOUT_FILENO);

  va_list ap;

  va_start(ap, fmt);

  if (is_tty)
    {
      if (priority <= LOG_ERR)
        {
          vfprintf(stderr, fmt, ap);
          fputc('\n', stderr);
        }
      else
        {
          vprintf(fmt, ap);
          putchar('\n');
        }
    }
  else
    sd_journal_printv(priority, fmt, ap);

  va_end(ap);
}
