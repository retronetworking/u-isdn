/**
 ** Streams RECONN module
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
#include "reconnect.h"
#include "isdn_proto.h"

extern void log_printmsg (void *log, const char *text, mblk_t * mp, const char*);

/*
 * Standard Streams stuff.
 */
static struct module_info reconn_minfo =
{
		0, "reconn", 0, INFPSZ, 200,100
};

static qf_open reconn_open;
static qf_close reconn_close;
static qf_put reconn_rput, reconn_wput;
static qf_srv reconn_rsrv, reconn_wsrv;

static struct qinit reconn_rinit =
{
		reconn_rput, reconn_rsrv, reconn_open, reconn_close, NULL, &reconn_minfo, NULL
};

static struct qinit reconn_winit =
{
		reconn_wput, reconn_wsrv, NULL, NULL, NULL, &reconn_minfo, NULL
};

struct streamtab reconninfo =
{&reconn_rinit, &reconn_winit, NULL, NULL};

struct reconn_ {
	short flags;
#define RECONN_INUSE 01
#define RECONN_INITED 02
#define RECONN_HIGHER_INTR 04
#define RECONN_LOWER_WANTED 010
#define RECONN_DISABLED 020
#define RECONN_PRINTFIRST 040
#define RECONN_FIRSTRECV 0100
#define RECONN_FIRSTSEND 0200
	enum { r_off,r_on,r_going_down } lowerstate; 
	queue_t *qptr;
};


/* Streams code to open the driver. */
static int
reconn_open (queue_t * q, dev_t dev, int flag, int sflag ERR_DECL)
{
	register struct reconn_ *reco;

	if (q->q_ptr) {
		return 0;
	}
	reco = malloc(sizeof(*reco));
	if(reco == NULL)
		ERR_RETURN(-ENOMEM);
	memset(reco,0,sizeof(*reco));
	reco->qptr = q;
	WR (q)->q_ptr = (char *) reco;
	q->q_ptr = (char *) reco;

	reco->lowerstate = r_off;
	reco->flags = RECONN_INUSE|RECONN_INITED;

	MORE_USE;
	return 0;
}


/* Streams code to close the driver. */
static void
reconn_close (queue_t * q, int dummy)
{
	register struct reconn_ *reco;

	reco = (struct reconn_ *) q->q_ptr;

	flushq (q, FLUSHALL);
	flushq (WR (q), FLUSHALL);
	free(reco);
	LESS_USE;
	return;
}


