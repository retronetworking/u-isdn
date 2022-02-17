#ifndef __MASTER
#define __MASTER

#include <sys/types.h>
#include <stdio.h>
#include <errno.h>
#include "f_signal.h"
#include <sys/wait.h>
#include <strings.h>
#include <syslog.h>
#include <malloc.h>
#include "streams.h"
#include "sioctl.h"
#include "primitives.h"
#include "isdn_34.h"
#include "dump.h"
#include "timeout.h"

extern int fd_mon;
extern char *progname;

extern void xquit (const char *s, const char *t);

#endif
