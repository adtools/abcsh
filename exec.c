/*
 * execute command tree
 */

#include "sh.h"
#include "c_test.h"
#include <ctype.h>
#include "ksh_stat.h"

/* Does ps4 get parameter substitutions done? */
#ifdef KSH
# define PS4_SUBSTITUTE(s)      substitute((s), 0)
#else
# define PS4_SUBSTITUTE(s)      (s)
#endif /* KSH */

static int      comexec  ARGS((struct op *t, struct tbl *volatile tp, char **ap,
                              int volatile flags));
static void     scriptexec ARGS((struct op *tp, char **ap));
static int      call_builtin ARGS((struct tbl *tp, char **wp));
static int      iosetup ARGS((struct ioword *iop, struct tbl *tp));
static int      herein ARGS((const char *content, int sub));
#ifdef KSH
static char     *do_selectargs ARGS((char **ap, bool_t print_menu));
#endif /* KSH */
#ifdef KSH
static int      dbteste_isa ARGS((Test_env *te, Test_meta meta));
static const char *dbteste_getopnd ARGS((Test_env *te, Test_op op,
                                         int do_eval));
static int      dbteste_eval ARGS((Test_env *te, Test_op op, const char *opnd1,
                                const char *opnd2, int do_eval));
static void     dbteste_error ARGS((Test_env *te, int offset, const char *msg));
#endif /* KSH */

#if (0)
void printflags(struct tbl *t);
#endif

/* this structure is used to store the current environment before executing */
/* a "subshell" or similar */

struct globals
{
    struct env *e;
    struct table *homedirs;
    struct table *taliases;
    struct table *aliases;
    Trap **sigtraps;
    void *path;
    Area *aperm;
    char *current_wd;
    int fd[NUFILE];

};

struct env *copyenv(struct globals *);
void restoreenv(struct globals *);

/*
 * handle systems that don't have F_SETFD
 */
#ifndef F_SETFD
# ifndef MAXFD
#   define  MAXFD 64
# endif
/* a bit field would be smaller, but this will work */
static char clexec_tab[MAXFD+1];
#endif

#if defined(AMIGA) && !defined(CLIBHACK)
extern int  amigain, amigaout;
#endif

/*
 * we now use this function always.
 */
int
fd_clexec(fd)
    int fd;
{
#ifndef F_SETFD
        if (fd >= 0 && fd < sizeof(clexec_tab)) {
                clexec_tab[fd] = 1;
                return 0;
        }
        return -1;
#else
        return fcntl(fd, F_SETFD, 1);
#endif
}


/*
 * execute command tree
 */
