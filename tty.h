/*
        tty.h -- centralized definitions for a variety of terminal interfaces

        created by DPK, Oct. 1986

*/

#include <termios.h>

extern int tty_fd;               /* dup'd tty file descriptor */
extern int tty_devtty;           /* true if tty_fd is from /dev/tty */
extern struct termios tty_state; /* saved tty state */

extern void        tty_init(int);
extern void        tty_close(void);
