/**
 ** Streams FAKEH module, used to pretend we're a Cisco box.
 ** Do turn off keepalives at the other end!
 **/
#undef DO_KEEPALIVES /* not yet..! */

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
#include "f_ip.h"
#include <sys/errno.h>
#include "streamlib.h"
#include "fakeh.h"
#include "isdn_proto.h"


/*
 * Standard Streams stuff.
 */
static struct module_info fakeh_minfo =
{
		0, "fakeh", 0, INFPSZ, 4096, 2048
};

static qf_open fakeh_open;
static qf_close fakeh_close;
static qf_put fakeh_rput,fakeh_wput;
static qf_srv fakeh_rsrv,fakeh_wsrv;

static struct qinit fakeh_rinit =
{
		fakeh_rput, fakeh_rsrv, fakeh_open, fakeh_close, NULL, &fakeh_minfo, NULL
};

static struct qinit fakeh_winit =
{
		fakeh_wput, fakeh_wsrv, NULL, NULL, NULL, &fakeh_minfo, NULL
};

struct streamtab fakehinfo =
{&fakeh_rinit, &fakeh_winit, NULL, NULL};

struct fakeh_ {
	queue_t *qptr;
	ushort_t offset;

	unsigned connected:1;
	unsigned do_ethertype:1;
#ifdef DO_KEEPALIVES
	int timer;
#ifdef NEW_TIMEOUT
	int timeout;
#endif
	unsigned long last_a;
	unsigned long last_b;
	unsigned long last_c;
	unsigned long last_d;
	unsigned char lastmap[4];
#endif
};

#ifdef DO_KEEPALIVES
static void
fake_h_timer (struct fakeh_ *fakeh)
{
	long s;
	mblk_t *mp;

	if (fakeh->timer == 0 || !fakeh->connected)
		return;
	if (fakeh->qptr == NULL) {
		fakeh->timer = 0;
		return;
	}
	switch(fakeh->lastmap[0]) {
	case 1: fakeh->last_a++;
	case 2: fakeh->last_b++;
	case 3: fakeh->last_c++;
	case 4: fakeh->last_d++;
	}

	if(canput(fakeh->qptr)) {
		mp = allocb(28,BPRI_LO);
		if(mp != NULL) {
			int i;
			*(long *)mp->b_wptr++ = htonl(0x0F008035);
			for (i=1;i<4;i++) {
				switch(fakeh->lastmap[i]) {
				default:*(long *)mp->b_wptr++ = 0;
				case 1: *(long *)mp->b_wptr++ = fakeh->last_a;
				case 2: *(long *)mp->b_wptr++ = fakeh->last_b;
				case 3: *(long *)mp->b_wptr++ = fakeh->last_c;
				case 4: *(long *)mp->b_wptr++ = fakeh->last_d;
				}
			}
			*(short *)mp->b_wptr++ = -1;
			*(long *)mp->b_wptr++ = jiffies;
			putnext(fakeh->qptr,mp);
		}
	}

	s = splstr();

#ifdef NEW_TIMEOUT
	fakeh->timeout =
#endif
			timeout ((void *)fake_h_timer, fakeh, fakeh->timer * HZ);
	splx(s);
}
#endif /* DO_KEEPALIVES */


/* Streams code to open the driver. */
static int
fakeh_open (queue_t * q, dev_t dev, int flag, int sflag ERR_DECL)
{
	register struct fakeh_ *fakeh;

	fakeh = malloc(sizeof(*fakeh));
	if(fakeh == NULL)
		ERR_RETURN(-ENOMEM);
	memset(fakeh,0,sizeof(*fakeh));
	WR (q)->q_ptr = (char *) fakeh;
	q->q_ptr = (char *) fakeh;

	fakeh->qptr = q;
	fakeh->connected = 0;
#ifdef DO_KEEPALIVES
	fakeh->last_a = 0;
	fakeh->last_b = 0;
	fakeh->last_c = 0;
	fakeh->last_d = 0;
	fakeh->lastmap[0] = 2;
	fakeh->lastmap[1] = 2;
	fakeh->lastmap[2] = 1;
	fakeh->lastmap[3] = 0;
#endif

	MORE_USE;
	return 0;
}

/* Streams code to close the driver. */
static void
fakeh_close (queue_t * q, int dummy)
{
	register struct fakeh_ *fakeh;

	fakeh = (struct fakeh_ *) q->q_ptr;

	flushq (q, FLUSHALL);
	flushq (WR (q), FLUSHALL);
	fakeh->qptr = NULL;

#ifdef DO_KEEPALIVES
	if (fakeh->connected & (fakeh->timer > 0)) {
#ifdef NEW_TIMEOUT
		untimeout (fakeh->timeout);
#else
		untimeout (fake_h_timer, ipmon);
#endif
	}
#endif
	free(fakeh);

	LESS_USE;
	return;
}


