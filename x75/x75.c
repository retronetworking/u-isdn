/**
 ** Streams X75 module
 **/

/*
 * Just a Streams wrapper for the X75 library, with special care taken to
 * handle ignorant people who, using the Siemens HCRX chip in auto mode, assign
 * the same values to command and response addresses...
 */

#include "f_module.h"
#include "primitives.h"
#include "kernel.h"
#include "f_signal.h"
#include "f_malloc.h"
#include "streams.h"
#ifdef DONT_ADDERROR
#include "f_user.h"
#endif
#include "streamlib.h"
#include "x75lib.h"
#include "x75.h"
#include "isdn_proto.h"

extern void log_printmsg (void *log, const char *, mblk_t *, const char*);

/*
 * Standard Streams stuff.
 */
static struct module_info x75_minfo =
{
		0, "x75", 0, INFPSZ, 800,100
};

static qf_open x75_open;
static qf_close x75_close;
static qf_put x75_rput,x75_wput;
static qf_srv x75_rsrv,x75_wsrv;

static struct qinit x75_rinit =
{
		x75_rput, x75_rsrv, x75_open, x75_close, NULL, &x75_minfo, NULL
};

static struct qinit x75_winit =
{
		x75_wput, x75_wsrv, NULL, NULL, NULL, &x75_minfo, NULL
};

struct streamtab x75info =
{&x75_rinit, &x75_winit, NULL, NULL};

struct x75_ {
	struct _x75 x75;
	char flags;
#define X75_INUSE 01
#define X75_CONN 02
#define X75_ADRWARN 04
#define X75_DISC 010
#define X75_CONNECT 020			  /* channel connected? */
	char connmode;
	queue_t *q;
	int nr;
	ushort_t concat;
	ushort_t offset;
	ushort_t radr_cmd, radr_meld, xadr_cmd, xadr_meld;
	unsigned swapped:1;				  /* Marker that we're receiving. */
	unsigned wide:1;					  /* addresses */
	unsigned didSend:1;
	unsigned didSendNew:1;			  /* Remember whether we were sending anything
								   * recently. If so, the incoming packet is
								   * assumed to be a response. This is
								   * necessary when command and response have
								   * the same address assigned to them. (See
								   * above.) */
};

/*
 * X75 changed state. Probably send an MSG_PROTO down.
 */

static int
x75__state (struct x75_ *x_75, uchar_t ind, ushort_t add)
{
	printf ("%sx75__state %d: Ind %d/%o\n",KERN_DEBUG , x_75 ->nr, ind, add);
	switch (ind) {
	case DL_ESTABLISH_IND:
	case DL_ESTABLISH_CONF:
		{
			mblk_t *mb;

			if (!(x_75->flags & X75_CONN)) {
				x_75->flags |= X75_CONN;

				if ((mb = allocb (3, BPRI_HI)) != NULL) {
					*((ushort_t *) mb->b_wptr)++ = PROTO_CONNECTED;
					DATA_TYPE(mb) = MSG_PROTO;
					putnext (x_75->q, mb);
				}
			}
		}
		break;
	case DL_RELEASE_IND:
	case DL_RELEASE_CONF:
		{
			mblk_t *mb;

			x_75->flags &= ~X75_CONN;
			if (!(x_75->flags & X75_DISC)) {
				x_75->flags |= X75_DISC;

				if ((mb = allocb (3, BPRI_HI)) != NULL) {
					*((ushort_t *) mb->b_wptr)++ = PROTO_DISCONNECT;
					DATA_TYPE(mb) = MSG_PROTO;
					putnext (WR (x_75->q), mb);
				}
			}
		}
		break;
	}
	return 0;
}

/*
 * Check if we can send data downstream.
 */
static int
x75__cansend (struct x75_ *x_75)
{
	return (x_75->q != NULL && canput (WR (x_75->q)->q_next));
}

/*
 * Check if we can send data upstream. I.e., set/clear RNR.
 */
static int
x75__canrecv (struct x75_ *x_75)
{
	return (x_75->q != NULL && canput (x_75->q->q_next));
}

/*
 * Flush downstream.
 */
static int
x75__flush (struct x75_ *x_75)
{
	return (x_75->q != NULL && putctlx1 (WR (x_75->q), M_FLUSH, FLUSHW));
}

/*
 * Send data block downstream.
 */
