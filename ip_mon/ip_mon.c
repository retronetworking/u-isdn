
/* Streams module to do logging+blocking of IP packet addresses */

#define UAREA

#include "f_module.h"
#include "primitives.h"
#include "f_ip.h"
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
#include "ip_mon.h"
#include "streamlib.h"
#include "isdn_proto.h"
#include "vanj.h"
#include "ppp.h"

static struct module_info ip_mon_minfo =
{
		0, "ip_mon", 0, INFPSZ, 200,100
};

static qf_open ip_mon_open;
static qf_close ip_mon_close;
static qf_put ip_mon_rput, ip_mon_wput;
static qf_srv ip_mon_rsrv, ip_mon_wsrv;

static struct qinit ip_mon_rinit =
{
		ip_mon_rput, ip_mon_rsrv, ip_mon_open, ip_mon_close, NULL, &ip_mon_minfo, NULL
};

static struct qinit ip_mon_winit =
{
		ip_mon_wput, ip_mon_wsrv, NULL, NULL, NULL, &ip_mon_minfo, NULL
};

struct streamtab ip_moninfo =
{&ip_mon_rinit, &ip_mon_winit, NULL, NULL};
struct streamtab ip_mon2info =
{&ip_mon_rinit, &ip_mon_winit, NULL, NULL};

#define NIP_MON 8
#define NIP_INFO 50

struct _ip_mon {
	queue_t *qptr;
	int timer;
#ifdef NEW_TIMEOUT
	int timeout;
#endif
	short nr;
	unsigned hastimer:1;
	unsigned encap:2;
#define ENCAP_NONE 0
#define ENCAP_ETHER 1
#define ENCAP_PPP 2
} ip_mon;

static mblk_t *ip_info[NIP_INFO];

static int last_index = -1;
static int nexti = 0;

static int
ip_mon_index (unsigned long local, unsigned long remote, int protocol, int *dosend, ushort_t localp, ushort_t remotep)
{
	mblk_t *mb;
	int i;
	struct _monitor *mon;
#define CMP(_m) ( \
		(_m)->local == local && \
		(_m)->remote == remote && \
		(protocol == 0 || _m->p_protocol == 0 || \
			(_m->p_protocol == protocol && \
			(_m->p_local == localp || _m->p_local == 0 || localp == 0) && \
			(_m->p_remote == remotep || _m->p_remote == 0 || remotep == 0)) \
		) \
	)
	if (last_index >= 0) {
		mon = (struct _monitor *) ip_info[last_index]->b_rptr;
		if (CMP(mon))
			return last_index;
	}
	for (i = 0; i < NIP_INFO; i++) {
		if(i == last_index)
			continue;
		if ((mb = ip_info[i]) == NULL) {
			nexti = i;
			break;
		}
		mon = (struct _monitor *) mb->b_rptr;
		if (CMP(mon))
			return last_index = i;
	}

	if ((mb = allocb (sizeof (struct _monitor), BPRI_MED)) == NULL)
		 return -1;

	mon = ((struct _monitor *) mb->b_wptr)++;
	bzero (mon, sizeof (struct _monitor));

	mon->local = local;
	mon->remote = remote;
	mon->p_protocol = protocol;
	mon->p_local = localp;
	mon->p_remote = remotep;
	mon->cap_b = ~0;
	mon->cap_p = ~0;
	if (ip_info[i = last_index = nexti] != NULL) {
		struct _monitor *m2 = (struct _monitor *)(ip_info[i]->b_rptr);
		if ((m2->sofar_b != 0 || m2->sofar_p != 0) && ip_mon.qptr != NULL && canput (ip_mon.qptr->q_next))
			putnext (ip_mon.qptr, ip_info[i]);
		else
			freemsg (ip_info[i]);
	}
	ip_info[i] = mb;
	if (++nexti >= NIP_INFO)
		nexti = 0;

	if (dosend != NULL)
		*dosend = 1;
	return i;
}


