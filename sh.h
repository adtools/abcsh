/*
 * Public Domain Bourne/Korn shell
 */

/* $Id$ */

#define ABC_VERSION "53.5"
#define RELEASE_DATE "24.7.2017"
#define RELEASE_COMMENT

#define STACK_SIZE "500000"

/* Start of common headers */

#ifdef __amigaos4__
#define __USE_INLINE__
#endif

#ifdef NEWLIB
# define sig_t _sig_func_ptr
#endif

#include <stdio.h>
#include <sys/types.h>
#include <setjmp.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>

#ifndef __amigaos4__
void *memmove(void *, const void *, size_t);
#endif

#include <errno.h>

#include <fcntl.h>

#ifndef O_ACCMODE
# define O_ACCMODE      (O_RDONLY|O_WRONLY|O_RDWR)
#endif /* !O_ACCMODE */

#ifndef DEFFILEMODE
# define DEFFILEMODE     (S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)
#endif /* !DEFFILEMODE */

#ifndef F_OK    /* access() arguments */
# define F_OK 0
# define X_OK 1
# define W_OK 2
# define R_OK 4
#endif /* !F_OK */

#include <limits.h>
#include <signal.h>

#if defined(AMIGA) && !defined(NEWLIB)
# define NSIG    32
# define SIGQUIT 7
# define SIGCHLD 8
# define SIGCLD  8
# define SIGHUP  9
# define SIGPIPE 10
# define SIGKILL 11
# define SIGALRM 12
#else
# define NSIG    32
#endif

#define KSH_SA_FLAGS   0

typedef void (*handler_t)(int);  /* signal handler */

#include "sigact.h"                     /* use sjg's fake sigaction() */

#ifndef offsetof
# define offsetof(type,id) ((size_t)&((type*)NULL)->id)
#endif

#define killpg(p, s)    kill(-(p), (s))

/* Special cases for execve(2) */
#define ksh_execve(p, av, ev, flags)    execve(p, av, ev)

//#ifdef AMIGA
//#define ksh_dupbase(fd, base) amigaos_dupbbase(fd, base)
//#else
#define ksh_dupbase(fd, base) fcntl(fd, F_DUPFD, base)
//#endif

#define ksh_sigsetjmp(env,sm)   setjmp(env)
#define ksh_siglongjmp(env,v)   longjmp((env), (v))
#define ksh_jmp_buf             jmp_buf

extern int dup2(int, int);

#define INT32   int

/* end of common headers */

#ifndef EXECSHELL
/* shell to exec scripts (see also $SHELL initialization in main.c) */
#  define EXECSHELL     "/SDK/C/sh"
#  define EXECSHELL_STR "EXECSHELL"
#endif

/* ISABSPATH() means path is fully and completely specified,
 * ISROOTEDPATH() means a .. as the first component is a no-op,
 * ISRELPATH() means $PWD can be tacked on to get an absolute path.
 *
 * OS           Path            ISABSPATH       ISROOTEDPATH    ISRELPATH
 * unix         /foo            yes             yes             no
 * unix         foo             no              no              yes
 * unix         ../foo          no              no              yes
 */

#ifdef AMIGA
# define PATHSEP        ':'
# define DIRSEP         '/'
# define DIRSEPSTR      "/"
# define ISDIRSEP(c)    ((c) == '/')
extern int amigaos_isabspath(const char *path);
extern int amigaos_isrootedpath(const char *path);
extern int amigaos_isrelpath(const char *path);
# define ISABSPATH(s)   amigaos_isabspath(s)
# define ISROOTEDPATH(s) amigaos_isrootedpath(s)
# define ISRELPATH(s)   amigaos_isrelpath(s)
# define FILECHCONV(c)  (isupper(c) ? tolower(c) : c)
# define FILECMP(s1, s2) strcasecmp(s1, s2)
# define FILENCMP(s1, s2, n) strncasecmp(s1, s2, n)
//extern char *ksh_strchr_dirsep(const char *path);
//extern char *ksh_strrchr_dirsep(const char *path);
# define ksh_strchr_dirsep(p)   strchr(p, DIRSEP)
# define ksh_strrchr_dirsep(p)  strrchr(p, DIRSEP)
#else
# define PATHSEP        ':'
# define DIRSEP         '/'
# define DIRSEPSTR      "/"
# define ISDIRSEP(c)    ((c) == '/')
# define ISABSPATH(s)   ISDIRSEP((s)[0])
# define ISRELPATH(s)   (!ISABSPATH(s))
# define ISROOTEDPATH(s) ISABSPATH(s)
# define FILECHCONV(c)  c
# define FILECMP(s1, s2) strcmp(s1, s2)
# define FILENCMP(s1, s2, n) strncmp(s1, s2, n)
# define ksh_strchr_dirsep(p)   strchr(p, DIRSEP)
# define ksh_strrchr_dirsep(p)  strrchr(p, DIRSEP)
#endif

