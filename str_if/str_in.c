/*
 * str_if.c - Streams top level module handles the if_ interfacing. Copyright
 * (C) 1990  Brad K. Clements, All Rights Reserved.
 * 
 * Adapted to the ISDN protocol handler by Matthias Urlichs,
 * urlichs@smurf.sub.org.
 */
#include "primitives.h"
#include "f_ip.h"

#if defined(AUX)
#define DOT .
#endif
#if defined(linux)
#define DOT ->
#endif

#include <sys/types.h>

#define NSTR 8

#include "streams.h"
#include "stropts.h"
#ifdef M_UNIX
#include <sys/socket.h>
#endif

#include <sys/time.h>
#ifdef DONT_ADDERROR
#include "f_user.h"
#endif
#ifdef AUX
#include <sys/mmu.h>
#endif
#ifdef M_UNIX
#include <sys/immu.h>
#endif
#ifdef linux
#include <net/netisr.h>
#else
#include <sys/page.h>
#include <sys/region.h>
#include <sys/proc.h>
#include <sys/systm.h>
#endif
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include "f_ioctl.h"
#include <sys/file.h>
#include <sys/uio.h>

#include "str_if.h"
#include "streamlib.h"
#include "isdn_proto.h"

struct str_if_info {
	struct ifnet if_net;
	int flags;
#define	PII_FLAGS_INUSE		01	  /* */
#define	PII_FLAGS_ATTACHED	02	  /* already if_attached	 */
#define	PII_FLAGS_TIMER   	04	  /* already if_attached	 */
	queue_t *q;
#ifdef NEW_TIMEOUT
	long timer;
#endif
};

#if DEBUG
int str_if_debug = 1;

#else
#define str_if_debug 0
#endif
#define DLOG if(str_if_debug) printf

static qf_open str_if_open;
static qf_close str_if_close;
static qf_put str_if_rput, str_if_wput;
static qf_srv str_if_rsrv, str_if_wsrv;


static struct module_info minfo =
{
		0xbad, "str_if", 0, INFPSZ, 4096,1024
};

static struct qinit r_init =
{
		str_if_rput, str_if_rsrv, str_if_open, str_if_close, NULL, &minfo, NULL
};
static struct qinit w_init =
{
		str_if_wput, str_if_wsrv, str_if_open, str_if_close, NULL, &minfo, NULL
};
struct streamtab str_ifinfo =
{
		&r_init, &w_init, NULL, NULL
};

typedef struct str_if_info PII;

static PII pii[NSTR];

static int str_output (struct ifnet *ifp, struct mbuf *m0, struct sockaddr *dst);
static int str_ioctl (struct ifnet *ifp, int cmd, caddr_t data);

static void
str_if_again (struct str_if_info *p)
{

	if(!(p->flags & PII_FLAGS_TIMER))
		return;
	p->flags &=~ PII_FLAGS_TIMER;
	qenable (p->q);
}

static int
str_attach (int unit)
{
	register struct ifnet *ifp = &pii[unit].if_net;

	ifp->if_name = "str";
	ifp->if_mtu = 1500;
	ifp->if_flags = IFF_POINTOPOINT;
	ifp->if_unit = unit;
	ifp->if_ioctl = (int (*)())str_ioctl;
	ifp->if_output = (int (*)())str_output;
	ifp->if_snd.ifq_maxlen = IFQ_MAXLEN;
	if_attach (ifp);
	pii[unit].flags |= PII_FLAGS_ATTACHED;
	return 0;
}





