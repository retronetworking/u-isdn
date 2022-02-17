/* needs the "strlog" Stremas module, with log_printmsg visible, for now */

#define UAREA

#include "f_module.h"
#include "primitives.h"
#include <sys/time.h>
#include "f_signal.h"
#include <sys/sysmacros.h>
#include "streams.h"
#include "f_malloc.h"
#include "port_m.h"
#include <sys/stropts.h>
#ifdef DONT_ADDERROR
#include "f_user.h"
#endif
#include <sys/errno.h>
#ifndef linux
#include <sys/reg.h>
#include <sys/var.h>
#endif
#include <sys/file.h>
#include <sys/conf.h>
#include "f_ioctl.h"
#include "f_termio.h"
#include <stddef.h>
#include "streamlib.h"
#include "smallq.h"
#include "lap.h"
#include "isdn_limits.h"
#include "isdn_proto.h"

extern void log_printmsg (void *log, const char *text, mblk_t * mp, const char*);

/* Debugging */
static unsigned short portm_debug = 0x110;

/* #define portm_debug 0 */

#ifdef CONFIG_DEBUG_STREAMS
#undef qenable
static inline void qenable(queue_t *q) { deb_qenable("PORT_M",0,q); }
#endif

enum P_state {
	P_free, P_master, P_driver, P_module,
};

typedef struct _portm_chan {
	enum P_state status;
	SUBDEV linkto;				  /* Driver <-> Module */
	queue_t *link;
	struct termio tty;
	char baud;
	int flags;
	queue_t *qptr;
	unsigned long passcount;
} *portm_chan;

/*
 * Data.
 */
static struct _portm_chan port_chan[NPORT];

/*
 * Standard Streams driver information.
 */
static struct module_info portm_minfo =
{
		0, "portman", 0, INFPSZ, 4096,1024
};

static qf_open portm_open;
static qf_close portm_close;
static qf_put portm_wput,portm_rput;
static qf_srv portm_wsrv,portm_rsrv;

static struct qinit portm_rinit =
{
		portm_rput, portm_rsrv, portm_open, portm_close, NULL, &portm_minfo, NULL
};

static struct qinit portm_winit =
{
		portm_wput, portm_wsrv, NULL, NULL, NULL, &portm_minfo, NULL
};

struct streamtab portminfo =
{&portm_rinit, &portm_winit, NULL, NULL};
struct streamtab portm2_info =
{&portm_rinit, &portm_winit, NULL, NULL};

static void poplist (queue_t * q, char initial);

int
portm_init (void)
{
	int i;

	printf ("Port Manager installed.\n");
	bzero (port_chan, sizeof (struct _portm_chan) * NPORT);

	return 0;
}

static ushort_t
portsig (enum P_state typ)
{
	switch (typ) {
		default:
		return CHAR2 ('?', '?');
	case P_driver:
		return PORT_DRIVER;
	case P_module:
		return PORT_MODULE;
	}
}

#ifdef CONFIG_DEBUG_STREAMS
static int
c_qattach (register struct streamtab *qinfo, register queue_t * qp, int flag)
{
	register queue_t *rq;
	register s;
	int sflg;
	int err,uerr;

	if (!(rq = allocq ())){
		printf (" :allocq");
		return (0);
	}
	qp = qp->q_next;
	sflg = 0;
	s = splstr ();
	rq->q_next = qp;
	WR (rq)->q_next = WR (qp)->q_next;
	if (WR (qp)->q_next) {
		OTHERQ (WR (qp)->q_next)->q_next = rq;
		sflg = MODOPEN;
	}
	WR (qp)->q_next = WR (rq);
	setq (rq, qinfo->st_rdinit, qinfo->st_wrinit);
	rq->q_flag |= QWANTR;
	WR (rq)->q_flag |= QWANTR;

	uerr = u.u_error;
	if ((err = (*rq->q_qinfo->qi_qopen) (rq, 0, flag, sflg)) < 0) {
		printf (" :No Open %d %x", u.u_error, rq->q_qinfo->qi_qopen);
		u.u_error = ((err == OPENFAIL) ? uerr : err);
		qdetach (rq, 0, 0);
		splx (s);
		return (0);
	}
	u.u_error = uerr;
	splx (s);
	return (1);
}
#endif


