/* Streams protocol mangling module */

#include "f_module.h"
#include "primitives.h"
#include "f_malloc.h"
#include <sys/types.h>
#include <sys/time.h>
#include "f_signal.h"
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/stropts.h>
#include "f_user.h"
#include <sys/errno.h>
#include <f_termio.h>
#ifndef linux
#include <sys/tty.h>
#endif
#include "streams.h"
#include "smallq.h"
#include "streamlib.h"
#include "isdn_limits.h"
#include "proto.h"
#include "isdn_proto.h"

static struct module_info proto_minfo =
{
		0, PROTO_NAME, 0, INFPSZ, 5000, 2000
};

static qf_open proto_open;
static qf_close proto_close;
static qf_put proto_wput,proto_rput;
static qf_srv proto_wsrv,proto_rsrv;

static struct qinit proto_rinit =
{
		proto_rput, proto_rsrv, proto_open, proto_close, NULL, &proto_minfo, NULL
};

static struct qinit proto_winit =
{
		proto_wput, proto_wsrv, NULL, NULL, NULL, &proto_minfo, NULL
};

struct streamtab protoinfo =
{&proto_rinit, &proto_winit, NULL, NULL};

#define CMDLEN 128

enum Pmode {
	P_UNKNOWN, P_CMD, P_NONE, P_LISTEN, P_CONN
};

struct _proto {
	queue_t *qptr;
	mblk_t *keep;
	enum Pmode mode;
	struct _smallq write_delay;
	struct termios tty;
	short reppos;
	uchar_t sCR, sLF, sBS, sBR;	  /* CR,LF,Backspace,Break (^C) */
	char nr;
	unsigned int cCarrier:2;				  /* Disconnect generates hangup? */
	unsigned int cBreak:1;				  /* Break switches to command mode? */
	unsigned int cSig:1;					  /* Send signals up when conn/disc */
	unsigned int talker:1;
	unsigned int binmode:1;
	char cmdline[CMDLEN];
	char repline[CMDLEN + 20];
	streamchar prefix[PREFIX_MAX];
};

static void setmode(struct _proto *proto, enum Pmode mode)
{
	char *x;
	switch(mode) {
	default       : x = "???"    ; break;
	case P_UNKNOWN: x = "unknown"; break;
	case P_CMD    : x = "command"; break;
	case P_NONE   : x = "none"   ; break;
	case P_LISTEN : x = "listen" ; break;
	case P_CONN   : x = "connect"; break;
	}
if(proto->mode != mode) printf("Switch mode to %s, talk %d\n",x,proto->talker);
	proto->mode = mode;
}

static int
proto_open (queue_t * q, dev_t dev, int flag, int sflag ERR_DECL)
{
	struct _proto *proto;

	if (q->q_ptr) {
		if (0)
			printf ("Protocol: already open?\n");
		return 0;
	}
	proto = malloc(sizeof(*proto));
	if(proto == NULL)
		ERR_RETURN(-ENOMEM);
	bzero ((caddr_t) proto, sizeof (struct _proto));

	proto->tty.c_iflag = IGNBRK | IGNPAR;
	proto->tty.c_cflag = B38400 | CS8 | CREAD | HUPCL;
	proto->sCR = 0x0D;
	proto->sLF = 0x0A;
	proto->sBS = 0x08;
	proto->sBR = 0x03;
	proto->cCarrier = 2;
	proto->cBreak = 1;
	proto->cSig = 0;
	proto->talker = 0;
	proto->mode = P_UNKNOWN;
	proto->reppos = 0;

	WR (q)->q_ptr = (char *) proto;
	q->q_ptr = (char *) proto;
	proto->qptr = q;

	MORE_USE;

	return 0;
}


static void mm_reply (struct _proto *proto, queue_t * q, mblk_t * mp, int err)
{
	if(q == proto->qptr || proto->prefix[0] == proto->prefix[1]) {
		m_reply(q,mp,err);
		return;
	}
	{
		mblk_t *mq = allocb (err ? 17 : 9, BPRI_HI);

		if (mq == NULL) {
			printf("* NoMem m_reply %d\n",err);
			freemsg (mq);
			return;
		}
		*mq->b_wptr++ = proto->prefix[PREFIX_LOCALPROTO];
		if (err == 0) {
			m_putid (mq, PROTO_NOERROR);
		} else {
			m_putid (mq, PROTO_ERROR);
			m_putsx (mq, PROTO_ERROR);
			m_puti (mq, err);
		}
		m_putdelim (mq);
		linkb (mq, mp);
		DATA_TYPE(mp) = MSG_DATA;
		DATA_TYPE(mq) = MSG_DATA;

		putnext (OTHERQ (q), mq);
	}
}

