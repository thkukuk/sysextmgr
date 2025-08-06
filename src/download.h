//SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <stdbool.h>

extern const char *wstatus2str(int wstatus);
extern int join_path(const char *url, const char *suffix, char **ret);
extern int download(const char *url, const char *fn, const char *dest, bool verify_signature);

