
/* Streams SLIP module */

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
/* #include <sys/user.h> */
#include <sys/errno.h>
#include "streamlib.h"
#include "alaw.h"
#include "isdn_proto.h"
#include "strslip.h"

#ifdef DONT_ADDERROR
#include "f_user.h"
#endif

#ifdef CONFIG_DEBUG_STREAMS
#undef qenable
static inline void qenable(queue_t *q) { deb_qenable("SLIP",0,q); }
#endif

static struct module_info slip_minfo =
{
		0, "slip", 0, INFPSZ, 200,100
};

static qf_open slip_open;
static qf_close slip_close;
static qf_put slip_wput, slip_rput;
static qf_srv slip_wsrv,slip_rsrv;

static struct qinit slip_rinit =
{
		slip_rput, slip_rsrv, slip_open, slip_close, NULL, &slip_minfo, NULL
};

static struct qinit slip_winit =
{
		slip_wput, slip_wsrv, NULL, NULL, NULL, &slip_minfo, NULL
};

struct streamtab slipinfo =
{&slip_rinit, &slip_winit, NULL, NULL};

struct _slip {
	mblk_t *msg;
	int errors;
	short nr;
	char flags;
#define SLIP_INUSE 01
#define SLIP_ESCAPE 02
#define SLIP_SENDSIG 04
};

int slip_cnt = NSLIP;

static void
slip_sendsig (struct _slip *slip, queue_t * q)
{
	printf ("slip %d: data dropped\n", slip->nr);
	if (slip->flags | SLIP_SENDSIG)
#ifdef EMT
		putctlx1 (q, M_SIG, SIGEMT);
#else
		putctlx1 (q,M_SIG, SIGIOT);
#endif
#if 0
	else
		putctlx1 (q, M_PROTO, DIAL_DROPPED);
#endif
}

static int
slip_open (queue_t * q, dev_t dev, int flag, int sflag ERR_DECL)
{
	struct _slip *slip;

	if (q->q_ptr != NULL) 
		return 0;
	
	slip = malloc(sizeof(*slip));
	if(slip == NULL)
		ERR_RETURN(-ENOSPC);
	memset(slip,0,sizeof(*slip));
	WR (q)->q_ptr = (char *) slip;
	q->q_ptr = (char *) slip;

	slip->flags = SLIP_INUSE | SLIP_SENDSIG;
	slip->errors = 0;

	MORE_USE;
	return 0;
}


static void
slip_wput (queue_t * q, mblk_t * mp)
{
	struct _slip *slip = (struct _slip *) q->q_ptr;

	switch (DATA_TYPE(mp)) {
	case M_IOCTL:
		{
			struct iocblk *iocb;

			iocb = (struct iocblk *) mp->b_rptr;

			switch (iocb->ioc_cmd) {
			case SLIP_MSGSIG:
				slip->flags |= SLIP_SENDSIG;
				goto iocack;
			case SLIP_MSGPROTO:
				slip->flags &= ~SLIP_SENDSIG;
			  iocack:
				DATA_TYPE(mp) = M_IOCACK;
				qreply (q, mp);
				break;
			default:
				putnext (q, mp);
				break;
			}
			break;
		}
	case CASE_DATA:
		putq (q, mp);
		break;
	case M_FLUSH:
		if (*mp->b_rptr & FLUSHW)
			flushq (q, FLUSHDATA);
		putnext (q, mp);
		break;
	default:
		putq (q, mp);
		break;
	}
	return;
}


#define END             0300	  /* indicates end of packet */
#define ESC             0333	  /* indicates byte stuffing */
#define ESC_END         0334	  /* ESC ESC_END means END data byte */
#define ESC_ESC         0335	  /* ESC ESC_ESC means ESC data byte */


