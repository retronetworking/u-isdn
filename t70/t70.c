/**
 ** Streams T70 module
 **/

#include "f_module.h"
#include "primitives.h"
#include "kernel.h"
#include "f_signal.h"
#include "f_malloc.h"
#include "streams.h"
#include "stropts.h"
#ifdef DONT_ADDERROR
#include "f_user.h"
#endif
#include "streamlib.h"
#include "t70.h"
#include "isdn_proto.h"


/*
 * Standard Streams stuff.
 */
static struct module_info t70_minfo =
{
		0, "t70", 0, INFPSZ, 200,100
};

static qf_open t70_open;
static qf_close t70_close;
static qf_put t70_rput,t70_wput;
static qf_srv t70_rsrv,t70_wsrv;

static struct qinit t70_rinit =
{
		t70_rput, t70_rsrv, t70_open, t70_close, NULL, &t70_minfo, NULL
};

static struct qinit t70_winit =
{
		t70_wput, t70_wsrv, NULL, NULL, NULL, &t70_minfo, NULL
};

struct streamtab t70info =
{&t70_rinit, &t70_winit, NULL, NULL};

struct t70_ {
	mblk_t *msg;
	ushort_t mtu;
	ushort_t offset;
};

/* Streams code to open the driver. */
static int
t70_open (queue_t * q, dev_t dev, int flag, int sflag ERR_DECL)
{
	register struct t70_ *t_70;

	if (q->q_ptr) {
		return 0;
	}
	t_70 = malloc(sizeof(*t_70));
	if(t_70 == NULL)
		ERR_RETURN(-ENOMEM);
	memset(t_70,0,sizeof(*t_70));
	WR (q)->q_ptr = (char *) t_70;
	q->q_ptr = (char *) t_70;

	t_70->mtu = T70_mtu;

	return 0;
}

/* Streams code to close the driver. */
static void
t70_close (queue_t * q, int dummy)
{
	register struct t70_ *t_70;

	t_70 = (struct t70_ *) q->q_ptr;

	if (t_70->msg != NULL) {
		freemsg (t_70->msg);
		t_70->msg = NULL;
	}
	flushq (q, FLUSHALL);
	flushq (WR (q), FLUSHALL);
	free(t_70);
	return;
}


static void
t70_proto (queue_t * q, mblk_t * mp, char down)
{
	register struct t70_ *t_70 = (struct t70_ *) q->q_ptr;
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
	case PROTO_OFFSET: {
			long z;
			if ((error = m_geti (mp, &z)) != 0)
				goto err;
			if (z < 0 || z >= 1024)
				goto err;
			if(!down) {
				t_70->offset = z;
				z += 2;
				freemsg(mp);
				if((mp = allocb(10,BPRI_MED)) != NULL) {
					m_putid(mp,PROTO_OFFSET);
					m_puti(mp,z);
					DATA_TYPE(mp) = MSG_PROTO;
					origmp = NULL;
				}
			}
		} break;
	case PROTO_CONNECTED:
		if (t_70->msg != NULL) {
			freemsg (t_70->msg);
			t_70->msg = NULL;
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
				case T70_MTU:
					if ((error = m_geti (mp, &z)) != 0)
						goto err;
					if ((z != 0) && (z < 4 || z >= 4090))
						goto err;
					t_70->mtu = z;
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
t70_wput (queue_t * q, mblk_t * mp)
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
t70_wsrv (queue_t * q)
{
	register struct t70_ *t_70 = (struct t70_ *) q->q_ptr;
	mblk_t *mp;

	while ((mp = getq (q)) != NULL) {
		switch (DATA_TYPE(mp)) {
		case MSG_PROTO:
			t70_proto (q, mp, 1);
			break;
		case CASE_DATA:
			{
				mblk_t *mh;
				int cont = 0;

				if (!canput (q->q_next)) {
					putbq (q, mp);
					return;
				}
				mh = allocb(2+t_70->offset,BPRI_LO);
				if(mh == NULL) {
					putbqf(q,mp);
					return;
				}
				DATA_TYPE(mh) = DATA_TYPE(mp);
				if ((t_70->mtu != 0) && (dsize (mp) > t_70->mtu)) {
					int ml = t_70->mtu;
					mblk_t *mr = dupmsg (mp);
					mblk_t *mz = mp;

					cont = 1;

					if (mr == NULL) {
						freeb(mh);
						putbqf (q, mp);
						return;
					}
					while ((ml -= mr->b_wptr - mr->b_rptr) > 0) {
						mz = mz->b_cont;
						{
							mblk_t *mr2 = mr;

							mr = unlinkb (mr2);
							freeb (mr2);
						}
					}
					if (ml == 0) {
						mblk_t *mr2 = mr;

						mr = unlinkb (mr2);
						freeb (mr2);
					} else {
						mz->b_wptr += ml;
						mr->b_rptr = mr->b_wptr + ml;
					}
					if (mz->b_cont != NULL) {
						freemsg (mz->b_cont);
						mz->b_cont = NULL;
					}
					putbq (q, mr);
				}
				if (/* XXX */ 0 || DATA_REFS(mp) > 1 ||
				    	DATA_START(mp) > mp->b_rptr - 2) {
					mh->b_rptr += t_70->offset+2;
					mh->b_wptr += t_70->offset+2;
					linkb (mh, mp);
					mp = mh;
				} else
					freeb (mh);
				*--mp->b_rptr = cont ? 0x80 : 0x00;
				*--mp->b_rptr = 0x01;

				putnext (q, mp);
			}
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
t70_rput (queue_t * q, mblk_t * mp)
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
t70_rsrv (queue_t * q)
{
	mblk_t *mp;
	register struct t70_ *t_70 = (struct t70_ *) q->q_ptr;

	while ((mp = getq (q)) != NULL) {
		switch (DATA_TYPE(mp)) {
		case MSG_PROTO:
			t70_proto (q, mp, 0);
			break;
		case CASE_DATA:
			{
				short tlen = dsize (mp);
				int cont;

				if (!canput (q->q_next)) {
					putbq (q, mp);
					return;
				}
				if (tlen < 1 || (unsigned)tlen <= *(uchar_t *) mp->b_rptr || *mp->b_rptr == 0) {
					freemsg (mp);
					continue;
				}
				tlen = 1 + (uchar_t) * mp->b_rptr;
				if(tlen > 1)
					cont = (*(mp->b_rptr + 1) & 0x80);
				else
					cont = 0;

				while (tlen >= mp->b_wptr - mp->b_rptr) {
					mblk_t *mp2 = mp;
					tlen -= mp->b_wptr - mp->b_rptr;
					mp = unlinkb (mp);
					freeb(mp2);
				}
				if(mp == NULL)
					continue;
				mp->b_rptr += tlen;
				if (t_70->msg == NULL)
					t_70->msg = mp;
				else
					linkb (t_70->msg, mp);
				if (!cont) {
					putnext (q, t_70->msg);
					t_70->msg = NULL;
				}
			}
			break;
		case M_HANGUP:
		case M_ERROR:
			if (t_70->msg != NULL) {
				freemsg (t_70->msg);
				t_70->msg = NULL;
			}
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
	return register_strmod(&t70info);
}

static int do_exit_module(void)
{
	return unregister_strmod(&t70info);
}
#endif
