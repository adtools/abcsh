/*
 * Process and job control
 */

/*
 * Reworked/Rewritten version of Eric Gisin's/Ron Natalie's code by
 * Larry Bouzane (larry@cs.mun.ca) and hacked again by
 * Michael Rendell (michael@cs.mun.ca)
 *
 * The interface to the rest of the shell should probably be changed
 * to allow use of vfork() when available but that would be way too much
 * work :)
 *
 */

#include "sh.h"
#include "ksh_stat.h"
#include "ksh_wait.h"
#include "ksh_times.h"
#include "tty.h"


/* Start of system configuration stuff */

/* We keep CHILD_MAX zombie processes around (exact value isn't critical) */
#ifndef CHILD_MAX
# ifdef _POSIX_CHILD_MAX
#  define CHILD_MAX     ((_POSIX_CHILD_MAX) * 2)
# else /* _POSIX_CHILD_MAX */
#  define CHILD_MAX     20
# endif /* _POSIX_CHILD_MAX */
#endif /* !CHILD_MAX */

/* End of system configuration stuff */


/* Order important! */
#define PRUNNING        0
#define PEXITED         1
#define PSIGNALLED      2
#define PSTOPPED        3

typedef struct proc     Proc;
struct proc {
        Proc    *next;          /* next process in pipeline (if any) */
        int     state;
        WAIT_T  status;         /* wait status */
        pid_t   pid;            /* process id */
        char    command[48];    /* process command string */
};

/* Notify/print flag - j_print() argument */
#define JP_NONE         0       /* don't print anything */
#define JP_SHORT        1       /* print signals processes were killed by */
#define JP_MEDIUM       2       /* print [job-num] -/+ command */
#define JP_LONG         3       /* print [job-num] -/+ pid command */
#define JP_PGRP         4       /* print pgrp */

/* put_job() flags */
#define PJ_ON_FRONT     0       /* at very front */
#define PJ_PAST_STOPPED 1       /* just past any stopped jobs */

/* Job.flags values */
#define JF_STARTED      0x001   /* set when all processes in job are started */
#define JF_WAITING      0x002   /* set if j_waitj() is waiting on job */
#define JF_W_ASYNCNOTIFY 0x004  /* set if waiting and async notification ok */
#define JF_XXCOM        0x008   /* set for `command` jobs */
#define JF_FG           0x010   /* running in foreground (also has tty pgrp) */
#define JF_SAVEDTTY     0x020   /* j->ttystate is valid */
#define JF_CHANGED      0x040   /* process has changed state */
#define JF_KNOWN        0x080   /* $! referenced */
#define JF_ZOMBIE       0x100   /* known, unwaited process */
#define JF_REMOVE       0x200   /* flaged for removal (j_jobs()/j_noityf()) */
#define JF_USETTYMODE   0x400   /* tty mode saved if process exits normally */
#define JF_SAVEDTTYPGRP 0x800   /* j->saved_ttypgrp is valid */

typedef struct job Job;
struct job {
        Job     *next;          /* next job in list */
        int     job;            /* job number: %n */
        int     flags;          /* see JF_* */
        int     state;          /* job state */
        int     status;         /* exit status of last process */
        pid_t   pgrp;           /* process group of job */
        pid_t   ppid;           /* pid of process that forked job */
        INT32   age;            /* number of jobs started */
        INT32   systime;        /* system time used by job */
        INT32   usrtime;        /* user time used by job */
        Proc    *proc_list;     /* process list */
        Proc    *last_proc;     /* last process in list */
#ifdef KSH
        Coproc_id coproc_id;    /* 0 or id of coprocess output pipe */
#endif /* KSH */
};

/* Flags for j_waitj() */
#define JW_NONE         0x00
#define JW_INTERRUPT    0x01    /* ^C will stop the wait */
#define JW_ASYNCNOTIFY  0x02    /* asynchronous notification during wait ok */
#define JW_STOPPEDWAIT  0x04    /* wait even if job stopped */

/* Error codes for j_lookup() */
#define JL_OK           0
#define JL_NOSUCH       1       /* no such job */
#define JL_AMBIG        2       /* %foo or %?foo is ambiguous */
#define JL_INVALID      3       /* non-pid, non-% job id */

static const char       *const lookup_msgs[] = {
                                null,
                                "no such job",
                                "ambiguous",
                                "argument must be %job or process id",
                                (char *) 0
                            };