static void
proto_prot (queue_t * q, mblk_t * mp)
{
	struct _proto *proto = (struct _proto *) q->q_ptr;
	ushort_t id;
	streamchar *origmp = mp->b_rptr;
	int error = 0;

	if (m_getid (mp, &id) != 0) {
		mp->b_rptr = origmp;
		mm_reply(proto,q,mp,ENXIO);
		return;
	}
	switch (id) {
	default:
		mp->b_rptr = origmp;
		break;
	case PROTO_DATA_IN:
	case PROTO_DATA_OUT:
		freemsg(mp);
		mp = NULL;
		break;
	case PROTO_TICK:
		freemsg(mp);
		mp = NULL;
		break;
	case PROTO_OFFSET:
		{
			long z;
			if ((error = m_geti (mp, &z)) != 0)
				goto err;
			if (z < 0 || z >= 1024) {
				error = -EINVAL;
				goto err;
			}
			freemsg(mp);
			if((mp = allocb(8,BPRI_MED)) != NULL) {
				m_putid(mp,PROTO_OFFSET);
				m_puti(mp,proto->prefix[0] != proto->prefix[1]);
				DATA_TYPE(mp) = M_EXPROTO;
				qreply(q,mp);
				mp = NULL;
			}
		}
		break;
	case PROTO_AT:
		switch (proto->mode) {
		case P_NONE:
		case P_LISTEN:
		case P_CONN:
			if(!proto->talker) {
				int ms;

				ms = splstr ();
				mp->b_rptr = origmp;
				if (proto->keep != NULL)
					freemsg (proto->keep);
				proto->keep = mp;
				splx (ms);
				mp = NULL;
				break;
			}
			/* FALL THRU */
		default:
			DATA_TYPE(mp) = M_DATA;
			m_getskip (mp);
			if(!proto->binmode) {
				for(origmp = mp->b_rptr; origmp < mp->b_wptr; origmp++) {
					if(*origmp == '\r')
						*origmp = proto->sCR;
					else if (*origmp == '\n')
						*origmp = proto->sLF;
				}
				if (mp->b_wptr + 2 <= DATA_END(mp)) {
					*mp->b_wptr++ = proto->sCR;
					*mp->b_wptr++ = proto->sLF;
				} else {
					mblk_t *mz = allocb (2, BPRI_MED);
	
					if (mz != NULL) {
						*mz->b_wptr++ = proto->sCR;
						*mz->b_wptr++ = proto->sLF;
						linkb (mp, mz);
					}
				}
			}
			putnext (q, mp);
			mp = NULL;
			break;
		}
		break;
	case PROTO_INTERRUPT:
		mp->b_rptr = origmp;
		*(ushort_t *) (mp->b_rptr) = PROTO_HAS_INTERRUPT;
#if 0
		goto conn_intr;
#else
		qreply(q,mp);
		mp = NULL;
		break;
#endif
	case PROTO_CONNECTED:
		mp->b_rptr = origmp;
		*(ushort_t *) (mp->b_rptr) = PROTO_HAS_CONNECTED;
		qreply (q, mp);
		mp = NULL;
		if(proto->mode != P_CONN) {
			if (proto->cSig)
				putctlx1 (q, M_SIG, SIGUSR1);
			setmode(proto, P_CONN);
			qenable (WR (q));
		}
		if (proto->cCarrier == 2)
			proto->cCarrier |= 1;
		break;
	case PROTO_LISTEN:
		mp->b_rptr = origmp;
		*(ushort_t *) (mp->b_rptr) = PROTO_HAS_LISTEN;
		qreply (q, mp);
		setmode(proto, P_LISTEN);
		mp = NULL;
		break;
	case PROTO_INCOMING:
	case PROTO_OUTGOING:
		mp->b_rptr = origmp;
		mm_reply(proto,q,mp,0);
		mp = NULL;
		break;
	case PROTO_DISCONNECT:
		mp->b_rptr = origmp;
		if (proto->cSig)
			putctlx1 (q, M_SIG, SIGUSR2);
		*(ushort_t *) (mp->b_rptr) = PROTO_HAS_DISCONNECT;
		qreply (q, mp);
		mp = NULL;
		{
			int ms = splstr ();

			if (proto->keep != NULL) {
				putbq (proto->qptr, proto->keep);
				proto->keep = NULL;
				qenable (proto->qptr);
			}
			splx (ms);
		}
#if 0
		putctlx1 (q, M_FLUSH, FLUSHW);	/* To prevent data confusion */
#endif
		if ((proto->mode >= P_NONE) && (proto->cCarrier & 1)) {
			putctlx (q, M_HANGUP);
		}
		setmode(proto, (proto->talker ? P_CMD : P_NONE));
		break;
	case PROTO_WILL_DISCONNECT:
		mp->b_rptr = origmp;
		if (proto->cSig)
			putctlx1 (q, M_SIG, SIGPWR);
		*(ushort_t *) (mp->b_rptr) = PROTO_DISCONNECT;
		qreply (q, mp);
		mp = NULL;
		setmode(proto, P_NONE);
		{
			int ms = splstr ();

			if (proto->keep != NULL) {
				putbq (proto->qptr, proto->keep);
				proto->keep = NULL;
				qenable (proto->qptr);
			}
			splx (ms);
		}
		break;
	case PROTO_WILL_INTERRUPT:
		mp->b_rptr = origmp;
		*(ushort_t *) (mp->b_rptr) = PROTO_INTERRUPT;
		qreply (q, mp);
		mp = NULL;
		break;
	case PROTO_MODULE:
		if (strnamecmp (q, mp)) {	/* Config information for me. */
			long z;

			while ((mp != NULL) && ((error = m_getsx (mp, &id)) == 0)) {
				switch (id) {
				default:
					error = -EINVAL;
				    goto err;
				case PROTO_MODULE:
					break;
				case PROTO_PREFIX:
					if ((error = m_getstr (mp,proto->prefix,PREFIX_MAX)) != 0)
						goto err;
					break;
				case PROTO_CR:
					if ((error = m_geti (mp, &z)) != 0)
						goto err;
					if (z < 0 || z >= 255) {
						goto err;
					}
					proto->sCR = z;
					break;
				case PROTO_LF:
					if ((error = m_geti (mp, &z)) != 0)
						goto err;
					if (z < 0 || z >= 255) {
						goto err;
					}
					proto->sLF = z;
					break;
				case PROTO_BACKSPACE:
					if ((error = m_geti (mp, &z)) != 0)
						goto err;
					if (z < 0 || z >= 255) {
						goto err;
					}
					proto->sBS = z;
					break;
				case PROTO_ABORT:
					if ((error = m_geti (mp, &z)) != 0)
						goto err;
					if (z < 0 || z >= 255) {
						goto err;
					}
					proto->sBR = z;
					break;
				case PROTO_BINCMD:
					proto->binmode = 1;
					break;
				case PROTO_ASCIICMD:
					proto->binmode = 0;
					break;
				case PROTO_CARRIER:
					if ((error = m_geti (mp, &z)) != 0)
						goto err;
					if (z < 0 || z > 2) {
						goto err;
					}
					proto->cCarrier = z;
					break;
				case PROTO_BREAK:
					if ((error = m_geti (mp, &z)) != 0)
						goto err;
					if (z < 0 || z > 1) {
						goto err;
					}
					proto->cBreak = z;
					break;
				case PROTO_SIGNALS:
					if ((error = m_geti (mp, &z)) != 0)
						goto err;
					if (z < 0 || z > 1) {
						goto err;
					}
					proto->cSig = z;
					break;
				case PROTO_ONLINE:
					proto->talker = 0;
					if (proto->mode == P_CMD)
						setmode(proto, P_UNKNOWN);
					break;
				case PROTO_OFFLINE:
					proto->talker = 1;
					if(proto->cCarrier & 2)
						proto->cCarrier &=~ 1;
					if (proto->mode == P_UNKNOWN || proto->mode == P_NONE)
						setmode(proto, P_CMD);
					qenable (WR (proto->qptr));
					if (proto->keep != NULL) {
						putbq (proto->qptr, proto->keep);
						proto->keep = NULL;
						qenable (proto->qptr);
					}
					break;
				}
			}
			mp->b_rptr = origmp;
			mm_reply (proto,q,mp,0);
			mp = NULL;
		} else {
			mp->b_rptr = origmp;
			mm_reply (proto,q,mp,ENXIO);
			mp = NULL;
		}
		break;
	}
  err:
	if (mp != NULL) {
		if (origmp != NULL)
			mp->b_rptr = origmp;
		mm_reply(proto,q,mp,error ? error : -EINVAL);
	}
	return;
}

