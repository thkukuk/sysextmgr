#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

#include "pager.h"

static FILE* setup_pager(void)
{
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

void pager(struct libscols_table *table, const char *fooder)
{
  FILE *out = setup_pager();

  if (out == stdout)
    {
      /* Standard print if no pager */
      scols_print_table(table);
    }
  else
    {
      /* Redirect stdout to the pager pipe */
      int original_stdout = dup(STDOUT_FILENO);
      dup2(fileno(out), STDOUT_FILENO);

      /* Now this prints to the PAGER because stdout points there */
      scols_print_table(table);
      if (fooder)
        printf("%s", fooder);

      /* Flush and restore stdout */
      fflush(stdout);
      dup2(original_stdout, STDOUT_FILENO);
      close(original_stdout);

      pclose(out);
    }
}