int
execute(t, flags)
        struct op * volatile t;
        volatile int flags;     /* if XEXEC don't fork */
{
        int i;
        volatile int rv = 0;
        int pv[2];
        char ** volatile ap;
        char *s, *cp;
        struct ioword **iowp;
        struct tbl *tp = NULL;
#if defined(AMIGA) && !defined(CLIBHACK)
        int savefd[2];
#endif
        if (t == NULL)
                return 0;

        /* Is this the end of a pipeline?  If so, we want to evaluate the
         * command arguments
        bool_t eval_done = FALSE;
        if ((flags&XFORK) && !(flags&XEXEC) && (flags&XPCLOSE)) {
                eval_done = TRUE;
                tp = eval_execute_args(t, &ap);
        }
         */
        if ((flags&XFORK) && !(flags&XEXEC) && t->type != TPIPE)
                return exchild(t, flags & ~XTIME, -1); /* run in sub-process */

        newenv(E_EXEC);
        if (trap)
                runtraps(0);

        if (t->type == TCOM) {
                /* Clear subst_exstat before argument expansion.  Used by
                 * null commands (see comexec() and c_eval()) and by c_set().
                 */
                subst_exstat = 0;

                current_lineno = t->lineno;     /* for $LINENO */

                /* POSIX says expand command words first, then redirections,
                 * and assignments last..
                 */
                ap = eval(t->args, t->u.evalflags | DOBLANK | DOGLOB | DOTILDE);
                if (flags & XTIME)
                        /* Allow option parsing (bizarre, but POSIX) */
                        timex_hook(t, &ap);
                if (Flag(FXTRACE) && ap[0]) {
                        shf_fprintf(shl_out, "%s",
                                PS4_SUBSTITUTE(str_val(global("PS4"))));
                        for (i = 0; ap[i]; i++)
                                shf_fprintf(shl_out, "%s%s", ap[i],
                                        ap[i + 1] ? space : newline);
                        shf_flush(shl_out);
                }
                if (ap[0])
                        tp = findcom(ap[0], FC_BI|FC_FUNC);
        }
        flags &= ~XTIME;

        if (t->ioact != NULL || t->type == TPIPE || t->type == TCOPROC) {
                e->savefd = (short *) alloc(sizeofN(short, NUFILE), ATEMP);
                /* initialize to not redirected */
                memset(e->savefd, 0, sizeofN(short, NUFILE));
        }

        /* do redirection, to be restored in quitenv() */
        if (t->ioact != NULL)
                for (iowp = t->ioact; *iowp != NULL; iowp++) {
                        if (iosetup(*iowp, tp) < 0) {
                                exstat = rv = 1;
                                /* Redirection failures for special commands
                                 * cause (non-interactive) shell to exit.
                                 */
                                if (tp && tp->type == CSHELL
                                    && (tp->flag & SPEC_BI))
                                        errorf(null);
                                /* Deal with FERREXIT, quitenv(), etc. */
                                goto Break;
                        }
                }

        switch(t->type) {
          case TCOM:
                rv = comexec(t, tp, ap, flags);
                break;

          case TPAREN:
          {
              //      rv = execute(t->left, flags|XFORK);
              /* save old enevironment */
              struct globals globenv;

              copyenv(&globenv);
              e->type = E_SUBSHELL;
              if(!(ksh_sigsetjmp(e->jbuf,0)))
              {
                  rv = execute(t->left, flags);
              }
              else
              {
                rv = exstat;
              }

              restoreenv(&globenv);
          }
                break;

          case TPIPE:
                flags |= XFORK;
                flags &= ~XEXEC;

#if defined(AMIGA) && !defined(CLIBHACK)
               savefd[0] = amigain;
               savefd[1] = amigaout;
#else
                e->savefd[0] = savefd(0, 0);
                (void) ksh_dup2(e->savefd[0], 0, FALSE); /* stdin of first */
                e->savefd[1] = savefd(1, 0);
#endif

                while (t->type == TPIPE) {
#if defined(AMIGA) && !defined(CLIBHACK)
                        pipe(pv);
                        amigaout = pv[1];
#else
                        openpipe(pv);
                        (void) ksh_dup2(pv[1], 1, FALSE); /* stdout of curr */
#endif
                        /* Let exchild() close pv[0] in child
                         * (if this isn't done, commands like
                         *    (: ; cat /etc/termcap) | sleep 1
                         *  will hang forever).
                         */
                        exchild(t->left, flags|XPIPEO|XCCLOSE, pv[0]);
#if defined(AMIGA) && !defined(CLIBHACK)
                        amigain = pv[0];
#else
                        (void) ksh_dup2(pv[0], 0, FALSE); /* stdin of next */
                        closepipe(pv);
#endif
                        flags |= XPIPEI;
                        t = t->right;
                }
#if defined(AMIGA) && !defined(CLIBHACK)
                amigaout = savefd[1];
#else
                restfd(1, e->savefd[1]); /* stdout of last */
                e->savefd[1] = 0; /* no need to re-restore this */
                /* Let exchild() close 0 in parent, after fork, before wait */
#endif
                i = exchild(t, flags|XPCLOSE, 0);
                if (!(flags&XBGND) && !(flags&XXCOM))
                        rv = i;
                break;

          case TLIST:
                while (t->type == TLIST) {
                        execute(t->left, flags & XERROK);
                        t = t->right;
                }
                rv = execute(t, flags & XERROK);
                break;

#ifdef KSH
          case TCOPROC:
          {
                /* Already have a (live) co-process? */
                if (coproc.job && coproc.write >= 0)
                        errorf("coprocess already exists");

                /* Can we re-use the existing co-process pipe? */
                coproc_cleanup(TRUE);

                /* do this before opening pipes, in case these fail */
                e->savefd[0] = savefd(0, 0);
                e->savefd[1] = savefd(1, 0);

                openpipe(pv);
                ksh_dup2(pv[0], 0, FALSE);
                close(pv[0]);
                coproc.write = pv[1];
                coproc.job = (void *) 0;

                if (coproc.readw >= 0)
                        ksh_dup2(coproc.readw, 1, FALSE);
                else {
                        openpipe(pv);
                        coproc.read = pv[0];
                        ksh_dup2(pv[1], 1, FALSE);
                        coproc.readw = pv[1];    /* closed before first read */
                        coproc.njobs = 0;
                        /* create new coprocess id */
                        ++coproc.id;
                }

                /* exchild() closes coproc.* in child after fork,
                 * will also increment coproc.njobs when the
                 * job is actually created.
                 */
                flags &= ~XEXEC;
                exchild(t->left, flags|XBGND|XFORK|XCOPROC|XCCLOSE,
                        coproc.readw);
                break;
          }
#endif /* KSH */

          case TASYNC:
                /* XXX non-optimal, I think - "(foo &)", forks for (),
                 * forks again for async...  parent should optimize
                 * this to "foo &"...
                 */
                rv = execute(t->left, (flags&~XEXEC)|XBGND|XFORK);
                break;

          case TOR:
          case TAND:
                rv = execute(t->left, XERROK);
                if ((t->right != NULL) && ((rv == 0) == (t->type == TAND)))
                        rv = execute(t->right, flags & XERROK);
                else
                        flags |= XERROK;
                break;

          case TBANG:
                rv = !execute(t->right, XERROK);
                break;

#ifdef KSH
          case TDBRACKET:
            {
                Test_env te;

                te.flags = TEF_DBRACKET;
                te.pos.wp = t->args;
                te.isa = dbteste_isa;
                te.getopnd = dbteste_getopnd;
                te.eval = dbteste_eval;
                te.error = dbteste_error;

                rv = test_parse(&te);
                break;
            }
#endif /* KSH */

          case TFOR:
#ifdef KSH
          case TSELECT:
            {
                volatile bool_t is_first = TRUE;
#endif /* KSH */
                ap = (t->vars != NULL) ?
                          eval(t->vars, DOBLANK|DOGLOB|DOTILDE)
                        : e->loc->argv + 1;
                e->type = E_LOOP;
                while (1) {
                        i = ksh_sigsetjmp(e->jbuf, 0);
                        if (!i)
                                break;
                        if ((e->flags&EF_BRKCONT_PASS)
                            || (i != LBREAK && i != LCONTIN))
                        {
                                quitenv();
                                unwind(i);
                        } else if (i == LBREAK) {
                                rv = 0;
                                goto Break;
                        }
                }
                rv = 0; /* in case of a continue */
                if (t->type == TFOR) {
                        while (*ap != NULL) {
                                setstr(global(t->str), *ap++, KSH_UNWIND_ERROR);
                                rv = execute(t->left, flags & XERROK);
                        }
                }
#ifdef KSH
                else { /* TSELECT */
                        for (;;) {
                                if (!(cp = do_selectargs(ap, is_first))) {
                                        rv = 1;
                                        break;
                                }
                                is_first = FALSE;
                                setstr(global(t->str), cp, KSH_UNWIND_ERROR);
                                rv = execute(t->left, flags & XERROK);
                        }
                }
            }
#endif /* KSH */
                break;

          case TWHILE:
          case TUNTIL:
                e->type = E_LOOP;
                while (1) {
                        i = ksh_sigsetjmp(e->jbuf, 0);
                        if (!i)
                                break;
                        if ((e->flags&EF_BRKCONT_PASS)
                            || (i != LBREAK && i != LCONTIN))
                        {
                                quitenv();
                                unwind(i);
                        } else if (i == LBREAK) {
                                rv = 0;
                                goto Break;
                        }
                }
                rv = 0; /* in case of a continue */
                while ((execute(t->left, XERROK) == 0) == (t->type == TWHILE))
                        rv = execute(t->right, flags & XERROK);
                break;

          case TIF:
          case TELIF:
                if (t->right == NULL)
                        break;  /* should be error */
                rv = execute(t->left, XERROK) == 0 ?
                        execute(t->right->left, flags & XERROK) :
                        execute(t->right->right, flags & XERROK);
                break;

          case TCASE:
                cp = evalstr(t->str, DOTILDE);
                for (t = t->left; t != NULL && t->type == TPAT; t = t->right)
                    for (ap = t->vars; *ap; ap++)
                        if ((s = evalstr(*ap, DOTILDE|DOPAT))
                            && gmatch(cp, s, FALSE))
                                goto Found;
                break;
          Found:
                rv = execute(t->left, flags & XERROK);
                break;

          case TBRACE:
                rv = execute(t->left, flags & XERROK);
                break;

          case TFUNCT:
                rv = define(t->str, t);
                break;

          case TTIME:
                /* Clear XEXEC so nested execute() call doesn't exit
                 * (allows "ls -l | time grep foo").
                 */
                rv = timex(t, flags & ~XEXEC);
                break;

          case TEXEC:           /* an eval'd TCOM */
                s = t->args[0];
                ap = makenv();
#ifndef F_SETFD
                for (i = 0; i < sizeof(clexec_tab); i++)
                        if (clexec_tab[i]) {
                                close(i);
                                clexec_tab[i] = 0;
                        }
#endif
                restoresigs();
                cleanup_proc_env();
                ksh_execve(t->str, t->args, ap, (flags & XINTACT) ? 1 : 0);
                if (errno == ENOEXEC)
                        scriptexec(t, ap);
#ifndef AMIGA
                else
                        errorf("%s: %s", s, strerror(errno));
#endif
        }
    Break:
        exstat = rv;

        quitenv();              /* restores IO */
        if ((flags&XEXEC))
                unwind(LEXIT);  /* exit child */
        if (rv != 0 && !(flags & XERROK)) {
                if (Flag(FERREXIT))
                        unwind(LERROR);
                trapsig(SIGERR_);
        }
        return rv;
}