/* Streams code to open the driver. */
static int
portm_open (queue_t * q, dev_t dev, int flag, int sflag
#ifdef DO_ADDERROR
		,int *err
#define U_ERROR *err
#else
#define U_ERROR u.u_error
#endif
)
{
	portm_chan ch;
	int ms;
	char listening;

	printf("port_open qptr=%x dev=%x flag=%x sflag=%x \n",q->q_ptr,dev,flag,sflag);
	if(q->q_ptr != NULL) return 0;
	dev = minor (dev);
	if (sflag == CLONEOPEN || sflag == MODOPEN) {
		if (port_chan->qptr == NULL) {
			printf ("portm_open: Master program not running!\n");
			U_ERROR = ENXIO;
			return OPENFAIL;
		}
		for (dev = 1; dev < NPORT; dev++)	/* Dev zero is the master entry */
			if (port_chan[dev].qptr == NULL && port_chan[dev].status == P_free)
				goto gotone;
		U_ERROR = ENOENT;
		if (portm_debug & 0x10)
			printf ("portm_open: no free device\n");
		return OPENFAIL;
	} else {
		/*
		* If we open a normal device, somebody has to be talking on it already
		* or else there's a possible security violation.
		* 
		* On the other hand, the master driver can't be opened more than once.
		*/
		if ((dev == 0) && (port_chan[dev].qptr != NULL)) {
			U_ERROR = EBUSY;
			return OPENFAIL;
		} else if ((dev != 0) && (port_chan[dev].qptr == NULL)) {
			U_ERROR = ENXIO;
			return OPENFAIL;
		}
	}

  gotone:
	ch = &port_chan[dev];

	if (ch->qptr == NULL) {
		bzero (ch, sizeof (struct _portm_chan));

		ch->tty.c_iflag = IGNBRK | IGNPAR;
		ch->tty.c_cflag = B38400 | CS8 | CREAD | HUPCL;

		ch->qptr = q;
		/* ch->oflag = flag; */
		ch->flags = 0;
		if (dev == 0)			  /* /dev/portman */
			ch->status = P_master;
		else if (sflag == MODOPEN)/* I_PUSH("port") */
			ch->status = P_module;
		else					  /* /dev/port */
			ch->status = P_driver;
		ch->linkto = 0;
		ch->link = NULL;
		ch->passcount = 1; /* Block (for now) */
	
		if (dev != 0 && port_chan->qptr != NULL) {
			mblk_t *mp;
#ifdef CONFIG_DEBUG_STREAMS
			struct fmodsw *fm;
			streamchar strl[] = "strlog";
			int i;

			if(portm_debug & 0x100) {
				for (fm = fmod_sw; fm < &fmod_sw[fmodcnt]; fm++) {
					if (fm->f_str == NULL)
						continue;
					for (i = 0; i < FMNAMESZ; i++) {
						if ((strl[i] == '\0') || (strl[i] != (streamchar)fm->f_name[i]))
							break;
					}
					if (strl[i] == '\0' && fm->f_name[i] == '\0')
						break;
				}
				if(fm < &fmod_sw[fmodcnt] && !c_qattach (fm->f_str, q, 0)) 
					printf (" -- can't attach %s\n", fm->f_name);
			}
#endif
			mp = allocb (32, BPRI_HI);
			if (mp != NULL) {
				m_putid (mp, PORT_OPEN);
				m_putsx (mp, portsig (ch->status));
				m_puti (mp, dev);
				putnext (port_chan->qptr, mp);
			}
		}
	}
	WR (q)->q_ptr = (caddr_t) ch;
	q->q_ptr = (caddr_t) ch;

	if (portm_debug & 0x10)
		printf ("PORT %d open (%x)\n", dev, q);
	MORE_USE;
	return dev;
}