static void
reconn_proto (queue_t * q, mblk_t * mp, char down)
{
	register struct reconn_ *reco = (struct reconn_ *) q->q_ptr;
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
	case PROTO_LISTEN:
		if(down) goto drop;
		reco->lowerstate = r_off;
		id = PROTO_HAS_LISTEN;
		q = OTHERQ(q);
		goto doit;
	case PROTO_INCOMING:
	case PROTO_OUTGOING:
		if(down)
			goto drop;
		if(reco->flags & RECONN_INITED) {
			reco->lowerstate = r_off;
			mp->b_rptr = origmp;
			putnext(q,mp);
			mp = allocb(3,BPRI_MED);
			if(mp != NULL) {
				origmp = NULL;
				DATA_TYPE(mp) = MSG_PROTO;
				m_putid(mp,PROTO_CONNECTED);
			}
		} else
			goto drop;
		break;
	case PROTO_CONNECTED:
		if(down)
			goto drop;
		reco->lowerstate = r_on;
		if(reco->flags & (RECONN_DISABLED|RECONN_INITED)) {
			mblk_t *mb;
			reco->flags &=~ (RECONN_DISABLED|RECONN_INITED);
			mb = allocb(3,BPRI_MED);
			if(mb != NULL) {
				m_putid(mb,PROTO_CONNECTED);
				DATA_TYPE(mb) = MSG_PROTO;
				putnext(q,mb);
			}
		}
		if(reco->flags & RECONN_PRINTFIRST) {
			if(!(reco->flags & RECONN_LOWER_WANTED))
				reco->flags |= RECONN_FIRSTSEND;
			reco->flags |= RECONN_FIRSTRECV;
		}
		reco->flags &= ~RECONN_LOWER_WANTED;
		qenable(WR(reco->qptr));
		q = OTHERQ(q);
		id = PROTO_HAS_CONNECTED;
		goto doit;
	case PROTO_ENABLE:
		if(down)
			goto drop;
		if(reco->flags & (RECONN_DISABLED|RECONN_INITED)) {
			mblk_t *mb;
			reco->flags &=~ (RECONN_DISABLED|RECONN_INITED);
			mb = allocb(3,BPRI_MED);
			if(mb != NULL) {
				m_putid(mb,PROTO_CONNECTED);
				DATA_TYPE(mb) = MSG_PROTO;
				putnext(q,mb);
			}
		}
		reco->flags &= ~RECONN_LOWER_WANTED;
		qenable(WR(reco->qptr));
		q = OTHERQ(q);
		id = PROTO_HAS_ENABLE;
		goto doit;
	case PROTO_DISABLE:
		if(down)
			goto drop;
		if((reco->flags & RECONN_INITED) || !(reco->flags & RECONN_DISABLED)) {
			mblk_t *mb;
			reco->flags |= RECONN_DISABLED;
			reco->flags &= ~RECONN_INITED;
			mb = allocb(3,BPRI_MED);
			if(mb != NULL) {
				m_putid(mb,PROTO_INTERRUPT);
				DATA_TYPE(mb) = MSG_PROTO;
				putnext(q,mb);
			}
		}
		reco->flags &= ~RECONN_LOWER_WANTED;
		flushq(WRQ(q),FLUSHDATA);
		q = OTHERQ(q);
		id = PROTO_HAS_DISABLE;
		goto doit;
	case PROTO_HAS_CONNECTED:
		if(reco->lowerstate != r_on && (reco->flags & RECONN_LOWER_WANTED)) {
			id = PROTO_WANT_CONNECTED;
			goto doit;
		} else
			goto drop;
		break;
	case PROTO_DISCONNECT:
		if(down) 
			goto drop;
		reco->flags &=~ (RECONN_FIRSTRECV|RECONN_FIRSTSEND);
		q = OTHERQ(q);
		id = PROTO_HAS_DISCONNECT;
		reco->lowerstate = r_off;
		if(reco->flags & RECONN_LOWER_WANTED) {
			mp->b_rptr = mp->b_wptr = origmp;
			m_putid(mp,id);
			putnext(q,mp);
			mp = allocb(3,BPRI_MED);
			if(mp != NULL) {
				origmp = NULL;
				DATA_TYPE(mp) = MSG_PROTO;
				m_putid(mp,PROTO_WANT_CONNECTED);
			}
			break;
		} else
			goto doit;
	case PROTO_INTERRUPT:
		if(!down)
			goto drop;
		reco->lowerstate = r_off;
		id = PROTO_HAS_INTERRUPT;
		q = OTHERQ(q);
		goto doit;
	case PROTO_WILL_DISCONNECT:
		q = OTHERQ(q);
		id = PROTO_DISCONNECT;
		reco->lowerstate = r_going_down;
	  doit: {
			mblk_t *mb = allocb(3,BPRI_MED);
			if(mb != NULL) {
				freemsg(mp);
				mp = mb;
				origmp = NULL;
				DATA_TYPE(mb) = MSG_PROTO;
				m_putid(mp,id);
			}
		}
		break;
	case PROTO_MODULE:
		if (strnamecmp (q, mp)) {	/* Config information for me. */
			while (mp != NULL && m_getsx (mp, &id) == 0) {
				switch (id) {
				case RECONNECT_DOPRINT:
					reco->flags |= RECONN_PRINTFIRST;
					break;
				case RECONNECT_NOPRINT:
					reco->flags &=~ RECONN_PRINTFIRST;
					break;
				case PROTO_MODULE:
					break;
				default:
					goto err;
				}
			}
			if (mp != NULL) {
				mp->b_rptr = origmp;
				m_reply(q,mp,0);
				mp = NULL;
			}
		}
		break;
	}
	if(mp != NULL)  {
		if(origmp != NULL)
			mp->b_rptr = origmp;
		putnext(q,mp);
	}
	return;
  drop:
  	if(mp != NULL)
  		freemsg(mp);
	return;
  err:
    mp->b_rptr = origmp;
    m_reply (q, mp, error ? error : -EINVAL);
}


