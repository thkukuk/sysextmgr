//SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define _unused_(x) x __attribute__((unused))
#define _pure_ __attribute__((__pure__))
#define _const_ __attribute__((__const__))

#define mfree(memory)                           \
        ({                                      \
                free(memory);                   \
                (typeof(memory)) NULL;          \
        })

static inline void freep(void *p) {
        *(void**)p = mfree(*(void**) p);
}

static inline void fclosep(FILE **f) {
  if (*f)
        fclose(*f);
}

#define _cleanup_(x) __attribute__((__cleanup__(x)))
#define _cleanup_fclose_ _cleanup_(fclosep)

/* from string-util-fundamental.h */

#define streq(a,b) (strcmp((a),(b)) == 0)
#define strneq(a, b, n) (strncmp((a), (b), (n)) == 0)
#define strcaseeq(a,b) (strcasecmp((a),(b)) == 0)
#define strncaseeq(a, b, n) (strncasecmp((a), (b), (n)) == 0)

extern char *startswith(const char *s, const char *prefix) _pure_;
extern char *endswith(const char *s, const char *suffix) _pure_;


static inline bool isempty(const char *a) {
        return !a || a[0] == '\0';
}