static int
x75__send (struct x75_ *x_75, char iscmd, mblk_t * mb2)
{
	mblk_t *mb;

	if (x_75->q == NULL) {
		printf("%sX75_send EIO\n",KERN_DEBUG );
		return -EIO;
	}
	if(x_75->wide) {
		if (/* XXX */ 0 || DATA_START(mb2)+2 > mb2->b_rptr
			    || DATA_REFS(mb2) > 2 ) { /* Compromise... */
			mb = allocb (x_75->offset + 2, BPRI_HI);
			if (mb == NULL)
				return -ENOMEM;
			mb->b_rptr += x_75->offset + 2;
			mb->b_wptr += x_75->offset + 2;
			linkb (mb, mb2);
		} else 
			mb = mb2;
		*--mb->b_rptr = (iscmd ? x_75->xadr_cmd : x_75->xadr_meld) & 0xFF;
		*--mb->b_rptr = (iscmd ? x_75->xadr_cmd : x_75->xadr_meld) >> 8;
	} else {
		if (/* XXX */ 0 || DATA_START(mb2)+1 > mb2->b_rptr
			    || DATA_REFS(mb2) > 2 ) { /* Compromise... */
			mb = allocb (x_75->offset + 1, BPRI_HI);
			if (mb == NULL)
				return -ENOMEM;
			mb->b_rptr += x_75->offset + 1;
			mb->b_wptr += x_75->offset + 1;
			linkb (mb, mb2);
		} else
			mb = mb2;
		*--mb->b_rptr = (iscmd ? x_75->xadr_cmd : x_75->xadr_meld);
	}
	putnext (WR (x_75->q), mb);
	x_75->didSendNew = 1;
	return 0;
}

/*
 * Send data block upstream.
 */
static int
x75__recv (struct x75_ *x_75, char isUI, mblk_t * mp)
{
	if (x_75->q == NULL)
		freemsg (mp);
	else if ((isUI != 0) != ((x_75->connmode & X75CONN_UI) != 0)) {
		if (!(x_75->flags & X75_ADRWARN)) {
			x_75->flags |= X75_ADRWARN;
			printf ("%sX75: Got %s frame, but want %s\n",KERN_WARNING,
					isUI ? "UI" : "I", isUI ? "I" : "UI");
		}
		freemsg (mp);
	} else
		putnext (x_75->q, mp);
	return 0;
}

/* Reenable blocked Streams queue when xmit buffer empties. */
static int
x75__backenable (struct x75_ *x_75)
{
	if (x_75->q != NULL) {
		WR(x_75->q)->q_flag |= QWANTR;
		qenable (x_75->q);
		qenable (WR (x_75->q));
	}
	return 0;
}

/*
 * Initialize with reasonable default values.
 */
static int
x75__init (struct x75_ *x_75)
{
	x_75->xadr_cmd = x_75->radr_meld = 1;
	x_75->radr_cmd = x_75->xadr_meld = 3;
	x_75->connmode = X75CONN_OUT;
	x_75->swapped = 0;
	x_75->concat = 0;
	x_75->offset = 0;
	x_75->x75.offset = 1;

	bzero (&x_75->x75, sizeof (struct _x75));

	x_75->x75.cansend = (P_candata) & x75__cansend;
	x_75->x75.canrecv = (P_candata) & x75__canrecv;
	x_75->x75.send = (P_data) & x75__send;
	x_75->x75.recv = (P_data) & x75__recv;
	x_75->x75.state = (P_state) & x75__state;
	x_75->x75.flush = (P_flush) & x75__flush;
	x_75->x75.backenable = (P_backenable) & x75__backenable;
	x_75->x75.ref = x_75;
	x_75->x75.debugnr = (x_75->nr) + 100;
	x75_initconn (&x_75->x75);
	return 0;
}

/* Streams code to open the driver. */
static int
x75_open (queue_t * q, dev_t dev, int flag, int sflag ERR_DECL)
{
	struct x75_ *x_75;
	int nr = 1;

	if (q->q_ptr) {
		return 0;
	}
	x_75 = malloc(sizeof(*x_75));
	if(x_75 == NULL)
		ERR_RETURN(-ENOMEM);
	memset(x_75,0,sizeof(*x_75));
	WR (q)->q_ptr = (char *) x_75;
	q->q_ptr = (char *) x_75;

	x_75->q = q;
	x_75->flags = X75_INUSE;
	x_75->didSend = x_75->didSendNew = 0;
	x_75->nr = nr++;
	x75__init (x_75);

	MORE_USE;
	return 0;
}