static void
str_if_proto (queue_t * q, mblk_t * mp, char down)
{
	PII *p = (PII *) q->q_ptr;
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
	case PROTO_CONNECTED:
#if 0
		if (!(p->flags & PII_FLAGS_CONFIGURED)) {
			*(ushort_t *) (mp->b_rptr + 1) = PROTO_DISCONNECT;
			qreply (q, mp);
			mp = NULL;
			break;
		}
#endif
		printf ("str_if%d Connected\n", p->if_net.if_unit);
		p->if_net.if_flags |= IFF_RUNNING;

		break;
	case PROTO_LISTEN:
		break;
	case PROTO_DISCONNECT:
		printf ("str_if%d Disconnected\n", p->if_net.if_unit);
		p->if_net.if_flags &= ~IFF_RUNNING;
		break;
	case PROTO_MODULE:
		if (strnamecmp (q, mp)) { /* Config information for me. */
			ulong_t x, y;
			long z;

			while (mp != NULL && m_getsx (mp, &id) == 0) {
				switch (id) {
				  err:
					printf (" :Err %s\n", mp->b_rptr);
					mp->b_rptr = origmp;
					m_reply (q, mp, EINVAL);
					mp = NULL;
				default:
					break;
				case STRIF_MTU:
					if (m_geti (mp, &z) != 0)
						goto err;
					if (z < 1 || z >= 4196)
						goto err;
					p->if_net.if_mtu = z;
					break;
				}
			}
			if(mp != NULL) {
				mp->b_rptr = origmp;
				m_reply(q,mp,0);
				mp = NULL;
			}
		}
		break;
	}
	if (mp != NULL) {
		if(origmp != NULL)
			mp->b_rptr = origmp;
		putnext (q, mp);
	}
}


void
str_ifinit (void)
{
	extern int ipprintfs;
	extern int ipforwarding;

	ipprintfs = 0;
	ipforwarding = 1;
}

static int
str_if_open (queue_t * q, dev_t dev, int flag, int sflag
#ifdef DO_ADDERROR
		,int *err
#define U_ERROR *err
#else
#define U_ERROR u.u_error
#endif
)
{
	register PII *p = NULL;
	register int x;
	int s;

	if (!suser ()){
		printf ("str_if: not superuser\n");
		U_ERROR = EPERM;
		return (OPENFAIL);
	}
	s = splstr ();

	if (!q->q_ptr) {
		for (x = 0; x < NSTR; x++) {
			if (!(pii[x].flags & PII_FLAGS_INUSE)) {	/* we can use it */
				p = &pii[x];
				break;
			}
		}
		if (p == NULL) {
			printf ("str_if: no free module\n");
			U_ERROR = ENOENT;
			return OPENFAIL;
		}
	} else
		p = (PII *) q->q_ptr;

	DLOG ("str_if%d: open\n", p - pii);
	if (!(p->flags & PII_FLAGS_ATTACHED))
		str_attach (p - pii);	  /* attach it */
	p->q = WR (q);
	WR (q)->q_ptr = q->q_ptr = (caddr_t) p;		/* set write Q and read Q to
												 * point here */
	p->flags = PII_FLAGS_INUSE | PII_FLAGS_ATTACHED;
	splx (s);
	return (0);
}

static void
str_if_close (queue_t *q, int dummy)
{
	PII *p = (PII *) q->q_ptr;
	int s;

	s = splimp ();
	if(p->flags & PII_FLAGS_TIMER) {
#ifdef NEW_TIMEOUT
		untimeout(p->timer);
#else
		untimeout(str_if_again,p);
#endif
	}
	p->flags &= ~(PII_FLAGS_INUSE | PII_FLAGS_TIMER);
	if_down (&p->if_net);
	printf ("str_if%d closed\n", p->if_net.if_unit);
	p->if_net.if_flags &= ~(IFF_RUNNING | IFF_UP);

	DLOG ("str_if%d: closed\n", p->if_net.if_unit);
	flushq (q, FLUSHALL);
	flushq (WR (q), FLUSHALL);
	splx (s);
}


/* Streams code to write data. */
static void
str_if_wput (queue_t * q, mblk_t * mp)
{
	switch (mp->b_datap->db_type) {
	case CASE_DATA: 
		freemsg (mp);
		break;
	case M_FLUSH:
		if (*mp->b_rptr & FLUSHW)
			flushq (q, FLUSHDATA);
		putnext (q, mp);
		break;
	case MSG_PROTO:
	default:
		putq (q, mp);
		break;
	}
	return;
}