/*
 * execute simple command
 */

static int
comexec(t, tp, ap, flags)
        struct op *t;
        struct tbl *volatile tp;
        register char **ap;
        int volatile flags;
{
        int i;
        int rv = 0;
        register char *cp;
        register char **lastp;
        static struct op texec; /* Must be static (XXX but why?) */
        int type_flags;
        int keepasn_ok;
        int fcflags = FC_BI|FC_FUNC|FC_PATH;

#ifdef KSH
        /* snag the last argument for $_ XXX not the same as at&t ksh,
         * which only seems to set $_ after a newline (but not in
         * functions/dot scripts, but in interactive and scipt) -
         * perhaps save last arg here and set it in shell()?.
         */

        if (Flag(FTALKING) && *(lastp = ap)) {
                struct tbl *tb;
                while (*++lastp);
                tb = typeset("_", LOCAL, 0,INTEGER,0);


                /* setstr() can't fail here */
                setstr(tb, *--lastp, KSH_RETURN_ERROR);
        }

#endif /* KSH */

        /* Deal with the shell builtins builtin, exec and command since
         * they can be followed by other commands.  This must be done before
         * we know if we should create a local block, which must be done
         * before we can do a path search (in case the assignments change
         * PATH).
         * Odd cases:
         *   FOO=bar exec > /dev/null           FOO is kept but not exported
         *   FOO=bar exec foobar                FOO is exported
         *   FOO=bar command exec > /dev/null   FOO is neither kept nor exported
         *   FOO=bar command                    FOO is neither kept nor exported
         *   PATH=... foobar                    use new PATH in foobar search
         */
        keepasn_ok = 1;
        while (tp && tp->type == CSHELL) {
                fcflags = FC_BI|FC_FUNC|FC_PATH;/* undo effects of command */
                if (tp->val.f == c_builtin) {
                        if ((cp = *++ap) == NULL) {
                                tp = NULL;
                                break;
                        }
                        tp = findcom(cp, FC_BI);
                        if (tp == NULL)
                                errorf("builtin: %s: not a builtin", cp);
                        continue;
                } else if (tp->val.f == c_exec) {
                        if (ap[1] == NULL)
                                break;
                        ap++;
                        flags |= XEXEC;
                } else if (tp->val.f == c_command) {
                        int optc, saw_p = 0;

                        /* Ugly dealing with options in two places (here and
                         * in c_command(), but such is life)
                         */
                        ksh_getopt_reset(&builtin_opt, 0);
                        while ((optc = ksh_getopt(ap, &builtin_opt, ":p"))
                                                                        == 'p')
                                saw_p = 1;
                        if (optc != EOF)
                                break;  /* command -vV or something */
                        /* don't look for functions */
                        fcflags = FC_BI|FC_PATH;
                        if (saw_p) {
                                fcflags |= FC_DEFPATH;
                        }
                        ap += builtin_opt.optind;
                        /* POSIX says special builtins lose their status
                         * if accessed using command.
                         */
                        keepasn_ok = 0;
                        if (!ap[0]) {
                                /* ensure command with no args exits with 0 */
                                subst_exstat = 0;
                                break;
                        }
                } else
                        break;
                tp = findcom(ap[0], fcflags & (FC_BI|FC_FUNC));
        }
        if (keepasn_ok && (!ap[0] || (tp && (tp->flag & KEEPASN))))
                type_flags = 0;
        else {
                /* create new variable/function block */
                newblock();
                /* ksh functions don't keep assignments, POSIX functions do. */
                if (keepasn_ok && tp && tp->type == CFUNC
                    && !(tp->flag & FKSH))
                        type_flags = 0;
                else
                        type_flags = LOCAL|LOCAL_COPY|EXPORTV;
        }
        if (Flag(FEXPORT))
                type_flags |= EXPORTV;
        for (i = 0; t->vars[i]; i++) {
                cp = evalstr(t->vars[i], DOASNTILDE);
                if (Flag(FXTRACE)) {
                        if (i == 0)
                                shf_fprintf(shl_out, "%s",
                                        PS4_SUBSTITUTE(str_val(global("PS4"))));
                        shf_fprintf(shl_out, "%s%s", cp,
                                t->vars[i + 1] ? space : newline);
                        if (!t->vars[i + 1])
                                shf_flush(shl_out);
                }
                typeset(cp, type_flags, 0, 0, 0);
        }

        if ((cp = *ap) == NULL) {
                rv = subst_exstat;
                goto Leave;
        } else if (!tp) {
                tp = findcom(cp, fcflags);
        }

        switch (tp->type) {
          case CSHELL:                  /* shell built-in */
                rv = call_builtin(tp, ap);
                break;

          case CFUNC:                   /* function call */
          {
                volatile int old_xflag;
                volatile Tflag old_inuse;
                const char *volatile old_kshname;

                if (!(tp->flag & ISSET)) {
                        struct tbl *ftp;

                        if (!tp->u.fpath) {
                                if (tp->u2.errno_) {
                                        warningf(TRUE,
                                "%s: can't find function definition file - %s",
                                                cp, strerror(tp->u2.errno_));
                                        rv = 126;
                                } else {
                                        warningf(TRUE,
                                "%s: can't find function definition file", cp);
                                        rv = 127;
                                }
                                break;
                        }
                        if (include(tp->u.fpath, 0, (char **) 0, 0) < 0) {
                                warningf(TRUE,
                            "%s: can't open function definition file %s - %s",
                                        cp, tp->u.fpath, strerror(errno));
                                rv = 127;
                                break;
                        }
                        if (!(ftp = findfunc(cp, hash(cp), FALSE))
                            || !(ftp->flag & ISSET))
                        {
                                warningf(TRUE,
                                        "%s: function not defined by %s",
                                        cp, tp->u.fpath);
                                rv = 127;
                                break;
                        }
                        tp = ftp;
                }

                /* ksh functions set $0 to function name, POSIX functions leave
                 * $0 unchanged.
                 */
                old_kshname = kshname;
                if (tp->flag & FKSH)
                        kshname = ap[0];
                else
                        ap[0] = (char *) kshname;
                e->loc->argv = ap;
                for (i = 0; *ap++ != NULL; i++)
                        ;
                e->loc->argc = i - 1;
                /* ksh-style functions handle getopts sanely,
                 * bourne/posix functions are insane...
                 */
                if (tp->flag & FKSH) {
                        e->loc->flags |= BF_DOGETOPTS;
                        e->loc->getopts_state = user_opt;
                        getopts_reset(1);
                }

                old_xflag = Flag(FXTRACE);
                Flag(FXTRACE) = tp->flag & TRACE ? TRUE : FALSE;

                old_inuse = tp->flag & FINUSE;
                tp->flag |= FINUSE;

                e->type = E_FUNC;
                i = ksh_sigsetjmp(e->jbuf, 0);
                if (i == 0) {
                        /* seems odd to pass XERROK here, but at&t ksh does */
                        exstat = execute(tp->val.t, flags & XERROK);
                        i = LRETURN;
                }
                kshname = old_kshname;
                Flag(FXTRACE) = old_xflag;
                tp->flag = (tp->flag & ~FINUSE) | old_inuse;
                /* Were we deleted while executing?  If so, free the execution
                 * tree.  todo: Unfortunately, the table entry is never re-used
                 * until the lookup table is expanded.
                 */
                if ((tp->flag & (FDELETE|FINUSE)) == FDELETE) {
                        if (tp->flag & ALLOC) {
                                tp->flag &= ~ALLOC;
                                tfree(tp->val.t, tp->areap);
                        }
                        tp->flag = 0;
                }
                switch (i) {
                  case LRETURN:
                  case LERROR:
                        rv = exstat;
                        break;
                  case LINTR:
                  case LEXIT:
                  case LLEAVE:
                  case LSHELL:
                        quitenv();
                        unwind(i);
                        /*NOTREACHED*/
                  default:
                        quitenv();
                        internal_errorf(1, "CFUNC %d", i);
                }
                break;
          }

          case CEXEC:           /* executable command */

          case CTALIAS:         /* tracked alias */
                if (!(tp->flag&ISSET)) {
                        /* errno_ will be set if the named command was found
                         * but could not be executed (permissions, no execute
                         * bit, directory, etc).  Print out a (hopefully)
                         * useful error message and set the exit status to 126.
                         */
                        if (tp->u2.errno_) {
                                warningf(TRUE, "%s: cannot execute - %s", cp,
                                        strerror(tp->u2.errno_));
                                rv = 126;       /* POSIX */
                        } else {
                                warningf(TRUE, "%s: not found", cp);
                                rv = 127;
                        }
                        break;
                }

#ifdef KSH
                /* set $_ to program's full path */
                /* setstr() can't fail here */
                {
                    struct tbl *tb;
                    tb = typeset("_",LOCAL|EXPORTV,0,INTEGER,0);
                    setstr(tb, tp->val.s, KSH_RETURN_ERROR);
                }
#endif /* KSH */

                if (flags&XEXEC) {
                        j_exit();
                        if (!(flags&XBGND)) {
                                setexecsig(&sigtraps[SIGINT], SS_RESTORE_ORIG);
                                setexecsig(&sigtraps[SIGQUIT], SS_RESTORE_ORIG);
                        }
                }

                /* to fork we set up a TEXEC node and call execute */
                texec.type = TEXEC;
                texec.left = t; /* for tprint */
                texec.str = tp->val.s;
                texec.args = ap;


                rv = exchild(&texec, flags, -1);
                break;
        }
  Leave:
        if (flags & XEXEC) {
                exstat = rv;
                unwind(LLEAVE);
        }
        return rv;
}