#ifndef __amigaos4__
typedef unsigned char uint8;
typedef   signed char  int8;
typedef unsigned short uint16;
typedef   signed short  int16;
typedef unsigned long uint32;
typedef   signed long  int32;
#endif

#define NELEM(a) (sizeof(a) / sizeof((a)[0]))
#define sizeofN(type, n) (sizeof(type) * (n))
#define BIT(i)  (1<<(i))        /* define bit in flag */

/* Table flag type - needs > 16 and < 32 bits */
typedef INT32 Tflag;

#define NUFILE  32              /* Number of user-accessible files */
#define FDBASE  10              /* First file usable by Shell */

/* you're not going to run setuid shell scripts, are you? */
#define eaccess(path, mode)     access(path, mode)

/* Make MAGIC a char that might be printed to make bugs more obvious, but
 * not a char that is used often.  Also, can't use the high bit as it causes
 * portability problems (calling strchr(x, 0x80|'x') is error prone).
 */
#define MAGIC           (7)     /* prefix for *?[!{,} during expand */
#define ISMAGIC(c)      ((unsigned char)(c) == MAGIC)
#define NOT             '!'     /* might use ^ (ie, [!...] vs [^..]) */

#define LINE    2048            /* input line size */
#define PATH    1024            /* pathname size (todo: PATH_MAX/pathconf()) */
#define ARRAYMAX 2047           /* max array index */

extern const char *kshname;     /* $0 */
extern pid_t kshpid;            /* $$, shell pid */
extern pid_t procpid;           /* pid of executing process */
extern uid_t ksheuid;           /* effective uid of shell */
extern int kshegid;             /* effective gid of shell */
extern uid_t kshuid;            /* real uid of shell */
extern int kshgid;              /* real gid of shell */
extern int exstat;              /* exit status */
extern int subst_exstat;        /* exit status of last $(..)/`..` */
extern const char *safe_prompt; /* safe prompt if PS1 substitution fails */

/*
 * Area-based allocation built on malloc/free
 */

typedef struct Area {
	struct link *freelist; /* free list */
} Area;

/* change this to so that APERM uses a pointer to the permanent space */
/* rather than it's explicit address */
/* allows a new permanent space to be created for subshells etc */

extern Area    *aperm;

#define APERM   aperm
#define ATEMP   &genv->area

#ifdef KSH_DEBUG
# define kshdebug_init()        kshdebug_init_()
# define kshdebug_printf(a)     kshdebug_printf_ a
# define kshdebug_dump(a)       kshdebug_dump_ a
#else /* KSH_DEBUG */
# define kshdebug_init()
# define kshdebug_printf(a)
# define kshdebug_dump(a)
#endif /* KSH_DEBUG */


/*
 * parsing & execution environment
 */
struct env {
	short   type;                   /* environment type - see below */
	short   flags;                  /* EF_* */
	Area    area;                   /* temporary allocation area */
	struct  block *loc;             /* local variables and functions */
	short  *savefd;                 /* original redirected fd's */
	struct  env *oenv;              /* link to previous environment */
	ksh_jmp_buf jbuf;               /* long jump back to env creator */
	struct temp *temps;             /* temp files */
};
extern struct env *genv;

/* struct env.type values */
#define E_NONE  0               /* dummy environment */
#define E_PARSE 1               /* parsing command # */
#define E_FUNC  2               /* executing function # */
#define E_INCL  3               /* including a file via . # */
#define E_EXEC  4               /* executing command tree */
#define E_LOOP  5               /* executing for/while # */
#define E_ERRH  6               /* general error handler # */
#define E_SUBSHELL 7            /* a subshell */

