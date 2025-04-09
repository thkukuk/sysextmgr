/* SPDX-License-Identifier: LGPL-2.1-or-later */

// based on systemd v258

#include <errno.h>
#include <assert.h>
#include <stdlib.h>
#include <fcntl.h>

#include "basics.h"
#include "tmpfile-util.h"
#include "umask-util.h"

static inline int RET_NERRNO(int ret) {
  if (ret < 0)
    return -errno;

  return ret;
}

/* This is much like mkostemp() but is subject to umask(). */
int mkostemp_safe(char *pattern) {
        assert(pattern);
        BLOCK_WITH_UMASK(0077);
        return RET_NERRNO(mkostemp(pattern, O_CLOEXEC));
}

void unlink_tempfilep(char (*p)[]) {
        assert(p);

        /* If the file is created with mkstemp(), it will (almost always) change the suffix.
         * Treat this as a sign that the file was successfully created. We ignore both the rare case
         * where the original suffix is used and unlink failures. */

        if (!endswith(*p, ".XXXXXX"))
                (void) unlink(*p);
}

int mkdtemp_malloc(const char *template, char **ret) {
        _cleanup_ (freep) char *p = NULL;

        assert(ret);

        if (template)
                p = strdup(template);
        else {
#if 0 /* XXX */
                const char *tmp;
		int r;

                r = tmp_dir(&tmp);
                if (r < 0)
                        return r;

                p = path_join(tmp, "XXXXXX");
#else
		return -EINVAL;
#endif
        }
        if (!p)
                return -ENOMEM;

        if (!mkdtemp(p))
                return -errno;

        *ret = TAKE_PTR(p);
        return 0;
}