INT32   j_systime, j_usrtime;   /* user and system time of last j_waitjed job */

static Job              *job_list;      /* job list */
static Job              *last_job;
static Job              *async_job;
static pid_t            async_pid;

static int              nzombie;        /* # of zombies owned by this process */
static INT32            njobs;          /* # of jobs started */
static int              child_max;      /* CHILD_MAX */

static void             j_set_async ARGS((Job *j));
static void             j_startjob ARGS((Job *j));
static int              j_waitj ARGS((Job *j, int flags, const char *where));
static void     j_sigchld ARGS((int sig));
static void             j_print ARGS((Job *j, int how, struct shf *shf));
static Job              *j_lookup ARGS((const char *cp, int *ecodep));
static Job              *new_job ARGS((void));
static Proc             *new_proc ARGS((void));
static void             check_job ARGS((Job *j));
static void             put_job ARGS((Job *j, int where));
static void             remove_job ARGS((Job *j, const char *where));
static int              kill_job ARGS((Job *j, int sig));

/* initialize job control */
void
j_init(mflagset)
        int mflagset;
{
        child_max = CHILD_MAX; /* so syscon() isn't always being called */

        /* Make sure SIGCHLD isn't ignored - can do odd things under SYSV */
        setsig(&sigtraps[SIGCHLD], SIG_DFL, SS_RESTORE_ORIG|SS_FORCE);

          if (Flag(FTALKING))
                tty_init(TRUE);
}

/* job cleanup before shell exit */
void
j_exit()
{
        /* kill stopped, and possibly running, jobs */
        Job     *j;
        int     killed = 0;

        for (j = job_list; j != (Job *) 0; j = j->next) {
                if (j->ppid == procpid
                    && (j->state == PSTOPPED
                        || (j->state == PRUNNING
                            && (j->flags & JF_FG))))
                {
                        killed = 1;
                        if (j->pgrp == 0)
                                kill_job(j, SIGHUP);
                        else
                                killpg(j->pgrp, SIGHUP);
                }
        }
        if (killed)
                sleep(1);
        j_notify();
}

/* execute tree in child subprocess */

