/**
 ** Streams fakecept module
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
#include "fakecept.h"
#include "isdn_proto.h"


/*
 * Standard Streams stuff.
 */
static struct module_info fakecept_minfo =
{
		0, "fakecept", 0, INFPSZ, 200,100
};

static qf_open fakecept_open;
static qf_close fakecept_close;
static qf_put fakecept_rput,fakecept_wput;
static qf_srv fakecept_rsrv,fakecept_wsrv;

static struct qinit fakecept_rinit =
{
		fakecept_rput, fakecept_rsrv, fakecept_open, fakecept_close, NULL, &fakecept_minfo, NULL
};

static struct qinit fakecept_winit =
{
		fakecept_wput, fakecept_wsrv, NULL, NULL, NULL, &fakecept_minfo, NULL
};

struct streamtab fakeceptinfo =
{&fakecept_rinit, &fakecept_winit, NULL, NULL};

struct fakecept_ {
	mblk_t *msg;
	ushort_t mtu;
	ushort_t offset;
};

/* Streams code to open the driver. */
static int
fakecept_open (queue_t * q, dev_t dev, int flag, int sflag ERR_DECL)
{
	register struct fakecept_ *fakecept;

	if (q->q_ptr) {
		return 0;
	}
	fakecept = malloc(sizeof(*fakecept));
	if(fakecept == NULL)
		ERR_RETURN(-ENOMEM);
	memset(fakecept,0,sizeof(*fakecept));
	WR (q)->q_ptr = (char *) fakecept;
	q->q_ptr = (char *) fakecept;

	fakecept->mtu = FAKECEPT_MTU-2;

	return 0;
}

/* Streams code to close the driver. */
static void
fakecept_close (queue_t * q, int dummy)
{
	register struct fakecept_ *fakecept;

	fakecept = (struct fakecept_ *) q->q_ptr;

	if (fakecept->msg != NULL) {
		freemsg (fakecept->msg);
		fakecept->msg = NULL;
	}
	flushq (q, FLUSHALL);
	flushq (WR (q), FLUSHALL);
	free(fakecept);
	return;
}


static void
fakecept_proto (queue_t * q, mblk_t * mp, char down)
{
	register struct fakecept_ *fakecept = (struct fakecept_ *) q->q_ptr;
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
				fakecept->offset = z;
				z += 4;
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
		if (fakecept->msg != NULL) {
			freemsg (fakecept->msg);
			fakecept->msg = NULL;
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
				case FAKECEPT_MTU_ID:
					if ((error = m_geti (mp, &z)) != 0)
						goto err;
					if (z < 4 || z >= 4090)
						goto err;
					fakecept->mtu = z - 2; /* Two less because of the two additional header bytes */
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
fakecept_wput (queue_t * q, mblk_t * mp)
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
fakecept_wsrv (queue_t * q)
{
	register struct fakecept_ *fakecept = (struct fakecept_ *) q->q_ptr;
	mblk_t *mp;

	while ((mp = getq (q)) != NULL) {
		switch (DATA_TYPE(mp)) {
		case MSG_PROTO:
			fakecept_proto (q, mp, 1);
			break;
		case CASE_DATA:
			{
				mblk_t *mh;
				int cont = 0;

				if (!canput (q->q_next)) {
					putbq (q, mp);
					return;
				}
				mh = allocb(4+fakecept->offset,BPRI_LO);
				if(mh == NULL) {
					putbqf(q,mp);
					return;
				}
				DATA_TYPE(mh) = DATA_TYPE(mp);
				if (dsize (mp) > fakecept->mtu) {
					int ml = fakecept->mtu;
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
				    	DATA_START(mp) > mp->b_rptr - 4) {
					mh->b_rptr += fakecept->offset+4;
					mh->b_wptr += fakecept->offset+4;
					linkb (mh, mp);
					mp = mh;
				} else
					freeb (mh);
				*--mp->b_rptr = 0x00;
				*--mp->b_rptr = 0x01;
/*				*--mp->b_rptr = cont ? 0x80 : 0x00;*/
				*--mp->b_rptr = 0x00;
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
fakecept_rput (queue_t * q, mblk_t * mp)
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
fakecept_rsrv (queue_t * q)
{
	mblk_t *mp;
	register struct fakecept_ *fakecept = (struct fakecept_ *) q->q_ptr;

	while ((mp = getq (q)) != NULL) {
		switch (DATA_TYPE(mp)) {
		case MSG_PROTO:
			fakecept_proto (q, mp, 0);
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
				tlen = 3 + (uchar_t) * mp->b_rptr;
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
				if (fakecept->msg == NULL)
					fakecept->msg = mp;
				else
					linkb (fakecept->msg, mp);
				if (!cont) {
					putnext (q, fakecept->msg);
					fakecept->msg = NULL;
				}
			}
			break;
		case M_HANGUP:
		case M_ERROR:
			if (fakecept->msg != NULL) {
				freemsg (fakecept->msg);
				fakecept->msg = NULL;
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
	return register_strmod(&fakeceptinfo);
}

static int do_exit_module(void)
{
	return unregister_strmod(&fakeceptinfo);
}
#endif