/* # indicates env has valid jbuf (see unwind()) */

/* struct env.flag values */
#define EF_FUNC_PARSE   BIT(0)  /* function being parsed */
#define EF_BRKCONT_PASS BIT(1)  /* set if E_LOOP must pass break/continue on */
#define EF_FAKE_SIGDIE  BIT(2)  /* hack to get info from unwind to quitenv */

/* Do breaks/continues stop at env type e? */
#define STOP_BRKCONT(t) ((t) == E_NONE || (t) == E_PARSE \
                         || (t) == E_FUNC || (t) == E_INCL)
/* Do returns stop at env type e? */
#define STOP_RETURN(t)  ((t) == E_FUNC || (t) == E_INCL)

/* values for ksh_siglongjmp(e->jbuf, 0) */
#define LRETURN 1               /* return statement */
#define LEXIT   2               /* exit statement */
#define LERROR  3               /* errorf() called */
#define LLEAVE  4               /* untrappable exit/error */
#define LINTR   5               /* ^C noticed */
#define LBREAK  6               /* break statement */
#define LCONTIN 7               /* continue statement */
#define LSHELL  8               /* return to interactive shell() */
#define LAEXPR  9               /* error in arithmetic expression */


/* option processing */
#define OF_CMDLINE      0x01    /* command line */
#define OF_SET          0x02    /* set builtin */
#define OF_SPECIAL      0x04    /* a special variable changing */
#define OF_INTERNAL     0x08    /* set internally by shell */
#define OF_ANY          (OF_CMDLINE | OF_SET | OF_SPECIAL | OF_INTERNAL)

struct option {
    const char  *name;  /* long name of option */
    char        c;      /* character flag (if any) */
    short       flags;  /* OF_* */
};
extern const struct option options[];

/*
 * flags (the order of these enums MUST match the order in misc.c(options[]))
 */
enum sh_flag {
        FEXPORT = 0,    /* -a: export all */
        FBRACEEXPAND,   /* enable {} globbing */
        FBGNICE,        /* bgnice */
        FCOMMAND,       /* -c: (invocation) execute specified command */
        FERREXIT,       /* -e: quit on error */
        FIGNOREEOF,     /* eof does not exit */
        FTALKING,       /* -i: interactive */
        FKEYWORD,       /* -k: name=value anywhere */
        FMARKDIRS,      /* mark dirs with / in file name completion */
        FNOCLOBBER,     /* -C: don't overwrite existing files */
        FNOEXEC,        /* -n: don't execute any commands */
        FNOGLOB,        /* -f: don't do file globbing */
        FNOLOG,		/* don't save functions in history (ignored) */
        FNOUNSET,       /* -u: using an unset var is an error */
        FPHYSICAL,      /* -o physical: don't do logical cd's/pwd's */
        FPOSIX,         /* -o posix: be posixly correct */
        FSH,            /* -o sh: favor sh behavour */
        FSTDIN,         /* -s: (invocation) parse stdin */
        FTRACKALL,      /* -h: create tracked aliases for all commands */
        FVERBOSE,       /* -v: echo input */
        FXTRACE,        /* -x: execution trace */
        FTALKING_I,     /* (internal): initial shell was interactive */
        FNFLAGS /* (place holder: how many flags are there) */
};

#define Flag(f) (shell_flags[(int) (f)])

extern char shell_flags[FNFLAGS];

extern char null[];        /* null value for variable */

extern char space[];
extern char newline[];

enum temp_type {
    TT_HEREDOC_EXP,     /* expanded heredoc */
    TT_HIST_EDIT        /* temp file used for history editing (fc -e) */
};
typedef enum temp_type Temp_type;
/* temp/heredoc files.  The file is removed when the struct is freed. */
struct temp {
        struct temp     *next;
        struct shf      *shf;
        int             pid;            /* pid of process parsed here-doc */
        Temp_type       type;
        char            *name;
};

/*
 * stdio and our IO routines
 */