static void
scriptexec(tp, ap)
        register struct op *tp;
        register char **ap;
{
        char *shell;

        shell = str_val(global(EXECSHELL_STR));
        if (shell && *shell)
                shell = search(shell, path, X_OK, (int *) 0);
        if (!shell || !*shell)
                shell = EXECSHELL;

        *tp->args-- = tp->str;
        *tp->args = shell;

        ksh_execve(tp->args[0], tp->args, ap, 0);
        /* report both the program that was run and the bogus shell */
        errorf("%s: %s: %s", tp->str, shell, strerror(errno));
}

int
shcomexec(wp)
        register char **wp;
{
        register struct tbl *tp;

        tp = tsearch(&builtins, *wp, hash(*wp));
        if (tp == NULL)
                internal_errorf(1, "shcomexec: %s", *wp);
        return call_builtin(tp, wp);
}

/*
 * Search function tables for a function.  If create set, a table entry
 * is created if none is found.
 */
struct tbl *
findfunc(name, h, create)
        const char *name;
        unsigned int h;
        int create;
{
        struct block *l;
        struct tbl *tp = (struct tbl *) 0;

        for (l = e->loc; l; l = l->next) {
                tp = tsearch(&l->funs, name, h);
                if (tp)
                        break;
                if (!l->next && create) {
                        tp = tenter(&l->funs, name, h);
                        tp->flag = DEFINED;
                        tp->type = CFUNC;
                        tp->val.t = (struct op *) 0;
                        break;
                }
        }
        return tp;
}

/*
 * define function.  Returns 1 if function is being undefined (t == 0) and
 * function did not exist, returns 0 otherwise.
 */
int
define(name, t)
        const char *name;
        struct op *t;
{
        struct tbl *tp;
        int was_set = 0;

        while (1) {
                tp = findfunc(name, hash(name), TRUE);

                if (tp->flag & ISSET)
                        was_set = 1;
                /* If this function is currently being executed, we zap this
                 * table entry so findfunc() won't see it
                 */
                if (tp->flag & FINUSE) {
                        tp->name[0] = '\0';
                        tp->flag &= ~DEFINED; /* ensure it won't be found */
                        tp->flag |= FDELETE;
                } else
                        break;
        }

        if (tp->flag & ALLOC) {
                tp->flag &= ~(ISSET|ALLOC);
                tfree(tp->val.t, tp->areap);
        }

        if (t == NULL) {                /* undefine */
                tdelete(tp);
                return was_set ? 0 : 1;
        }

        tp->val.t = tcopy(t->left, tp->areap);
        tp->flag |= (ISSET|ALLOC);
        if (t->u.ksh_func)
                tp->flag |= FKSH;

        return 0;
}

/*
 * add builtin
 */
void
builtin(name, func)
        const char *name;
        int (*func) ARGS((char **));
{
        register struct tbl *tp;
        Tflag flag;

        /* see if any flags should be set for this builtin */
        for (flag = 0; ; name++) {
                if (*name == '=')       /* command does variable assignment */
                        flag |= KEEPASN;
                else if (*name == '*')  /* POSIX special builtin */
                        flag |= SPEC_BI;
                else if (*name == '+')  /* POSIX regular builtin */
                        flag |= REG_BI;
                else
                        break;
        }

        tp = tenter(&builtins, name, hash(name));
        tp->flag = DEFINED | flag;
        tp->type = CSHELL;
        tp->val.f = func;
}

