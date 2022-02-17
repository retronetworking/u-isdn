#ifndef _CONFIG_H
#define _CONFIG_H

/* =()<#define BYTEORDER @<BYTEORDER>@>()= */
#define BYTEORDER 1234

/* =()<#define SUBDEV @<SUBDEV>@>()= */
#define SUBDEV uchar_t

/* The Linux Streams package already has this one */
#if !defined(linux)
/* =()<typedef @<STREAMCHAR>@ streamchar;>()= */
typedef unsigned char streamchar;
#endif

/* =()<#define UP_DELAY @<UP_DELAY>@>()= */
#define UP_DELAY (HZ / 2)

#ifdef KERNEL
/* =()<#define @<NEED_SPL>@_NEED_SPL>()= */
#define DONT_NEED_SPL

/* =()<#define @<NEED_MEMCPY>@_NEED_MEMCPY>()= */
#define DONT_NEED_MEMCPY

#if !defined(NEW_TIMEOUT) && !defined(OLD_TIMEOUT)
/* =()<#define @<TIMEOUT>@_TIMEOUT>()= */
#define NEW_TIMEOUT
#endif

/* =()<#define @<ADDERROR>@_ADDERROR>()= */
#define DONT_ADDERROR

#else
#define OLD_TIMEOUT
#define DONT_ADDERROR
#define DONT_NEED_MEMCPY
#define DONT_NEED_SPL
#endif

/* =()<#define @<ADDERROR>@_ADDERROR>()= */
#define DONT_ADDERROR

#ifdef KERNEL
/* =()<#define CONF_MOD2 @<CONF_MOD2>@>()= */
#define CONF_MOD2 0x30
/* =()<#define CONF_DEBUG @<CONF_DEBUG>@>()= */
#define CONF_DEBUG 0x5016
#else
#ifdef DO_DEBUGGING

#ifndef CONFIG_DEBUG_STREAMS
#define CONFIG_DEBUG_STREAMS
#endif
#ifndef CONFIG_DEBUG_ISDN
#define CONFIG_DEBUG_ISDN
#endif

#endif
#endif

/* =()<#define SIGRET @<SIGRET>@>()= */
#define SIGRET void
typedef SIGRET (*sigfunc__t)(int);

/* =()<#define @<SIGTYPE>@_SIGTYPE>()= */
#define SYSV_SIGTYPE

/* =()<#define @<HAVE_SETSID>@_HAVE_SETSID>()= */
#define DO_HAVE_SETSID

/* =()<#define HAVE_SETPGRP_@<HAVE_SETPGRP>@>()= */
#define HAVE_SETPGRP_0

/* =()<#define @<NEED_STRDUP>@_NEED_STRDUP>()= */
#define DONT_NEED_STRDUP

#ifdef DO_NEED_STRDUP
extern char *strdup (const char *xx);
#endif

/* =()<#define @<NEED_WRITEV>@_NEED_WRITEV>()= */
#define DONT_NEED_WRITEV

#ifdef DO_NEED_WRITEV
#include <sys/uio.h>
extern int writev(int fd, struct iovec *vp, int vpcount);
#endif

/* =()<#define @<ANSWER_IMMEDIATE>@_ANSWER_IMMEDIATE>()= */
#define DONT_ANSWER_IMMEDIATE

/* =()<#define @<TEI_FIXED>@_TEI_FIXED>()= */
#define DO_TEI_FIXED

/* =()<#define @<TEI_IMMEDIATE>@_TEI_IMMEDIATE>()= */
#define DONT_TEI_IMMEDIATE

/* =()<#define @<L2_DOWN>@_L2_DOWN>()= */
#define DO_L2_DOWN

/* =()<#define @<MULTI_TEI>@_MULTI_TEI>()= */
#define DONT_MULTI_TEI

/* =()<#define ROUTE_PATH "@<ROUTE_PATH>@">()= */
#define ROUTE_PATH "/sbin/route"

/* =()<#define LOCKNAME "@<LOCKNAME>@">()= */
#define LOCKNAME "/var/lock/uucp/LCK..%s"

#endif							/* _CONFIG_H */
