/*
 * str_if.c - Streams top level module handles the if_ interfacing. Copyright
 * (C) 1990  Brad K. Clements, All Rights Reserved.
 * 
 * Adapted to the ISDN protocol handler and Linux networking
 * by Matthias Urlichs, urlichs@smurf.sub.org.
 */
#if defined(AUX)
#define DOT .
#endif
#if defined(linux)
#define DOT ->
#define UAREA
#endif

#undef UNREGISTER   /* does not work yet */

#include "f_module.h"
#include "primitives.h"
#include "f_ip.h"
#include "f_malloc.h"
#include "ppp.h"

#include <sys/types.h>

#define NSTR 8

#include "primitives.h"
#include "streams.h"
#include <sys/stropts.h>
#include <sys/socket.h>

#include <sys/time.h>
#ifdef DONT_ADDERROR
#include "f_user.h"
#endif
#include <linux/syscompat.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include "f_ioctl.h"
#include <sys/file.h>
#include <sys/uio.h>

#include "str_if.h"
#include "streamlib.h"
#include "isdn_proto.h"

/* Copyright 1994, Mattias Urlichs. */

/*
	Based on a skeleton driver written 1993 by Donald Becker.

	Copyright 1993 United States Government as represented by the Director,
	National Security Agency.  This software may only be used and distributed
	according to the terms of the GNU Public License as modified by SRC,
	incorporated herein by reference.

	The author may be reached as becker@super.org or
	C/O Supercomputing Research Ctr., 17100 Science Dr., Bowie MD 20715

	Modified 1994 by Matthias Urlichs to behave like a Streams interface.
*/

#include <linux/config.h>

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/interrupt.h>
#include <linux/ptrace.h>
#include <linux/ioport.h>
#include <linux/in.h>
#include <linux/malloc.h>
#include <linux/string.h>
#include <asm/system.h>
#include <asm/bitops.h>
#include <asm/io.h>
#include <asm/dma.h>
#include <errno.h>

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>

#ifdef CONFIG_DEBUG_STREAMS
int strl_debug = 1;
#else
#define strl_debug 0
#endif
#define DLOG if(strl_debug) printf

/* Information that need to be kept for each board. */
struct strl_local {
	struct enet_statistics stats;
	queue_t *q;
	short offset;
	short flags;
#define PII_FLAGS_PPP 01
#if 0
#ifdef NEW_TIMEOUT
	long timer;
#endif
#endif
};

/* Index to functions, as function prototypes. */


static int strl_init(struct device *dev);
static int strl_open(struct device *dev);
static int strl_send_packet(struct sk_buff *skb, struct device *dev);
static int strl_close(struct device *dev);
static struct enet_statistics *strl_get_stats(struct device *dev);

static int
strl_header(unsigned char *buff, struct device *dev, unsigned short type,
	  void *daddr, void *saddr, unsigned len, struct sk_buff *skb)
{
	return 0;
}
static unsigned short strl_type_trans (struct sk_buff *skb, struct device *dev)
{
  return htons(ETH_P_IP);
}

static int
strl_rebuild_header(void *buff, struct device *dev, unsigned long raddr,
		struct sk_buff *skb)
{
  return(0);
}

static int strl_init(struct device *dev)
{
	return 0;
}


/* Open/initialize the driver. */
static int
strl_open(struct device *dev)
{
	struct strl_local *lp = (struct strl_local *)dev->priv;

	if(lp == NULL || lp->q == NULL)
		return -ENXIO;

	dev->tbusy = 0;
	dev->interrupt = 0;
	dev->start = 1;
	/* dev->flags |= IFF_UP; */
	return 0;
}

