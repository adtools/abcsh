/*
        tty.h -- centralized definitions for a variety of terminal interfaces

        created by DPK, Oct. 1986

        Rearranged to work with autoconf, added TTY_state, get_tty/set_tty
                                                Michael Rendell, May '94

        last edit:      30-Jul-1987     D A Gwyn
*/
/* $Id$ */

/* some useful #defines */
#ifdef EXTERN
# define I__(i) = i
#else
# define I__(i)
# define EXTERN extern
# define EXTERN_DEFINED
#endif

/* Don't know of a system on which including sys/ioctl.h with termios.h
 * causes problems.  If there is one, these lines need to be deleted and
 * aclocal.m4 needs to have stuff un-commented.
 */
#ifdef SYS_IOCTL_WITH_TERMIOS
# define SYS_IOCTL_WITH_TERMIOS
#endif /* SYS_IOCTL_WITH_TERMIOS */
#ifdef SYS_IOCTL_WITH_TERMIO
# define SYS_IOCTL_WITH_TERMIO
#endif /* SYS_IOCTL_WITH_TERMIO */

/* Assume BSD tty stuff.  Uses TIOCGETP, TIOCSETN; uses TIOCGATC/TIOCSATC if
 * available, otherwise it uses TIOCGETC/TIOCSETC (also uses TIOCGLTC/TIOCSLTC
 * if available)
 */
#  ifdef AMIGA
#define CBREAK 1
#define ECHO 2
struct sgttyb
{
        char sg_erase;
        char sg_kill;
        char sg_flags;
};
struct tchars
{
        char t_intrc;
        char t_quitc;
        char t_eofc;
};
#  else
#   include <sys/ioctl.h>
#  endif
typedef struct {
        struct sgttyb   sgttyb;
#  ifdef TIOCGATC
        struct lchars   lchars;
#  else /* TIOCGATC */
        struct tchars   tchars;
#   ifdef TIOCGLTC
        struct ltchars  ltchars;
#   endif /* TIOCGLTC */
#  endif /* TIOCGATC */
} TTY_state;

/* Flags for set_tty() */
#define TF_NONE         0x00
#define TF_WAIT         0x01    /* drain output, even it requires sleep() */
#define TF_MIPSKLUDGE   0x02    /* kludge to unwedge RISC/os 5.0 tty driver */

EXTERN int              tty_fd I__(-1); /* dup'd tty file descriptor */
EXTERN int              tty_devtty;     /* true if tty_fd is from /dev/tty */
EXTERN TTY_state        tty_state;      /* saved tty state */

extern int      get_tty ARGS((int fd, TTY_state *ts));
extern int      set_tty ARGS((int fd, TTY_state *ts, int flags));
extern void     tty_init ARGS((int init_ttystate));
extern void     tty_close ARGS((void));

/* be sure not to interfere with anyone else's idea about EXTERN */
#ifdef EXTERN_DEFINED
# undef EXTERN_DEFINED
# undef EXTERN
#endif
#undef I__
