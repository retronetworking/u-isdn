/**
 ** Streams TIMER module
 **/

#include "f_module.h"
#include "primitives.h"
#include <sys/types.h>
#include <sys/time.h>
#include "f_signal.h"
#include "f_malloc.h"
#include <sys/param.h>
#include <sys/sysmacros.h>
#include "streams.h"
#include "stropts.h"
#ifdef DONT_ADDERROR
#include "f_user.h"
#endif
#include <sys/errno.h>
#include "streamlib.h"
#include "timer.h"
#include "isdn_proto.h"


#ifdef KERNEL
#ifdef linux
#include <linux/sched.h>
#define TIME (jiffies/HZ)
#else
extern struct timeval time;
#define TIME time.tv_sec
#endif

#else
extern struct timeval Time;
#define TIME time.tv_sec
#define time Time
#endif

/*
 * Standard Streams stuff.
 */
static struct module_info timer_minfo =
{
		0, "timer", 0, INFPSZ, 4096,2048,
};

static qf_open timer_open;
static qf_close timer_close;
static qf_put timer_rput, timer_wput;
static qf_srv timer_rsrv, timer_wsrv;

static struct qinit timer_rinit =
{
		timer_rput, timer_rsrv, timer_open, timer_close, NULL, &timer_minfo, NULL
};

static struct qinit timer_winit =
{
		timer_wput, timer_wsrv, NULL, NULL, NULL, &timer_minfo, NULL
};

struct streamtab timerinfo =
{&timer_rinit, &timer_winit, NULL, NULL};

struct timer_ {
	short flags;
#define TIMER_INUSE 01
#define TIMER_TIMER 02
/* #define TIMER_INT 04 */
#define TIMER_WHEN_IN 010
#define TIMER_WHEN_OUT 020
#define TIMER_INCOMING 040
#define TIMER_IFDATA_IN 0100
#define TIMER_IFDATA_OUT 0200
	int interval;
	int pretime;
	int maxread, maxwrite;
	int lastread, lastwrite;
#ifdef NEW_TIMEOUT
	int timer;
#endif
	queue_t *qptr;
};


/* Streams code to open the driver. */
static int
timer_open (queue_t * q, dev_t dev, int flag, int sflag ERR_DECL)
{
	register struct timer_ *tim;

	if (q->q_ptr) {
		return 0;
	}
	tim = malloc(sizeof(*tim));
	if(tim == NULL)
		ERR_RETURN(-ENOMEM);
	memset(tim,0,sizeof (*tim));
	tim->qptr = q;
	WR (q)->q_ptr = (char *) tim;
	q->q_ptr = (char *) tim;

	tim->flags = TIMER_INUSE|TIMER_IFDATA_IN|TIMER_IFDATA_OUT|TIMER_WHEN_IN|TIMER_WHEN_OUT;
	tim->interval = 60*HZ;
	tim->pretime = 55*HZ;
	tim->maxread=0; tim->maxwrite=0;
	tim->lastread=tim->lastwrite=TIME;

	MORE_USE;
	return 0;
}

static void
timer_timeout(struct timer_ *tim)
{
	if(!(tim->flags & TIMER_TIMER))
		return;
	if((tim->maxread  != 0 && tim->maxread  < TIME-tim->lastread)
    && (tim->maxwrite != 0 && tim->maxwrite < TIME-tim->lastwrite)) {
		mblk_t *mb = allocb(3,BPRI_MED);
		if(mb != NULL) {
			m_putid(mb, /* (tim->flags & TIMER_INT) ? PROTO_WILL_INTERRUPT : */ PROTO_WILL_DISCONNECT);
			DATA_TYPE(mb) = MSG_PROTO;
			putnext(tim->qptr,mb);
			tim->flags &=~ TIMER_TIMER;
			return;
		}
	}
#ifdef NEW_TIMEOUT
	tim->timer = 
#endif
		timeout((void *)timer_timeout,tim,tim->interval);
}

