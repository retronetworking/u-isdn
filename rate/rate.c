/**
 ** Streams RATE module
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
#include "rate.h"
#include "isdn_proto.h"


/*
 * Standard Streams stuff.
 */
static struct module_info rate_minfo =
{
		0, "rate", 0, INFPSZ, 4096,2048,
};

static qf_open rate_open;
static qf_close rate_close;
static qf_put rate_rput, rate_wput;
static qf_srv rate_rsrv, rate_wsrv;

static struct qinit rate_rinit =
{
		rate_rput, rate_rsrv, rate_open, rate_close, NULL, &rate_minfo, NULL
};

static struct qinit rate_winit =
{
		rate_wput, rate_wsrv, NULL, NULL, NULL, &rate_minfo, NULL
};

struct streamtab rateinfo =
{&rate_rinit, &rate_winit, NULL, NULL};

struct rate_ {
	char flags;
#define RATE_INUSE 01
#define RATE_TIMER 02
#define RATE_INT 04
#define RATE_WHEN_IN 010
#define RATE_WHEN_OUT 020
#define RATE_INCOMING 040
	long cur_in, cur_out;
	long allow_in, allow_out;
#ifdef NEW_TIMEOUT
	int timer;
#endif
	queue_t *qptr;
};


/* Streams code to open the driver. */
static int
rate_open (queue_t * q, dev_t dev, int flag, int sflag ERR_DECL)
{
	register struct rate_ *rat;

	if (q->q_ptr) {
		return 0;
	}
	rat = malloc(sizeof(*rat));
	if(rat == NULL)
		ERR_RETURN(-ENOMEM);
	memset(rat,0,sizeof (*rat));
	rat->qptr = q;
	WR (q)->q_ptr = (char *) rat;
	q->q_ptr = (char *) rat;

	rat->flags = RATE_INUSE;
	rat->cur_in = 0;
	rat->cur_out = 0;
	rat->allow_in = 0;
	rat->allow_out = 0;

	MORE_USE;
	return 0;
}

static void
rate_timeout(struct rate_ *rat)
{
	if(!(rat->flags & RATE_TIMER))
		return;

	if(rat->allow_in > 0) {
		if(rat->cur_in < rat->allow_in) {
			rat->cur_in = 0;
		} else {
			rat->cur_in -= rat->allow_in;
			if(rat->cur_in < rat->allow_in)
				qenable(rat->qptr);
		}
	}
	if(rat->allow_out > 0) {
		if(rat->allow_out > rat->cur_out) {
			rat->cur_out = 0;
		} else {
			rat->cur_out -= rat->allow_out;
			if(rat->cur_out < rat->allow_out)
				qenable(WR(rat->qptr));
		}
	}

#ifdef NEW_TIMEOUT
	rat->timer = 
#endif
		timeout((void *)rate_timeout,rat,HZ);
}

/* Streams code to close the driver. */
static void
rate_close (queue_t * q, int dummy)
{
	register struct rate_ *rat;

	rat = (struct rate_ *) q->q_ptr;

	if(rat->flags & RATE_TIMER) {
		rat->flags &=~ RATE_TIMER;
#ifdef NEW_TIMEOUT
		untimeout(rat->timer);
#else
		untimeout(rate_timeout,rat);
#endif
	}

	flushq (q, FLUSHALL);
	flushq (WR (q), FLUSHALL);
	free(rat);
	LESS_USE;
	return;
}


static void
rate_proto (queue_t * q, mblk_t * mp, char down)
{
	register struct rate_ *rat = (struct rate_ *) q->q_ptr;
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
	case PROTO_CONNECTED:
		if(down) break;
		if(!(rat->flags & RATE_TIMER)) {
			rat->flags |= RATE_TIMER;
#ifdef NEW_TIMEOUT
			rat->timer =
#endif
				timeout((void *)rate_timeout,rat,HZ);
		}
		break;
	case PROTO_DISCONNECT:
	case PROTO_INTERRUPT:
		if(rat->flags & RATE_TIMER) {
			rat->flags &=~ RATE_TIMER;
#ifdef NEW_TIMEOUT
			untimeout(rat->timer);
#else
			untimeout(rate_timeout,rat);
#endif
		}
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
				case RATE_IN:
					if ((error = m_geti (mp, &z)) != 0)
						goto err;
					if(z < 200)
						goto err;
					rat->allow_in = z;
					break;
				case RATE_OUT:
					if ((error = m_geti (mp, &z)) != 0)
						goto err;
					if(z < 200)
						goto err;
					rat->allow_out = z;
					break;
				case RATE_INOUT:
					if ((error = m_geti (mp, &z)) != 0)
						goto err;
					if(z < 200)
						goto err;
					rat->allow_in = z;
					rat->allow_out= z;
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
rate_wput (queue_t * q, mblk_t * mp)
{
	/* struct rate_ *rat = (struct rate_ *) q->q_ptr; */

	switch (DATA_TYPE(mp)) {
	case MSG_PROTO:
	case CASE_DATA:
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
rate_wsrv (queue_t * q)
{
	mblk_t *mp;
	struct rate_ *rat = (struct rate_ *) q->q_ptr;

	while ((mp = getq (q)) != NULL) {
		switch (DATA_TYPE(mp)) {
		case MSG_PROTO:
			rate_proto (q, mp, 1);
			break;
		case CASE_DATA:
			if(!canput (q->q_next)) {
				putbq(q,mp);
				return;
			}
			if((rat->flags & RATE_TIMER) && (rat->allow_out > 0)) {
				rat->cur_out += msgdsize(mp);
				putnext(q,mp);
				if(rat->cur_out > rat->allow_out) 
					return;
				break;
			} /* else FALL THRU */
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
rate_rput (queue_t * q, mblk_t * mp)
{
	/* struct rate_ *rat = (struct rate_ *) q->q_ptr; */

	switch (DATA_TYPE(mp)) {
	case M_FLUSH:
		if (*mp->b_rptr & FLUSHR) {
			flushq (q, FLUSHDATA);
		}
		putnext (q, mp);		  /* send it along too */
		break;
	case MSG_PROTO:
	default:
		putq (q, mp);			  /* queue it for my service routine */
		break;
	}
	return;
}

/* Streams code to scan the read queue. */
static void
rate_rsrv (queue_t * q)
{
	mblk_t *mp;
	struct rate_ *rat = (struct rate_ *) q->q_ptr;

	while ((mp = getq (q)) != NULL) {
		switch (DATA_TYPE(mp)) {
		case MSG_PROTO:
			rate_proto (q, mp, 0);
			break;
		case CASE_DATA:
			if(!canput (q->q_next)) {
				putbq(q,mp);
				return;
			}
			if((rat->flags & RATE_TIMER) && (rat->allow_in > 0)) {
				rat->cur_in += msgdsize(mp);
				putnext(q,mp);
				if(rat->cur_in > rat->allow_in) 
					return;
				break;
			} /* else FALL THRU */
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
	return register_strmod(&rateinfo);
}

static int do_exit_module(void)
{
	return unregister_strmod(&rateinfo);
}
#endif
