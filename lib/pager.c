#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

#include "pager.h"

FILE* setup_pager(void) {
    /* 1. Only use a pager if output is a TTY (terminal) */
    if (!isatty(STDOUT_FILENO))
        return stdout;

    /* 2. Check for PAGER environment variable, default to 'less' */
    const char *pager = getenv("PAGER");
    if (!pager) 
        pager = "less";

    /* 3. Set 'less' options if using it (S: chop long lines, R: allow colors) */
    setenv("LESS", "SR", 0);

    /* 4. Open a pipe to the pager command */
    FILE *f = popen(pager, "w");
    if (!f)
        return stdout;

    return f;
}