static int
strl_send_packet(struct sk_buff *skb, struct device *dev)
{
	struct strl_local *lp = (struct strl_local *)dev->priv;
	mblk_t *mb;

	if (skb == NULL)  {
printk("SK NULL\n");
		return 0;
	}

	if(lp == NULL || lp->q == NULL)  {
printk("Q NULL\n");
		dev->tbusy = 1; /* This is a gross hack... */
		return -ENXIO;
	}
	
	if(!canput(WR(lp->q)->q_next)) {
/* printk("Queue full\n"); */
		dev->tbusy = 1;
		return -EAGAIN;
	}
	
	mb = allocb(skb->len+lp->offset, BPRI_LO);
	if(mb == NULL)  {
printk("No Buff\n");
		dev->tbusy = 1;
		return -ENOBUFS;
	}

	dev->tbusy = 0;

	mb->b_rptr += lp->offset;
	mb->b_wptr += lp->offset;
	if(lp->flags & PII_FLAGS_PPP) {
		mb->b_rptr -= 2;
		*(ushort_t *)mb->b_rptr = PPP_IP;
	}

	memcpy(mb->b_wptr,skb->data,skb->len);
	mb->b_wptr += skb->len;
	dev_kfree_skb (skb, FREE_WRITE);
	putnext(WR(lp->q),mb);
	lp->stats.tx_packets++;

	return 0;
}

/* The inverse routine to strl_open(). */
static int
strl_close(struct device *dev)
{
	struct strl_local *lp = (struct strl_local *)dev->priv;

	dev->tbusy = 1;
	dev->start = 0;
	dev->flags &=~ IFF_UP;

	if(lp != NULL && lp->q != NULL) {
		flushq(lp->q,FLUSHDATA);
		flushq(WR(lp->q),FLUSHDATA);
	}

	return 0;
}

/* Get the current statistics.	This may be called with the card open or
   closed. */
static struct enet_statistics *
strl_get_stats(struct device *dev)
{
	struct strl_local *lp = (struct strl_local *)dev->priv;

	if(lp != NULL)
		return &lp->stats;
	else
		return NULL;
}


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


static void
str_if_proto (queue_t * q, mblk_t * mp, char isdown)
{
	struct device *dev = (struct device *) q->q_ptr;
	struct strl_local *lp = (struct strl_local *)dev->priv;
	streamchar *origmp = mp->b_rptr;
	ushort_t id;

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
			if(!isdown) {
				if (lp->flags & PII_FLAGS_PPP) {
					z += 2;
					lp->offset = z;
				} else {
					lp->offset = z;
					z = 0;
				}
				freemsg(mp);
				if((mp = allocb(10,BPRI_MED)) != NULL) {
					m_putid(mp,PROTO_OFFSET);
					m_puti(mp,z);
					mp->b_datap->db_type = MSG_PROTO;
					origmp = NULL;
				}
			}
		}
		break;
	case PROTO_CONNECTED:
		dev->flags |= IFF_RUNNING;
		break;
	case PROTO_DISCONNECT:
		dev->flags &= ~(IFF_UP|IFF_RUNNING);
		break;
	case PROTO_INTERRUPT:
		dev->flags &= ~IFF_RUNNING;
		break;
	case PROTO_MODULE:
		if (strnamecmp (q, mp)) { /* Config information for me. */
			long z;

			while (mp != NULL && m_getsx (mp, &id) == 0) {
				switch (id) {
				  err:
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
					dev->mtu = z;
					break;
				case STRIF_PPP:
					lp->flags |= PII_FLAGS_PPP;
					break;
				case STRIF_NOPPP:
					lp->flags &= ~PII_FLAGS_PPP;
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
		if (origmp != NULL)
			mp->b_rptr = origmp;
		putnext (q, mp);
	}
}


#ifndef UNREGISTER
void **netfree = NULL;
#endif

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
	struct strl_local *lp;
	struct device *netdev;
	int s;
	int err = 0;
	int nr = 0;
	int allocated = 1;
#ifndef UNREGISTER
	void **freedev;
#endif

	if (!suser ()){
		printf ("str_if: not superuser\n");
		U_ERROR = EPERM;
		return (OPENFAIL);
	}
	s = splstr ();

	if (q->q_ptr) {
		splx(s);
		return 0;
	}
#ifndef UNREGISTER
	freedev = netfree;
	if(freedev != NULL) {
		netfree = *freedev;
		freedev++;
		netdev = (void *)freedev;
		lp = (struct strl_local *)netdev->priv;
		allocated = 0;
	} else
#endif
	{
		struct device *nd;
		netdev = malloc(sizeof(*netdev));
		if(netdev == NULL) {
			splx(s);
			return OPENFAIL;
		}
		memset(netdev,0,sizeof(*netdev));
		lp = malloc(sizeof(*lp));
		if(lp == NULL) {
			free(netdev);
			splx(s);
			return OPENFAIL;
		}
		memset(lp,0,sizeof(*lp));
		nr=0;
		do {
			nr++;
			for(nd=dev_base;nd != NULL; nd = nd->next) {
				if(nd->open == strl_open && nd->base_addr == nr)
					break;
			}
		} while(nd!=NULL);
		ether_setup(netdev);
		netdev->priv = lp;
		netdev->base_addr = nr;
	}
	netdev->flags = IFF_POINTOPOINT;
	netdev->rebuild_header = strl_rebuild_header;
	netdev->open = strl_open;
	netdev->stop = strl_close;
	netdev->init = strl_init;
	netdev->get_stats = strl_get_stats;
	netdev->hard_header = strl_header;
	netdev->hard_start_xmit = strl_send_packet;
	netdev->hard_header_len = 0;
	netdev->addr_len = 0;
	netdev->type = ARPHRD_SLIP;
	netdev->type_trans = strl_type_trans;
	netdev->mtu = 576;
	netdev->family = AF_INET;
	netdev->pa_alen = sizeof(unsigned long);

	lp->q = q;
	WR (q)->q_ptr = q->q_ptr = (caddr_t) netdev;

	if(allocated) {
		netdev->name = malloc(8);
		if (netdev->name == NULL) {
			err = -ENOMEM;
		} else {
			sprintf(netdev->name,"str%d",nr);
			err = register_netdev(netdev);
		}
	}
	if(err < 0) {
		q->q_ptr = NULL;
		if(allocated) {
			free(netdev);
			free(lp);
		}
		U_ERROR = err;
		splx(s);
		return OPENFAIL;
	}

	splx (s);
	MORE_USE;
	return (0);
}