/* Streams code to close the driver. */
static void
portm_close (queue_t *q, int dummy)
{
	portm_chan ch = (portm_chan) q->q_ptr;
	int ms = splstr ();

	if (portm_debug & 0x10)
		printf ("PORT %d: Closed.\n", ch - port_chan);
	switch (ch->status) {
	case P_driver:
	case P_module:
		if (ch->linkto != 0) {
			port_chan[ch->linkto].linkto = 0;
			port_chan[ch->linkto].link = NULL;
			port_chan[ch->linkto].passcount = 1;
			ch->linkto = 0;
			ch->link = NULL;
		}
		if (port_chan->qptr != NULL) {
			mblk_t *mp = allocb (32, BPRI_HI);

			if (mp == NULL)
				break;
			m_putid (mp, PORT_CLOSED);
			m_putsx (mp, portsig (ch->status));
			m_puti (mp, ch - port_chan);
			putnext (port_chan->qptr, mp);
		}
		break;
	case P_master:{
			SUBDEV i;

			for (i = 1; i < NPORT; i++) {
				if (port_chan[i].status != P_free) {
					putctlx (port_chan[i].qptr, M_HANGUP);
					port_chan[i].linkto = 0;
					port_chan[i].link = NULL;
					port_chan[i].passcount = 0;
				}
			}
		} break;
	}

	ch->status = P_free;
	ch->qptr = NULL;

	splx (ms);
	LESS_USE;
	return;
}


mblk_t *
sendback (portm_chan ch, mblk_t * mp)
{
	mblk_t *mb;
	ushort_t id;
	int err;
	streamchar *origmb;

	mb = pullupm (mp, 2);
	if (mb == NULL)
		return mp;

	origmb = mb->b_rptr;
	if ((err = m_getid (mb, &id)) != 0) {
		freemsg (mb);
		return NULL;
	}
	if ((mp = allocb (32, BPRI_MED)) == 0) {
		mb->b_rptr = origmb;
		return mb;
	}
	m_putid (mp, id);
	if (ch->status == P_driver)
		m_putsx (mp, PORT_DRIVER);
	else
		m_putsx (mp, PORT_MODULE);
	m_puti (mp, ch - port_chan);
	if (mb->b_rptr < mb->b_wptr)
		linkb (mp, mb);
	else
		freemsg (mb);
	mb->b_datap->db_type = M_DATA;
	if (port_chan->qptr != NULL)
		putnext (port_chan->qptr, mp);
	else
		freemsg (mp);
	return NULL;
}

mblk_t *
sendlink (portm_chan ch, mblk_t * mp)
{
	mblk_t *mb;
	unsigned int slen;

	if (ch->passcount == 0) {
		putnext (ch->link, mp);
		return NULL;
	} else if (ch->passcount == PASS_ONEMSG) {
		putnext (ch->link, mp);
		ch->passcount = 1;
		return NULL;
	} else if (ch->passcount == 1) {
		return mp;
	} else if ((slen = dsize (mp)) < ch->passcount) {
		ch->passcount -= slen;
		putnext (ch->link, mp);
		return NULL;
	} else {					  /* Split! */
		int ml = ch->passcount - 1;
		mblk_t *mr = dupmsg (mp);
		mblk_t *mz = mp;

		if (mr == NULL)
			return mp;
		ch->passcount = 1;
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
		putnext (ch->link, mp);
		return mr;
	}
}

mblk_t *
sendbackdata (portm_chan ch, mblk_t * mb)
{
	mblk_t *mp;
	mblk_t *mbret = NULL;
	unsigned int slen;

	if ((mp = allocb (32, BPRI_MED)) == 0) {
		return mb;
	}
	if (ch->passcount == 0) ;
	else if (ch->passcount == PASS_ONEMSG) 
		ch->passcount = 1;
	else if (ch->passcount == 1) {
		freeb (mp);
		return mb;
	} else if ((slen = dsize (mb)) < ch->passcount)
		ch->passcount -= slen;
	else {						  /* Split */
		int ml = ch->passcount - 1;
		mblk_t *mr = dupmsg (mb);
		mblk_t *mz = mb;

		if (mr == NULL) {
			freeb (mp);
			return mb;
		}
		ch->passcount = 1;
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
		mbret = mr;
	}
	m_putid (mp, PORT_DATA);
	if (ch->status == P_driver)
		m_putsx (mp, PORT_DRIVER);
	else
		m_putsx (mp, PORT_MODULE);
	m_puti (mp, ch - port_chan);
	m_putdelim (mp);
	linkb (mp, mb);
	if (port_chan->qptr != NULL)
		putnext (port_chan->qptr, mp);
	else
		freemsg (mp);
	return mbret;
}