/* Streams code to close the driver. */
static void
x75_close (queue_t * q, int dummy)
{
	register struct x75_ *x_75;

	x_75 = (struct x75_ *) q->q_ptr;

	x75_changestate (&x_75->x75, PH_DEACTIVATE_IND, 1);
	flushq (q, FLUSHALL);
	flushq (WR (q), FLUSHALL);
	if (0)
		printf ("X75 driver closed.\n");
	free(x_75);
	LESS_USE;
	return;
}


static void
x75_proto (queue_t * q, mblk_t * mp, char down)
{
	register struct x75_ *x_75 = (struct x75_ *) q->q_ptr;
	char dont = 0;
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
				x_75->offset = z;
				z += 2;

				x_75->x75.offset = z;
				z += 2;

				freemsg(mp);
				if((mp = allocb(10,BPRI_MED)) != NULL) {
					m_putid(mp,PROTO_OFFSET);
					m_puti(mp,z);
					DATA_TYPE(mp) = M_EXPROTO;
					origmp = NULL;
				}
			}
		} break;
	case PROTO_CONNECTED:
		x_75->flags &= ~X75_DISC;
		if (!down)
			x_75->flags |= X75_CONNECT;
		if (x_75->flags & X75_CONN)
			break;				  /* Already connected */
		else if (down)
			x_75->flags |= X75_CONN;
		else					  /* Upstream message.. */
			dont = 1;

		/*
		 * Now figure out what to do.
		 */
		if (x_75->connmode & X75CONN_UI) {
			dont = 0;
		} else if ((x_75->connmode & X75CONN_OUT) && !x_75->swapped) {
			x75_changestate (&x_75->x75, DL_ESTABLISH_REQ, 0);
		} else if ((x_75->connmode & X75CONN_IN) && x_75->swapped) {
			x75_changestate (&x_75->x75, DL_ESTABLISH_REQ, 0);
		} else if (x_75->connmode == 0) {
			x75_changestate (&x_75->x75, DL_ESTABLISH_CONF, 0);
		} else {
			;
		}
		break;
	case PROTO_LISTEN:
		break;
	case PROTO_INTERRUPT:
	case PROTO_DISCONNECT:
		x_75->flags &= ~X75_CONN;
		if (!down)
			x_75->flags &= ~X75_CONNECT;
		if (x_75->connmode & X75CONN_UI)
			dont = 0;
		else if (down && !(x_75->flags & X75_DISC)) {
			x75_changestate (&x_75->x75, DL_RELEASE_REQ, 0);
			dont = 1;
		} else {
			x_75->flags |= X75_DISC;
			x75_changestate (&x_75->x75, DL_RELEASE_IND, 1);
		}
		break;
	case PROTO_INCOMING:
		if (x_75->flags & X75_CONN)
			break;
		if (!x_75->swapped) {
			uchar_t x;

			/* Swap send and receive addresses. */

			x_75->swapped = 1;
			x = x_75->xadr_cmd;
			x_75->xadr_cmd = x_75->radr_cmd;
			x_75->radr_cmd = x;
			x = x_75->xadr_meld;
			x_75->xadr_meld = x_75->radr_meld;
			x_75->radr_meld = x;

		}
		break;
	case PROTO_OUTGOING:
		if (x_75->flags & X75_CONN)
			break;
		if (x_75->swapped) {
			uchar_t x;

			/* Unswap send and receive addresses. */

			x_75->swapped = 0;
			x = x_75->xadr_cmd;
			x_75->xadr_cmd = x_75->radr_cmd;
			x_75->radr_cmd = x;
			x = x_75->xadr_meld;
			x_75->xadr_meld = x_75->radr_meld;
			x_75->radr_meld = x;
		}
		break;
#if 0
	case PROTO_CLOSED:
		x75_changestate (&x_75->x75, DL_RELEASE_REQ, 0);
		x_75->flags &= ~X75_CONN;
		break;