#ifndef AMIGA
int
exchild(t, flags, close_fd)
        struct op       *t;
        int             flags;
        int             close_fd;       /* used if XPCLOSE or XCCLOSE */
{

        static Proc     *last_proc;     /* for pipelines */

        int             i;
        Proc            *p;
        Job             *j;
        int             rv = 0;
        int             forksleep;
        int             ischild;

        if (flags & XEXEC)
                /* Clear XFORK|XPCLOSE|XCCLOSE|XCOPROC|XPIPEO|XPIPEI|XXCOM|XBGND
                 * (also done in another execute() below)
                 */
                return execute(t, flags & (XEXEC | XERROK));

        p = new_proc();
        p->next = (Proc *) 0;
        p->state = PRUNNING;
        WSTATUS(p->status) = 0;
        p->pid = 0;

        /* link process into jobs list */
        if (flags&XPIPEI) {     /* continuing with a pipe */
                if (!last_job)
                        internal_errorf(1, "exchild: XPIPEI and no last_job - pid %d", (int) procpid);
                j = last_job;
                last_proc->next = p;
                last_proc = p;
        } else {
                j = new_job(); /* fills in j->job */
                /* we don't consider XXCOM's foreground since they don't get
                 * tty process group and we don't save or restore tty modes.
                 */
                j->flags = (flags & XXCOM) ? JF_XXCOM
                        : ((flags & XBGND) ? 0 : (JF_FG|JF_USETTYMODE));
                j->usrtime = j->systime = 0;
                j->state = PRUNNING;
                j->pgrp = 0;
                j->ppid = procpid;
                j->age = ++njobs;
                j->proc_list = p;
#ifdef KSH
                j->coproc_id = 0;
#endif /* KSH */
                last_job = j;
                last_proc = p;
                put_job(j, PJ_PAST_STOPPED);
        }

        snptreef(p->command, sizeof(p->command), "%T", t);

        /* create child process */
        forksleep = 1;
        while ((i = fork()) < 0 && errno == EAGAIN && forksleep < 32) {
                if (intrsig)     /* allow user to ^C out... */
                        break;
                sleep(forksleep);
                forksleep <<= 1;
        }
        if (i < 0) {
                kill_job(j, SIGKILL);
                remove_job(j, "fork failed");
                errorf("cannot fork - try again");
        }
        ischild = i == 0;
        if (ischild)
                p->pid = procpid = getpid();
        else
                p->pid = i;

        /* used to close pipe input fd */
        if (close_fd >= 0 && (((flags & XPCLOSE) && !ischild)
                              || ((flags & XCCLOSE) && ischild)))
                close(close_fd);
        if (ischild) {          /* child */
#ifdef KSH
                /* Do this before restoring signal */
                if (flags & XCOPROC)
                        coproc_cleanup(FALSE);
#endif /* KSH */
                cleanup_parents_env();
                if ((flags & XBGND) {
                        setsig(&sigtraps[SIGINT], SIG_IGN,
                                SS_RESTORE_IGN|SS_FORCE);
                        setsig(&sigtraps[SIGQUIT], SIG_IGN,
                                SS_RESTORE_IGN|SS_FORCE);
                        if (!(flags & (XPIPEI | XCOPROC))) {
                                int fd = open("/dev/null", 0);
                                (void) ksh_dup2(fd, 0, TRUE);
                                close(fd);
                        }
                }
                remove_job(j, "child"); /* in case of `jobs` command */
                nzombie = 0;
                Flag(FTALKING) = 0;
                tty_close();
                cleartraps();
                execute(t, (flags & XERROK) | XEXEC); /* no return */
                internal_errorf(0, "exchild: execute() returned");
                unwind(LLEAVE);
                /* NOTREACHED */
        }

        /* shell (parent) stuff */
        /* Ensure next child gets a (slightly) different $RANDOM sequence */
        change_random();
        if (!(flags & XPIPEO)) {        /* last process in a job */
                j_startjob(j);
#ifdef KSH
                if (flags & XCOPROC) {
                        j->coproc_id = coproc.id;
                        coproc.njobs++; /* n jobs using co-process output */
                        coproc.job = (void *) j; /* j using co-process input */
                }
#endif /* KSH */
                if (flags & XBGND) {
                        j_set_async(j);
                        if (Flag(FTALKING)) {
                                shf_fprintf(shl_out, "[%d]", j->job);
                                for (p = j->proc_list; p; p = p->next)
                                        shf_fprintf(shl_out, " %d", p->pid);
                                shf_putchar('\n', shl_out);
                                shf_flush(shl_out);
                        }
                } else
                        rv = j_waitj(j, JW_NONE, "jw:last proc");
        }

        return rv;
}
#endif

/* start the last job: only used for `command` jobs */
void
startlast()
{
        if (last_job) { /* no need to report error - waitlast() will do it */
                /* ensure it isn't removed by check_job() */
                last_job->flags |= JF_WAITING;
                j_startjob(last_job);
        }
}

/* wait for last job: only used for `command` jobs */
int
waitlast()
{
        int     rv;
        Job     *j;

        j = last_job;
        if (!j || !(j->flags & JF_STARTED)) {
                if (!j)
                        warningf(TRUE, "waitlast: no last job");
                else
                        internal_errorf(0, "waitlast: not started");
                return 125; /* not so arbitrary, non-zero value */
        }

        rv = j_waitj(j, JW_NONE, "jw:waitlast");

        return rv;
}

/* wait for child, interruptable. */
int
waitfor(cp, sigp)
        const char *cp;
        int     *sigp;
{
        int     rv;
        Job     *j;
        int     ecode;
        int     flags = JW_INTERRUPT|JW_ASYNCNOTIFY;

        *sigp = 0;

        if (cp == (char *) 0) {
                /* wait for an unspecified job - always returns 0, so
                 * don't have to worry about exited/signaled jobs
                 */
                for (j = job_list; j; j = j->next)
                        /* at&t ksh will wait for stopped jobs - we don't */
                        if (j->ppid == procpid && j->state == PRUNNING)
                                break;
                if (!j) {
                        return -1;
                }
        } else if ((j = j_lookup(cp, &ecode))) {
                /* don't report normal job completion */
                flags &= ~JW_ASYNCNOTIFY;
                if (j->ppid != procpid) {
                        return -1;
                }
        } else {
                if (ecode != JL_NOSUCH)
                        bi_errorf("%s: %s", cp, lookup_msgs[ecode]);
                return -1;
        }

        /* at&t ksh will wait for stopped jobs - we don't */
        rv = j_waitj(j, flags, "jw:waitfor");

        if (rv < 0) /* we were interrupted */
                *sigp = 128 + -rv;

        return rv;
}

/* kill (built-in) a job */
int
j_kill(cp, sig)
        const char *cp;
        int     sig;
{
        Job     *j;
        Proc    *p;
        int     rv = 0;
        int     ecode;

        if ((j = j_lookup(cp, &ecode)) == (Job *) 0) {
                bi_errorf("%s: %s", cp, lookup_msgs[ecode]);
                return 1;
        }

        if (j->pgrp == 0) {
                if (kill_job(j, sig) < 0) {
                        bi_errorf("%s: %s", cp, strerror(errno));
                        rv = 1;
                }
        } else {
                if (killpg(j->pgrp, sig) < 0) {
                        bi_errorf("%s: %s", cp, strerror(errno));
                        rv = 1;
                }
        }
        return rv;
}

/* are there any running or stopped jobs ? */
int
j_stopped_running()
{
        Job     *j;
        int     which = 0;

        for (j = job_list; j != (Job *) 0; j = j->next) {
        }
        if (which) {
                shellf("You have %s%s%s jobs\n",
                        which & 1 ? "stopped" : "",
                        which == 3 ? " and " : "",
                        which & 2 ? "running" : "");
                return 1;
        }

        return 0;
}

/* list jobs for jobs built-in */
int
j_jobs(cp, slp, nflag)
        const char *cp;
        int     slp;            /* 0: short, 1: long, 2: pgrp */
        int     nflag;
{
        Job     *j, *tmp;
        int     how;
        int     zflag = 0;

        if (nflag < 0) { /* kludge: print zombies */
                nflag = 0;
                zflag = 1;
        }
        if (cp) {
                int     ecode;

                if ((j = j_lookup(cp, &ecode)) == (Job *) 0) {
                        bi_errorf("%s: %s", cp, lookup_msgs[ecode]);
                        return 1;
                }
        } else
                j = job_list;
        how = slp == 0 ? JP_MEDIUM : (slp == 1 ? JP_LONG : JP_PGRP);
        for (; j; j = j->next) {
                if ((!(j->flags & JF_ZOMBIE) || zflag)
                    && (!nflag || (j->flags & JF_CHANGED)))
                {
                        j_print(j, how, shl_stdout);
                        if (j->state == PEXITED || j->state == PSIGNALLED)
                                j->flags |= JF_REMOVE;
                }
                if (cp)
                        break;
        }
        /* Remove jobs after printing so there won't be multiple + or - jobs */
        for (j = job_list; j; j = tmp) {
                tmp = j->next;
                if (j->flags & JF_REMOVE)
                        remove_job(j, "jobs");
        }
        return 0;
}

/* list jobs for top-level notification */
void
j_notify()
{
        Job     *j, *tmp;
        for (j = job_list; j; j = j->next) {
                /* Remove job after doing reports so there aren't
                 * multiple +/- jobs.
                 */
                if (j->state == PEXITED || j->state == PSIGNALLED)
                        j->flags |= JF_REMOVE;
        }
        for (j = job_list; j; j = tmp) {
                tmp = j->next;
                if (j->flags & JF_REMOVE)
                        remove_job(j, "notify");
        }
        shf_flush(shl_out);
}

/* Return pid of last process in last asynchornous job */
pid_t
j_async()
{
        if (async_job)
                async_job->flags |= JF_KNOWN;

        return async_pid;
}

/* Make j the last async process
 *
 * If jobs are compiled in then this routine expects sigchld to be blocked.
 */
static void
j_set_async(j)
        Job *j;
{
        Job     *jl, *oldest;

        if (async_job && (async_job->flags & (JF_KNOWN|JF_ZOMBIE)) == JF_ZOMBIE)
                remove_job(async_job, "async");
        if (!(j->flags & JF_STARTED)) {
                internal_errorf(0, "j_async: job not started");
                return;
        }
        async_job = j;
        async_pid = j->last_proc->pid;
        while (nzombie > child_max) {
                oldest = (Job *) 0;
                for (jl = job_list; jl; jl = jl->next)
                        if (jl != async_job && (jl->flags & JF_ZOMBIE)
                            && (!oldest || jl->age < oldest->age))
                                oldest = jl;
                if (!oldest) {
                        /* XXX debugging */
                        if (!(async_job->flags & JF_ZOMBIE) || nzombie != 1) {
                                internal_errorf(0, "j_async: bad nzombie (%d)", nzombie);
                                nzombie = 0;
                        }
                        break;
                }
                remove_job(oldest, "zombie");
        }
}

/* Start a job: set STARTED, check for held signals and set j->last_proc
 *
 * If jobs are compiled in then this routine expects sigchld to be blocked.
 */
static void
j_startjob(j)
        Job *j;
{
        Proc    *p;

        j->flags |= JF_STARTED;
        for (p = j->proc_list; p->next; p = p->next)
                ;
        j->last_proc = p;
}

/*
 * wait for job to complete or change state
 *
 * If jobs are compiled in then this routine expects sigchld to be blocked.
 */
static int
j_waitj(j, flags, where)
        Job     *j;
        int     flags;          /* see JW_* */
        const char *where;
{
        int     rv;

        /*
         * No auto-notify on the job we are waiting on.
         */
        j->flags |= JF_WAITING;
        if (flags & JW_ASYNCNOTIFY)
                j->flags |= JF_W_ASYNCNOTIFY;

                flags |= JW_STOPPEDWAIT;

        while ((volatile int) j->state == PRUNNING
                || ((flags & JW_STOPPEDWAIT)
                    && (volatile int) j->state == PSTOPPED))
        {
                j_sigchld(SIGCHLD);
                if (fatal_trap) {
                        int oldf = j->flags & (JF_WAITING|JF_W_ASYNCNOTIFY);
                        j->flags &= ~(JF_WAITING|JF_W_ASYNCNOTIFY);
                        runtraps(TF_FATAL);
                        j->flags |= oldf; /* not reached... */
                }
                if ((flags & JW_INTERRUPT) && (rv = trap_pending())) {
                        j->flags &= ~(JF_WAITING|JF_W_ASYNCNOTIFY);
                        return -rv;
                }
        }
        j->flags &= ~(JF_WAITING|JF_W_ASYNCNOTIFY);

        if (j->flags & JF_FG) {
                WAIT_T  status;

                j->flags &= ~JF_FG;
                if (tty_fd >= 0) {
                        /* Only restore tty settings if job was originally
                         * started in the foreground.  Problems can be
                         * caused by things like `more foobar &' which will
                         * typically get and save the shell's vi/emacs tty
                         * settings before setting up the tty for itself;
                         * when more exits, it restores the `original'
                         * settings, and things go down hill from there...
                         */
                        if (j->state == PEXITED && j->status == 0
                            && (j->flags & JF_USETTYMODE))
                        {
                                get_tty(tty_fd, &tty_state);
                        } else {
                                set_tty(tty_fd, &tty_state,
                                    (j->state == PEXITED) ? 0 : TF_MIPSKLUDGE);
                                /* Don't use tty mode if job is stopped and
                                 * later restarted and exits.  Consider
                                 * the sequence:
                                 *      vi foo (stopped)
                                 *      ...
                                 *      stty something
                                 *      ...
                                 *      fg (vi; ZZ)
                                 * mode should be that of the stty, not what
                                 * was before the vi started.
                                 */
                                if (j->state == PSTOPPED)
                                        j->flags &= ~JF_USETTYMODE;
                        }
                }
        }

        j_usrtime = j->usrtime;
        j_systime = j->systime;
        rv = j->status;

        if (!(flags & JW_ASYNCNOTIFY) 
            && (j->state != PSTOPPED))
        {
                j_print(j, JP_SHORT, shl_out);
                shf_flush(shl_out);
        }
        if (j->state != PSTOPPED
            && (!(flags & JW_ASYNCNOTIFY)))
                remove_job(j, where);

        return rv;
}

/* SIGCHLD handler to reap children and update job states
 *
 * If jobs are compiled in then this routine expects sigchld to be blocked.
 */
static void
j_sigchld(sig)
        int     sig;
{
        int             errno_ = errno;
        Job             *j;
        Proc            UNINITIALIZED(*p);
        int             pid;
        WAIT_T          status;
        struct tms      t0, t1;

        ksh_times(&t0);
        do {
                pid = wait(&status);

                if (pid <= 0)   /* return if would block (0) ... */
                        break;  /* ... or no children or interrupted (-1) */

                ksh_times(&t1);

                /* find job and process structures for this pid */
                for (j = job_list; j != (Job *) 0; j = j->next)
                        for (p = j->proc_list; p != (Proc *) 0; p = p->next)
                                if (p->pid == pid)
                                        goto found;
found:
                if (j == (Job *) 0) {
                        /* Can occur if process has kids, then execs shell
                        warningf(TRUE, "bad process waited for (pid = %d)",
                                pid);
                         */
                        t0 = t1;
                        continue;
                }

                j->usrtime += t1.tms_cutime - t0.tms_cutime;
                j->systime += t1.tms_cstime - t0.tms_cstime;
                t0 = t1;
                p->status = status;
                if (WIFSIGNALED(status))
                        p->state = PSIGNALLED;
                else
                        p->state = PEXITED;

                check_job(j);   /* check to see if entire job is done */
        }
        while (0);

        errno = errno_;

        return;
}

/*
 * Called only when a process in j has exited/stopped (ie, called only
 * from j_sigchld()).  If no processes are running, the job status
 * and state are updated, asynchronous job notification is done and,
 * if unneeded, the job is removed.
 *
 * If jobs are compiled in then this routine expects sigchld to be blocked.
 */
static void
check_job(j)
        Job     *j;
{
        int     jstate;
        Proc    *p;

        /* XXX debugging (nasty - interrupt routine using shl_out) */
        if (!(j->flags & JF_STARTED)) {
                internal_errorf(0, "check_job: job started (flags 0x%x)",
                        j->flags);
                return;
        }

        jstate = PRUNNING;
        for (p=j->proc_list; p != (Proc *) 0; p = p->next) {
                if (p->state == PRUNNING)
                        return; /* some processes still running */
                if (p->state > jstate)
                        jstate = p->state;
        }
        j->state = jstate;

        switch (j->last_proc->state) {
        case PEXITED:
                j->status = WEXITSTATUS(j->last_proc->status);
                break;
        case PSIGNALLED:
                j->status = 128 + WTERMSIG(j->last_proc->status);
                break;
        default:
                j->status = 0;
                break;
        }

#ifdef KSH
        /* Note when co-process dies: can't be done in j_wait() nor
         * remove_job() since neither may be called for non-interactive 
         * shells.
         */
        if (j->state == PEXITED || j->state == PSIGNALLED) {
                /* No need to keep co-process input any more
                 * (at leasst, this is what ksh93d thinks)
                 */
                if (coproc.job == j) {
                        coproc.job = (void *) 0;
                        /* XXX would be nice to get the closes out of here
                         * so they aren't done in the signal handler.
                         * Would mean a check in coproc_getfd() to
                         * do "if job == 0 && write >= 0, close write".
                         */
                        coproc_write_close(coproc.write);
                }
                /* Do we need to keep the output? */
                if (j->coproc_id && j->coproc_id == coproc.id
                    && --coproc.njobs == 0)
                        coproc_readw_close(coproc.read);
        }
#endif /* KSH */

        j->flags |= JF_CHANGED;
        if (!(j->flags & (JF_WAITING|JF_FG))
            && j->state != PSTOPPED)
        {
                if (j == async_job || (j->flags & JF_KNOWN)) {
                        j->flags |= JF_ZOMBIE;
                        j->job = -1;
                        nzombie++;
                } else
                        remove_job(j, "checkjob");
        }
}

/*
 * Print job status in either short, medium or long format.
 *
 * If jobs are compiled in then this routine expects sigchld to be blocked.
 */
static void
j_print(j, how, shf)
        Job             *j;
        int             how;
        struct shf      *shf;
{
        Proc    *p;
        int     state;
        WAIT_T  status;
        int     coredumped;
        char    jobchar = ' ';
        char    buf[64];
        const char *filler;
        int     output = 0;

        if (how == JP_PGRP) {
                /* POSIX doesn't say what to do it there is no process
                 * group leader.  We arbitrarily return
                 * last pid (which is what $! returns).
                 */
                shf_fprintf(shf, "%d\n", j->pgrp ? j->pgrp
                                : (j->last_proc ? j->last_proc->pid : 0));
                return;
        }
        j->flags &= ~JF_CHANGED;
        filler = j->job > 10 ?  "\n       " : "\n      ";
        if (j == job_list)
                jobchar = '+';
        else if (j == job_list->next)
                jobchar = '-';

        for (p = j->proc_list; p != (Proc *) 0;) {
                coredumped = 0;
                switch (p->state) {
                case PRUNNING:
                        strcpy(buf, "Running");
                        break;
                case PSTOPPED:
                        strcpy(buf, sigtraps[WSTOPSIG(p->status)].mess);
                        break;
                case PEXITED:
                        if (how == JP_SHORT)
                                buf[0] = '\0';
                        else if (WEXITSTATUS(p->status) == 0)
                                strcpy(buf, "Done");
                        else
                                shf_snprintf(buf, sizeof(buf), "Done (%d)",
                                        WEXITSTATUS(p->status));
                        break;
                case PSIGNALLED:
                        if (WIFCORED(p->status))
                                coredumped = 1;
                        /* kludge for not reporting `normal termination signals'
                         * (ie, SIGINT, SIGPIPE)
                         */
                        if (how == JP_SHORT && !coredumped
                            && (WTERMSIG(p->status) == SIGINT
                                || WTERMSIG(p->status) == SIGPIPE)) {
                                buf[0] = '\0';
                        } else
                                strcpy(buf, sigtraps[WTERMSIG(p->status)].mess);
                        break;
                }

                if (how != JP_SHORT) {
                        if (p == j->proc_list)
                                shf_fprintf(shf, "[%d] %c ", j->job, jobchar);
                        else
                                shf_fprintf(shf, "%s", filler);
                }

                if (how == JP_LONG)
                        shf_fprintf(shf, "%5d ", p->pid);

                if (how == JP_SHORT) {
                        if (buf[0]) {
                                output = 1;
                                shf_fprintf(shf, "%s%s ",
                                        buf, coredumped ? " (core dumped)" : null);
                        }
                } else {
                        output = 1;
                        shf_fprintf(shf, "%-20s %s%s%s", buf, p->command,
                                p->next ? "|" : null,
                                coredumped ? " (core dumped)" : null);
                }

                state = p->state;
                status = p->status;
                p = p->next;
                while (p && p->state == state
                       && WSTATUS(p->status) == WSTATUS(status))
                {
                        if (how == JP_LONG)
                                shf_fprintf(shf, "%s%5d %-20s %s%s", filler, p->pid,
                                        space, p->command, p->next ? "|" : null);
                        else if (how == JP_MEDIUM)
                                shf_fprintf(shf, " %s%s", p->command,
                                        p->next ? "|" : null);
                        p = p->next;
                }
        }
        if (output)
                shf_fprintf(shf, newline);
}

/* Convert % sequence to job
 *
 * If jobs are compiled in then this routine expects sigchld to be blocked.
 */
static Job *
j_lookup(cp, ecodep)
        const char *cp;
        int     *ecodep;
{
        Job             *j, *last_match;
        Proc            *p;
        int             len, job = 0;

        if (digit(*cp)) {
                job = atoi(cp);
                /* Look for last_proc->pid (what $! returns) first... */
                for (j = job_list; j != (Job *) 0; j = j->next)
                        if (j->last_proc && j->last_proc->pid == job)
                                return j;
                /* ...then look for process group (this is non-POSIX),
                 * but should not break anything (so FPOSIX isn't used).
                 */
                for (j = job_list; j != (Job *) 0; j = j->next)
                        if (j->pgrp && j->pgrp == job)
                                return j;
                if (ecodep)
                        *ecodep = JL_NOSUCH;
                return (Job *) 0;
        }
        if (*cp != '%') {
                if (ecodep)
                        *ecodep = JL_INVALID;
                return (Job *) 0;
        }
        switch (*++cp) {
          case '\0': /* non-standard */
          case '+':
          case '%':
                if (job_list != (Job *) 0)
                        return job_list;
                break;

          case '-':
                if (job_list != (Job *) 0 && job_list->next)
                        return job_list->next;
                break;

          case '0': case '1': case '2': case '3': case '4':
          case '5': case '6': case '7': case '8': case '9':
                job = atoi(cp);
                for (j = job_list; j != (Job *) 0; j = j->next)
                        if (j->job == job)
                                return j;
                break;

          case '?':             /* %?string */
                last_match = (Job *) 0;
                for (j = job_list; j != (Job *) 0; j = j->next)
                        for (p = j->proc_list; p != (Proc *) 0; p = p->next)
                                if (strstr(p->command, cp+1) != (char *) 0) {
                                        if (last_match) {
                                                if (ecodep)
                                                        *ecodep = JL_AMBIG;
                                                return (Job *) 0;
                                        }
                                        last_match = j;
                                }
                if (last_match)
                        return last_match;
                break;

          default:              /* %string */
                len = strlen(cp);
                last_match = (Job *) 0;
                for (j = job_list; j != (Job *) 0; j = j->next)
                        if (strncmp(cp, j->proc_list->command, len) == 0) {
                                if (last_match) {
                                        if (ecodep)
                                                *ecodep = JL_AMBIG;
                                        return (Job *) 0;
                                }
                                last_match = j;
                        }
                if (last_match)
                        return last_match;
                break;
        }
        if (ecodep)
                *ecodep = JL_NOSUCH;
        return (Job *) 0;
}

static Job      *free_jobs;
static Proc     *free_procs;

/* allocate a new job and fill in the job number.
 *
 * If jobs are compiled in then this routine expects sigchld to be blocked.
 */
static Job *
new_job()
{
        int     i;
        Job     *newj, *j;

        if (free_jobs != (Job *) 0) {
                newj = free_jobs;
                free_jobs = free_jobs->next;
        } else
                newj = (Job *) alloc(sizeof(Job), APERM);

        /* brute force method */
        for (i = 1; ; i++) {
                for (j = job_list; j && j->job != i; j = j->next)
                        ;
                if (j == (Job *) 0)
                        break;
        }
        newj->job = i;

        return newj;
}

/* Allocate new process strut
 *
 * If jobs are compiled in then this routine expects sigchld to be blocked.
 */
static Proc *
new_proc()
{
        Proc    *p;

        if (free_procs != (Proc *) 0) {
                p = free_procs;
                free_procs = free_procs->next;
        } else
                p = (Proc *) alloc(sizeof(Proc), APERM);

        return p;
}

/* Take job out of job_list and put old structures into free list.
 * Keeps nzombies, last_job and async_job up to date.
 *
 * If jobs are compiled in then this routine expects sigchld to be blocked.
 */
static void
remove_job(j, where)
        Job     *j;
        const char *where;
{
        Proc    *p, *tmp;
        Job     **prev, *curr;

        prev = &job_list;
        curr = *prev;
        for (; curr != (Job *) 0 && curr != j; prev = &curr->next, curr = *prev)
                ;
        if (curr != j) {
                internal_errorf(0, "remove_job: job not found (%s)", where);
                return;
        }
        *prev = curr->next;

        /* free up proc structures */
        for (p = j->proc_list; p != (Proc *) 0; ) {
                tmp = p;
                p = p->next;
                tmp->next = free_procs;
                free_procs = tmp;
        }

        if ((j->flags & JF_ZOMBIE) && j->ppid == procpid)
                --nzombie;
        j->next = free_jobs;
        free_jobs = j;

        if (j == last_job)
                last_job = (Job *) 0;
        if (j == async_job)
                async_job = (Job *) 0;
}

/* put j in a particular location (taking it out job_list if it is there
 * already)
 *
 * If jobs are compiled in then this routine expects sigchld to be blocked.
 */
static void
put_job(j, where)
        Job     *j;
        int     where;
{
        Job     **prev, *curr;

        /* Remove job from list (if there) */
        prev = &job_list;
        curr = job_list;
        for (; curr && curr != j; prev = &curr->next, curr = *prev)
                ;
        if (curr == j)
                *prev = curr->next;

        switch (where) {
        case PJ_ON_FRONT:
                j->next = job_list;
                job_list = j;
                break;

        case PJ_PAST_STOPPED:
                prev = &job_list;
                curr = job_list;
                for (; curr && curr->state == PSTOPPED; prev = &curr->next,
                                                        curr = *prev)
                        ;
                j->next = curr;
                *prev = j;
                break;
        }
}

/* nuke a job (called when unable to start full job).
 *
 * If jobs are compiled in then this routine expects sigchld to be blocked.
 */
static int
kill_job(j, sig)
        Job     *j;
        int     sig;
{
        Proc    *p;
        int     rval = 0;

        for (p = j->proc_list; p != (Proc *) 0; p = p->next)
                if (p->pid != 0)
                        if (kill(p->pid, sig) < 0)
                                rval = -1;
        return rval;

}