/* Streams code to write data. */
static void
portm_wput (queue_t * q, mblk_t * mp)
{
	register portm_chan ch = (portm_chan) q->q_ptr;

#ifdef CONFIG_DEBUG_STREAMS
	if (mp == NULL) {
		long x;
		long *xx = &x;

		printf ("StackDump\n");
		for (x = 0; x < 0x80; x++) {
			printf (" %x", *++xx);
		}
	}
#endif
	if (portm_debug & 0400)
		printf ("portm_wput %x %x\n", q, mp);
	switch (mp->b_datap->db_type) {
	case M_IOCTL:
	CASE_DATA
	case MSG_PROTO:
		putq (q, mp);
		break;
	case M_FLUSH:
		if (*mp->b_rptr & FLUSHW)
			flushq (q, 0);
		if (ch->status == P_module) {
			if (ch->linkto == 0)
				putnext (q, mp);
			else
				freemsg (mp);
		} else if (ch->linkto != 0)
			putnext (ch->link, mp);
		else if (*mp->b_rptr & FLUSHR) {
			flushq (RD (q), 0);
			*mp->b_rptr &= ~FLUSHW;
			qreply (q, mp);
		} else
			freemsg (mp);
		break;
	default:{
			if (ch->status == P_module)
				putnext (q, mp);
			else {
				log_printmsg (NULL, "Strange PORT", mp, KERN_WARNING);
				/* putctl1(RD(q)->b_next, M_ERROR, ENXIO); */
				freemsg (mp);
			}
		} break;
	}
	return;
}