#define shl_spare       (&shf_iob[0])   /* for c_read()/c_print() */
#define shl_stdout      (&shf_iob[1])
#define shl_out         (&shf_iob[2])
extern int shl_stdout_ok;

/*
 * trap handlers
 */
typedef struct trap {
        int     signal;         /* signal number */
        const char *name;       /* short name */
        const char *mess;       /* descriptive name */
        char   *trap;           /* trap command */
        volatile sig_atomic_t set;   /* trap pending */
        int     flags;          /* TF_* */
        sig_t cursig;       /* current handler (valid if TF_ORIG_* set) */
        sig_t shtrap;       /* shell signal handler */
} Trap;

/* values for Trap.flags */
#define TF_SHELL_USES   BIT(0)  /* shell uses signal, user can't change */
#define TF_USER_SET     BIT(1)  /* user has (tried to) set trap */
#define TF_ORIG_IGN     BIT(2)  /* original action was SIG_IGN */
#define TF_ORIG_DFL     BIT(3)  /* original action was SIG_DFL */
#define TF_EXEC_IGN     BIT(4)  /* restore SIG_IGN just before exec */
#define TF_EXEC_DFL     BIT(5)  /* restore SIG_DFL just before exec */
#define TF_DFL_INTR     BIT(6)  /* when received, default action is LINTR */
#define TF_TTY_INTR     BIT(7)  /* tty generated signal (see j_waitj) */
#define TF_CHANGED      BIT(8)  /* used by runtrap() to detect trap changes */
#define TF_FATAL        BIT(9)  /* causes termination if not trapped */

/* values for setsig()/setexecsig() flags argument */
#define SS_RESTORE_MASK 0x3     /* how to restore a signal before an exec() */
#define SS_RESTORE_CURR 0       /* leave current handler in place */
#define SS_RESTORE_ORIG 1       /* restore original handler */
#define SS_RESTORE_DFL  2       /* restore to SIG_DFL */
#define SS_RESTORE_IGN  3       /* restore to SIG_IGN */
#define SS_FORCE        BIT(3)  /* set signal even if original signal ignored */
#define SS_USER         BIT(4)  /* user is doing the set (ie, trap command) */
#define SS_SHTRAP       BIT(5)  /* trap for internal use (CHLD,ALRM,WINCH) */

#define SIGEXIT_        0       /* for trap EXIT */
#define SIGERR_         NSIG /* for trap ERR */

extern volatile sig_atomic_t trap;        /* traps pending? */
extern volatile sig_atomic_t intrsig;     /* pending trap interrupts command */
extern volatile sig_atomic_t fatal_trap;/* received a fatal signal */
extern Trap sigtraps[NSIG+1];

/*
 * TMOUT support
 */
/* values for ksh_tmout_state */
enum tmout_enum {
                TMOUT_EXECUTING = 0,    /* executing commands */
                TMOUT_READING,          /* waiting for input */
                TMOUT_LEAVING           /* have timed out */
        };
extern unsigned int ksh_tmout;
extern enum tmout_enum ksh_tmout_state;

/* For "You have stopped jobs" message */
extern int really_exit;


/*
 * fast character classes
 */
#define C_ALPHA  BIT(0)         /* a-z_A-Z */
#define C_DIGIT  BIT(1)         /* 0-9 */
#define C_LEX1   BIT(2)         /* \0 \t\n|&;<>() */
#define C_VAR1   BIT(3)         /* *@#!$-? */
#define C_IFSWS  BIT(4)         /* \t \n (IFS white space) */
#define C_SUBOP1 BIT(5)         /* "=-+?" */
#define C_SUBOP2 BIT(6)         /* "#%" */
#define C_IFS    BIT(7)         /* $IFS */
#define C_QUOTE  BIT(8)         /*  \n\t"#$&'()*;<>?[\`| (needing quoting) */

extern  short ctypes [];

#define ctype(c, t)     !!(ctypes[(unsigned char)(c)]&(t))
#define letter(c)       ctype(c, C_ALPHA)
#define digit(c)        ctype(c, C_DIGIT)
#define letnum(c)       ctype(c, C_ALPHA|C_DIGIT)

extern int ifs0; /* for "$*" */


/* Argument parsing for built-in commands and getopts command */