static void
str_if_close (queue_t *q, int dummy)
{
	struct device *dev = (struct device *) q->q_ptr;
	struct strl_local *lp = (struct strl_local *)dev->priv;
	int s;
#ifndef UNREGISTER
	void **freedev;
#endif

	s = splimp ();

	dev_close(dev);
#ifdef UNREGISTER
	unregister_netdev(dev);
	dev->priv = NULL;
#endif

#if 0
	if(lp->timer) {
#ifdef NEW_TIMEOUT
		untimeout(lp->timer);
#else
		untimeout(str_if_again,p);
#endif
	}
	lp->timer = 0;
#endif

	lp->q = NULL;
	flushq (q, FLUSHALL);
	flushq (WR (q), FLUSHALL);
#ifdef UNREGISTER
	free(lp);
	free(dev);
	LESS_USE;
#else
	freedev = (void *)dev;
	freedev--;
	*freedev = netfree;
	netfree = freedev;
#endif
	splx (s);
}


/* Streams code to write data. */
static void
str_if_wput (queue_t * q, mblk_t * mp)
{
	/* struct device *dev = (struct device *) q->q_ptr; */
	/* struct strl_local *lp = (struct strl_local *)dev->priv; */
	switch (mp->b_datap->db_type) {
	case M_FLUSH:
		if (*mp->b_rptr & FLUSHW)
			flushq (q, FLUSHDATA);
		putnext (q, mp);
		break;
	CASE_DATA
	case MSG_PROTO:
	default:
		putq (q, mp);
		break;
	}
}

