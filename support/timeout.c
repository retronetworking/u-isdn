#include "primitives.h"
#include <stdio.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/uio.h>
#include <errno.h>
#include "f_ioctl.h"
#include <sys/sysmacros.h>
#include "f_signal.h"
#include <sys/wait.h>
#include "f_strings.h"
#include <syslog.h>
#include <sys/time.h>
#include <sys/callout.h>
#include <malloc.h>
#include <sys/param.h>
#include "timeout.h"
#include "kernel.h"

struct timeval callout_time;
struct callout *callout_list = NULL;
struct timeval callout_schedtime; /* Time last timeout was set */
int callout_doasync = 0;
int in_timeout = 0;

void
callout_settimeout (void)
{
	SIG_TYPE sm;
	SIG_SUSPEND (sm,SIGALRM);

	if (callout_list == NULL) {
		SIG_RESUME (sm);
		return;
	}
	if (callout_list->c_time < 0)
		callout_list->c_time = 0;
	callout_time.tv_sec = callout_list->c_time / 1000;
	callout_time.tv_usec = (callout_list->c_time % 1000) * 1000;
	if (gettimeofday (&callout_schedtime, NULL)) {
		syslog (LOG_CRIT,"gettimeofday: %m");
		return;
	}
	if (callout_doasync) {
		struct itimerval itm;

		timerclear (&itm.it_interval);
		itm.it_value = callout_time;
		setitimer (ITIMER_REAL, &itm, NULL);
	}
	SIG_RESUME (sm);
}

void
callout_timeout (void (*func) (void *), void *arg, int tim)
{
	struct callout *newp, **oldpp;
	SIG_TYPE sm;
	SIG_SUSPEND(sm,SIGALRM);

	if (0)
		syslog (LOG_DEBUG, "Timeout %x:%x in %d.%03d seconds.", (int) func, (int) arg, tim / 1000, tim % 1000);

	/*
	 * Allocate timeout.
	 */
	if ((newp = (struct callout *) malloc (sizeof (struct callout))) == NULL) {
		syslog (LOG_CRIT, "Out of memory!");
		return;
	}
	newp->c_arg = arg;
	(void *) newp->c_func = func;

	/*
	 * Find correct place to link it in and decrement its time by the amount of
	 * time used by preceding timeouts.
	 */
	for (oldpp = &callout_list;
			*oldpp && (*oldpp)->c_time <= tim;
			oldpp = &(*oldpp)->c_next)
		tim -= (*oldpp)->c_time;
	newp->c_time = tim;
	newp->c_next = *oldpp;
	if (*oldpp)
		(*oldpp)->c_time -= tim;
	*oldpp = newp;

	/*
	 * If this is now the first callout then we have to set a new itimer.
	 */
	if (callout_list == newp)
		callout_settimeout ();

	SIG_RESUME (sm);
}

void
callout_untimeout (void (*func) (void *), void *arg)
{
	struct callout **copp, *freep;
	int reschedule = 0;
	SIG_TYPE sm;
	SIG_SUSPEND(sm,SIGALRM);

	if (0)
		syslog (LOG_DEBUG, "Untimeout %x:%x.", (int) func, (int) arg);

	/*
	 * If the first callout is unscheduled then we have to set a new itimer.
	 */
	if (callout_list &&
			(void *) callout_list->c_func == func &&
			callout_list->c_arg == arg)
		reschedule = 1;

	/*
	 * Find first matching timeout.  Add its time to the next timeouts time.
	 */
	for (copp = &callout_list; *copp; copp = &(*copp)->c_next)
		if ((void *) (*copp)->c_func == func &&
				(*copp)->c_arg == arg) {
			freep = *copp;
			*copp = freep->c_next;
			if (*copp)
				(*copp)->c_time += freep->c_time;
			free (freep);
			break;
		}
	if (reschedule && callout_list != NULL)
		callout_settimeout ();
	SIG_RESUME (sm);
}


void
callout_adjtimeout (void)
{
	int timediff;
	struct timeval tv;
	SIG_TYPE sm;
	SIG_SUSPEND(sm,SIGALRM);

	if (callout_list == NULL) {
		SIG_RESUME (sm);
		return;
	}
	/*
	 * Make sure that the clock hasn't been warped dramatically. Account for
	 * recently expired, but blocked timer by adding small fudge factor.
	 */
	if (gettimeofday (&tv, NULL)) {
		syslog (LOG_ERR, " gettimeofday");
		_exit (1);
	}
	timediff = (tv.tv_sec - callout_schedtime.tv_sec) * 1000
			+ (tv.tv_usec - callout_schedtime.tv_usec) / 1000;
	if (timediff < 0 ||
			timediff > callout_list->c_time + 60 * 1000)	/* One minute of fudge */
		return;

	callout_list->c_time -= timediff;	/* OK, Adjust time */
	if (timediff != 0)
		callout_settimeout ();
	SIG_RESUME (sm);
}


void
callout_alarm (void)
{
	struct callout *freep;
	SIG_TYPE sm;

	if(in_timeout++)
		return;

	SIG_SUSPEND(sm, SIGALRM);

	/* syslog(LOG_DEBUG,"Alarm"); */

	/*
	 * Call and free first scheduled timeout and any that were scheduled for
	 * the same time.
	 */
	while (callout_list) {
		freep = callout_list;	  /* Remove entry before calling */
		callout_list = freep->c_next;
		/* syslog(LOG_DEBUG,"Call %x(%x)",freep->c_func,freep->c_arg); */
		(*freep->c_func) (freep->c_arg);
		free (freep);
		if (callout_list != NULL && callout_list->c_time > 0)
			break;
	}
	in_timeout--;

	if (callout_list)
		callout_settimeout ();
	SIG_RESUME (sm);
}

void
callout_sync (void)
{
	SIG_TYPE sm;
	SIG_SUSPEND(sm,SIGALRM);

	if (callout_doasync) {
		struct itimerval itm;

		callout_doasync = 0;

		timerclear (&itm.it_interval);
		timerclear (&itm.it_value);
		setitimer (ITIMER_REAL, &itm, NULL);

		bsd_signal (SIGALRM, SIG_DFL);
	}
	SIG_RESUME (sm);
}

void
callout_async (void)
{
	SIG_TYPE sm;
	SIG_SUSPEND(sm,SIGALRM);

	if (!callout_doasync) {
		callout_doasync = 1;
		bsd_signal (SIGALRM, (sigfunc__t) callout_alarm);
		callout_settimeout ();
	}
	SIG_RESUME (sm);
}


void
timeout (void *a, void *b, int c)
{
	callout_timeout ((void (*) (void *))a, b, (c * 1000) / HZ);
}

void
untimeout (void *a, void *b)
{
	callout_untimeout ((void (*) (void *))a, b);
}

char
callout_hastimer (void)
{
	return callout_list != NULL;
}

void
callout_dump (char *deb_file, unsigned int deb_line)
{
	struct callout *cl = callout_list;

	while (cl) {
		if (cl->c_func == NULL) {
			fprintf (stderr, "%s:%d: F %p arg %p; ", deb_file, deb_line, cl, cl->c_arg);
			while (1) ;
		}
		cl = cl->c_next;
	}
}