/* Streams code to close the driver. */
static void
timer_close (queue_t * q, int dummy)
{
	register struct timer_ *tim;

	tim = (struct timer_ *) q->q_ptr;

	if(tim->flags & TIMER_TIMER) {
		tim->flags &=~ TIMER_TIMER;
#ifdef NEW_TIMEOUT
		untimeout(tim->timer);
#else
		untimeout(timer_timeout,tim);
#endif
	}

	flushq (q, FLUSHALL);
	flushq (WR (q), FLUSHALL);
	free(tim);
	LESS_USE;
	return;
}


static void
timer_proto (queue_t * q, mblk_t * mp, char down)
{
	register struct timer_ *tim = (struct timer_ *) q->q_ptr;
	streamchar *origmp = mp->b_rptr;
	ushort_t id;
	int error = 0;

    if (m_getid (mp, &id) != 0) {
        mp->b_rptr = origmp;
        putnext (q, mp);
        return;
    }
	switch (id) {
	default:
		break;
	case PROTO_TICK:
		if(tim->flags & TIMER_TIMER) {
#ifdef NEW_TIMEOUT
			untimeout(tim->timer);
#else
			untimeout(timer_timeout,tim);
#endif

#ifdef NEW_TIMEOUT
			tim->timer = 
#endif
				timeout((void *)timer_timeout,tim,tim->interval);
		}
		break;
	case PROTO_INCOMING:
		tim->flags |= TIMER_INCOMING;
		break;
	case PROTO_OUTGOING:
		tim->flags &=~ TIMER_INCOMING;
		break;
	case PROTO_CONNECTED:
		if(down) break;
		switch(tim->flags & (TIMER_TIMER|TIMER_WHEN_IN|TIMER_WHEN_OUT|TIMER_INCOMING)) {
		case TIMER_WHEN_IN|TIMER_WHEN_OUT|TIMER_INCOMING:
		case TIMER_WHEN_IN|TIMER_WHEN_OUT:
		case TIMER_WHEN_IN               |TIMER_INCOMING:
		case               TIMER_WHEN_OUT:
			if(tim->flags & TIMER_TIMER)
				break;
#ifdef NEW_TIMEOUT
			tim->timer =
#endif
				timeout((void *)timer_timeout,tim,tim->pretime);
			tim->flags |= TIMER_TIMER;
			break;
		default:
			break;
		}
		break;
	case PROTO_DISCONNECT:
	case PROTO_INTERRUPT:
		if(tim->flags & TIMER_TIMER) {
			tim->flags &=~ TIMER_TIMER;
#ifdef NEW_TIMEOUT
			untimeout(tim->timer);
#else
			untimeout(timer_timeout,tim);
#endif
		}
		break;
	case PROTO_DATA_IN:
		tim->lastread = TIME;
		break;
	case PROTO_DATA_OUT:
		tim->lastwrite = TIME;
		break;
	case PROTO_MODULE:
		if (strnamecmp (q, mp)) {	/* Config information for me. */
			long z;

			while (mp != NULL && m_getsx (mp, &id) == 0) {
				switch (id) {
				default:
					goto err;
				case PROTO_MODULE:
					break;
				case TIMER_IF_OUT:
					tim->flags |= TIMER_WHEN_OUT;
					tim->flags &=~ TIMER_WHEN_IN;
					break;
				case TIMER_IF_IN:
					tim->flags |= TIMER_WHEN_IN;
					tim->flags &=~ TIMER_WHEN_OUT;
					break;
				case TIMER_IF_BOTH:
					tim->flags |= (TIMER_WHEN_IN|TIMER_WHEN_OUT);
					break;
				case TIMER_DATA_OUT:
					tim->flags |= TIMER_IFDATA_OUT;
					tim->flags &=~ TIMER_IFDATA_IN;
					break;
				case TIMER_DATA_IN:
					tim->flags |= TIMER_IFDATA_IN;
					tim->flags &=~ TIMER_IFDATA_OUT;
					break;
				case TIMER_DATA_BOTH:
					tim->flags |= (TIMER_IFDATA_IN|TIMER_IFDATA_OUT);
					break;
				case TIMER_DATA_NONE:
					tim->flags &=~ (TIMER_IFDATA_IN|TIMER_IFDATA_OUT);
					break;
#if 0
				case TIMER_DO_DISC:
					tim->flags &=~ TIMER_INT;
					break;
				case TIMER_DO_INT:
					tim->flags |= TIMER_INT;
					break;
#endif
				case TIMER_INTERVAL:
					if ((error = m_geti (mp, &z)) != 0)
						goto err;
					if(z < 1 || z > 24*60*60)
						goto err;
					tim->interval = z*HZ;
					break;
				case TIMER_PRETIME:
					if ((error = m_geti (mp, &z)) != 0)
						goto err;
					if(z < 1 || z > 24*60*60)
						goto err;
					tim->pretime = z*HZ;
					break;
				case TIMER_READMAX:
					if ((error = m_geti (mp, &z)) != 0)
						goto err;
					if (z < 2 || z >= 7*24*60*60)
						goto err;
					tim->maxread = z;
					break;
				case TIMER_WRITEMAX:
					if ((error = m_geti (mp, &z)) != 0)
						goto err;
					if (z < 2 || z >= 7*24*60*60)
						goto err;
					tim->maxwrite = z;
					break;
				}
			}
			if (mp != NULL) {
				mp->b_rptr = origmp;
				m_reply(q,mp,0);
				mp = NULL;
			}
		}
	}
	if(mp != NULL)  {
		if(origmp != NULL)
			mp->b_rptr = origmp;
		putnext(q,mp);
	}
	return;
  err:
    mp->b_rptr = origmp;
    m_reply (q, mp, error ? error : -EINVAL);
}