/* Values for Getopt.flags */
#define GF_ERROR        BIT(0)  /* call errorf() if there is an error */
#define GF_PLUSOPT      BIT(1)  /* allow +c as an option */
#define GF_NONAME       BIT(2)  /* don't print argv[0] in errors */

/* Values for Getopt.info */
#define GI_MINUS        BIT(0)  /* an option started with -... */
#define GI_PLUS         BIT(1)  /* an option started with +... */
#define GI_MINUSMINUS   BIT(2)  /* arguments were ended with -- */

typedef struct {
        int             optind;
        int             uoptind;/* what user sees in $OPTIND */
        char            *optarg;
        int             flags;  /* see GF_* */
        int             info;   /* see GI_* */
        unsigned int    p;      /* 0 or index into argv[optind - 1] */
        char            buf[2]; /* for bad option OPTARG value */
} Getopt;

extern Getopt builtin_opt;      /* for shell builtin commands */
extern Getopt user_opt;         /* parsing state for getopts builtin command */

/* This for co-processes */

typedef INT32 Coproc_id; /* something that won't (realisticly) wrap */
struct coproc {
        int     read;           /* pipe from co-process's stdout */
        int     readw;          /* other side of read (saved temporarily) */
        int     write;          /* pipe to co-process's stdin */
        Coproc_id id;           /* id of current output pipe */
        int     njobs;          /* number of live jobs using output pipe */
        void    *job;           /* 0 or job of co-process using input pipe */
};
extern struct coproc coproc;

/* Used in jobs.c and by coprocess stuff in exec.c */
extern sigset_t sm_default, sm_sigchld;
extern const char ksh_version[];

/* name of called builtin function (used by error functions) */
extern char     *builtin_argv0;
extern Tflag    builtin_flag;   /* flags of called builtin (SPEC_BI, etc.) */

/* current working directory, and size of memory allocated for same */
extern char     *current_wd;
extern int      current_wd_size;

#define x_cols 80               /* for pr_menu(exec.c) */

/* These to avoid bracket matching problems */
#define OPAREN  '('
#define CPAREN  ')'
#define OBRACK  '['
#define CBRACK  ']'
#define OBRACE  '{'
#define CBRACE  '}'

/* Used by v_evaluate() and setstr() to control action when error occurs */
#define KSH_UNWIND_ERROR        0       /* unwind the stack (longjmp) */
#define KSH_RETURN_ERROR        1       /* return 1/0 for success/failure */

extern int lastresult; /* Last return code set by an extrenal command or subprocess */

/* this structure is used to store the current environment before executing */
/* a "subshell" or similar */

struct globals
{
    Area *aperm;
    struct env *e;
    struct table *homedirs;
    struct table *taliases;
    struct table *aliases;
    void *path;
    char *current_wd;
    int current_wd_size;
    int fd[NUFILE];
    char *ctypes[UCHAR_MAX + 1];
    Trap sigtraps[NSIG+1];
};

/* Used by v_evaluate() and setstr() to control action when error occurs */
#define KSH_UNWIND_ERROR 0 /* unwind the stack (longjmp) */
#define KSH_RETURN_ERROR 1 /* return 1/0 for success/failure */

#include "shf.h"
#include "table.h"
#include "tree.h"
#include "expand.h"
#include "lex.h"