/* Streams code to scan the write queue. */
static void
str_if_wsrv (queue_t * q)
{
	mblk_t *mp;
	PII *p = (PII *) q->q_ptr;

	while ((mp = getq (q)) != NULL) {
		switch (mp->b_datap->db_type) {
		case M_IOCTL:
			{
				struct iocblk *i = (struct iocblk *) mp->b_rptr;

				switch (i->ioc_cmd) {

				case SIOCGETU:	  /* get unit number */
					if (mp->b_cont = allocb (sizeof (int), BPRI_MED)) {
						*(int *) mp->b_cont->b_wptr = p->if_net.if_unit;
						mp->b_cont->b_wptr += i->ioc_count = sizeof (int);

						mp->b_datap->db_type = M_IOCACK;
						qreply (q, mp);
						break;
					}
					i->ioc_error = ENOSR;
					i->ioc_count = 0;
					mp->b_datap->db_type = M_IOCNAK;
					qreply (q, mp);
					break;
				default:
					putnext (q, mp);
				}
			} break;
		case MSG_PROTO:
			str_if_proto (q, mp, 1);
			break;
		case M_FLUSH:
			if (*mp->b_rptr & FLUSHW)
				flushq (q, FLUSHDATA);
			putnext (q, mp);
			break;
		case CASE_DATA:
			if(p->if_net.if_flags & IFF_RUNNING) {
				freemsg(mp);
				break;
			} /* else FALL THRU */
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


/* Streams code to read data. */
static void
str_if_rput (queue_t * q, mblk_t * mp)
{
	register PII *p = (PII *) q->q_ptr;

	switch (mp->b_datap->db_type) {

	case M_FLUSH:
		if (*mp->b_rptr & FLUSHR) {
			flushq (q, FLUSHDATA);
		}
		putnext (q, mp);		  /* send it along too */
		break;
	case MSG_PROTO:
	case CASE_DATA:
	default:
		putq (q, mp);			  /* queue it for my service routine */
		break;

	case M_ERROR:
	case M_HANGUP:
		printf ("str_if%d hungup\n", p->if_net.if_unit);
		p->if_net.if_flags &= ~IFF_RUNNING;
		putnext (q, mp);
		break;
	}
	return;
}

static void
str_if_rsrv (queue_t *q)
{
	register mblk_t *mp, *m0;

	register PII *p;

	p = (PII *) q->q_ptr;

	while ((mp = getq (q)) != NULL) {
		switch (mp->b_datap->db_type) {
		case MSG_PROTO:
			str_if_proto (q, mp, 0);
			break;
		case CASE_DATA:
			if(p->if_net.if_flags & IFF_RUNNING) {
				struct ifnet **ifp;
				struct mbuf *mb1, *mb2 = NULL, *mbtail = NULL;
				int len, xlen, count, s;
				uchar_t *rptr;
				int hdroffset = sizeof (struct ifnet *);

				if(!(p->if_net.if_flags & IFF_UP)) {
					/* Not yet. Wait. */
					putbq(q,mp);
					return;
				}

				m0 = mp;		  /* remember first message block */

				len = 0;
				mb1 = NULL;
				xlen = (uchar_t *) mp->b_wptr - (rptr = (uchar_t *) mp->b_rptr);
				while (mp) {
					if (len < 1) {
						MGET (mb2, M_DONTWAIT, MT_DATA);
						if (!mb2) {
							printf("* No MBlk! %d\n",dsize(m0));
							p->if_net.if_ierrors++;
							putbq (q, m0);
							if(!(p->flags & PII_FLAGS_TIMER)) {
								p->flags |= PII_FLAGS_TIMER;
#ifdef NEW_TIMER
								p->timer =
#endif
								timeout(str_if_again,p,HZ/5); /* try again */
							}
							if (mb1 != NULL)
								m_freem (mb1);	/* discard what we've used
												 * already */
							return;
						}
						len = MLEN - hdroffset;
						mb2->m_len = 0;
						if (mb1 != NULL) {
							mbtail->m_next = mb2;
							mbtail = mb2;
						} else
							mbtail = mb1 = mb2;
					}
					count = imin (xlen, len);
					bcopy ((char *) rptr, mtod (mb2, char *)+ mb2->m_len + hdroffset, count);

					rptr += count;
					len -= count;
					xlen -= count;
					mb2->m_len += count + hdroffset;
					hdroffset = 0;
					if (xlen < 1) {		/* move to the next mblk */
						mp = mp->b_cont;
						if (mp)
							xlen = (uchar_t *) mp->b_wptr - (rptr = (uchar_t *) mp->b_rptr);
					}
				}
				ifp = (struct ifnet **) (mtod (mb1, uchar_t *));
				*ifp = &p->if_net; /* stick ifnet * in front of packet */
				freemsg (m0);
				p->if_net.if_ipackets++;

				s = splimp ();
				if (IF_QFULL (&ipintrq)) {
					IF_DROP (&ipintrq);
					p->if_net.if_ierrors++;
					m_freem (mb1);
				} else {
					IF_ENQUEUE (&ipintrq, mb1);
					schednetisr (NETISR_IP);
				}
				splx (s);
				break;
			} /* else FALL THRU */
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

/* ifp output procedure */
static int
str_output (struct ifnet *ifp, struct mbuf *m0, struct sockaddr *dst)
{
	register PII *p = &pii[ifp->if_unit];
	struct mbuf *m1;
	int error, len;
	ushort_t protocol;
	mblk_t *mp;

	error = 0;
	if ((ifp->if_flags & (IFF_RUNNING|IFF_UP)) != (IFF_RUNNING|IFF_UP) || !p->q) {
		error = ENETDOWN;
		goto getout;
	}
	switch (dst->sa_family) {
	case AF_INET:
		break;
	default:
		printf ("str%d: af%d not supported\n", ifp->if_unit, dst->sa_family);
		error = EAFNOSUPPORT;
		goto getout;
	}
	len = 0;
	for (m1 = m0; m1; m1 = m1->m_next)
		len += m1->m_len;

	if (!canput (p->q->q_next) || (mp = allocb (len, BPRI_LO)) == NULL) {
		error = ENOBUFS;
		goto getout;
	}
	for (m1 = m0; m1; m1 = m1->m_next) {		/* copy all data */
		bcopy (mtod (m1, char *), (char *) mp->b_wptr, m1->m_len);

		mp->b_wptr += m1->m_len;
	}
	putnext (p->q, mp);

	p->if_net.if_opackets++;
  getout:
	m_freem (m0);
	if (error) {
		p->if_net.if_oerrors++;
	}
	return (error);
}

/*
 * if_ ioctl requests
 */
static int
str_ioctl (struct ifnet *ifp, int cmd, caddr_t data)
{
	register struct ifaddr *ifa = (struct ifaddr *) data;
	register struct ifreq *ifr = (struct ifreq *) data;
	int error = 0;
	int s = splimp ();

	if (ifa == NULL) {
		splx (s);
		sysdump("Verify",NULL,0xDEADBEEF);
		return (EFAULT);
	}
	switch (cmd) {
	case SIOCSIFFLAGS:
		if (!suser ()){
			error = EPERM;
			break;
		}
		ifp->if_flags = (ifp->if_flags & IFF_CANTCHANGE)
				| (ifr->ifr_flags & ~IFF_CANTCHANGE);

		if(((struct str_if_info *)ifp)->flags & PII_FLAGS_INUSE)
			qenable(((struct str_if_info *)ifp)->q);
		break;
	case SIOCGIFFLAGS:
		ifr->ifr_flags = ifp->if_flags;
		break;
	case SIOCSIFADDR:
		if (ifa->ifa_addr DOT sa_family != AF_INET)
			error = EAFNOSUPPORT;
		break;
	case SIOCSIFDSTADDR:
		if (ifa->ifa_addr DOT sa_family != AF_INET)
			error = EAFNOSUPPORT;
		break;

	case SIOCSIFMTU:
		if (!suser ()){
			error = EPERM;
			break;
		}
		if (ifr->ifr_mtu < 120 || ifr->ifr_mtu > 4090) {
			error = EINVAL;
			break;
		}
		ifp->if_mtu = ifr->ifr_mtu;
		break;
	case SIOCGIFMTU:
		ifr->ifr_mtu = ifp->if_mtu;
		break;
	default:
		error = EINVAL;
		break;
	}
	splx (s);
	return (error);
}
