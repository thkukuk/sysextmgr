/* SPDX-License-Identifier: LGPL-2.1-or-later */

// based on systemd v258

#include <assert.h>
#include <stdint.h>
#include <errno.h>

#include "strv.h"

char* strv_find(char * const *l, const char *name) {
        assert(name);

        STRV_FOREACH(i, l)
                if (streq(*i, name))
                        return *i;

        return NULL;
}

char** strv_free(char **l) {
        STRV_FOREACH(k, l)
                free(*k);

        return mfree(l);
}


size_t strv_length(char * const *l) {
        size_t n = 0;

        STRV_FOREACH(i, l)
                n++;

        return n;
}