/* alloc.c */
Area *	ainit(Area *);
void	afreeall(Area *);
void *	alloc(size_t, Area *);
void *	areallocarray(void *, size_t, size_t, Area *);
void *	aresize(void *, size_t, Area *);
void	afree(void *, Area *);
/* c_ksh.c */
int     c_hash(char **);
int     c_cd(char **);
int     c_pwd(char **);
int     c_print(char **);
int     c_whence(char **);
int     c_command(char **);
int     c_typeset(char **);
int     c_alias(char **);
int     c_unalias(char **);
int     c_let(char **);
int     c_jobs(char **);
int     c_fgbg(char **);
int     c_kill(char **);
void    getopts_reset(int);
int     c_getopts(char **);
int     c_bind(char **);
/* c_sh.c */
int     c_label(char **);
int     c_shift(char **);
int     c_umask(char **);
int     c_dot(char **);
int     c_wait(char **);
int     c_read(char **);
int     c_eval(char **);
int     c_trap(char **);
int     c_brkcont(char **);
int     c_exitreturn(char **);
int     c_set(char **);
int     c_unset(char **);
int     c_ulimit(char **);
int     c_times(char **);
int     timex(struct op *, int);
void    timex_hook(struct op *, char ** volatile *);
int     c_exec(char **);
int     c_builtin(char **);
/* c_test.c */
int     c_test(char **);
/* eval.c */
char *  substitute(const char *, int);
char ** eval(char **, int);
char *  evalstr(char *, int);
char *  evalonestr(char *, int);
char    *debunk(char *, const char *, size_t);
void    expand(char *, XPtrV *, int);
int glob_str(char *, XPtrV *, int);
/* exec.c */
int     execute(struct op * volatile, volatile int);
int     shcomexec(char **);
struct tbl * findfunc(const char *, unsigned int, int);
int     define(const char *, struct op *);
void    builtin(const char *, int (*)(char **));
struct tbl *    findcom(const char *, int);
void    flushcom(int);
char *  search(const char *, const char *, int, int *);
int     search_access(const char *, int, int *);
int     pr_menu(char *const *);
int     pr_list(char *const *);

void    copyenv(struct globals *);
void    restoreenv(struct globals *);

/* expr.c */
int     evaluate(const char *, long *, int, bool);
int     v_evaluate(struct tbl *, const char *, volatile int, bool);
/* history.c */
void    init_histvec(void);
void    hist_init(Source *);
void    hist_finish(void);
void    histsave(int, const char *, int);
#ifdef HISTORY
int	c_fc(char **);
void	sethistsize(int);
void	sethistfile(const char *);
char **	histpos(void);
int	histnum(int);
int	findhist(int, int, const char *, int);
int	findhistrel(const char *);
char  **hist_get_newest(int);
#endif /* HISTORY */
/* io.c */
void    errorf(const char *, ...)
        __attribute__((__noreturn__, __format__ (printf, 1, 2)));
void    warningf(int, const char *, ...)
        __attribute__((__format__ (printf, 2, 3)));
void    bi_errorf(const char *, ...)
        __attribute__((__format__ (printf, 1, 2)));
void    internal_errorf(int, const char *, ...)
        __attribute__((__format__ (printf, 2, 3)));
void    error_prefix(int);
void    shellf(const char *, ...)
        __attribute__((__format__ (printf, 1, 2)));
void    shprintf(const char *, ...)
        __attribute__((__format__ (printf, 1, 2)));
#ifdef KSH_DEBUG
void    kshdebug_init_(void);
void    kshdebug_printf_(const char *, ...)
        __attribute__((__format__ (printf, 1, 2)));
void    kshdebug_dump_(const char *, const void *, int);
#endif /* KSH_DEBUG */
int     can_seek(int);
void    initio(void);
int     ksh_dup2(int, int, int);
int     savefd(int);
void    restfd(int, int);
void    openpipe(int *);
void    closepipe(int *);
int     check_fd(char *, int, const char **);
void    coproc_init(void);
void    coproc_read_close(int);
void    coproc_readw_close(int);
void    coproc_write_close(int);
int     coproc_getfd(int, const char **);
void    coproc_cleanup(int);
struct temp *maketemp(Area *, Temp_type, struct temp **);
/* jobs.c */
void    j_init(int);
void    j_exit(void);
void    j_change(void);
int     exchild(struct op *, int, int);
void    startlast(void);
int     waitlast(void);
int     waitfor(const char *, int *);
int     j_kill(const char *, int);
int     j_resume(const char *, int);
int     j_jobs(const char *, int, int);
int     j_njobs(void);
void    j_notify(void);
pid_t   j_async(void);
int     j_stopped_running(void);
/* lex.c */
int     yylex(int cf);
void    yyerror(const char *, ...)
        __attribute__((__noreturn__, __format__ (printf, 1, 2)));