/*
 * Flush replies
 */
static void
proto_reply_send (struct _proto *ch, short flush)
{
	if ((flush || ch->reppos >= sizeof (ch->repline)) && (ch->reppos > 0)) {
		mblk_t *rep = allocb (ch->reppos, BPRI_LO);

		if (rep == NULL) {
			ch->reppos = -1;
		} else {
			bcopy (ch->repline, rep->b_wptr, ch->reppos);
			rep->b_wptr += ch->reppos;
			putnext (ch->qptr, rep);
			ch->reppos = 0;
		}
	}
	if (ch->reppos == -1 && putctlx (ch->qptr, M_HANGUP) == 0)
		ch->reppos = -2;
}

/*
 * Send a reply
 */
static void
proto_reply (struct _proto *ch, uchar_t c)
{
	proto_reply_send (ch, 0);
	if (ch->reppos >= 0)
		ch->repline[ch->reppos++] = c;
}



/*
 * Low-level processing of incoming characters.
 * 
 * Essentially a very stupid line editor. Setting up the "line" Stremas module
 * would be too much work; besides, "line" usually works in the other
 * direction, thus it might be dangerous too.
 */
static void
proto_cmdproc (struct _proto *ch, uchar_t c)
{
	int cmdlen;
	short cc;

	for (cmdlen = 0; cmdlen < CMDLEN; cmdlen++)
		if (ch->cmdline[cmdlen] == 0)
			break;
	if (c == ch->sCR)
		cc = 0x10D;
	else if (c == ch->sLF)
		cc = 0x10A;
	else if (c == ch->sBS)
		cc = 0x108;
	else if (c == ch->sBR)
		cc = 0x103;
	else
		cc = c;
	switch (cc) {
	default:
		if (cmdlen >= CMDLEN - 1) {
			/* putctlx1 (ch->qptr, M_DATA, ch->sBS); */
			return;
		}
		ch->cmdline[cmdlen++] = c;
		ch->cmdline[cmdlen] = '\0';
		proto_reply (ch, c);
		break;
	case 0x103:
		ch->cmdline[0] = 0;
		proto_reply (ch, ch->sCR);
		proto_reply (ch, ch->sLF);
		break;
	case 0x108:
		if (cmdlen > 0) {
			proto_reply (ch, ch->sBS);
			proto_reply (ch, ' ');
			proto_reply (ch, ch->sBS);
		}
		ch->cmdline[--cmdlen] = '\0';
		break;
	case 0x10D:
	case 0x10A:
		if (cmdlen == 0) {
			proto_reply (ch, ch->sCR);
			proto_reply (ch, ch->sLF);
			return /* 0 */ ;
		} else {
			mblk_t *mb = allocb (8 + cmdlen, BPRI_MED);

			if (mb == NULL) {
				putctlx (ch->qptr, M_HANGUP);
				return /* EAGAIN */ ;
			}
			/* Endianness alert */
			*((ushort_t *) mb->b_wptr)++ = PROTO_AT;
			*mb->b_wptr++ = ' ';

			for (cmdlen = 0; ch->cmdline[cmdlen] != 0; cmdlen++)
				*mb->b_wptr++ = ch->cmdline[cmdlen];

			DATA_TYPE(mb) = MSG_PROTO;
			putnext (WR (ch->qptr), mb);

			proto_reply (ch, ch->sCR);
			proto_reply (ch, ch->sLF);
		}
		ch->cmdline[0] = 0;
		break;
	}
	return /* 0 */ ;
}