static void
slip_wsrv (queue_t * q)
{
	mblk_t *mp;

	while ((mp = getq (q)) != NULL) {
		switch (DATA_TYPE(mp)) {
		case CASE_DATA:
			{
				mblk_t *mp2, *mq;
				uchar_t *cp, *cp2;
				int mcount = 2, ucount = 0;		/* start, end */

				if (!canput (q->q_next)) {
					putbq (q, mp);
					return;
				}
				for (mp2 = mp; mp2; mp2 = mp2->b_cont) {
					mcount += (mp2->b_wptr - mp2->b_rptr);
					ucount += (mp2->b_wptr - mp2->b_rptr);
					for (cp = (uchar_t *) mp2->b_rptr; cp < (uchar_t *) mp2->b_wptr; cp++) {
#define EXT(ch) ((ch) == END || (ch) == ESC)
						if (EXT (*cp))
							mcount++;
					}
				}

				mq = allocb (mcount, BPRI_MED);
				if (mq == NULL) {
					putbqf (q, mp);
					return;
				}
				cp2 = (uchar_t *) mq->b_wptr;
				*cp2++ = END;
				for (mp2 = mp; mp2; mp2 = mp2->b_cont) {
					for (cp = (uchar_t *) mp2->b_rptr; cp < (uchar_t *) mp2->b_wptr; cp++) {
						uchar_t ch = *cp;
        				if(EXT(ch)) {
	    					*cp2++ = ESC;
	    					if(ch == ESC)
								*cp2++ = ESC_ESC;
							else
	    						*cp2++ = ESC_END;
						} else
	    					*cp2++ = (ch);
					}
				}
				*cp2++ = END;
				(uchar_t *) mq->b_wptr = cp2;

				putnext (q, mq);
				freemsg (mp);

				continue;
			}
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


static void
slip_close (queue_t * q, int dummy)
{
	struct _slip *slip = (struct _slip *) q->q_ptr;

	if (slip->msg != NULL) {
		freemsg (slip->msg);
		slip->msg = NULL;
	}
	flushq (q, FLUSHALL);
	flushq (WR (q), FLUSHALL);
	free(slip);

	LESS_USE;
	return;
}


static void
slip_rput (queue_t * q, mblk_t * mp)
{
	struct _slip *slip = (struct _slip *) q->q_ptr;

	switch (DATA_TYPE(mp)) {

	case M_FLUSH:
		if (*mp->b_rptr & FLUSHR) {
			if (slip->msg != NULL) {
				freemsg (slip->msg);
				slip->msg = NULL;
			}
			flushq (q, FLUSHDATA);
		}
		putnext (q, mp);		  /* send it along too */
		break;

	case CASE_DATA:
		putq (q, mp);			  /* queue it for my service routine */
		break;

	case M_HANGUP:
	case M_ERROR:
	default:
		putq (q, mp);			  /* don't know what to do with this, so send
								   * it along */

	}
	return;
}

static void
slip_rsrv (queue_t * q)
{
	mblk_t *mp;
	register struct _slip *slip = (struct _slip *) q->q_ptr;

	while ((mp = getq (q)) != NULL) {
		switch (DATA_TYPE(mp)) {
		case CASE_DATA:
			{
				mblk_t *mp2, *mq = slip->msg;
				uchar_t *cp;

				while (mp != NULL) {
					for (cp = (uchar_t *) mp->b_rptr; cp < (uchar_t *) mp->b_wptr; cp++) {
						if (*cp == END) {
							if(!canput(q->q_next)) 
								goto out;
							if (slip->flags & SLIP_ESCAPE) {
								slip->flags &= ~SLIP_ESCAPE;
							}
							if (mq != NULL && mq->b_rptr != mq->b_wptr) {
								mp2 = copyb (mq);
								if (mp2 == NULL) {
								  outagain:
									timeout((void *)qenable,q,HZ/10);
								  out:
									(uchar_t *) mp->b_rptr = cp;
									putbq (q, mp);
									return;
								}
								putnext (q, mp2);
							} else if (mq != NULL) {
								if (mq->b_rptr != mq->b_wptr) {
									slip->errors++;
								}
							}
						  breakpacket:
							if (mq == NULL) {
								mq = allocb (SLIP_MAX, BPRI_MED);
								if (mq == NULL) {
									goto outagain;
								}
								slip->msg = mq;
							} else
								mq->b_wptr = mq->b_rptr;
							continue;
						} else if (slip->msg == NULL) {
							continue;
						} else if (slip->flags & SLIP_ESCAPE) {
							slip->flags &= ~SLIP_ESCAPE;
							if (*cp == ESC_END)
								*cp = END;
							else if (*cp == ESC_ESC)
								*cp = ESC;
							else
								goto breakpacket;
							goto realchar;
						} else if (*cp == ESC) {
							slip->flags |= SLIP_ESCAPE;
						} else
						  realchar:
						if (mq != NULL && mq->b_wptr - mq->b_rptr < SLIP_MAX) {
							*mq->b_wptr++ = *cp;
						} else
							goto breakpacket;
					}
					mp2 = mp->b_cont;
					freeb (mp);
					mp = mp2;
				}
				break;
			}
		case M_HANGUP:
			slip_sendsig(slip,q);
			putnext(q,mp);
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
	return register_strmod(&slipinfo);
}

static int do_exit_module(void)
{
	return unregister_strmod(&slipinfo);
}
#endif