Source * pushs(int, Area *);
void    set_prompt(int, Source *);
void    pprompt(const char *, int);
/* main.c */
int     include(const char *, int, char **, int);
int     command(const char *);
int     shell(Source *volatile, int volatile);
void    unwind(int) __attribute__((__noreturn__));
void    newenv(int);
void    quitenv(struct shf *);
void    cleanup_parents_env(void);
void    cleanup_proc_env(void);
/* misc.c */
void    setctypes(const char *, int);
void    initctypes(void);
char *  ulton(unsigned long, int);
char *  str_save(const char *, Area *);
char *  str_nsave(const char *, int, Area *);
int     option(const char *);
char *  getoptions(void);
void    change_flag(enum sh_flag, int, int);
int     parse_args(char **, int, int *);
int     getn(const char *, int *);
int     bi_getn(const char *, int *);
char *  strerror(int);
int     gmatch(const char *, const char *, int);
int     has_globbing(const char *, const char *);
const unsigned char *pat_scan(const unsigned char *, const unsigned char *,
        int);
void    qsortp(void **, size_t, int (*)(const void *, const void *));
int     xstrcmp(const void *, const void *);
void    ksh_getopt_reset(Getopt *, int);
int     ksh_getopt(char **, Getopt *, const char *);
void    print_value_quoted(const char *);
void    print_columns(struct shf *, int, char *(*)(void *, int, char *, int),
        void *, int, int prefcol);
int     strip_nuls(char *, int);
int     blocking_read(int, char *, int);
int     reset_nonblock(int fd);
char    *ksh_get_wd(char *, int);
/* path.c */
int     make_path(const char *, const char *, char **, XString *, int *);
void    simplify_path(char *);
char    *get_phys_path(const char *);
void    set_current_wd(char *);
/* syn.c */
void    initkeywords(void);
struct op * compile(Source *);
/* table.c */
unsigned int    hash(const char *);
void    ktinit(struct table *, Area *, int);
struct tbl *    ktsearch(struct table *, const char *, unsigned int);
struct tbl *    ktenter(struct table *, const char *, unsigned int);
void    ktdelete(struct tbl *);
void    ktwalk(struct tstate *, struct table *);
struct tbl *    ktnext(struct tstate *);
struct tbl **   ktsort(struct table *);
/* trap.c */
void    inittraps(void);
void    alarm_init(void);
Trap *  gettrap(const char *, int);
void trapsig(int);
void    intrcheck(void);
int     fatal_trap_check(void);
int     trap_pending(void);
void    runtraps(int);
void    runtrap(Trap *);
void    cleartraps(void);
void    restoresigs(void);
void    settrap(Trap *, char *);
int     block_pipe(void);
void    restore_pipe(int);
int     setsig(Trap *, sig_t, int);
void    setexecsig(Trap *, int);
/* tree.c */
int     fptreef(struct shf *, int, const char *, ...);
char *  snptreef(char *, int, const char *, ...);
struct op *     tcopy(struct op *, Area *);
char *  wdcopy(const char *, Area *);
char *  wdscan(const char *, int);
char *  wdstrip(const char *);
void    tfree(struct op *, Area *);
/* var.c */
void    newblock(void);
void    popblock(void);
void    initvar(void);
struct tbl *    global(const char *);
struct tbl *    local(const char *, bool);
char *  str_val(struct tbl *);
long    intval(struct tbl *);
int     setstr(struct tbl *, const char *, int);
struct tbl *setint_v(struct tbl *, struct tbl *, bool);
void    setint(struct tbl *, long);
int     getint(struct tbl *, long *, bool);
struct tbl *    typeset(const char *, Tflag, Tflag, int, int);
void    unset(struct tbl *, int);
char  * skip_varname(const char *, int);
char    *skip_wdvarname(const char *, int);
int     is_wdvarname(const char *, int);
int     is_wdvarassign(const char *);
char ** makenv(void);
void    change_random(void);
int     array_ref_len(const char *);
char *  arrayname(const char *);
void    set_array(const char *, int, char **);
/* version.c */

/*amigaos.c*/

char *convert_path_u2a(const char *);
char *convert_path_a2u(const char *);
char *convert_path_multi(const char *);
bool *assign_posix(void);
void SetAmiUpdateENVVariable(const char *);

unsigned int alarm(unsigned int);
int getppid(void);
int fork(void);
int wait(int *);
int pause(void);
int amigaos_dupbbase(int, int);
int pipe(int filedes[]);