/*
 * find command
 * either function, hashed command, or built-in (in that order)
 */
struct tbl *
findcom(name, flags)
        const char *name;
        int     flags;          /* FC_* */
{
        static struct tbl temp;
        unsigned int h = hash(name);
        struct tbl *tp = NULL, *tbi;
        int insert = Flag(FTRACKALL);   /* insert if not found */
        char *fpath;                    /* for function autoloading */
        char *npath;

        if (ksh_strchr_dirsep(name) != NULL) {
                insert = 0;
                /* prevent FPATH search below */
                flags &= ~FC_FUNC;
                goto Search;
        }
        tbi = (flags & FC_BI) ? tsearch(&builtins, name, h) : NULL;
        /* POSIX says special builtins first, then functions, then
         * POSIX regular builtins, then search path...
         */
        if ((flags & FC_SPECBI) && tbi && (tbi->flag & SPEC_BI))
                tp = tbi;
        if (!tp && (flags & FC_FUNC)) {
                tp = findfunc(name, h, FALSE);
                if (tp && !(tp->flag & ISSET)) {
                        if ((fpath = str_val(global("FPATH"))) == null) {
                                tp->u.fpath = (char *) 0;
                                tp->u2.errno_ = 0;
                        } else
                                tp->u.fpath = search(name, fpath, R_OK,
                                        &tp->u2.errno_);
                }
        }
        if (!tp && (flags & FC_REGBI) && tbi && (tbi->flag & REG_BI))
                tp = tbi;
        /* todo: posix says non-special/non-regular builtins must
         * be triggered by some user-controllable means like a
         * special directory in PATH.  Requires modifications to
         * the search() function.  Tracked aliases should be
         * modified to allow tracking of builtin commands.
         * This should be under control of the FPOSIX flag.
         * If this is changed, also change c_whence...
         */
        if (!tp && (flags & FC_UNREGBI) && tbi)
                tp = tbi;
        if (!tp && (flags & FC_PATH) && !(flags & FC_DEFPATH)) {
                tp = tsearch(taliases, name, h);
                if (tp && (tp->flag & ISSET) && eaccess(tp->val.s, X_OK) != 0) {
                        if (tp->flag & ALLOC) {
                                tp->flag &= ~ALLOC;
                                afree(tp->val.s, APERM);
                        }
                        tp->flag &= ~ISSET;
                }
        }

  Search:
        if ((!tp || (tp->type == CTALIAS && !(tp->flag&ISSET)))
            && (flags & FC_PATH))
        {
                if (!tp) {
                        if (insert && !(flags & FC_DEFPATH)) {
                                tp = tenter(taliases, name, h);
                                tp->type = CTALIAS;
                        } else {
                                tp = &temp;
                                tp->type = CEXEC;
                        }
                        tp->flag = DEFINED;     /* make ~ISSET */
                }
                npath = search(name, flags & FC_DEFPATH ? def_path : path,
                                X_OK, &tp->u2.errno_);
                if (npath) {
                        tp->val.s = tp == &temp ? npath : str_save(npath, APERM);
                        tp->flag |= ISSET|ALLOC;
                } else if ((flags & FC_FUNC)
                           && (fpath = str_val(global("FPATH"))) != null
                           && (npath = search(name, fpath, R_OK,
                                              &tp->u2.errno_)) != (char *) 0)
                {
                        /* An undocumented feature of at&t ksh is that it
                         * searches FPATH if a command is not found, even
                         * if the command hasn't been set up as an autoloaded
                         * function (ie, no typeset -uf).
                         */
                        tp = &temp;
                        tp->type = CFUNC;
                        tp->flag = DEFINED; /* make ~ISSET */
                        tp->u.fpath = npath;
                }
        }
        return tp;
}

/*
 * flush executable commands with relative paths
 */
void
flushcom(all)
        int all;                /* just relative or all */
{
        struct tbl *tp;
        struct tstate ts;

        for (twalk(&ts, taliases); (tp = tnext(&ts)) != NULL; )
                if ((tp->flag&ISSET) && (all || !ISDIRSEP(tp->val.s[0]))) {
                        if (tp->flag&ALLOC) {
                                tp->flag &= ~(ALLOC|ISSET);
                                afree(tp->val.s, APERM);
                        }
                        tp->flag &= ~ISSET;
                }
}

/* Check if path is something we want to find.  Returns -1 for failure. */
int
search_access(path, mode, errnop)
        const char *path;
        int mode;
        int *errnop;            /* set if candidate found, but not suitable */
{
        int ret, err = 0;
        struct stat statb;

        if (stat(path, &statb) < 0)
                return -1;
        ret = eaccess(path, mode);
        if (ret < 0)
                err = errno; /* File exists, but we can't access it */
        else if (mode == X_OK
                 && (!S_ISREG(statb.st_mode)
                     /* This 'cause access() says root can execute everything */
                     || !(statb.st_mode & (S_IXUSR|S_IXGRP|S_IXOTH))))
        {
                ret = -1;
                err = S_ISDIR(statb.st_mode) ? EISDIR : EACCES;
        }
        if (err && errnop && !*errnop)
                *errnop = err;
        return ret;
}

/*
 * search for command with PATH
 */
char *
search(name, path, mode, errnop)
        const char *name;
        const char *path;
        int mode;               /* R_OK or X_OK */
        int *errnop;            /* set if candidate found, but not suitable */
{
        const char *sp, *p;
        char *xp;
        XString xs;
        int namelen;

        if (errnop)
                *errnop = 0;
        if (ksh_strchr_dirsep(name)) {
                if (search_access(name, mode, errnop) == 0)
                {
#ifdef AMIGA
                    if((name[0] == '.') && (name[1] == '/'))
                        return (char *) &name[2];
#endif
                    return (char *) name;
                }
                return NULL;
        }

        namelen = strlen(name) + 1;
        Xinit(xs, xp, 128, ATEMP);

        sp = path;
        while (sp != NULL) {
                xp = Xstring(xs, xp);
                if (!(p = strchr(sp, PATHSEP)))
                        p = sp + strlen(sp);
                if (p != sp) {
                        XcheckN(xs, xp, p - sp);
                        memcpy(xp, sp, p - sp);
                        xp += p - sp;
                        *xp++ = DIRSEP;
                }
                sp = p;
                XcheckN(xs, xp, namelen);
                memcpy(xp, name, namelen);
                if (search_access(Xstring(xs, xp), mode, errnop) == 0)
                {
#ifdef AMIGA
                    sp = Xclose(xs, xp + namelen);
                    if((sp[0] == '.') && (sp[1] == '/') && !strcmp(&sp[2], name))
                    {
                        sp =strdup(name);
                        Xfree(xs, xp);
                    }
                    return (char *) sp;
#else
                        return Xclose(xs, xp + namelen);
#endif
                }
                if (*sp++ == '\0')
                        sp = NULL;
        }
        Xfree(xs, xp);
        return NULL;
}

