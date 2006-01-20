/*
 * command history
 *
 * only implements in-memory history.
 */

#include <sys/stat.h>
#include "sh.h"

/* No history to be compiled in: dummy routines to avoid lots more ifdefs */
void
init_histvec(void)
{
}
void
hist_init(Source *s)
{
}
void
hist_finish(void)
{
}
void
histsave(int lno, const char *cmd, int dowrite)
{
        errorf("history not enabled");
}