/* Streams code to scan the write queue. */
static void
portm_wsrv (queue_t * q)
{
	mblk_t *mp, *mb;
	register portm_chan ch = (portm_chan) q->q_ptr;
	ushort_t id;
	int err = 0;
	streamchar *origmb;


	while ((mp = getq (q)) != NULL) {
		if (portm_debug & 0400)
			printf ("wsrv %x.%x.%x\n", mp, mp->b_datap, mp->b_cont);
		mb = NULL;
		switch (ch->status) {
		case P_master:{
				mb = pullupm (mp, -1);
				if (mb == NULL) {
					putbqf (q, mp);
					return;
				}
				origmb = mb->b_rptr;
				switch (mb->b_datap->db_type) {
				default:
					freemsg (mb);
					break;
				case M_IOCTL:
					mb->b_datap->db_type = M_IOCNAK;
					qreply(q,mp);
					break;
				CASE_DATA {
						long nm = 0;

						if ((err = m_getid (mb, &id)) != 0)
							goto err_out;
						switch (id) {
						case PORT_LINK:{
								long xmod = 0;
								long xdrv = 0;
								long xerr = 0;

								while ((err = m_getsx (mb, &id)) == 0) {
									switch (id) {
									case PORT_MODDRV:
									case PORT_MODULE:
										if (xmod != 0) {
											printf ("Mod2 ");
											err = EINVAL;
											goto err_out;
										}
										if ((err = m_geti (mb, &xmod)) != 0) {
											printf ("ModErr %d ", err);
											goto err_out;
										}
										if (xmod <= 0 || xmod >= NPORT) {
											printf (" XMod %d ", xmod);
											err = ENXIO;
											goto err_out;
										}
										break;
									case PORT_DRIVER:
										if (xdrv != 0) {
											printf ("Drv2");
											err = EINVAL;
											goto err_out;
										}
										if ((err = m_geti (mb, &xdrv)) != 0) {
											printf ("DrvErr %d ", err);
											goto err_out;
										}
										if (xdrv <= 0 || xdrv >= NPORT) {
											printf (" XDrv %d ", xdrv);
											err = ENXIO;
											goto err_out;
										}
										break;
									case PROTO_ERROR:
										if (xerr != 0) {
											printf ("Err2 ");
											err = EINVAL;
											goto err_out;
										}
										if ((err = m_geti (mb, &xerr)) != 0) {
											printf ("Err Err %d ", err);
											goto err_out;
										}
										if (xerr == 0)
											xerr = -1;
										else if (xerr < -1) {
											printf ("XErr %d ", xerr);
											err = ENXIO;
											goto err_out;
										}
										break;
									}
								}
								if (xmod != 0 && port_chan[xmod].qptr != NULL) {
									if (port_chan[xmod].status != P_module && port_chan[xmod].status != P_driver) {
										err = EIO;
										goto err_out;
									}
									if (port_chan[xmod].linkto != 0) {
										qenable (port_chan[xmod].link);
										qenable (port_chan[port_chan[xmod].linkto].link);
										port_chan[port_chan[xmod].linkto].linkto = 0;
										port_chan[port_chan[xmod].linkto].link = NULL;
										port_chan[port_chan[xmod].linkto].passcount = 1;
										port_chan[xmod].linkto = 0;
										port_chan[xmod].link = NULL;
										port_chan[xmod].passcount = 1;
									}
									if (xerr != 0) {
										if (xerr == -1)
											putctlx (port_chan[xmod].qptr, M_HANGUP);
										else if (xerr > 0)
											putctlx1 (port_chan[xmod].qptr, M_ERROR, err);
									}
								}
								if (xdrv != 0 && port_chan[xdrv].qptr != NULL) {
									if (port_chan[xdrv].status != P_driver) {
										printf ("BadChan ");
										err = EIO;
										goto err_out;
									}
									if (port_chan[xdrv].linkto != 0) {
										qenable (port_chan[xdrv].link);
										qenable (port_chan[port_chan[xdrv].linkto].link);
										port_chan[port_chan[xdrv].linkto].linkto = 0;
										port_chan[port_chan[xdrv].linkto].link = NULL;
										port_chan[port_chan[xdrv].linkto].passcount = 1;
										port_chan[xdrv].linkto = 0;
										port_chan[xdrv].link = NULL;
										port_chan[xdrv].passcount = 1;
									}
									if (xerr != 0) {
										if (xerr == -1)
											putctlx (port_chan[xdrv].qptr, M_HANGUP);
										else if (xerr > 0)
											putctlx1 (port_chan[xdrv].qptr, M_ERROR, err);
									}
								}
								if (xmod == xdrv) {		/* Includes both being
														 * zero */
									printf ("NoDdv+Mod ");
									err = ENXIO;
									goto err_out;
								}
								if (xdrv != 0 && port_chan[xdrv].qptr == NULL) {
									printf ("Drv Closed ");
									err = ENXIO;
									goto err_out;
								}
								if (xmod != 0 && port_chan[xmod].qptr == NULL) {
									printf ("Mod Closed ");
									err = ENXIO;
									goto err_out;
								}
								if (xmod != 0 && xdrv != 0 && xerr == 0) {
									port_chan[xmod].linkto = xdrv;
									port_chan[xmod].link = port_chan[xdrv].qptr;
									port_chan[xdrv].linkto = xmod;
									if (port_chan[xmod].status == P_driver)
										port_chan[xdrv].link = port_chan[xmod].qptr;
									else
										port_chan[xdrv].link = WR (port_chan[xmod].qptr);
									qenable (port_chan[xmod].link);
									qenable (port_chan[xdrv].link);
								}
								mb->b_rptr = origmb;
								md_reply (q, mb, 0);
							} break;
						case PORT_DATA:
						case PORT_SETUP:
						case PORT_IOCTL:
						default:{
								ushort_t theID = id;
								long size = -1;
								long baud = -1;
								long flag = -1;
								long xerr = 0;

								while ((err = m_getsx (mb, &id)) == 0) {
									switch (id) {
									case PORT_PASS:
										if (size != -1) {
											printf ("Size2 ");
											err = EINVAL;
											goto err_out;
										}
										if ((err = m_geti (mb, &size)) != 0) {
											printf ("SizErr %d ", err);
											goto err_out;
										}
										break;
									case PORT_BAUD:
										if (baud != -1) {
											printf ("Baud2 ");
											err = EINVAL;
											goto err_out;
										}
										if ((err = m_geti (mb, &baud)) != 0) {
											printf ("Baud Err %d ", err);
											goto err_out;
										}
										if (baud <= 0 || baud > B38400) {
											printf ("Baud %d ", baud);
											err = EINVAL;
											goto err_out;
										}
										break;
									case PORT_FLAGS:
										if (flag != -1) {
											printf ("Flag2 ");
											err = EINVAL;
											goto err_out;
										}
										if ((err = m_geti (mb, &flag)) != 0) {
											printf ("Flag Err %d ", err);
											goto err_out;
										}
										if (flag <= 0 || flag > 0xFF) {
											printf ("Flag %d ", flag);
											err = EINVAL;
											goto err_out;
										}
										break;
									case PROTO_ERROR:
										if (xerr != 0) {
											printf ("Err2 ");
											err = EINVAL;
											goto err_out;
										}
										if ((err = m_geti (mb, &xerr)) != 0) {
											printf ("Err Err %d ", err);
											goto err_out;
										}
										if (xerr == 0)
											xerr = -1;
										else if (xerr < -1) {
											printf ("XErr %d ", xerr);
											err = ENXIO;
											goto err_out;
										}
										break;
									case PORT_MODDRV:
									case PORT_MODULE:
									case PORT_DRIVER:
										if (nm != 0) {
											printf ("Mod2 ");
											err = EINVAL;
											goto err_out;
										}
										if ((err = m_geti (mb, &nm)) != 0) {
											printf ("Mod Err %d ", err);
											goto err_out;
										}
										if (nm <= 0 || nm >= NPORT) {
											printf ("NM %d ", nm);
											err = ENXIO;
											goto err_out;
										}
										if (id == PORT_MODULE && port_chan[nm].status != P_module) {
											printf ("ModDrv ");
											err = EINVAL;
											goto err_out;
										}
										if (id == PORT_DRIVER && port_chan[nm].status != P_driver) {
											printf ("DrvMod ");
											err = EINVAL;
											goto err_out;
										}
									}
								}
								if (nm == 0) {
									printf ("NoNM ");
									err = ENXIO;
									goto err_out;
								}
								switch (theID) {
								default:
									mb->b_datap->db_type = MSG_PROTO;
									mb->b_rptr = origmb;
									break;
								case PORT_SETUP:
								case PORT_DATA:
								case PORT_IOCTL:
									mp = dupb (mb);
									if (mp == NULL) {
										err = ENOMEM;
										goto err_out;
									}
									if (size != -1) {
										port_chan[nm].passcount = size;
										qenable (port_chan[nm].qptr);
										qenable (WR(port_chan[nm].qptr));
									}
									if (baud != 0)
										port_chan[nm].baud = baud;
									if (flag != 0)
										port_chan[nm].flags = flag;
									mp = pullupm(mp,1);
									break;
								}
								if (theID == PORT_SETUP) {
									freemsg (mp);
								} else if(theID == PORT_IOCTL) {
									struct iocblk *iocb;
									if(mp->b_wptr-mb->b_rptr < sizeof(*iocb)) {
										printf("iocsize only %d!\n",mb->b_wptr-mb->b_rptr);
										freemsg(mp);
										err = EINVAL;
										goto err_out;
									} else if(mp->b_wptr-mp->b_rptr > sizeof(*iocb)) {
										mblk_t *mx = allocb(sizeof(*iocb),BPRI_MED);
										if(mx == NULL) {
											freemsg(mp);
											err = ENOMEM;
											goto err_out;
										}
										*((struct iocblk *)mx->b_wptr)++ = *((struct iocblk *)mp->b_rptr)++;
										linkb(mx,mp);
										mp = mx;
									} 
									iocb = (struct iocblk *)mp->b_rptr;
									if(xerr != 0) {
										mp->b_datap->db_type = M_IOCACK;
									} else {
										mp->b_datap->db_type = M_IOCNAK;
										if(xerr > 0)
											iocb->ioc_error = xerr;
									}
									putnext (port_chan[nm].qptr, mp);
								} else if (port_chan[nm].status == P_driver || theID == PORT_DATA)
									putnext (port_chan[nm].qptr, mp);
								else
									putnext (WR (port_chan[nm].qptr), mp);
								mp->b_wptr = mp->b_rptr; /* kill data; not interesting */
								goto err_out;
							} break;	/* default */
						}		  /* switch id */
					} break;	  /* case DATA */
				}				  /* switch type */
			} break;			  /* case P_master */
		case P_driver:
			switch (mp->b_datap->db_type) {
			default:
				freemsg (mp);
				break;
			case M_IOCTL:
				mb = allocb(20,BPRI_MED);
				if(mp != NULL) {
					m_putid(mp,PORT_IOCTL);
					m_putsx (mp, portsig (ch->status));
					m_puti (mp, ch - port_chan);
					if(port_chan->qptr != NULL) {
						putnext (port_chan->qptr, mb);
						break;
					}
					freeb(mb);
				}
				mp->b_datap->db_type = M_IOCNAK;
				qreply(q,mp);
				break;
			CASE_DATA
				if(dsize(mp) == 0) {
					freemsg(mp);
					mp = allocb(20,BPRI_MED);
					if(mp != NULL) {
						m_putid(mp,PORT_WANTCLOSED);
						m_putsx (mp, portsig (ch->status));
						m_puti (mp, ch - port_chan);
						if(port_chan->qptr != NULL && canput(port_chan->qptr->q_next))
							putnext (port_chan->qptr, mp);
						else
							freemsg(mp);
					}
					continue;
				}
				if (ch->linkto != 0) {
					if (mp->b_datap->db_type >= QPCTL || canput (ch->link->q_next))
						putnext (ch->link, mp);
					else {
						putbqf (q, mp);
						return;
					}
					break;
				}
				if ((mp = sendbackdata (ch, mp)) != NULL) {
					putbqf (q, mp);
					return;
				}
				break;
			case MSG_PROTO:
				if ((mp = sendback (ch, mp)) != NULL) {
					putbqf (q, mp);
					return;
				}
				break;
			}
			break;
		case P_module:
			switch (mp->b_datap->db_type) {
			case M_IOCTL:
				mb = allocb(20,BPRI_MED);
				if(mp != NULL) {
					m_putid(mp,PORT_IOCTL);
					m_putsx (mp, portsig (ch->status));
					m_puti (mp, ch - port_chan);
					if(port_chan->qptr != NULL) {
						putnext (port_chan->qptr, mb);
						break;
					}
					freeb(mb);
				}
				mp->b_datap->db_type = M_IOCNAK;
				qreply(q,mp);
				break;
			case MSG_PROTO:
				if ((mp = sendback (ch, mp)) != NULL) {
					putbqf (q, mp);
					return;
				}
				break;
			CASE_DATA
				if (ch->flags & PORT_F_PASS) {
					if (ch->linkto != 0) {
						if (mp->b_datap->db_type >= QPCTL || canput (ch->link->q_next))
							putnext (ch->link, mp);
						else {
							putbqf (q, mp);
							return;
						}
						break;
					}
					if (mp->b_datap->db_type >= QPCTL || canput (q->q_next))
						putnext (q, mp);
					else {
						putbqf (q, mp);
						return;
					}
				} else if ((mp = sendbackdata (ch, mp)) != NULL) {
					putbqf (q, mp);
					timeout(qenable,q,HZ/5);
					return;
				}
				break;
			default:
				if (mp->b_datap->db_type >= QPCTL || canput (q->q_next))
					putnext (q, mp);
				else {
					putbqf (q, mp);
					return;
				}
			}					  /* switch type */
			break;
		default:
			freemsg (mp);
			break;
		}						  /* switch status */
		continue;
	  err_out:
		if (mb != NULL)
			mb->b_rptr = origmb;
		if (port_chan->qptr == NULL) {
			if (mb != NULL)
				freemsg (mb);
			else
				freemsg (mp);
			continue;
		}
		if(ch->status == P_master)
			md_reply (q, mb ? mb : mp, err);
		else
			m_reply (q, mb ? mb : mp, err);
	}							  /* while */
#if 1
	if (ch->status == P_module && ch->linkto != 0)
		qenable (WR(ch->link));
#endif
	return;
}



