#ifndef __TIMEOUT
#define __TIMEOUT

#include <sys/types.h>
#include <sys/time.h>
#include <sys/callout.h>

extern struct timeval callout_time;

void callout_sync ();
void callout_async ();

void callout_timeout (void (*func) (void *), void *arg, int tim);
void callout_untimeout (void (*func) (void *), void *arg);

/* sync case only */
void callout_adjtimeout (void);
void callout_alarm (void);
char callout_hastimer (void);

#define c_dump() callout_dump(__FILE__,__LINE__)
void callout_dump (char *file, unsigned int line);

#endif