static void
fakeh_proto (queue_t * q, mblk_t * mp, char down)
{
	register struct fakeh_ *fakeh = (struct fakeh_ *) q->q_ptr;
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
			if (m_geti (mp, &z) != 0)
				goto err;
			if (z < 0 || z >= 1024)
				goto err;
			if(!down) {
				fakeh->offset = z;
				if(fakeh->do_ethertype)
					z += 2;
				else
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
		fakeh->connected = 1;
#ifdef DO_KEEPALIVES
		if (fakeh->timer > 0) {
#ifdef NEW_TIMEOUT
			fakeh->timeout =
#endif
				timeout ((void *)fake_h_timer, fakeh, fakeh->timer * HZ);
		}
#endif
		break;
	case PROTO_DISCONNECT:
#ifdef DO_KEEPALIVES
		if(fakeh->connected && (fakeh->timer > 0)) {
#ifdef NEW_TIMEOUT
			untimeout (fakeh->timeout);
#else
			untimeout (fake_h_timer, ipmon);
#endif
		}
#endif
		fakeh->connected = 0;
		break;
	case PROTO_MODULE:
		if (strnamecmp (q, mp)) {	/* Config information for me. */
#ifdef DO_KEEPALIVES
			long z;
#endif

			while (mp != NULL && m_getsx (mp, &id) == 0) {
				switch (id) {
				default:
				  	goto err;
				case PROTO_MODULE:
					break;
				case PROTO_TYPE_NONE:
					fakeh->do_ethertype = 0;
					break;
				case PROTO_TYPE_ETHER:
					fakeh->do_ethertype = 1;
					break;
#ifdef DO_KEEPALIVES
				case FAKEH_SETMAP:
					if ((error = m_geti (mp, &z)) != 0)
						goto err;
					fakeh->lastmap[0] = z;
					if ((error = m_geti (mp, &z)) != 0)
						goto err;
					fakeh->lastmap[1] = z;
					if ((error = m_geti (mp, &z)) != 0)
						goto err;
					fakeh->lastmap[2] = z;
					if ((error = m_geti (mp, &z)) != 0)
						goto err;
					fakeh->lastmap[3] = z;
					break;
				case FAKEH_TIMEOUT:
					if ((error = m_geti (mp, &z)) != 0)
						goto err;
					if ((z < 4 || z >= 4090) && (z != 0))
						goto err;
					if (z > 0 && fakeh->timeout == 0 && fakeh->connected)
#ifdef NEW_TIMEOUT
					fakeh->timeout =
#endif
						timeout ((void *)fake_h_timer, fakeh, fakeh->timer * HZ);
					fakeh->timer = z;
					break;
#endif
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
fakeh_wput (queue_t * q, mblk_t * mp)
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
fakeh_wsrv (queue_t * q)
{
	register struct fakeh_ *fakeh = (struct fakeh_ *) q->q_ptr;
	mblk_t *mp;

	while ((mp = getq (q)) != NULL) {
		switch (DATA_TYPE(mp)) {
		case MSG_PROTO:
			fakeh_proto (q, mp, 1);
			break;
		case CASE_DATA:
			{
				mblk_t *mh;
				short offlen;

				if (!canput (q->q_next)) {
					putbq (q, mp);
					return;
				}
				if(fakeh->do_ethertype)
					offlen = 2;
				else
					offlen = 4;
				if (/* XXX */ 0 || DATA_REFS(mp) > 1 ||
				    	DATA_START(mp) > mp->b_rptr - offlen) {
					mh = allocb(fakeh->offset+offlen,BPRI_LO);
					if(mh == NULL) {
						putbqf(q,mp);
						return;
					}
					DATA_TYPE(mh) = DATA_TYPE(mp);
					mh->b_rptr += fakeh->offset+offlen;
					mh->b_wptr += fakeh->offset+offlen;
					linkb (mh, mp);
					mp = mh;
				}
				if(fakeh->do_ethertype) {
					mp->b_rptr -= 2;
					*(short *)mp->b_rptr = htons(0x0F00);
				} else {
					mp->b_rptr -= 4;
					*(long *)mp->b_rptr = htonl(0x0F000800);
				}
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
fakeh_rput (queue_t * q, mblk_t * mp)
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
fakeh_rsrv (queue_t * q)
{
	mblk_t *mp;
	register struct fakeh_ *fakeh = (struct fakeh_ *) q->q_ptr;

	while ((mp = getq (q)) != NULL) {
		switch (DATA_TYPE(mp)) {
		case MSG_PROTO:
			fakeh_proto (q, mp, 0);
			break;
		case CASE_DATA:
			{
				unsigned long xhdr;
				short tlen = dsize (mp);
				mblk_t *mq;

				if (!canput (q->q_next)) {
					putbq (q, mp);
					return;
				}
				if (tlen < 4) {
					freemsg (mp);
					continue;
				}
				mq = pullupm(mp,4);
				if(mq == NULL) {
					putbq (q, mp);
					return;
				}
				mp = mq;
				if(fakeh->do_ethertype) {
					xhdr = *(short *)mp->b_rptr & htons(0xFF00);
					mp->b_rptr += 2;
				} else {
					xhdr = *(long *)mp->b_rptr & htonl(0xFF00FFFF);
					mp->b_rptr += 4;
				}
				if (xhdr != (fakeh->do_ethertype ? htons(0x0F00) : htonl(0x0F000800))) {
#ifdef DO_KEEPALIVES
					if(fakeh->do_ethertype) {
						mp->b_rptr -= 2;
						xhdr = *(long *)mp->b_rptr & htonl(0xFF00FFFF);
						mp->b_rptr += 4;
					}
					if(xhdr == ntohl(0x8F008035)) {
						mq = pullupm(mp,12);
						if (mq == NULL) {
							freemsg (mp);
							continue;
						}
						mp = mq;
						fakeh->last_a = *(long *)mp->b_rptr++;
						fakeh->last_b = *(long *)mp->b_rptr++;
						fakeh->last_c = *(long *)mp->b_rptr++;
					}
#endif
					freemsg(mp);
					continue;
				}
				putnext (q, mp);
			}
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
	return register_strmod(&fakehinfo);
}

static int do_exit_module(void)
{
	return unregister_strmod(&fakehinfo);
}
#endif
