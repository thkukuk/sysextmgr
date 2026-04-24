// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <syslog.h>

extern void set_max_log_level(int level);
extern void reset_max_log_level(void);
extern void log_msg(int priority, const char *fmt, ...);