#endif
	case PROTO_MODULE:
		if (strnamecmp (q, mp)) { /* Config information for me. */
			ulong_t x, y;
			long z;

			if (mp == NULL) {
				putbqf (q, mp);
				return;
			}
			while (mp != NULL && m_getsx (mp, &id) == 0) {
				switch (id) {
				default:
					error = -EINVAL;
					goto err;
				case PROTO_MODULE:
					break;
				case X75_IGNORESABM:
					x_75->x75.ignoresabm = 1;
					break;
				case X75_NOIGNORESABM:
					x_75->x75.ignoresabm = 0;
					break;
				case X75_POLL:
					x_75->x75.poll = 1;
					break;
				case X75_NOPOLL:
					x_75->x75.poll = 0;
					break;
				case X75_WIDEADDR:
					if ((x_75->flags & X75_CONN) && (x_75->wide != 1))
						goto err;
					x_75->wide = 1;
					break;
				case X75_NOTWIDEADDR:
					if ((x_75->flags & X75_CONN) && (x_75->wide != 0))
						goto err;
					x_75->wide = 0;
					break;
				case X75_WIDE:
					if ((x_75->flags & X75_CONN) && (x_75->x75.wide != 1))
						goto err;
					x_75->x75.wide = 1;
					break;
				case X75_NOTWIDE:
					if ((x_75->flags & X75_CONN) && (x_75->x75.wide != 0))
						goto err;
					x_75->x75.wide = 0;
					break;
				case X75_K:
					if ((error = m_geti (mp, &z)) != 0)
						goto err;
					if (z < 1 || z >= (x_75->x75.wide ? 128 : 8))
						goto err;
					if ((x_75->flags & X75_CONN) && (x_75->x75.k != z))
						goto err;
					x_75->x75.k = z;
					break;
				case X75_N1:
					if ((error = m_geti (mp, &z)) != 0)
						goto err;
					if (z < 1 || z >= 100)
						goto err;
					x_75->x75.N1 = z;
					break;
				case X75_T1:
					if ((error = m_geti (mp, &z)) != 0)
						goto err;
					if (z < 1 || z >= 100)
						goto err;
					x_75->x75.RUN_T1 = z;
					break;
				case X75_T3:
					if ((error = m_geti (mp, &z)) != 0)
						goto err;
					if (z < 1 || z >= 1000)
						goto err;
					x_75->x75.RUN_T3 = z;
					break;
				case X75_CONCAT:
					if ((error = m_geti (mp, &z)) != 0)
						goto err;
					x_75->concat = z;
					break;
				case X75_DEBUG:
					if ((error = m_getx (mp, &x)) != 0)
						goto err;
					x_75->x75.debug = x;
					break;
				case X75_ADR:
					if ((error = m_getx (mp, &x)) != 0)
						goto err;
					if ((error = m_getx (mp, &y)) != 0)
						goto err;
					if ((x_75->flags & X75_CONN)) {
						if(x_75->swapped) {
							if(x_75->xadr_cmd != y || x_75->radr_meld != y) goto err;
							if(x_75->radr_cmd != x || x_75->xadr_meld != x) goto err;
						} else {
							if(x_75->xadr_cmd != x || x_75->radr_meld != x) goto err;
							if(x_75->radr_cmd != y || x_75->xadr_meld != y) goto err;
						}
						goto err;
					}
					if (x < 1 || x >= 256)
						goto err;
					if (y < 1 || y >= 256)
						goto err;
#if 0
					if (!(x & 0x01))
						goto err;
					if (!(y & 0x01))
						goto err;
#endif
					x_75->xadr_cmd = x_75->radr_meld = x;
					x_75->radr_cmd = x_75->xadr_meld = y;
					x_75->swapped = 0;
					break;
				case X75_CONNMODE:
					if ((error = m_getx (mp, &x)) != 0)
						goto err;
					if ( /* x < 0 || */ x > 15)
						goto err;
					if ((x_75->flags & X75_CONN) && (x_75->connmode != x))
						goto err;
					x_75->connmode = x;
				}
			}
			if(mp != NULL) {
				mp->b_rptr = origmp;
				m_reply(q,mp,0);
				mp = NULL;
			}
		}
	}
	if (mp != NULL) {
		if (dont)
			freemsg (mp);
		else {
			if(origmp != NULL)
				mp->b_rptr = origmp;
			putnext (q, mp);
		}
	}
	return;
  err:
	mp->b_rptr = origmp;
	m_reply (q, mp, error ? error : -EINVAL);
}


/* Streams code to write data. */
static void
x75_wput (queue_t * q, mblk_t * mp)
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
		putq (q, mp);
		break;
	}
	return;
}