static void
proto_rput (queue_t * q, mblk_t * mp)
{
	switch (DATA_TYPE(mp)) {
	case M_FLUSH:
		if (*mp->b_rptr & FLUSHR)
			flushq (q, 0);
		putnext (q, mp);
		break;
	default:
		putq (q, mp);
		break;
	}
	return;
}

static void
proto_rsrv (queue_t * q)
{
	struct _proto *proto = (struct _proto *) q->q_ptr;
	mblk_t *mp;

	while ((mp = getq (q)) != NULL) {
		switch (DATA_TYPE(mp)) {
		case MSG_PROTO:
			if(proto->prefix[0] == proto->prefix[1] || proto->mode <= P_NONE) {
				proto_prot (q, mp);
				break;
			}
			if(mp->b_rptr <= DATA_START(mp)) {
				mblk_t *mq = allocb(1,BPRI_HI);
				if(mq == NULL) {
					putbqf(q,mp);
					return;
				}
				linkb(mq,mp);
				mp = mq;
			}
			*--mp->b_rptr = proto->prefix[PREFIX_PROTO];
			goto def;
		case CASE_DATA:
			DATA_TYPE(mp) = M_DATA;
			switch (proto->mode) {
			case P_NONE:
			case P_CMD:
				freemsg (mp);
				continue;
			case P_UNKNOWN:
				putbqf (q, mp);
				return;
			default:
				if ((proto->talker && proto->prefix[0] == proto->prefix[1]) || !canput (q->q_next)) {
					putbq (q, mp);
					return;
				}
				if(proto->prefix[0] != proto->prefix[1]) {
					if(mp->b_rptr <= DATA_START(mp)) {
						int i;
						for(i=0;i<PREFIX_MAX;i++) {
							if(*mp->b_rptr == proto->prefix[i]) {
								mblk_t *mq = allocb(1,BPRI_HI);
								if(mq == NULL) {
									putbqf(q,mp);
									return;
								}
								*--mp->b_rptr = proto->prefix[PREFIX_DATA];
								linkb(mq,mp);
								mp = mq;
								break;
							}
						}
					} else
						*--mp->b_rptr = proto->prefix[PREFIX_DATA];
				}
			def:
				DATA_TYPE(mp) = M_DATA;
#ifndef linux
				if (proto->tty.c_iflag & (ICRNL | INLCR | ISTRIP | IGNCR)) {
					unsigned char c;
					unsigned short flag = proto->tty.c_iflag;
					mblk_t *mm;

					for (mm = mp; mm != NULL; mm = mm->b_cont) {
						streamchar *endp = mm->b_wptr;
						streamchar *src = mm->b_rptr;
						uchar_t *dest = mm->b_rptr;

						while (src < endp) {
							c = *src++;

							if (flag & ISTRIP)
								c &= 0x7F;
							switch (c) {
							case '\r':
								if (flag & IGNCR)
									continue;
								else if (flag & ICRNL)
									c = '\n';
								break;
							case '\n':
								if (flag & INLCR)
									c = '\r';
								break;
							default:;
							}
							*dest++ = c;
						}
						mm->b_wptr = dest;
					}
				}
#endif
				putnext (q, mp);
				break;
			}
			continue;
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
proto_wput (queue_t * q, mblk_t * mp)
{
	struct _proto *proto = (struct _proto *) q->q_ptr;

	switch (DATA_TYPE(mp)) {
	case M_FLUSH:
		if (*mp->b_rptr & FLUSHW) {
			flushq (q, 0);
			S_flush(&proto->write_delay);
		}
		putnext (q, mp);
		break;
	default:
		putq (q, mp);
		break;
	}
	return;
}

static void
proto_wsrv (queue_t * q)
{
	struct _proto *proto = (struct _proto *) q->q_ptr;
	mblk_t *mp;
	int realq = 1;

	while ((mp = getq (q)) != NULL || (realq = 0) || (mp = S_dequeue(&proto->write_delay)) != NULL) {
if(!realq)printf("FromDel %p\n", &proto->write_delay);
		switch (DATA_TYPE(mp)) {
		case MSG_PROTO:
			freemsg (mp);
			break;
		case CASE_DATA:
			switch (proto->mode) {
			case P_CMD: DoCmd:
				if(proto->binmode) {
            		mblk_t *mb = allocb (4, BPRI_MED);
		
            		if (mb == NULL) {
                		putctlx (RD(q), M_HANGUP);
						freemsg(mp);
                		return /* EAGAIN */ ;
            		}
            		/* Endianness alert */
            		*((ushort_t *) mb->b_wptr)++ = PROTO_AT;
            		*mb->b_wptr++ = ' ';

		            DATA_TYPE(mb) = MSG_PROTO;
					linkb(mb,mp);
					putnext(q,mb);
				} else {
					while ((mp = pullupm (mp, 0)) != NULL)
						proto_cmdproc (proto, *mp->b_rptr++);
					proto_reply_send (proto, 1);
				}
				continue;
			case P_UNKNOWN:
			case P_NONE:
			case P_LISTEN:
#if 0
				if(realq) {
					if(proto->write_delay.nblocks > 20)
						freemsg(mp);
					else
						S_enqueue(&proto->write_delay, mp);
				} else {
					S_requeue(&proto->write_delay, mp);
					return;
				}
				goto nextone;
#else
				putbq(q,mp);
				return;
#endif
			default:
				{
					streamchar ch = *mp->b_rptr;
					if(proto->prefix[0] != proto->prefix[1]) {
						if(ch == proto->prefix[PREFIX_DATA]) {
							mp->b_rptr++;
						}
						else if(ch == proto->prefix[PREFIX_PROTO]) {
							mp->b_rptr++;
							DATA_TYPE(mp) = MSG_PROTO;
						}
						else if(ch == proto->prefix[PREFIX_LOCALPROTO]) {
							mp->b_rptr++;
							proto_prot(q,mp);
							continue;
						}
						else {
							ch = 0;
						}
					} else if(proto->talker)
						goto DoCmd;
					if (!canput (q->q_next)) {
						if(proto->prefix[0] != proto->prefix[1]) {
							if(ch != 0)
								*--mp->b_rptr = ch;

						}
						DATA_TYPE(mp) = MSG_DATA;
						putbq (q, mp);
						return;
					}
				}
			}
#ifndef linux
			if (proto->tty.c_oflag & (ONLCR | OCRNL)) {
				short flag = proto->tty.c_oflag;
				mblk_t *mm, *mm2;

				for (mm2 = mm = mp; mm != NULL; mm2 = mm = mm2->b_cont) {
					unsigned char *endp = (uchar_t *) mm->b_wptr;
					unsigned char *src = (uchar_t *) mm->b_rptr;
					unsigned char *lim = (uchar_t *) DATA_END(mm);
					unsigned char *dest = (uchar_t *) mm->b_wptr;

					if (flag & ONLCR) {	/* Possible expansion. */
						short addlen = 0;

						while (src < dest) {
							if (*src++ == '\n')
								addlen++;
						}
						dest += addlen;
					}
					src = endp;
					endp = mm->b_rptr;
					if (dest <= (uchar_t *) DATA_END(mm))
						(uchar_t *) mm->b_wptr = dest;
					else {		  /* Too much expansion. (Happens with any
								   * three-characters-plus-Newline data block.)
								   * Allocate another mblk, link it in, start
								   * expanding into it. */
						uchar_t *lim2;

						mm->b_wptr = DATA_END(mm);
						mm2 = allocb (dest - (uchar_t *) DATA_END(mm), BPRI_MED);
						if (mm2 == NULL)
							continue;
						mm2->b_cont = mm->b_cont;
						mm->b_cont = mm2;
						lim2 = (uchar_t *) mm2->b_rptr;
						dest = (uchar_t *) mm2->b_wptr = lim2 + (dest - (uchar_t *) DATA_END(mm));
						while (src > endp) {
							char c = *--src;

							switch (c) {
							case '\n':
								if (flag & ONLCR) {
									*--dest = '\n';
									if (dest == lim2) {
										dest = (uchar_t *) mm->b_wptr;
										*--dest = '\r';
										goto norm;
									}
									c = '\r';
								}
								break;
							case '\r':
								if (flag & OCRNL)
									c = '\n';
								break;
							default:;
							}
							*--dest = c;
							if (dest == lim2) {
								dest = (uchar_t *) mm->b_wptr;
								goto norm;
							}
						}
					}
				  norm:
					while (src > endp) {
						char c = *--src;

						switch (c) {
						case '\n':
							if (flag & ONLCR) {
								*--dest = '\n';
								c = '\r';
							}
							break;
						case '\r':
							if (flag & OCRNL)
								c = '\n';
							break;
						default:;
						}
						*--dest = c;
					}
					(uchar_t *) mm->b_rptr = dest;
				}
			}
#endif
			putnext (q, mp);
			break;
		case M_IOCTL:
			{
				struct iocblk *iocb;
				int error = -EINVAL;

				iocb = (struct iocblk *) mp->b_rptr;

				if(0)printf("Proto IOC %x\n",iocb->ioc_cmd);
				switch (iocb->ioc_cmd) {
#if 1 /* ndef linux */
#ifdef TCGETA
				case TCGETA:
					{
						struct termio *tty;
						mblk_t *m0 = allocb (sizeof (struct termio), BPRI_MED);
						if (m0 == NULL) {
							error = -ENOMEM;
							goto iocnak;
						}
						m0->b_cont = mp->b_cont;
						mp->b_cont = m0;
						tty = ((struct termio *) m0->b_wptr)++;
						bzero(tty,sizeof(*tty));
						tty->c_cflag = proto->tty.c_cflag;
						tty->c_iflag = proto->tty.c_iflag;
						tty->c_lflag = proto->tty.c_lflag;
						tty->c_oflag = proto->tty.c_oflag;
						tty->c_line  = proto->tty.c_line ;
#if NCC <= NCCS
						memcpy(tty->c_cc,proto->tty.c_cc,NCC);
#else
						memcpy(tty->c_cc,proto->tty.c_cc,NCCS);
#endif

						goto iocack;
					}
#endif
#endif
#ifdef TCFLSH
				case TCFLSH:
					{
						goto iocack; /* We don't flush */
					}
#endif
#if 1 /* ndef linux */
#ifdef TCGETS
				case TCGETS:
					{
						mblk_t *m0 = allocb (sizeof (struct termios), BPRI_MED);

						if (m0 == NULL) {
							error = -ENOMEM;
							goto iocnak;
						}
						m0->b_cont = mp->b_cont;
						mp->b_cont = m0;
						*((struct termios *) m0->b_wptr)++ = proto->tty;

						goto iocack;
					}
#endif
#endif
#ifdef UIOCTTSTAT
				case UIOCTTSTAT:
					{
						mblk_t *m0 = allocb (3, BPRI_MED);
						if (m0 == NULL) {
							error = -ENOMEM;
							goto iocnak;
						}
						m0->b_cont = mp->b_cont;
						mp->b_cont = m0;

						*m0->b_wptr++ = 0;
						*m0->b_wptr++ = 0;
						*m0->b_wptr++ = 1;

						goto iocack;
					}
#endif
#if 1 /* ndef linux */
#ifdef TCSETA
				case TCSETA:
				case TCSETAW:
				case TCSETAF:
					{
						int ms;
						struct termio *tty;

						if (mp->b_cont == NULL || iocb->ioc_count != sizeof (struct termio)) {
							goto iocnak;
						}
						ms = splstr ();
						tty = (struct termio *) mp->b_cont->b_rptr;
						if (proto->cBreak && !(tty->c_cflag & CBAUD)) {
							mblk_t *mb;

							if (proto->keep != NULL) {
								putbq (proto->qptr, proto->keep);
								proto->keep = NULL;
								qenable (proto->qptr);
							}
							if (proto->mode >= P_NONE) {
								setmode(proto, P_CMD);
								mb = allocb (3, BPRI_MED);
								if (mb != NULL) {
									*((ushort_t *) mb->b_wptr)++ = PROTO_AT;
									DATA_TYPE(mb) = MSG_PROTO;
									putnext (q, mb);
								}
							}
						}
						proto->tty.c_cflag = tty->c_cflag;
						proto->tty.c_iflag = tty->c_iflag;
						proto->tty.c_lflag = tty->c_lflag;
						proto->tty.c_oflag = tty->c_oflag;
						proto->tty.c_line  = tty->c_line ;
#if NCC <= NCCS
						memcpy(proto->tty.c_cc,tty->c_cc,NCC);
#else
						memcpy(proto->tty.c_cc,tty->c_cc,NCCS);
#endif
#if NCC < NCCS
						memset(proto->tty.c_cc+NCC,0,NCCS-NCC);
#endif
#if 0
						proto->tty.c_iflag |= IGNBRK | IGNPAR;
						proto->tty.c_iflag &= ~(BRKINT | INPCK | IUCLC | IXON | IXANY | IXOFF);
						proto->tty.c_oflag &= ~(ONOCR | OLCUC | NLDLY | OFILL | OFDEL | CRDLY | TABDLY | BSDLY | VTDLY | FFDLY);
						proto->tty.c_cflag &= ~(CBAUD | CSIZE | CSTOPB | PARENB | PARODD | CLOCAL);
						proto->tty.c_cflag |= B38400 | CS8 | HUPCL;
						/* bzero(proto->tty.c_cc,NCC); */
#endif
						splx (ms);
						goto iocack;

					}
#endif
#endif
#if 1 /* ndef linux */
#ifdef TCSETS
				case TCSETS:
				case TCSETSW:
				case TCSETSF:
#if 0 /* defined(TIOCSETA) && (TIOCSETA != TCSETS) */
				case TIOCSETA:
				case TIOCSETAW:
				case TIOCSETAF:
#endif
					{
						int ms;

						if (mp->b_cont == NULL || iocb->ioc_count != sizeof (struct termios)) {
							printf("termios: want %d, got %d\n",sizeof(struct termios),iocb->ioc_count);
							goto iocnak;
						}
						ms = splstr ();
						proto->tty = *(struct termios *) mp->b_cont->b_rptr;
						if (proto->cBreak && !(proto->tty.c_cflag & CBAUD)) {
							mblk_t *mb;

							if (proto->keep != NULL) {
								putbq (proto->qptr, proto->keep);
								proto->keep = NULL;
								qenable (proto->qptr);
							}
							if (proto->mode >= P_NONE) {
								setmode(proto, P_CMD);
								mb = allocb (3, BPRI_MED);
								if (mb != NULL) {
									*((ushort_t *) mb->b_wptr)++ = PROTO_AT;
									DATA_TYPE(mb) = MSG_PROTO;
									putnext (q, mb);
								}
							}
						}
#if 0
						proto->tty.c_iflag |= IGNBRK | IGNPAR;
						proto->tty.c_iflag &= ~(BRKINT | INPCK | IUCLC | IXON | IXANY | IXOFF);
						proto->tty.c_oflag &= ~(ONOCR | OLCUC | NLDLY | OFILL | OFDEL | CRDLY | TABDLY | BSDLY | VTDLY | FFDLY);
						proto->tty.c_cflag &= ~(CBAUD | CSIZE | CSTOPB | PARENB | PARODD | CLOCAL);
						proto->tty.c_cflag |= B38400 | CS8 | HUPCL;
						/* bzero(proto->tty.c_cc,NCC); */
#endif
						splx (ms);
						goto iocack;

					}
#endif
#endif
#ifdef TCSBRK
				case TCSBRK:
#endif
#ifdef TCSBRKM
				case TCSBRKM:
#endif
#ifdef TCRESET
				case TCRESET:
#endif
#if defined(TCRESET) || defined(TCSBRKM) || defined(TCSBRK)
					{
						if (proto->cBreak) {
							int ms;

							if (proto->mode >= P_NONE) {
								mblk_t *mz;

								setmode(proto, P_CMD);
								mz = allocb (3, BPRI_MED);
								if (mz != NULL) {
									*((ushort_t *) mz->b_wptr)++ = PROTO_AT;
									DATA_TYPE(mz) = MSG_PROTO;
									putnext (q, mz);
								}
							}
							ms = splstr ();
							if (proto->keep != NULL) {
								putbq (proto->qptr, proto->keep);
								proto->keep = NULL;
								qenable (proto->qptr);
							}
							splx (ms);
						}
					}
					goto iocack;
#endif
				case TCXONC:
#ifdef TIOCMGET
				case TIOCMGET:
#endif
#ifdef TIOCMBIS
				case TIOCMBIS:
#endif
#ifdef TIOCMBIC
				case TIOCMBIC:
#endif
#ifdef TIOCMSET
				case TIOCMSET:
#endif
#ifdef TCCBRKM
				case TCCBRKM:
#endif
#ifdef TCSETDTR
				case TCSETDTR:
#endif
#ifdef TCCLRDTR
				case TCCLRDTR:
#endif
#ifdef TIOCSDTR
				case TIOCSDTR:
#endif
#ifdef TIOCCDTR
				case TIOCCDTR:
#endif
#ifdef TIOCSBRK
				case TIOCSBRK:
#endif
#ifdef TIOCCBRK
				case TIOCCBRK:
#endif
#ifdef LDSETT
				case LDSETT:
#endif
#ifdef UIOCMODEM
				case UIOCMODEM:
#endif
#ifdef UIOCNOMODEM
				case UIOCNOMODEM:
#endif
#ifdef UIOCEMODEM
				case UIOCEMODEM:
#endif
#ifdef UIOCDTRFLOW
				case UIOCDTRFLOW:
#endif
#ifdef UIOCFLOW
				case UIOCFLOW:
#endif
#ifdef UIOCNOFLOW
				case UIOCNOFLOW:
#endif
				  iocack:
					DATA_TYPE(mp) = M_IOCACK;
					qreply (q, mp);
					break;
				default:
					putnext (q, mp);
					break;
				  iocnak:
					DATA_TYPE(mp) = M_IOCNAK;
					if(error < 0)
						error = -error;
					iocb->ioc_error = error;
					qreply (q, mp);
				}
				break;
			}
		default:
			if (DATA_TYPE(mp) > QPCTL || canput (q->q_next)) {
				putnext (q, mp);
				continue;
			} else {
				putbqf (q, mp);
				return;
			}
		}
	}
	return;
}


static void
proto_close (queue_t * q, int dummy)
{
	struct _proto *proto;
	int ms;

	proto = (struct _proto *) q->q_ptr;

	flushq (q, FLUSHALL);
	flushq (WR (q), FLUSHALL);
if(0)printf("FlushDel %p\n", &proto->write_delay);
	S_flush (&proto->write_delay);

	proto->qptr = NULL;
	ms = splstr ();
	if (proto->keep != NULL) {
		freemsg (proto->keep);
		proto->keep = NULL;
	}
	free(proto);
	splx (ms);
	LESS_USE;
	return;
}



#ifdef MODULE
static int do_init_module(void)
{
	return register_strmod(&protoinfo);
}

static int do_exit_module(void)
{
	return unregister_strmod(&protoinfo);
}
#endif