static int
call_builtin(tp, wp)
        struct tbl *tp;
        char **wp;
{
        int rv;

        builtin_argv0 = wp[0];
        builtin_flag = tp->flag;
        shf_reopen(1, SHF_WR, shl_stdout);
        shl_stdout_ok = 1;
        ksh_getopt_reset(&builtin_opt, GF_ERROR);
        rv = (*tp->val.f)(wp);
        shf_flush(shl_stdout);
        shl_stdout_ok = 0;
        builtin_flag = 0;
        builtin_argv0 = (char *) 0;
        return rv;
}

/*
 * set up redirection, saving old fd's in e->savefd
 */
static int
iosetup(iop, tp)
        register struct ioword *iop;
        struct tbl *tp;
{
        register int u = -1;
        char *cp = iop->name;
        int iotype = iop->flag & IOTYPE;
        int do_open = 1, do_close = 0, UNINITIALIZED(flags);
        struct ioword iotmp;
        struct stat statb;

        if (iotype != IOHERE)
                cp = evalonestr(cp, DOTILDE|(Flag(FTALKING_I) ? DOGLOB : 0));

        /* Used for tracing and error messages to print expanded cp */
        iotmp = *iop;
        iotmp.name = (iotype == IOHERE) ? (char *) 0 : cp;
        iotmp.flag |= IONAMEXP;

        if (Flag(FXTRACE))
                shellf("%s%s\n",
                        PS4_SUBSTITUTE(str_val(global("PS4"))),
                        snptreef((char *) 0, 32, "%R", &iotmp));

        switch (iotype) {
          case IOREAD:
                flags = O_RDONLY;
                break;

          case IOCAT:
                flags = O_WRONLY | O_APPEND | O_CREAT;
                break;

          case IOWRITE:
                flags = O_WRONLY | O_CREAT | O_TRUNC;
                /* The stat() is here to allow redirections to
                 * things like /dev/null without error.
                 */
                if (Flag(FNOCLOBBER) && !(iop->flag & IOCLOB)
                    && (stat(cp, &statb) < 0 || S_ISREG(statb.st_mode)))
                        flags |= O_EXCL;
                break;

          case IORDWR:
                flags = O_RDWR | O_CREAT;
                break;

          case IOHERE:
                do_open = 0;
                /* herein() returns -2 if error has been printed */
                u = herein(iop->heredoc, iop->flag & IOEVAL);
                /* cp may have wrong name */
                break;

          case IODUP:
          {
                const char *emsg;

                do_open = 0;
                if (*cp == '-' && !cp[1]) {
                        u = 1009;        /* prevent error return below */
                        do_close = 1;
                } else if ((u = check_fd(cp,
                                X_OK | ((iop->flag & IORDUP) ? R_OK : W_OK),
                                &emsg)) < 0)
                {
                        warningf(TRUE, "%s: %s",
                                snptreef((char *) 0, 32, "%R", &iotmp), emsg);
                        return -1;
                }
                break;
          }
        }
        if (do_open) {
                u = open(cp, flags, 0666);
        }
        if (u < 0) {
                /* herein() may already have printed message */
                if (u == -1)
                        warningf(TRUE, "cannot %s %s: %s",
                               iotype == IODUP ? "dup"
                                : (iotype == IOREAD || iotype == IOHERE) ?
                                    "open" : "create", cp, strerror(errno));
                return -1;
        }
        /* Do not save if it has already been redirected (i.e. "cat >x >y"). */
        if (e->savefd[iop->unit] == 0)
                /* c_exec() assumes e->savefd[fd] set for any redirections.
                 * Ask savefd() not to close iop->unit - allows error messages
                 * to be seen if iop->unit is 2; also means we can't lose
                 * the fd (eg, both dup2 below and dup2 in restfd() failing).
                 */
                e->savefd[iop->unit] = savefd(iop->unit, 1);

        if (do_close)
                close(iop->unit);
        else if (u != iop->unit) {
                if (ksh_dup2(u, iop->unit, TRUE) < 0) {
                        warningf(TRUE,
                                "could not finish (dup) redirection %s: %s",
                                snptreef((char *) 0, 32, "%R", &iotmp),
                                strerror(errno));
                        if (iotype != IODUP)
                                close(u);
                        return -1;
                }
                if (iotype != IODUP)
                        close(u);
#ifdef KSH
                /* Touching any co-process fd in an empty exec
                 * causes the shell to close its copies
                 */
                else if (tp && tp->type == CSHELL && tp->val.f == c_exec) {
                        if (iop->flag & IORDUP) /* possible exec <&p */
                                coproc_read_close(u);
                        else                    /* possible exec >&p */
                                coproc_write_close(u);
                }
#endif /* KSH */
        }
        if (u == 2) /* Clear any write errors */
                shf_reopen(2, SHF_WR, shl_out);
        return 0;
}

/*
 * open here document temp file.
 * if unquoted here, expand here temp file into second temp file.
 */
static int
herein(content, sub)
        const char *content;
        int sub;
{
        volatile int fd = -1;
        struct source *s, *volatile osource;
        struct shf *volatile shf;
        struct temp *h;
        int i;

        /* ksh -c 'cat << EOF' can cause this... */
        if (content == (char *) 0) {
                warningf(TRUE, "here document missing");
                return -2; /* special to iosetup(): don't print error */
        }

        /* Create temp file to hold content (done before newenv so temp
         * doesn't get removed too soon).
         */
        h = maketemp(ATEMP, TT_HEREDOC_EXP, &e->temps);
        if (!(shf = h->shf) || (fd = open(h->name, O_RDONLY, 0)) < 0) {
                warningf(TRUE, "can't %s temporary file %s: %s",
                        !shf ? "create" : "open",
                        h->name, strerror(errno));
                if (shf)
                        shf_close(shf);
                return -2 /* special to iosetup(): don't print error */;
        }

        osource = source;
        newenv(E_ERRH);
        i = ksh_sigsetjmp(e->jbuf, 0);
        if (i) {
                source = osource;
                quitenv();
                shf_close(shf); /* after quitenv */
                close(fd);
                return -2; /* special to iosetup(): don't print error */
        }
        if (sub) {
                /* Do substitutions on the content of heredoc */
                s = pushs(SSTRING, ATEMP);
                s->start = s->str = content;
                source = s;
                if (yylex(ONEWORD) != LWORD)
                        internal_errorf(1, "herein: yylex");
                source = osource;
                shf_puts(evalstr(yylval.cp, 0), shf);
        } else
                shf_puts(content, shf);

        quitenv();

        if (shf_close(shf) == EOF) {
                close(fd);
                warningf(TRUE, "error writing %s: %s", h->name,
                        strerror(errno));
                return -2; /* special to iosetup(): don't print error */
        }

        return fd;
}