/* Streams code to scan the write queue. */
static void
x75_wsrv (queue_t * q)
{
	register struct x75_ *x_75 = (struct x75_ *) q->q_ptr;
	mblk_t *mp;

	while ((mp = getq (q)) != NULL) {
		switch (DATA_TYPE(mp)) {
		case MSG_PROTO:
			x75_proto (q, mp, 1);
			break;
		case CASE_DATA:
			{
				mblk_t *mp2;
				int err;
				char isUI = ((x_75->connmode & X75CONN_UI) != 0);

				DATA_TYPE(mp) = MSG_DATA;

				/*
				 * Concatenate data blocks; useful if a line discipline
				 * insists on sening one line or even one char at a time.
				 */
				if(q->q_first != NULL && x_75->concat > 0) {
					int sz=dsize(mp);
					while(sz < x_75->concat && q->q_first != NULL && DATA_TYPE(q->q_first) == M_DATA && (sz+=dsize(q->q_first)) <= x_75->concat) {
						mp2 = getq(q);
						if(mp2 != NULL)
							linkb(mp,mp2);
					}
				}
				/*
				 * Not connecting? Forget it...
				 */
				if (!(x_75->flags & X75_CONNECT)) {
					putbqf (q, mp);
					return;
				}
				if (x_75->x75.status == S_down && ((x_75->connmode & (X75CONN_DATA)) == X75CONN_DATA))
					x75_changestate (&x_75->x75, DL_ESTABLISH_REQ, 0);
				if (!x75_cansend (&x_75->x75, isUI)) {
					putbqff (q, mp);/* assume backenable gets called */
					return;
				} else if ((err = x75_send (&x_75->x75, isUI, mp)) == 0)
					break;
				else {
					printf ("%sx75_send: Err %d\n",KERN_DEBUG , err);
					putctlerr (RD (q), err);
					freemsg (mp);
				}
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
	x75_check_pending (&x_75->x75,0);
	return;
}

/* Streams code to read data. */
static void
x75_rput (queue_t * q, mblk_t * mp)
{
	struct x75_ *x_75 = (struct x75_ *) q->q_ptr;

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

	case M_ERROR:
	case M_HANGUP:
		x75_changestate (&x_75->x75, PH_DEACTIVATE_IND, 1);
	default:
		putq (q, mp);
	}
	return;
}

/* Streams code to scan the read queue. */
static void
x75_rsrv (queue_t * q)
{
	mblk_t *mp;
	register struct x75_ *x_75 = (struct x75_ *) q->q_ptr;
	int err;
	int isCmd;
	ushort_t adr;

	x_75->didSend |= x_75->didSendNew;	/* if we had a timeout */
	x_75->didSendNew = 0;

	while ((mp = getq (q)) != NULL) {
		switch (DATA_TYPE(mp)) {
		case MSG_PROTO:
			x75_proto (q, mp, 0);
			break;
		case CASE_DATA:
			/* Pass data if no connection? */
			if (mp->b_rptr + 2 > mp->b_wptr || !(x_75->flags & X75_CONNECT)) {
				freemsg (mp);
				continue;
			}
			if(x_75->wide) {
				adr = (uchar_t) * mp->b_rptr++ << 8;
				adr |= (uchar_t) * mp->b_rptr++;
			} else {
				adr = (uchar_t) * mp->b_rptr++;
			}
			if (adr != x_75->radr_cmd && adr != x_75->radr_meld) {
				if (!(x_75->flags & X75_ADRWARN)) {
					x_75->flags |= X75_ADRWARN;
					printf ("%sX75: got address %x, expected %x or %x\n",KERN_WARNING , adr, x_75->radr_cmd, x_75->radr_meld);
				}
				if(0) log_printmsg (NULL, "Bad Addr", mp, KERN_INFO);
				freemsg (mp);
				continue;
			}
			isCmd = (adr == x_75->radr_cmd);
#if 1 /* an ugly hack, useful when send and recv addresses are identical */
			if (isCmd && (adr == x_75->radr_meld) && x_75->didSend
					&& (dsize (mp) == (x_75->x75.wide ? 2 : 1)))
				isCmd = 0;
#endif
			if ((err = x75_recv (&x_75->x75, isCmd, mp)) != 0) {
				printf ("%sx75_recv err %d\n",KERN_DEBUG , err);
				/* putbq(q,mp); return; */
				/* always freed, so forget it */
			}
			break;
		case M_HANGUP:
		case M_ERROR:
			x75_changestate (&x_75->x75, PH_DEACTIVATE_IND, 0);
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
	x_75->didSend = x_75->didSendNew;
	x_75->didSendNew = 0;
	x75_check_pending (&x_75->x75,0);
	return;
}





#ifdef MODULE
static int do_init_module(void)
{
	return register_strmod(&x75info);
}

static int do_exit_module(void)
{
	return unregister_strmod(&x75info);
}
#endif
