/* Streams module to do Van Jacobsen TCP/IP compression */

#include "f_module.h"
#include "primitives.h"
#include "f_ip.h"
#include "f_malloc.h"
#include "ppp.h"
#include <sys/types.h>
#include <sys/time.h>
#include "f_signal.h"
#include <sys/param.h>
#include <sys/sysmacros.h>
#include "streams.h"
#include <sys/stropts.h>
#ifdef DONT_ADDERROR
#include "f_user.h"
#endif
#include <sys/errno.h>
#include "vanj.h"
#include "van_j.h"
#include "compress.h"
#include "streamlib.h"
#include "isdn_proto.h"

static struct module_info van_j_minfo =
{
		0, "van_j", 0, INFPSZ, 4096,1024
};

static qf_open van_j_open;
static qf_close van_j_close;
static qf_put van_j_rput,van_j_wput;
static qf_srv van_j_rsrv,van_j_wsrv;

static struct qinit van_j_rinit =
{
		van_j_rput, van_j_rsrv, van_j_open, van_j_close, NULL, &van_j_minfo, NULL
};

static struct qinit van_j_winit =
{
		van_j_wput, van_j_wsrv, NULL, NULL, NULL, &van_j_minfo, NULL
};

struct streamtab van_jinfo =
{&van_j_rinit, &van_j_winit, NULL, NULL};

#define NVAN_J 16

static struct _van_j {
	queue_t *q_ptr;
	struct compress comp;
	char flags;
#define FLAGS_VANJ  (VAN_J_ACTIVE|VAN_J_PASSIVE|VAN_J_PPP)
	ushort_t offsetup, offsetdown;
} van_j_van_j[NVAN_J];


static void
van_j_proto (queue_t * q, mblk_t * mp, char isdown)
{
	register struct _van_j *van_j = (struct _van_j *) q->q_ptr;
	streamchar *origmp = mp->b_rptr;
	ushort_t id;

	/* In case we want to printf("%s") this... */
	if (mp->b_wptr < mp->b_datap->db_lim)
		*mp->b_wptr = '\0';
	if (m_getid (mp, &id) != 0) {
		mp->b_rptr = origmp;
		putnext (q, mp);
		return;
	}
	switch (id) {
	default:
		break;
	case PROTO_OFFSET:
		{
			long z;
			if (m_geti (mp, &z) != 0)
				goto err;
			if (z < 0 || z >= 1024)
				goto err;
			if(isdown) {
				if (van_j->flags & VAN_J_PPP) 
					van_j->offsetup = z+2;
				else
					van_j->offsetup = 0;
			} else {
				if (van_j->flags & VAN_J_PPP)
					van_j->offsetdown = z+2;
				else
					van_j->offsetdown = 0;
			}
			break;
		}
	case PROTO_CONNECTED:
		van_j->flags |= VAN_J_CONN;
		if ((van_j->flags & (VAN_J_ACTIVE | VAN_J_PASSIVE)) ==
				(VAN_J_ACTIVE | VAN_J_PASSIVE))
			van_j->flags &= ~VAN_J_ACTIVE;
		break;
	case PROTO_DISCONNECT:
		printf ("X-Disconnect\n");
		van_j->flags &= ~VAN_J_CONN;
		break;
	case PROTO_MODULE:
		if (strnamecmp (q, mp)) {
			u_long x;

			if (van_j->flags & VAN_J_CONN)
				goto err;
			while (mp != NULL && m_getsx (mp, &id) == 0) {
				switch (id) {
				  err:
					printf (" :Err %s\n", mp->b_rptr);
					mp->b_rptr = origmp;
					m_reply (q, mp, EINVAL);
					mp = NULL;
				default:
					break;
				case VANJ_PPP:
					if (van_j->flags & VAN_J_CONN)
						break;
					if(!(van_j->flags & VAN_J_PPP)) {
						van_j->offsetup += 2;
						van_j->offsetdown += 2;
					}
					van_j->flags |= VAN_J_PPP;
					break;
				case VANJ_NORM:
					if (van_j->flags & VAN_J_CONN)
						break;
					if(van_j->flags & VAN_J_PPP) {
						if (van_j->offsetup >= 2)
							van_j->offsetup -= 2;
						if (van_j->offsetdown >= 2)
							van_j->offsetdown -= 2;
					}
					van_j->flags &= ~VAN_J_PPP;
					break;
				case VANJ_MODE:
					if (van_j->flags & VAN_J_CONN)
						break;
					if (m_getx (mp, &x) != 0)
						goto err;
					switch (x) {
					case VANJ_MODE_OFF:
						van_j->flags &= ~VAN_J_ACTIVE;
						van_j->flags &= ~VAN_J_PASSIVE;
						break;
					case VANJ_MODE_PASSIVE:
						van_j->flags &= ~VAN_J_ACTIVE;
						van_j->flags |= VAN_J_PASSIVE;
						break;
					case VANJ_MODE_ACTIVE:
						van_j->flags |= VAN_J_ACTIVE;
						van_j->flags &= ~VAN_J_PASSIVE;
						break;
					default:
						goto err;
					}
				}
			}
			if (mp != NULL) {
				mp->b_rptr = origmp;
				m_reply (q,mp,0);
				mp = NULL;
			}
		}
	}
	if (mp != NULL) {
		if (origmp != NULL)
			mp->b_rptr = origmp;
		putnext (q, mp);
	}
}