/* Streams code to write data. */
static void
timer_wput (queue_t * q, mblk_t * mp)
{
	struct timer_ *tim = (struct timer_ *) q->q_ptr;

	switch (DATA_TYPE(mp)) {
	case CASE_DATA:
		if(tim->flags & TIMER_IFDATA_OUT)
			tim->lastwrite = TIME;
		/* FALL THRU */
	case MSG_PROTO:
		putq (q, mp);
		break;
	case M_FLUSH:
		if (*mp->b_rptr & FLUSHW)
			flushq (q, FLUSHDATA);
		putnext (q, mp);
		break;
	default:
		putnext (q, mp);
		break;
	}
	return;
}

/* Streams code to scan the write queue. */
static void
timer_wsrv (queue_t * q)
{
	mblk_t *mp;

	while ((mp = getq (q)) != NULL) {
		switch (DATA_TYPE(mp)) {
		case MSG_PROTO:
			timer_proto (q, mp, 1);
			break;
		default:
			if((DATA_TYPE(mp) > QPCTL) || canput (q->q_next)) {
				putnext (q, mp);
				continue;
			} else {
				putbq (q, mp);
				return;
			}
		}
	}
	return;
}

/* Streams code to read data. */
static void
timer_rput (queue_t * q, mblk_t * mp)
{
	struct timer_ *tim = (struct timer_ *) q->q_ptr;

	switch (DATA_TYPE(mp)) {
	case M_FLUSH:
		if (*mp->b_rptr & FLUSHR) {
			flushq (q, FLUSHDATA);
		}
		putnext (q, mp);		  /* send it along too */
		break;
	case CASE_DATA:
		if(tim->flags & TIMER_IFDATA_IN)
			tim->lastread = TIME;
		/* FALL THRU */
	case MSG_PROTO:
	default:
		putq (q, mp);			  /* queue it for my service routine */
		break;
	}
	return;
}

/* Streams code to scan the read queue. */
static void
timer_rsrv (queue_t * q)
{
	mblk_t *mp;

	while ((mp = getq (q)) != NULL) {
		switch (DATA_TYPE(mp)) {
		case MSG_PROTO:
			timer_proto (q, mp, 0);
			break;
		default:
			   if((DATA_TYPE(mp) > QPCTL) || canput (q->q_next)) {
				putnext (q, mp);
				continue;
			} else {
				putbq (q, mp);
				return;
			}
		}
	}
	return;
}


#ifdef MODULE
static int do_init_module(void)
{
	return register_strmod(&timerinfo);
}

static int do_exit_module(void)
{
	return unregister_strmod(&timerinfo);
}
#endif