/* Streams code to scan the write queue. */
static void
str_if_wsrv (queue_t * q)
{
	mblk_t *mp;
	struct device *dev = (struct device *) q->q_ptr;
	struct strl_local *lp = (struct strl_local *)dev->priv;

	while ((mp = getq (q)) != NULL) {
		switch (mp->b_datap->db_type) {
		case M_IOCTL:
			{
				struct iocblk *i = (struct iocblk *) mp->b_rptr;
				mblk_t *m0;

				switch (i->ioc_cmd) {

				case SIOCGETU:	  /* get unit number */
					if ((m0 = allocb (sizeof (int), BPRI_MED)) != NULL) {
						m0->b_cont = mp->b_cont;
						mp->b_cont = m0;
						*((int *) m0->b_wptr)++ = dev->base_addr;
						i->ioc_count = sizeof (int);

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
			}
			break;
		case MSG_PROTO:
			str_if_proto (q, mp, 1);
			break;
		CASE_DATA
			if(lp->flags & PII_FLAGS_PPP) {
#if 0
				mblk_t *mq = pullupm(mp,2);
				if (mq != NULL) {
					u_short i;
					mp = mq;
					i = 
				}
#endif
			} else if(!((IFF_UP|IFF_RUNNING) & ~dev->flags)) {
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
	dev->tbusy = 0;
	mark_bh(NET_BH);
}


/* Streams code to read data. */
static void
str_if_rput (queue_t * q, mblk_t * mp)
{
	struct device *dev = (struct device *) q->q_ptr;
	/* struct strl_local *lp = (struct strl_local *)dev->priv; */

	switch (mp->b_datap->db_type) {

	case M_FLUSH:
		if (*mp->b_rptr & FLUSHR) {
			flushq (q, FLUSHDATA);
		}
		putnext (q, mp);		  /* send it along too */
		break;
	case MSG_PROTO:
	CASE_DATA
	default:
		putq (q, mp);			  /* queue it for my service routine */
		break;

	case M_ERROR:
	case M_HANGUP:
		if(0)printf ("%s hungup\n", dev->name);
		dev->flags &= ~(IFF_UP&IFF_RUNNING);
		putnext (q, mp);
		break;
	}
}

static void
str_if_rsrv (queue_t *q)
{
	register mblk_t *mp, *m0;

	struct device *dev = (struct device *)q->q_ptr;
	struct strl_local *lp = (struct strl_local *)dev->priv;

	while ((mp = getq (q)) != NULL) {
		switch (mp->b_datap->db_type) {
		case MSG_PROTO:
			str_if_proto (q, mp, 0);
			break;
		CASE_DATA
			if(lp->flags & PII_FLAGS_PPP) {
				m0 = pullupm(mp,2);
				if(m0 == NULL)
					goto def;
				mp = m0;
				if(*(ushort_t *)mp->b_rptr == PPP_IP)
					mp->b_rptr += 2;
				else
					goto def;
			}
            if(!((IFF_UP | IFF_RUNNING) & ~dev->flags)) {
				struct strl_local *lp = (struct strl_local *)dev->priv;
				struct sk_buff *skb;
				int len = msgdsize(mp);
				int offset = 0;

				skb = alloc_skb(len, GFP_ATOMIC);
				if (skb == NULL) {
					printk(KERN_INFO "%s: Memory squeeze, dropping packet.\n", dev->name);
					lp->stats.rx_dropped++;
					putbqf(q,mp);
					return;
				}
				while(mp != NULL) {
					int xlen = mp->b_wptr-mp->b_rptr;
					memcpy(skb->data+offset, mp->b_rptr, xlen);
					offset += xlen;
					{
						mblk_t *m1 = mp->b_cont;
						freeb(mp);
						mp = m1;
					}
				}
				skb->len = len;
				skb->dev = dev;

				netif_rx(skb);
				lp->stats.rx_packets++;
				break;
			}
		default:
		def:
			if (mp->b_datap->db_type > QPCTL || canput (q->q_next)) {
				putnext (q, mp);
				continue;
			} else {
				putbq (q, mp);
				return;
			}
		}
	}
	dev->tbusy = 0;
	mark_bh(NET_BH);
}

#if 0
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
#endif

void str_ifinit(void) { }


#ifdef linux
static int do_init_module(void)
{
	return register_strmod(&str_ifinfo);
}

static int do_exit_module(void)
{
	return unregister_strmod(&str_ifinfo);
}
#endif
