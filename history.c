/*
 * command history
 *
 * only implements in-memory history.
 */

#include "sh.h"
#include "ksh_stat.h"

/* No history to be compiled in: dummy routines to avoid lots more ifdefs */
void
init_histvec()
{
}
void
hist_init(s)
	Source *s;
{
}
void
hist_finish()
{
}
void
histsave(lno, cmd, dowrite)
	int lno;
	const char *cmd;
	int dowrite;
{
	errorf("history not enabled");
}