#ifdef KSH
/*
 *      ksh special - the select command processing section
 *      print the args in column form - assuming that we can
 */
static char *
do_selectargs(ap, print_menu)
        register char **ap;
        bool_t print_menu;
{
        static const char *const read_args[] = {
                                        "read", "-r", "REPLY", (char *) 0
                                    };
        char *s;
        int i, argct;

        for (argct = 0; ap[argct]; argct++)
                ;
        while (1) {
                /* Menu is printed if
                 *      - this is the first time around the select loop
                 *      - the user enters a blank line
                 *      - the REPLY parameter is empty
                 */
                if (print_menu || !*str_val(global("REPLY")))
                        pr_menu(ap);
                shellf("%s", str_val(global("PS3")));
                if (call_builtin(findcom("read", FC_BI), (char **) read_args))
                        return (char *) 0;
                s = str_val(global("REPLY"));
                if (*s) {
                        i = atoi(s);
                        return (i >= 1 && i <= argct) ? ap[i - 1] : null;
                }
                print_menu = 1;
        }
}

struct select_menu_info {
        char    *const *args;
        int     arg_width;
        int     num_width;
} info;

static char *select_fmt_entry ARGS((void *arg, int i, char *buf, int buflen));

/* format a single select menu item */
static char *
select_fmt_entry(arg, i, buf, buflen)
        void *arg;
        int i;
        char *buf;
        int buflen;
{
        struct select_menu_info *smi = (struct select_menu_info *) arg;

        shf_snprintf(buf, buflen, "%*d) %s",
                smi->num_width, i + 1, smi->args[i]);
        return buf;
}

/*
 *      print a select style menu
 */
int
pr_menu(ap)
        char *const *ap;
{
        struct select_menu_info smi;
        char *const *pp;
        int nwidth, dwidth;
        int i, n;

        /* Width/column calculations were done once and saved, but this
         * means select can't be used recursively so we re-calculate each
         * time (could save in a structure that is returned, but its probably
         * not worth the bother).
         */

        /*
         * get dimensions of the list
         */
        for (n = 0, nwidth = 0, pp = ap; *pp; n++, pp++) {
                i = strlen(*pp);
                nwidth = (i > nwidth) ? i : nwidth;
        }
        /*
         * we will print an index of the form
         *      %d)
         * in front of each entry
         * get the max width of this
         */
        for (i = n, dwidth = 1; i >= 10; i /= 10)
                dwidth++;

        smi.args = ap;
        smi.arg_width = nwidth;
        smi.num_width = dwidth;
        print_columns(shl_out, n, select_fmt_entry, (void *) &smi,
                dwidth + nwidth + 2);

        return n;
}
#endif /* KSH */
#ifdef KSH

/*
 *      [[ ... ]] evaluation routines
 */

extern const char *const dbtest_tokens[];
extern const char db_close[];

/* Test if the current token is a whatever.  Accepts the current token if
 * it is.  Returns 0 if it is not, non-zero if it is (in the case of
 * TM_UNOP and TM_BINOP, the returned value is a Test_op).
 */
static int
dbteste_isa(te, meta)
        Test_env *te;
        Test_meta meta;
{
        int ret = 0;
        int uqword;
        char *p;

        if (!*te->pos.wp)
                return meta == TM_END;

        /* unquoted word? */
        for (p = *te->pos.wp; *p == CHAR; p += 2)
                ;
        uqword = *p == EOS;

        if (meta == TM_UNOP || meta == TM_BINOP) {
                if (uqword) {
                        char buf[8];    /* longer than the longest operator */
                        char *q = buf;
                        for (p = *te->pos.wp; *p == CHAR
                                              && q < &buf[sizeof(buf) - 1];
                                              p += 2)
                                *q++ = p[1];
                        *q = '\0';
                        ret = (int) test_isop(te, meta, buf);
                }
        } else if (meta == TM_END)
                ret = 0;
        else
                ret = uqword
                        && strcmp(*te->pos.wp, dbtest_tokens[(int) meta]) == 0;

        /* Accept the token? */
        if (ret)
                te->pos.wp++;

        return ret;
}

static const char *
dbteste_getopnd(te, op, do_eval)
        Test_env *te;
        Test_op op;
        int do_eval;
{
        char *s = *te->pos.wp;

        if (!s)
                return (char *) 0;

        te->pos.wp++;

        if (!do_eval)
                return null;

        if (op == TO_STEQL || op == TO_STNEQ)
                s = evalstr(s, DOTILDE | DOPAT);
        else
                s = evalstr(s, DOTILDE);

        return s;
}

static int
dbteste_eval(te, op, opnd1, opnd2, do_eval)
        Test_env *te;
        Test_op op;
        const char *opnd1;
        const char *opnd2;
        int do_eval;
{
        return test_eval(te, op, opnd1, opnd2, do_eval);
}

static void
dbteste_error(te, offset, msg)
        Test_env *te;
        int offset;
        const char *msg;
{
        te->flags |= TEF_ERROR;
        internal_errorf(0, "dbteste_error: %s (offset %d)", msg, offset);
}
#endif /* KSH */


void tbl_copy(struct table *src, struct table *dst, Area *ap);

/* have to copy stuff lacking fork ()  */

void
blk_copy(struct block *src)
{
  register struct block *l;
  char **tw, **rw;
  static char *const empty[] = {null};

  /* this is the deepest nested block, also called `globals' */
  if (src->next)
    blk_copy(src->next);


  newblock();
  l = e->loc;
  l->argc = src->argc;

  if (l->argc)
    {
      /* copy the argument vector */
      for (tw = src->argv; *tw++ != NULL; ) ;
      rw = l->argv = (char **)alloc((int)(tw - src->argv) * sizeof(*tw),
                                     &l->area);
      for (tw = src->argv; *tw != NULL; )
      {
         *rw++ = wdcopy(*tw++, &l->area);
      }
      *rw = NULL;

    }
  else
  {
         l->argv = (char **)empty;
     l->argv[0] = src->argv[0]; /* preserve $0, just doing l->arv=src->argv
                                                                   preserves everything which is not what
                                                                   we want */
/*
 *        KPrintF("%s %ld: %s\n",__FILE__,__LINE__,l->argv[0]);
 */
  }

/*
 * KPrintF("%s\n",l->argv[0]);
 * KPrintF("%s\n",l->argv[1]);
 * KPrintF("%s\n",l->argv[2]);
 * KPrintF("%s\n",l->argv[3]);
 * KPrintF("%s\n",l->argv[4]);
 */

  tbl_copy(&src->vars, &l->vars, &l->area);
  tbl_copy(&src->funs, &l->funs, &l->area);

  /* they're not used anyway (ie. always 0), but something like this
     will have to be done when they ARE used. Perhaps it will be ATEMP
     instead of APERM? */
  l->error = src->error ? str_save(src->error, APERM) : 0;
  l->exit  = src->exit ? str_save(src->exit, APERM) : 0;
/*
 * KPrintF("%s %ld: %s\n",__FILE__,__LINE__,l->argv[0]);
 */
}

