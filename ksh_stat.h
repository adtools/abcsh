/* Wrapper around the ugly sys/stat includes/ifdefs */
/* $Id$ */

/* assumes <sys/types.h> already included */
#include <sys/stat.h>

#ifndef S_ISVTX
# define S_ISVTX        01000   /* sticky bit */
#endif /* S_ISVTX */

#ifndef S_IXUSR
# define S_IXUSR        00100   /* user execute bit */
#endif /* S_IXUSR */
#ifndef S_IXGRP
# define S_IXGRP        00010   /* user execute bit */
#endif /* S_IXGRP */
#ifndef S_IXOTH
# define S_IXOTH        00001   /* user execute bit */
#endif /* S_IXOTH */