/* Streams code to write data. */
static void
reconn_wput (queue_t * q, mblk_t * mp)
{
	switch (DATA_TYPE(mp)) {
	case CASE_DATA:
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
reconn_wsrv (queue_t * q)
{
	register struct reconn_ *reco = (struct reconn_ *) q->q_ptr;
	mblk_t *mp;

	while ((mp = getq (q)) != NULL) {
		switch (DATA_TYPE(mp)) {
		case MSG_PROTO:
			reconn_proto (q, mp, 1);
			break;

		case M_FLUSH:
			if (*mp->b_rptr & FLUSHW)
				flushq (q, FLUSHDATA);
			goto def;
		case CASE_DATA:
			if(reco->flags & RECONN_DISABLED) {
				freemsg(mp);
				continue;
			}
			switch(reco->lowerstate) {
			case r_off:
				putbq(q,mp);
				if(!(reco->flags & RECONN_LOWER_WANTED)) {
					if(reco->flags & RECONN_PRINTFIRST)
						log_printmsg(NULL,"WantConn",mp,KERN_DEBUG);
					if((mp = allocb(3,BPRI_HI)) != NULL) {
						reco->flags |= RECONN_LOWER_WANTED;
						m_putid(mp,PROTO_WANT_CONNECTED);
						DATA_TYPE(mp) = MSG_PROTO;
						putnext(q,mp);
					} else
						qretry(q);
				}
				return;
			case r_on:
				if(reco->flags & RECONN_FIRSTSEND) {
					reco->flags &=~ RECONN_FIRSTSEND;
					log_printmsg(NULL,"SendOne",mp,KERN_DEBUG);
				}
				break;
			case r_going_down:
				putbq(q,mp);
#if 1
				log_printmsg(NULL,"WantConn",mp,KERN_DEBUG);
#endif
				reco->flags |= RECONN_LOWER_WANTED;
				return;
			}
		default:
		def:
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
reconn_rput (queue_t * q, mblk_t * mp)
{
	switch (DATA_TYPE(mp)) {

	case M_FLUSH:
		if (*mp->b_rptr & FLUSHR) {
			flushq (q, FLUSHDATA);
		}
		putnext (q, mp);		  /* send it along too */
		break;
	case CASE_DATA:
		/* FALL THRU */
	case MSG_PROTO:
		putq (q, mp);			  /* queue it for my service routine */
		break;

	default:
		putq (q, mp);
	}
	return;
}

/* Streams code to scan the read queue. */
static void
reconn_rsrv (queue_t * q)
{
	mblk_t *mp;
	register struct reconn_ *reco = (struct reconn_ *) q->q_ptr;

	while ((mp = getq (q)) != NULL) {
		switch (DATA_TYPE(mp)) {
		case MSG_PROTO:
			reconn_proto (q, mp, 0);
			break;
		case CASE_DATA:
			if(reco->flags & RECONN_FIRSTRECV) {
				reco->flags &=~ RECONN_FIRSTRECV;
				log_printmsg(NULL,"RecvOne",mp,KERN_DEBUG);
			}
			/* FALL THRU */
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
	return register_strmod(&reconninfo);
}

static int do_exit_module(void)
{
	return unregister_strmod(&reconninfo);
}
#endif
