
/* Streams countging module */

#include "f_module.h"
#include "primitives.h"
#include "kernel.h"
#include "f_signal.h"
#include "f_malloc.h"
#include "stropts.h"
#ifdef DONT_ADDERROR
#include "f_user.h"
#endif
#include "streams.h"
#include "streamlib.h"

#define MAXB 100

#if !defined(KERNEL)
#include "kernel.h"
extern struct timeval Time;

#define time Time
#else

#ifdef linux
#ifdef time
#undef time
#endif
#define time Time
#else
extern struct timeval time;
#endif
#endif

#define CFLAG_INUSE 01
#define CFLAG_LOGUP 02
#define CFLAG_LOGDOWN 04

static struct module_info count_minfo =
{
		0, "count", 0, INFPSZ, 0, 0
};

static qf_open count_open;
static qf_close count_close;
static qf_put count_rput, count_wput;

static struct qinit count_rinit =
{
		count_rput, NULL, count_open, count_close, NULL, &count_minfo, NULL
};

static struct qinit count_winit =
{
		count_wput, NULL, NULL, NULL, NULL, &count_minfo, NULL
};

struct streamtab countinfo =
{&count_rinit, &count_winit, NULL, NULL};

#include "count.h"
#include "isdn_limits.h"

struct _count {
	char flags;
	int nr;
	long wnum, rnum, wsize, rsize;
	struct timeval last;
};

static int
count_open (queue_t * q, dev_t dev, int flag, int sflag ERR_DECL)
{
	struct _count *count;
	static int nr = 1;

	if (q->q_ptr)
		return 0;

	count = malloc(sizeof(*count));
	if(count == NULL)
		ERR_RETURN(-ENOMEM);
	memset(count,0,sizeof *count);

	WR (q)->q_ptr = (char *) count;
	q->q_ptr = (char *) count;

	count->flags = CFLAG_INUSE|CFLAG_LOGUP|CFLAG_LOGDOWN;
	count->nr = nr++;
	count->wnum = count->rnum = 0;
	count->wsize = count->rsize = 0;
	MORE_USE;

	return 0;
}

int
mtim (struct _count *ct)
{
	int xx;

#ifndef KERNEL
	struct timezone tz;

	gettimeofday (&Time, &tz);
#else
#ifdef linux
	struct timeval Time;

        Time.tv_usec = (jiffies % HZ) * (1000000 / HZ);
        Time.tv_sec = jiffies / HZ;

#endif
#endif
	xx = ((time.tv_sec - ct->last.tv_sec) * 100 +
			(time.tv_usec - ct->last.tv_usec) / 10000);
	ct->last = time;
	return xx;

}

static void
count_wput (queue_t * q, mblk_t * mp)
{
	int thissize;
	register struct _count *count;

	count = (struct _count *) q->q_ptr;

	thissize = dsize (mp);
	if(count->flags & CFLAG_LOGDOWN)
		printf ("%d<%d.%d ", count->nr, thissize, mtim (count));
	count->wsize += thissize;
	count->wnum++;

	putnext (q, mp);
	return;
}

static void
count_rput (queue_t * q, mblk_t * mp)
{
	int thissize;
	register struct _count *count;

	count = (struct _count *) q->q_ptr;

	thissize = dsize (mp);
	if(count->flags & CFLAG_LOGUP)
		printf ("%d>%d.%d ", count->nr, thissize, mtim (count));
	count->rnum++;
	count->rsize += thissize;

	putnext (q, mp);
	return;
}


static void
count_close (queue_t * q, int dummy)
{
	struct _count *count;

	count = (struct _count *) q->q_ptr;

	flushq (q, FLUSHALL);
	flushq (WR (q), FLUSHALL);
	printf ("Counter %d: out %ld/%ld, in %ld/%ld.\n", count->nr,
			count->wnum, count->wsize, count->rnum, count->rsize);
	free(count);
	LESS_USE;
	return;
}

#ifdef MODULE
static int do_init_module(void)
{
	return register_strmod(&countinfo);
}

static int do_exit_module(void)
{
	return unregister_strmod(&countinfo);
}
#endif