/* need to copy tables, since no fork () */

/* assume initialized source and destination tables */
void
tbl_copy(struct table *src, struct table *dst, Area *ap)
{
  struct tbl *t, *tn;
  struct tstate ts;

  twalk(&ts,src);
  tinit(dst, ap, 0);
  while ((t = tnext(&ts)))
    {


      tn = tenter(dst, t->name, hash(t->name));
      tn->flag = t->flag;
      tn->type = t->type;
      if (t->flag & INTEGER)
        tn->val.i = t->val.i;
      else if (t->flag & EXPORTV)
      {
        tn->val.s = str_save(t->val.s, ap);
      }
      else if (t->type == CFUNC)
      {
        tn->val.t = t->val.t ? tcopy(t->val.t, ap) : 0;
      }
      else
      {
        tn->val.s = (t->flag & ALLOC) ?
                      str_save(t->val.s /* + t->type */, ap) : 0;
      }
      /* new entries in struct tbl */
      tn->index = t->index;
      tn->areap = ap;

      tn->u2.field = t->u2.field;
      if (t->flag & ARRAY)
      {
        struct tbl *tmp, **list = &tn->u.array;
        *list = NULL;
        for (tmp = t->u.array; tmp; tmp = tmp->u.array)
        {
          int size = sizeof(struct tbl) + strlen(tmp->name) + 1;
          struct tbl *new;

          *list = new = (struct tbl *)alloc(size, ap);
          memcpy(new, tmp, size);
          new->areap = ap;
          if (tmp->flag & ALLOC)
            new->val.s = str_save(tmp->val.s /*+ tmp->type */, ap);
          list = &new->u.array;
        }
      }
      tn->u.array = t->u.array;
      if ((tn->flag & SPECIAL) && !strcmp(tn->name, "PATH"))
        path = str_save(tn->val.s + tn->type, ap);
    }
}




struct env *copyenv(struct globals *globenv )
{
    /* make a copy of the curent environment tree */
    struct env *copy;
    struct block *b;
    short *old;
    int i;
    char *p;

    /* save pointers to current environment */

    globenv->e = e;
    globenv->homedirs = homedirs;
    globenv->aliases = aliases;
    globenv->taliases = taliases;
    globenv->aperm = aperm;
   // globenv->sigtraps = sigtraps;

    /* save our old path, will be copied in code in tbl_copy */
    globenv->path = path;

    /* save our current working directory */

    globenv->current_wd = strdup(current_wd);

    /* set up new "permanent storage" */

    aperm = malloc(sizeof(Area));

    ainit(aperm);

    copy = (struct env *)malloc(sizeof(struct env));
    homedirs = malloc(sizeof(struct table));
    taliases = malloc(sizeof(struct table));
    aliases  = malloc(sizeof(struct table));
    /* init tables */
        tinit(taliases, APERM, 0);
        tinit(aliases, APERM, 0);
        tinit(homedirs, APERM, 0);

    /*set base environment */
    copy->type = E_NONE;
    copy->temps = NULL;
    ainit(&copy->area);

    copy->savefd = NULL; /* ??? */
    copy->oenv = NULL;  /* oenv ??*/
    copy->loc = NULL;

/* copy these */

  tbl_copy(globenv->homedirs, homedirs, APERM);
  tbl_copy(globenv->taliases, taliases, APERM);
  tbl_copy(globenv->aliases,  aliases, APERM);

  b = e->loc;
  old = e->savefd;

  /* now transfer environment, and use the copy */

  e = copy;
  blk_copy(b);

  if (old)
  {
    e->savefd = (short*) alloc(sizeofN(short, NUFILE), ATEMP);
    bcopy(old, e->savefd, sizeofN(short, NUFILE));
  }

  /* Duplicate and save file handles */

  for( i=0; i<NUFILE;i++)
  {
      globenv->fd[i] = savefd(i,TRUE);
     // if(globenv->fd[i]>=0) dup2(i,globenv->fd[i]);
  }
  /* note other stuff nay need doing! */


}

/* restore the old env defined in globenv */

void restoreenv(struct globals *globenv)
{

    int i;
    /* simply free the aperm area */

    afreeall(aperm);
    free(aperm);
    free(homedirs);
    free(taliases);
    free(aliases);

    /* determine how to free e */

    /* restore old tables and environment */

 //   sigtraps = globenv->sigtraps;
    homedirs = globenv->homedirs;
    taliases = globenv->taliases;
    aliases = globenv->aliases;
    aperm = globenv->aperm;
    path = globenv->path;
    e = globenv->e;

  for( i=0; i<NUFILE;i++)
  {
      if(globenv->fd[i]>=0) restfd(i,globenv->fd[i]);
  }
  /* restore working dir */

  if (globenv->current_wd)
  {
    set_current_wd(globenv->current_wd);
    chdir(current_wd);
    free(globenv->current_wd);
  }

}

#if(0)
void printflags(struct tbl *t)
{
      printf("  flags %d ",t->flag);
      if(t->flag & ALLOC)   printf("ALLOC ");
      if(t->flag & DEFINED) printf("DEFINED ");
      if(t->flag & ISSET)   printf("ISSET ");
      if(t->flag & EXPORTV) printf("EXPORTV ");
      if(t->flag & TRACE)   printf("TRACE ");
      if(t->flag & SPECIAL) printf("SPECIAL ");
      if(t->flag & INTEGER)   printf("INTEGER ");
      if(t->flag & RDONLY)   printf("RDONLY ");
      if(t->flag & LOCAL)   printf("LOCAL ");
      if(t->flag & ARRAY)   printf("ARRAY ");
      if(t->flag & LJUST)   printf("LJUST ");
      if(t->flag & RJUST)   printf("RJUST ");
      if(t->flag & ZEROFIL)   printf("ZEROFIL ");
      if(t->flag & LCASEV)   printf("LCASEV ");
      if(t->flag & UCASEV_AL)   printf("UCASEV_ALTRACE ");
      if(t->flag & INT_U)   printf("INT_U ");
      if(t->flag & INT_L)   printf("INT_L");
      if(t->flag & IMPORTV)   printf("IMPORTV ");
      if(t->flag & LOCAL_COPY)   printf("LOCAL_COPY ");
      if(t->flag & EXPRINEVAL)   printf("EXPRINEVAL ");
      if(t->flag & EXPRLVALUE)   printf("EXPRLVALUE ");
      printf("\n");

}
#endif