static void
ip_mon_sendup (int i)
{
	mblk_t *mb, *mb2;
	struct _monitor *m2;

	if (ip_mon.qptr == NULL || !canput (ip_mon.qptr->q_next) || (mb = ip_info[i]) == NULL)
		return;
	m2 = (struct _monitor *)(mb->b_rptr);
	if (m2->sofar_b == 0 && m2->sofar_p == 0)
		return;

	mb2 = copyb (mb);
	if (mb2 != 0) {
		struct _monitor *m = (struct _monitor *) mb->b_rptr;

		putnext (ip_mon.qptr, mb2);
		m->sofar_b = 0;
		m->sofar_p = 0;
	}
}

static void
ip_mon_proto (queue_t * q, mblk_t * mp, char senddown)
{
	register struct _ip_mon *ipmon = (struct _ip_mon *) q->q_ptr;
	char *origmp = mp->b_rptr;
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
	case PROTO_DISCONNECT:{
			int i;

			for (i = 0; i < NIP_INFO; i++)
				ip_mon_sendup (i);
		} break;
	case PROTO_MODULE:
		if (strnamecmp (q, mp)) {
			long x;

#if 0
			if (ipmon->flags & IP_MON_CONN)
				goto err;
#endif
			while (mp != NULL && m_getsx (mp, &id) == 0) {
				switch (id) {
				default:
					goto err;
				case PROTO_MODULE:
					break;
				case PROTO_TYPE_NONE:
					ipmon->encap = ENCAP_NONE;
					break;
				case PROTO_TYPE_PPP:
					ipmon->encap = ENCAP_PPP;
					break;
				case PROTO_TYPE_ETHER:
					ipmon->encap = ENCAP_ETHER;
					break;
				case IP_MON_TIMEOUT:
					if ((error = m_geti (mp, &x)) != 0)
						goto err;
					if (x < 2 || x > 99999) 
						goto err;
					ipmon->timer = x;
					break;
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
	return;
  err:
	mp->b_rptr = origmp;
	m_reply (q, mp, error ? error : -EINVAL);
}


static void
ip_mon_timer (struct _ip_mon *ipmon)
{
	long s;

	if (ipmon->timer == 0)
		return;
	if (ip_mon.qptr == NULL) {
		ipmon->timer = 0;
		return;
	}

	s = splstr();
	ip_mon_sendup (nexti++);
	if (nexti >= NIP_INFO)
		nexti = 0;

#ifdef NEW_TIMEOUT
	ipmon->timeout =
#endif
		timeout ((void *)ip_mon_timer, ipmon, ipmon->timer * HZ);
	splx(s);
}

static int
ip_mon_open (queue_t * q, dev_t dev, int flag, int sflag ERR_DECL)
{
	struct _ip_mon *ipmon;
	static int nr = 1;
	int do_timeout = 1;

	dev = minor (dev);
	if (sflag == MODOPEN) {
		ipmon = malloc(sizeof(*ipmon));
		if(ipmon == NULL)
			ERR_RETURN(-ENOMEM);
	} else if (ip_mon.qptr != NULL) {
		printf ("IP_MON: Master already open\n");
		ERR_RETURN(-EBUSY);
	} else if (dev != 0 || sflag == CLONEOPEN) {
		printf ("IP_MON: Bad minor number: dev %d, sflag %d\n", dev, sflag);
		ERR_RETURN(-ENXIO);
	} else {
		ipmon = &ip_mon;
		do_timeout = (ip_mon.qptr == NULL);
	}
	memset(ipmon,0,sizeof (*ipmon));
	if (ipmon != &ip_mon)
		ipmon->nr = nr++;
	WR (q)->q_ptr = (char *) ipmon;
	q->q_ptr = (char *) ipmon;
	ipmon->qptr = q;

	ipmon->timer = 10;
	if(do_timeout) {
		ipmon->hastimer = 1;
#ifdef NEW_TIMEOUT
		ipmon->timeout =
#endif
			timeout ((void *)ip_mon_timer, ipmon, HZ * ipmon->timer);
	}
	printf ("IP_MON driver %d opened.\n", ipmon->nr);

	MORE_USE;
	return 0;
}


static void
ip_mon_wput (queue_t * q, mblk_t * mp)
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

static mblk_t *count_packet (struct _ip_mon *ipmon, mblk_t *mp, char doswap)
{
	mblk_t *mq;
	struct ip *ipp;
	struct _monitor *mon;
	int i, send = 0;
	int ms;
	ushort_t localp, remotep;

	if(dsize(mp) < sizeof(struct ip)) {
		printf("ip_%c: size %d\n",doswap?'r':'w',dsize(mp));
		freemsg(mp);
		return NULL;
	}
	if(ipmon->encap) {
		short encap;
		mq = pullupm (mp, 2);	/* encap header */

		if (mq == NULL) 
			return mp;
		encap = *(short *)(mq->b_rptr);
		if((ipmon->encap == ENCAP_PPP && encap != htons(PPP_IP)) ||
		   (ipmon->encap == ENCAP_ETHER && encap != htons(ETH_P_IP)))
		    return mq;
		mp = mq;
	}
	mq = pullupm (mp, sizeof (struct ip) + 2);	/* IP header */

	if (mq == NULL) {
		return mp; /* oh well -- don't count it */
	}
	ms = splstr();
	ipp = (struct ip *) (mq->b_rptr+(ipmon->encap ? 2 : 0));

	switch (ipp->ip_p) {
	default:
		localp = remotep = 0;
		break;
	case IPPROTO_TCP:
		mp = pullupm (mq, (ipp->ip_hl << 2) + sizeof (struct tcphdr)+2);

		if (mp != NULL) {
			struct tcphdr *tcp;

			mq = mp;
			ipp = (struct ip *) (mq->b_rptr+(ipmon->encap ? 2 : 0));
			tcp = (struct tcphdr *) (((char *)ipp) + (ipp->ip_hl << 2));
			localp = tcp->th_sport;
			remotep = tcp->th_dport;
		} else
			return mq;
		break;
	case IPPROTO_UDP:
		mp = pullupm (mq, (ipp->ip_hl << 2) + sizeof (struct udphdr)+2);

		if (mp != NULL) {
			struct udphdr *udp;

			mq = mp;
			ipp = (struct ip *) (mq->b_rptr+(ipmon->encap ? 2 : 0));
			udp = (struct udphdr *) (((char *)ipp) + (ipp->ip_hl << 2));
			localp = udp->uh_sport;
			remotep = udp->uh_dport;
		} else
			return mq;
		break;
	case IPPROTO_ICMP:
		mp = pullupm (mq, (ipp->ip_hl << 2) + sizeof (struct icmp) + 2);
		if(mp != NULL) {
			struct icmp *icm;

			mq = mp;
			ipp = (struct ip *) (mq->b_rptr+(ipmon->encap ? 2 : 0));
			icm = (struct icmp *) (((char *)ipp) + (ipp->ip_hl << 2));
			if(doswap) {
				localp = htons(icm->icmp_code);
				remotep= htons(icm->icmp_type);
			} else {
				localp = htons(icm->icmp_type);
				remotep= htons(icm->icmp_code);
			}
		} else
			return mq;
		break;
	}

	if(doswap)
		i = ip_mon_index (ADR(ipp->ip_dst), ADR(ipp->ip_src), ipp->ip_p, &send,remotep,localp);
	else
		i = ip_mon_index (ADR(ipp->ip_src), ADR(ipp->ip_dst), ipp->ip_p, &send,localp,remotep);
	if (i < 0) {
		splx (ms);
		return mq;
	}
	mon = (struct _monitor *) ip_info[i]->b_rptr;
	mon->sofar_b += dsize (mq);
	mon->sofar_p++;
	if (mon->sofar_b > mon->cap_b || mon->sofar_p > mon->cap_p) {
		splx (ms);
		printf ("Drop packet %lx %lx\n", ntohl(ADR(ipp->ip_src)), ntohl(ADR(ipp->ip_dst)));
		freemsg (mq);
		return NULL;
	}
	splx (ms);
	if (send)
		ip_mon_sendup (i);
	return mq;
}

static void
ip_mon_wsrv (queue_t * q)
{
	mblk_t *mp;
	struct _ip_mon *ipmon = (struct _ip_mon *) q->q_ptr;

	if (q == ip_mon.qptr) {
		while ((mp = getq (q)) != NULL) {
			struct _monitor *mon;
			int i;
			int ms = splstr ();

			if (mp->b_rptr - mp->b_wptr != sizeof (struct _monitor)
					|| mp->b_cont != NULL) {
				freemsg (mp);
				putctlerr (RD (q), -EIO);
				flushq (q, FLUSHDATA);
				splx (ms);
				return;
			}
			mon = (struct _monitor *) mp->b_rptr;
			i = ip_mon_index (mon->local, mon->remote, mon->p_protocol, NULL,mon->p_local,mon->p_remote);
			if (i < 0)
				freemsg (mp);
			else {
				if (ip_info[i] != NULL)
					qreply (q, ip_info[i]);
				ip_info[i] = mp;
			}
			splx (ms);
		}
	} else {
		while ((mp = getq (q)) != NULL) {
			switch (DATA_TYPE(mp)) {
			case MSG_PROTO:
				ip_mon_proto (q, mp, 1);
				break;
			case CASE_DATA:
				{
				mblk_t *mq;

				if(!canput(q->q_next)) {
					putbq(q,mp);
					return;
				}
				mq = count_packet(ipmon, mp,0);
				if(mq != NULL)
					putnext (q, mq);
				}
				break;
			case M_FLUSH:
				if (*mp->b_rptr & FLUSHW)
					flushq (q, FLUSHDATA);
				/* FALL THRU */
			default:
				putnext (q, mp);
				continue;
			}
		}
	}
	return;
}


static void
ip_mon_close (queue_t * q, int dummy)
{
	struct _ip_mon *ipmon = (struct _ip_mon *) q->q_ptr;

	flushq (q, FLUSHALL);
	flushq (WR (q), FLUSHALL);
	printf ("IP_MON driver %d closed.\n", ipmon->nr);
	if (ipmon->hastimer) {
#ifdef NEW_TIMEOUT
		untimeout (ipmon->timeout);
#else
		untimeout (ip_mon_timer, ipmon);
#endif
	}
	ipmon->qptr = NULL;

	if(ipmon != &ip_mon)
		free(ipmon);

	LESS_USE;
	return;
}


static void
ip_mon_rput (queue_t * q, mblk_t * mp)
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

	case M_HANGUP:
	case M_ERROR:
	default:
		putnext (q, mp);		  /* don't know what to do with this, so send
								   * it along */

	}
	return;
}

static void
ip_mon_rsrv (queue_t * q)
{
	mblk_t *mp;
	struct _ip_mon *ipmon = (struct _ip_mon *) q->q_ptr;

	while ((mp = getq (q)) != NULL) {
		switch (DATA_TYPE(mp)) {
		case MSG_PROTO:
			ip_mon_proto (q, mp, 0);
			break;
		case CASE_DATA:
			{
				mblk_t *mq;

				if(!canput(q->q_next)) {
					putbq(q,mp);
					return;
				}
				mq = count_packet(ipmon, mp,1);
				if(mq != NULL)
					putnext (q, mq);
			}
			break;
		default:
			putnext (q, mp);
			continue;
		}
	}
	return;
}



#ifdef MODULE
static int devmajor = 0;

static int do_init_module(void)
{
	int err;
	err = register_strmod(&ip_moninfo);
	if (err < 0)
		return err;
	err = register_strdev(0,&ip_moninfo,0);
	if(err < 0) {
		unregister_strmod(&ip_moninfo);
		return err;
	}
	devmajor = err;
	return 0;
}

static int do_exit_module(void)
{
	int err1 = unregister_strmod(&ip_moninfo);
	int err2 = unregister_strdev(devmajor,&ip_moninfo,0);
	return err1 || err2;
}
#endif