/* Streams code to read data. */
static void
portm_rput (queue_t * q, mblk_t * mp)
{
	portm_chan ch = (portm_chan) q->q_ptr;

#ifdef CONFIG_DEBUG_STREAMS
	if (mp == NULL) {
		long x;
		long *xx = &x;

		printf ("StackDump\n");
		for (x = 0; x < 0x80; x++) {
			printf (" %x", *++xx);
		}
	}
#endif
	if (portm_debug & 0400)
		printf ("portm_rput %x %x\n", q, mp);
	switch (mp->b_datap->db_type) {
	CASE_DATA
	case MSG_PROTO:
		putq (q, mp);
		break;
	case M_FLUSH:
		if (*mp->b_rptr & FLUSHR)
			flushq (q, 0);
		if (ch->status == P_driver || ch->linkto == 0)
			putnext (q, mp);
		else {
			putnext (ch->link, mp);
		}
		break;
	default:{
			if (ch->status == P_module)
				putnext (q, mp);
			else {
				log_printmsg (NULL, "Strange PORT", mp, KERN_WARNING);
				/* putctl1(RD(q)->b_next, M_ERROR, ENXIO); */
				freemsg (mp);
			}
		} break;
	}
	return;
}


/* Streams code to scan the read queue. */
static void
portm_rsrv (queue_t * q)
{
	mblk_t *mb;
	register portm_chan ch = (portm_chan) q->q_ptr;
	ushort_t id;
	int err = 0;

	while ((mb = getq (q)) != NULL) {
		streamchar *origmb = mb->b_rptr;

		if (portm_debug & 0400)
			printf ("rsrv %x.%x.%x\n", mb, mb->b_datap, mb->b_cont);
		switch (ch->status) {
		case P_master:
			switch (mb->b_datap->db_type) {
			default:
				freemsg (mb);
				break;
			case MSG_PROTO:
			CASE_DATA
				mb->b_datap->db_type = M_DATA;
				if (canput (q->q_next))
					putnext (q, mb);
				else {
					putbqf (q, mb);
					return;
				}
				break;
			}
			break;
		case P_driver:
			switch (mb->b_datap->db_type) {
			default:
				freemsg (mb);
				break;
			CASE_DATA
			case MSG_PROTO:
				if (canput (q->q_next))
					putnext (q, mb);
				else {
					putbqf (q, mb);
					return;
				}
			}
			break;
		case P_module:
			if (mb->b_datap->db_type == M_DATA && ch->linkto != 0) {
				if ((mb->b_datap->db_type < QPCTL && !canput (ch->link->q_next)) || (mb = sendlink (ch, mb)) != NULL) {
					putbqf (q, mb);
					return;
				}
				break;
			}
			switch (mb->b_datap->db_type) {
			default:
				if (mb->b_datap->db_type >= QPCTL || canput (q->q_next))
					putnext (q, mb);
				else {
					putbqf (q, mb);
					return;
				}
				continue;
			}
			break;
		default:
			freemsg (mb);
			break;
		}
	}
#if 1
	if (ch->status == P_driver && ch->linkto != 0)  
		qenable (OTHERQ(ch->link));
#endif
	return;
}




#ifdef MODULE
static int devmajor = 0;

static int do_init_module(void)
{
	int err;
	err = register_strmod(&portminfo);
	if(err) return err;
	err = register_strdev(0,&portminfo,0);
	if(err) {
		unregister_strmod(&portminfo);
		return err;
	}
	devmajor = err;
	return 0;
}

static int do_exit_module(void)
{
	int err1 = unregister_strmod(&portminfo);
	int err2 = unregister_strdev(devmajor,&portminfo,0);
	return err1 || err2;
}
#endif
