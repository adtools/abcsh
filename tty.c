#include "sh.h"
#include "ksh_stat.h"
#define EXTERN
#include "tty.h"
#undef EXTERN

#ifdef __amigaos4__
#define TIOCGETP 1
#define TIOCGETC 2
#define TIOCSETN 3
#define TIOCSETC 4
#endif

int
get_tty(fd, ts)
        int fd;
        TTY_state *ts;
{
        int ret;

        ret = ioctl(fd, TIOCGETP, &ts->sgttyb);
#   ifdef TIOCGATC
        if (ioctl(fd, TIOCGATC, &ts->lchars) < 0)
                ret = -1;
#   else
        if (ioctl(fd, TIOCGETC, &ts->tchars) < 0)
                ret = -1;
#    ifdef TIOCGLTC
        if (ioctl(fd, TIOCGLTC, &ts->ltchars) < 0)
                ret = -1;
#    endif /* TIOCGLTC */
#   endif /* TIOCGATC */
        return ret;
}

int
set_tty(fd, ts, flags)
        int fd;
        TTY_state *ts;
        int flags;
{
        int ret = 0;

#   if defined(__mips) && (defined(_SYSTYPE_BSD43) || defined(__SYSTYPE_BSD43))
        /* Under RISC/os 5.00, bsd43 environment, after a tty driver
         * generated interrupt (eg, INTR, TSTP), all output to tty is
         * lost until a SETP is done (there must be a better way of
         * doing this...).
         */
        if (flags & TF_MIPSKLUDGE)
                ret = ioctl(fd, TIOCSETP, &ts->sgttyb);
        else
#   endif /* _SYSTYPE_BSD43 */
            ret = ioctl(fd, TIOCSETN, &ts->sgttyb);
#   ifdef TIOCGATC
        if (ioctl(fd, TIOCSATC, &ts->lchars) < 0)
                ret = -1;
#   else
        if (ioctl(fd, TIOCSETC, &ts->tchars) < 0)
                ret = -1;
#    ifdef TIOCGLTC
        if (ioctl(fd, TIOCSLTC, &ts->ltchars) < 0)
                ret = -1;
#    endif /* TIOCGLTC */
#   endif /* TIOCGATC */
        return ret;
}


/* Initialize tty_fd.  Used for saving/reseting tty modes upon
 * foreground job completion and for setting up tty process group.
 */
void
tty_init(init_ttystate)
        int init_ttystate;
{
        int     do_close = 1;
        int     tfd;

        if (tty_fd >= 0) {
                close(tty_fd);
                tty_fd = -1;
        }
        tty_devtty = 1;

        /* SCO can't job control on /dev/tty, so don't try... */
#if !defined(__SCO__)
#if defined(__amigaos4__)
        if ((tfd = -1) < 0) {
#else   
        if ((tfd = open("/dev/tty", O_RDWR, 0)) < 0) {
#endif

/* X11R5 xterm on mips doesn't set controlling tty properly - temporary hack */
# if !defined(__mips) || !(defined(_SYSTYPE_BSD43) || defined(__SYSTYPE_BSD43))
                if (tfd < 0) {
                        tty_devtty = 0;
                        warningf(FALSE,
                                "No controlling tty (open /dev/tty: %s)",
                                strerror(errno));
                }
# endif /* __mips  */
        }
#else /* !__SCO__ */
        tfd = -1;
#endif /* __SCO__ */

        if (tfd < 0) {
                do_close = 0;
                if (isatty(0))
                        tfd = 0;
                else if (isatty(2))
                        tfd = 2;
                else {
                        warningf(FALSE, "Can't find tty file descriptor");
                        return;
                }
        }
        if ((tty_fd = ksh_dupbase(tfd, FDBASE)) < 0) {
                warningf(FALSE, "j_ttyinit: dup of tty fd failed: %s",
                        strerror(errno));
        } else if (fd_clexec(tty_fd) < 0) {
                warningf(FALSE, "j_ttyinit: can't set close-on-exec flag: %s",
                        strerror(errno));
                close(tty_fd);
                tty_fd = -1;
        } else if (init_ttystate)
                get_tty(tty_fd, &tty_state);
        if (do_close)
                close(tfd);
}

void
tty_close()
{
        if (tty_fd >= 0) {
                close(tty_fd);
                tty_fd = -1;
        }
}
