#ifndef __MYUSER
#define __MYUSER

#ifdef KERNEL

#ifdef linux
#include "compat.h"
#endif

#ifdef M_UNIX /* SCO et al */
#include <sys/ndir.h>
#ifdef DIRSIZ
#undef DIRSIZ
#endif
#define DIRSIZ 14
#endif
#include <sys/user.h>

#else /* !KERNEL */

#ifndef MAIN
extern
#endif
struct user {
	int u_error;
} u;

#endif /* !KERNEL */

#endif /* __MYUSER */