static int
van_j_open (queue_t * q, dev_t dev, int flag, int sflag
#ifdef DO_ADDERROR
		,int *err
#endif
)
{
	struct _van_j *van_j;

	if(q->q_ptr != NULL)
		return 0;
	van_j = malloc(sizeof(van_j));
	if(van_j == NULL)
		return OPENFAIL;
	memset(van_j,0,sizeof(*van_j));
	WR (q)->q_ptr = (char *) van_j;
	q->q_ptr = (char *) van_j;

	van_j->flags = VAN_J_ACTIVE;
	compress_init (&van_j->comp);

	MORE_USE;
	return 0;
}


static void
van_j_wput (queue_t * q, mblk_t * mp)
{
	switch (mp->b_datap->db_type) {
	case MSG_PROTO:
	CASE_DATA
		putq (q, mp);
		break;
	case M_FLUSH:
		if (*mp->b_rptr & FLUSHW)
			flushq (q, FLUSHDATA);
		putnext (q, mp);
		break;
	default:{
			putnext (q, mp);
			break;
		}
	}
	return;
}



static void
van_j_wsrv (queue_t * q)
{
	mblk_t *mp;

	register struct _van_j *van_j = (struct _van_j *) q->q_ptr;

	while ((mp = getq (q)) != NULL) {
		switch (mp->b_datap->db_type) {
		case MSG_PROTO:
			van_j_proto (q, mp, 1);
			break;
		CASE_DATA {
				mblk_t *mq;
				struct ip *p_ip;
				ushort_t protocol;

				if(!canput(q->q_next)) {
					putbq(q,mp);
					return;
				}

				mq = pullupm (mp, 128);	/* IP header, stuff */

				if (mq == NULL) {
					putbqf (q, mp);
					return;
				}
				p_ip = (struct ip *)mq->b_rptr;

				if (van_j->flags & VAN_J_PPP) {
					protocol = *(u_short *)mq->b_rptr;

					if ((p_ip->ip_p == PPP_IP) && (van_j->flags & VAN_J_ACTIVE)) {
						mq->b_rptr += 2;

						protocol = compress_tcp (&van_j->comp, &mq);
						if(mq->b_rptr-2 < mq->b_datap->db_base) {
							mp = allocb (van_j->offsetdown, BPRI_MED);
							if (mp == NULL) {
								freemsg (mq);
								return;
							}
							mp->b_datap->db_type = mq->b_datap->db_type;
							mp->b_rptr += van_j->offsetdown;
							mp->b_wptr += van_j->offsetdown;
							linkb (mp, mq);
							mq = mp;
						}
						mq->b_rptr -= 2;
						*(ushort_t *)mq->b_rptr = protocol;
					}
				} else {
					p_ip = (struct ip *) mq->b_rptr;
					if (p_ip->ip_p == IPPROTO_TCP
							&& van_j->flags & VAN_J_ACTIVE)
						protocol = compress_tcp (&van_j->comp, &mq);
					else
						protocol = htons (PPP_PROTO_IP);

					switch (ntohs (protocol)) {
					case PPP_PROTO_VJC_COMP:
						*mq->b_rptr |= 0x80;
						break;
					case PPP_PROTO_VJC_UNCOMP:
						*mq->b_rptr |= 0x70;
						break;
					default:
						break;
					}
				}

				putnext (q, mq);

				continue;
			}
		case M_FLUSH:
			if (*mp->b_rptr & FLUSHW)
				flushq (q, FLUSHDATA);
			/* FALL THRU */
		default:
            if (mp->b_datap->db_type > QPCTL || canput (q->q_next)) {
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
van_j_close (queue_t * q, int dummy)
{
	struct _van_j *van_j = (struct _van_j *) q->q_ptr;

	flushq (q, FLUSHALL);
	flushq (WR (q), FLUSHALL);
	printf ("VAN_J driver %d closed.\n", van_j - van_j_van_j);
	free(van_j);

	LESS_USE;
	return;
}


static void
van_j_rput (queue_t * q, mblk_t * mp)
{
	switch (mp->b_datap->db_type) {

	case M_FLUSH:
		if (*mp->b_rptr & FLUSHR) {
			flushq (q, FLUSHDATA);
		}
		putnext (q, mp);		  /* send it along too */
		break;

	case MSG_PROTO:
	CASE_DATA
		putq (q, mp);			  /* queue it for my service routine */
		break;

	case M_HANGUP:
	case M_ERROR:
	default:
		putnext (q, mp);		  /* don't know what to do with this, so send
								   * it along */

	}
	return;
}

static void
van_j_rsrv (queue_t * q)
{
	mblk_t *mp;
	register struct _van_j *van_j = (struct _van_j *) q->q_ptr;

	while ((mp = getq (q)) != NULL) {
		switch (mp->b_datap->db_type) {
		case MSG_PROTO:
			van_j_proto (q, mp, 0);
			break;
		CASE_DATA {
				mblk_t *mq;
				ushort_t protocol = 0;

				if(!canput(q->q_next)) {
					putbq(q,mp);
					return;
				}
				if (van_j->flags & VAN_J_PPP) {
					mq = pullupm (mp, 2);
					if (mq == NULL) {
						putbq (q, mp);
						return;
					}
					protocol = *(ushort_t *) mq->b_rptr;
					mq->b_rptr += 2;
					mp = pullupm (mq, 0);
				} else {
					uchar_t ch = *mp->b_rptr;

					if (ch & 0x80) {
						if (van_j->flags & VAN_J_ACTIVE)
							protocol = PPP_PROTO_VJC_COMP;
						else {
							printf("VJ: packet dropped, not active\n");
							freemsg (mp), mp = NULL;
						}
					} else if (ch >= 0x70) {
						protocol = PPP_PROTO_VJC_UNCOMP;
						*mp->b_rptr &= ~0x30;
						van_j->flags |= VAN_J_ACTIVE;
					} else
						protocol = htons (PPP_PROTO_IP);
				}

				if (mp != NULL) {
					switch (ntohs (protocol)) {
					case PPP_PROTO_VJC_COMP:
						mq = pullupm(mp,16);
						if(mq == NULL) {
							putbqf(q,mp);
							return;
						}
						mp = mq;
						uncompress_tcp (&van_j->comp, &mp, protocol);
						protocol = PPP_IP;
						break;
					case PPP_PROTO_VJC_UNCOMP:
						mq = pullupm(mp,128);
						if(mq == NULL) {
							putbqf(q,mp);
							return;
						}
						mp = mq;
						uncompress_tcp (&van_j->comp, &mp, protocol);
						protocol = PPP_IP;
						break;
					case PPP_PROTO_IP:
						break;
					default:
						printf ("VanJ: Unknown protocol 0x%x\n", ntohs (protocol));
						freemsg (mp);
						mp = NULL;
					}
					if (van_j->flags & VAN_J_PPP) {
						if(mp->b_rptr-2 < mp->b_datap->db_base)  {
							mq = allocb (van_j->offsetup, BPRI_MED);
							if (mq == NULL) {
								freemsg (mp);
								return;
							}
							mq->b_datap->db_type = mp->b_datap->db_type;
							mq->b_wptr += van_j->offsetup;
							mq->b_rptr += van_j->offsetup;
							linkb (mq, mp);
							mp = mq;
						}
						mp->b_rptr -= 2;
						*(ushort_t *)mp->b_rptr = protocol;
					}
				}
				if (mp != NULL)
					putnext (q, mp);
				break;

			}
		default:
            if (mp->b_datap->db_type > QPCTL || canput (q->q_next)) {
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
	return register_strmod(&van_jinfo);
}

static int do_exit_module(void)
{
	return unregister_strmod(&van_jinfo);
}
#endif
