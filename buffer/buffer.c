/**
 ** Streams BUF module
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
#include "buffer.h"
#include "isdn_proto.h"


/*
 * Standard Streams stuff.
 */
static struct module_info buf_minfo =
{
		0, "buffer", 0, INFPSZ, 4096, 2048
};

static qf_open buf_open;
static qf_close buf_close;
static qf_put buf_rput,buf_wput;
static qf_srv buf_rsrv,buf_wsrv;

static struct qinit buf_rinit =
{
		buf_rput, buf_rsrv, buf_open, buf_close, NULL, &buf_minfo, NULL
};

static struct qinit buf_winit =
{
		buf_wput, buf_wsrv, NULL, NULL, NULL, &buf_minfo, NULL
};

struct streamtab bufinfo =
{&buf_rinit, &buf_winit, NULL, NULL};

/* Streams code to open the driver. */
static int
buf_open (queue_t * q, dev_t dev, int flag, int sflag ERR_DECL)
{
	if (q->q_ptr) {
		return 0;
	}
	/* do nothing... */
	return 0;
}

/* Streams code to close the driver. */
static void
buf_close (queue_t * q, int dummy)
{
	flushq (q, FLUSHALL);
	flushq (WR (q), FLUSHALL);
	return;
}


static void
buf_proto (queue_t * q, mblk_t * mp, char down)
{
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
	case PROTO_MODULE:
		if (strnamecmp (q, mp)) {	/* Config information for me. */
			long z;

			while (mp != NULL && m_getsx (mp, &id) == 0) {
				switch (id) {
				default:
					goto err;
				case PROTO_MODULE:
					break;
				case BUF_UP:
					if ((error = m_geti (mp, &z)) != 0)
						goto err;
					if ((z < 10) || (z > 32*1024))
						goto err;
					RDQ(q)->q_flag |= QWANTR;
					RDQ(q)->q_hiwat = z;
					RDQ(q)->q_lowat = z>>2;
					qenable(RDQ(q));
					break;
				case BUF_DOWN:
					if ((error = m_geti (mp, &z)) != 0)
						goto err;
					if ((z < 10) || (z > 32*1024))
						goto err;
					WRQ(q)->q_flag |= QWANTR;
					WRQ(q)->q_hiwat = z;
					WRQ(q)->q_lowat = z>>2;
					qenable(WRQ(q));
					break;
				case BUF_BOTH:
					if ((error = m_geti (mp, &z)) != 0)
						goto err;
					if ((z < 10) || (z > 32*1024))
						goto err;
					RDQ(q)->q_flag |= QWANTR;
					WRQ(q)->q_flag |= QWANTR;
					RDQ(q)->q_hiwat = z;
					WRQ(q)->q_hiwat = z;
					RDQ(q)->q_lowat = z>>2;
					WRQ(q)->q_lowat = z>>2;
					qenable(RDQ(q));
					qenable(WRQ(q));
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
		if (origmp != NULL)
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
buf_wput (queue_t * q, mblk_t * mp)
{
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
buf_wsrv (queue_t * q)
{
	mblk_t *mp;

	while ((mp = getq (q)) != NULL) {
		switch (DATA_TYPE(mp)) {
		case MSG_PROTO:
			buf_proto (q, mp, 1);
			break;
		case CASE_DATA:
			if (!canput (q->q_next)) {
				putbq (q, mp);
				return;
			}
			putnext (q, mp);
			break;
		case M_FLUSH:
			if (*mp->b_rptr & FLUSHW)
				flushq (q, FLUSHDATA);
			/* FALL THRU */
		default:
			if (DATA_TYPE(mp) > QPCTL || canput (q->q_next)) {
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
buf_rput (queue_t * q, mblk_t * mp)
{
	switch (DATA_TYPE(mp)) {

	case M_FLUSH:
		if (*mp->b_rptr & FLUSHR) {
			flushq (q, FLUSHDATA);
		}
		putnext (q, mp);		  /* send it along too */
		break;
	case MSG_PROTO:
	case CASE_DATA:
		putq (q, mp);			  /* queue it for my service routine */
		break;

	default:
		putq (q, mp);
	}
	return;
}

/* Streams code to scan the read queue. */
static void
buf_rsrv (queue_t * q)
{
	mblk_t *mp;

	while ((mp = getq (q)) != NULL) {
		switch (DATA_TYPE(mp)) {
		case MSG_PROTO:
			buf_proto (q, mp, 0);
			break;
		case CASE_DATA:
			if (!canput (q->q_next)) {
				putbq (q, mp);
				return;
			}
			putnext (q, mp);
			break;
		case M_HANGUP:
		case M_ERROR:
			/* FALL THRU */
		default:
			if (DATA_TYPE(mp) > QPCTL || canput (q->q_next)) {
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
	return register_strmod(&bufinfo);
}

static int do_exit_module(void)
{
	return unregister_strmod(&bufinfo);
}
#endif
