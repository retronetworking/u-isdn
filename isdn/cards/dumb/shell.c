/* BSC

HSCX

CCR1 0D
MODE 1C
CCR2 32
XCCR 07 
RCCR 07
TIMR E9
MASK 00
XBCH 00
RAH1 00
RAH2 00
CMDR 41
TSAX 2F 03
TSAR 2F 03

ISAC

ADF1 80
MASK 32
MODE 3A
TIMR E9
XAD1 00
XAD2 tei
SAB1 SAB2
XPCR 00
XTCR 70
ADF1 00
MOCR 00
SQXR 20
CMDR 41

*/
#ifdef linux
#define SLOW_IO_BY_JUMPING
#if 0 /* def _ncp_ */
#define REALLY_SLOW_IO
#endif
#endif

#include "f_module.h"
#include "primitives.h"
#include "streams.h"
#include "isdn_12.h"
#include "smallq.h"
#include "isdn_limits.h"
#include <sys/stream.h>
#include "streamlib.h"
#include <sys/errno.h>
#ifdef SCO
#include <sys/immu.h>
#endif
#include <sys/sysmacros.h>
#include <stddef.h>

#ifdef M_UNIX
#define ByteOut(_where,_what) iooutb(_where,_what)
#define ByteIn(_where) ioinb(_where)
#endif
#ifdef linux
#include <asm/io.h>
#include <linux/ptrace.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#define ByteOut(_where,_what) outb_p(_what,_where)
#define ByteIn(_where) inb_p(_where)
#endif

#if 1
#define CEC(x) do{int i;for(i=0;i<1000;i++) if(!(x)) break; } while(0);
#else
#define CEC(x) if(0)printf("c");
#endif

#define NEW_XMIT 0

#define r __rw.__r
#define w __rw.__w
#define ISAC_R_FIFO_SIZE 32
#define ISAC_W_FIFO_SIZE 32
#define HSCX_R_FIFO_SIZE 32
#define HSCX_W_FIFO_SIZE 32

#define FIFO(x) fifo[(x,0)] /* keep side effects but address at offset zero */

#include "Config.h"

typedef struct _isac {
	union {
		struct {
			volatile Byte fifo[ISAC_R_FIFO_SIZE];
/* 00 */	volatile Byte ISTA,STAR,MODE,TIMR;
/* 04 */	volatile Byte EXIR,RBCL,SAPR,RSTA;
/* 08 */	volatile Byte __f1,RHCR,RBCH,__f3;
/* 0C */	volatile Byte __f4[4];
/* 10 */	volatile Byte SPCR,CIR0,MOR0,CIR1;
/* 14 */	volatile Byte MOR1,C1R,C2R,B1CR;
/* 18 */	volatile Byte B2CR,ADF2,MOSR,SQRR;
		} __r;
		struct {
			volatile Byte fifo[ISAC_W_FIFO_SIZE];
/* 00 */	volatile Byte MASK,CMDR,MODE,TIMR;
/* 04 */	volatile Byte XAD1,XAD2,SAP1,SAP2;
/* 08 */	volatile Byte TEI1,TEI2,__f1,__f2;
/* 0C */	volatile Byte __f3[4];
/* 10 */	volatile Byte SPCR,CIX0,MOX0,CIX1;
/* 14 */	volatile Byte MOX1,C1R,C2R,STCR;
/* 18 */	volatile Byte ADF1,ADF2,MOCR,SQXR;
		} __w;
	} __rw;
} *__isac;

typedef struct _hscx {
	union {
		struct {
			volatile Byte fifo[HSCX_R_FIFO_SIZE];
/* 00 */	volatile Byte ISTA,STAR,MODE,TIMR;
/* 04 */	volatile Byte EXIR,RBCL,_f1,RSTA;
/* 08 */	volatile Byte RAL1,RHCR,__f2,__f3;
/* 0C */	volatile Byte CCR2,RBCH,VSTR,CCR1;
		} __r;
		struct {
			volatile Byte fifo[HSCX_W_FIFO_SIZE];
/* 00 */	volatile Byte MASK,CMDR,MODE,TIMR;
/* 04 */	volatile Byte XAD1,XAD2,RAH1,RAH2;
/* 08 */	volatile Byte RAL1,RAL2,XBCL,BGR;
/* 0C */	volatile Byte CCR2,XBCH,RLCR,CCR1;
/* 10 */	volatile Byte TSAX,TSAR,XCCR,RCCR;
		} __w;
	} __rw;
} *__hscx;

#define ByteInISAC(_dumb,_what) InISAC((_dumb),offsetof(struct _isac,r._what))
#define ByteOutISAC(_dumb,_what,_data) OutISAC((_dumb),offsetof(struct _isac,w._what), (_data))

#define ByteInHSCX(_dumb,_hcr,_what) InHSCX((_dumb),(_hcr),offsetof(struct _hscx,r._what))
#define ByteOutHSCX(_dumb,_hcr,_what,_data) OutHSCX((_dumb),(_hcr),offsetof(struct _hscx,w._what),(_data))

#ifdef linux
#define SetSPL(x) spl5()
#else
#define SetSPL(x) spl((x))
#endif

#ifdef linux

#ifdef _avm_
#define CARDTYPE avm
#include "avm_io.c"
#endif

#ifdef _bsc_
#define CARDTYPE bsc
#include "bsc_io.c"
#endif

#ifdef _ncp_
#define CARDTYPE ncp
#include "ncp_io.c"
#endif

#ifdef _teles_
#define CARDTYPE teles
#include "teles_io.c"
#endif

#define DUMBTIME 300 /* poll: times per second */

#define xxappxx(a,b) a##b
#define NAME(a,b) xxappxx(a,b) /* isn't C wonderful */
#define xxstrxx(a) #a
#define STRING(a) xxstrxx(a) /* ditto */

void NAME(CARDTYPE,poll)(struct _dumb *dumb);
#else
void NAME(CARDTYPE,poll)(void *);
#endif

static void toggle_off(struct _dumb * dumb)
{
	int i;
	ByteOutISAC(dumb,MASK,0xFF);
	for(i=1;i <= dumb->numHSCX; i++)
		ByteOutHSCX(dumb,i,MASK,0xFF);
	
}
static void toggle_on(struct _dumb * dumb)
{
	int i;
	for(i=1;i <= dumb->numHSCX; i++)
		ByteOutHSCX(dumb,i,MASK,0x00);
	ByteOutISAC(dumb,MASK,0x00);
}

static int dumb_mode (struct _isdn1_card * card, short channel, char mode,
	char listen)
{
	struct _dumb * dumb = (struct _dumb *) card;
	unsigned long ms = SetSPL(dumb->ipl);
	int err = 0;

	switch(channel) {
	case 0:
		DEBUG(info) printf(KERN_INFO "ISDN%d ISAC %s<%d>%s\n",dumb-dumbdata,mode?(mode==1?"standby":"up"):"down",mode,listen?" listen":"");
		ISAC_mode(dumb,mode,listen);
		if(mode == M_OFF) {
			int j;
			for(j=1;j <= dumb->numHSCX;j++)
				HSCX_mode(dumb,j,M_OFF,0);
		}
		break;
	default:
		if(channel > 0 && channel <= dumb->numHSCX) {
			DEBUG(info) printf(KERN_INFO "ISDN%d HSCX%d %s<%d>%s\n",dumb-dumbdata,channel,mode?"up":"down",mode,listen?" listen":"");
			err = HSCX_mode(dumb,channel,mode,listen);
			if (err < 0) {
				printf(KERN_WARNING "ISDN err %d %d\n",channel, err);
				splx(ms);
				return -err;
			}

			dumb->chan[channel].nblk = 0;
			dumb->chan[channel].maxblk = 10;
			break;
		} else {
			printf(KERN_WARNING "ISDN badChan %d\n",channel);
			splx(ms);
			return EINVAL;
		}
	}
	NAME(CARDTYPE,poll)(dumb);
	splx(ms);
	return err;
}

static int dumb_prot (struct _isdn1_card * card, short channel, mblk_t * proto)
{
	return EINVAL;
}

/*
 * Check if buffer space is available
 */
static int dumb_candata (struct _isdn1_card * card, short channel)
{
	struct _dumb * dumb = (struct _dumb *)card;
	return (dumb->chan[channel].q_out.nblocks < 4);
}

/*
 * Enqueue the data.
 */
static int dumb_data (struct _isdn1_card * card, short channel, mblk_t * data)
{
	struct _dumb * dumb = (struct _dumb *)card;
	S_enqueue(&dumb->chan[channel].q_out, data);
	NAME(CARDTYPE,poll)((struct _dumb *) card);
	return 0;
}

/*
 * Flush the send queue.
 */
static int dumb_flush (struct _isdn1_card * card, short channel)
{
	struct _dumb * dumb = (struct _dumb *)card;
	S_flush(&dumb->chan[channel].q_out);
	return 0;
}

static int ISACpresent(struct _dumb *dumb)
{
	unsigned char Bt;
	if(((Bt = ByteInISAC(dumb,STAR)) & ~0x4F) != 0) {
		printf("<STAR %02x> ",Bt);
		return 0;
	}
	if(((Bt = ByteInISAC(dumb,EXIR)) & ~0x10) != 0) {
		printf("<EXIR %02x> ",Bt);
		return 0;
	}
	if(((Bt = ByteInISAC(dumb,RBCH)) & 0x8F) != 0x00) {
		printf("<RBCH %02x> ",Bt);
		return 0;
	}
	ByteOutISAC(dumb,MODE,0xD1);
	if((Bt = ByteInISAC(dumb,MODE)) != 0xD1) {
		printf("<MODE %02x> ",Bt);
		return 0;
	}
#if 0
	if((Bt = ByteInISAC(dumb,RBCL)) != 0) {
		printf("<RBCL %02x> ",Bt);
		return 0;
	}
#endif
	ByteOutISAC(dumb,MODE,0xC1);
	if((Bt = ByteInISAC(dumb,MODE)) != 0xC1) {
		printf("<MODE %02x> ",Bt);
		return 0;
	}
	return 1;
}

static void InitHSCX(struct _dumb * dumb)
{
	int i;
	for (i=1; i <= dumb->numHSCX; i++) 
		InitHSCX_ (dumb,i);
}


static void ISAC_kick(struct _dumb * dumb)
{
	mblk_t *sendb;
	uchar_t *sendp = NULL;
	
	unsigned long ms = SetSPL(dumb->ipl);
	DEBUG(isac) printf(KERN_DEBUG "K%d ",dumb-dumbdata);
	if(dumb->chan[0].locked) {
		DEBUG(isac) printf("lck ");
		splx(ms);
		return;
	}

	if(dumb->chan[0].mode != M_HDLC) {
		if(0) DEBUG(isac) { printf("Flush Off\n"); }
		S_flush(&dumb->chan[0].q_out);
		if(dumb->chan[0].mode == M_OFF) {
			if(dumb->chan[0].m_in != NULL) {
				freemsg(dumb->chan[0].m_in);
				dumb->chan[0].m_in = dumb->chan[0].m_in_run = NULL;
			}
		}
		if(dumb->chan[0].m_out != NULL) {
			freemsg(dumb->chan[0].m_out);
			dumb->chan[0].m_out = dumb->chan[0].m_out_run = NULL;
		}
		splx(ms);
		return;
	}

	if ((ByteInISAC(dumb,STAR) & 0x40) == 0) { /* ! XFW */
		DEBUG(isac) if(dumb->chan[0].q_out.nblocks+(dumb->chan[0].m_out_run != NULL))
			printf("NRdy%d\n",dumb->chan[0].q_out.nblocks+(dumb->chan[0].m_out_run != NULL));
		splx(ms);
		return;
	}
	
	dumb->chan[0].locked = 1;
	if(dumb->chan[0].m_out_run == NULL) {
		sendb = dumb->chan[0].m_out = dumb->chan[0].m_out_run = S_dequeue(&dumb->chan[0].q_out);
		if(sendb != NULL)
			sendp = dumb->chan[0].m_out_run->b_rptr;
	} else {
		sendb = dumb->chan[0].m_out_run;
		sendp = dumb->chan[0].p_out;
	}
	splx(ms);

	if (sendb != NULL) {
		short numb = 0;
		DEBUG(isac) printf(".si");
		do {
			short thisb = (uchar_t *)sendb->b_wptr-sendp;
			if(thisb > ISAC_W_FIFO_SIZE-numb) thisb = ISAC_W_FIFO_SIZE-numb;
			for(;thisb > 0; thisb--)
				ByteOutISAC(dumb,FIFO(numb++),*sendp++);
			while(sendp >= (uchar_t *)sendb->b_wptr) {
				sendb = sendb->b_cont;
				if(sendb != NULL)
					sendp = (uchar_t *)sendb->b_rptr;
				else
					break;
			}
		} while((numb < ISAC_W_FIFO_SIZE) && (sendb != NULL));
		if(sendb != NULL) {
			ByteOutISAC(dumb,CMDR, 0x08); /* XTF */
			dumb->chan[0].m_out_run = sendb;
			dumb->chan[0].p_out = sendp;
		} else {
			ByteOutISAC(dumb,CMDR, 0x0A); /* XTF|XME */
			freemsg(dumb->chan[0].m_out);
			dumb->chan[0].m_out = dumb->chan[0].m_out_run = NULL;
			if(dumb->chan[0].q_out.nblocks < 2)
				isdn2_backenable(&dumb->card,0);
		}
	}
	dumb->chan[0].locked = 0;
	DEBUG(isac) printf("\n");
}

#ifdef __GNUC__
/* inline */
#endif
static void HSCX_kick(struct _dumb * dumb, u_char hscx)
{
	mblk_t *sendb;
	uchar_t *sendp = NULL;
	hdlc_buf bufp = &dumb->chan[hscx];

	unsigned long ms = SetSPL(dumb->ipl);
	DEBUG(hscxout) printf(KERN_DEBUG "K%d.%d ",dumb-dumbdata,hscx);
	if(bufp->locked) {
		DEBUG(hscxout) { printf("Lck\n"); }
		splx(ms);
		return;
	}
	if(bufp->listen) {
		DEBUG(hscxout) { printf("Listen\n"); }
		splx(ms);
		return;
	}
	
	if(bufp->mode == M_OFF) {
		DEBUG(hscxout) { if(bufp->q_out.nblocks > 0) printf("Flush Off\n"); }
        S_flush(&bufp->q_out);
        if(bufp->m_in != NULL) {
            freemsg(bufp->m_in);
            bufp->m_in = bufp->m_in_run = NULL;
        }
        if(bufp->m_out != NULL) {
            freemsg(bufp->m_out);
            bufp->m_out = bufp->m_out_run = NULL;
        }
		splx(ms);
		return;
	}
	
	CEC(ByteInHSCX(dumb,hscx,STAR) & 0x04);
	if ((ByteInHSCX(dumb,hscx,STAR) & 0x40) == 0) { /* ! XFW */
		DEBUG(hscxout)printf("NR\n");
		splx(ms);
		return;
	}
	
	bufp->locked = 1;
	splx(ms);
#if 0
	do {
#endif
    	if(bufp->m_out_run == NULL) {
        	sendb = bufp->m_out = bufp->m_out_run = S_dequeue(&bufp->q_out);
        	if(sendb != NULL)
            	sendp = bufp->m_out_run->b_rptr;
    	} else {
        	sendb = bufp->m_out_run;
        	sendp = bufp->p_out;
    	}
	
		if (sendb != NULL) {
			short numb = 0;
			CEC(ByteInHSCX(dumb,hscx,STAR) & 0x04);
#ifdef CONFIG_DEBUG_ISDN
			if(sendp == NULL) {
				DEBUG(hscxout)printf("\n");
				printf(KERN_WARNING "PNull! %p %p %p %p %p %d\n",sendb,sendp,bufp->m_out,bufp->m_out_run,bufp->p_out,numb);
				goto exhopp;
			}
#endif
			DEBUG(hscxout) printf(".s");
			do {
				short thisb = (uchar_t *)sendb->b_wptr-sendp;
				if(thisb > HSCX_W_FIFO_SIZE-numb) thisb = HSCX_W_FIFO_SIZE-numb;
				DEBUG(hscxout) printf(">%d ",thisb);
				for(;thisb > 0; thisb --)
					ByteOutHSCX(dumb,hscx,FIFO(numb++),*sendp++);
				while(sendp >= (uchar_t *)sendb->b_wptr) {
					sendb = sendb->b_cont;
					DEBUG(hscxout)printf("=%p ",sendb);
					if(sendb != NULL)
						sendp = (uchar_t *)sendb->b_rptr;
					else
						break;
				}
			} while((numb < HSCX_W_FIFO_SIZE) && (sendb != NULL));
#if 1
			if(ByteInHSCX(dumb,hscx,EXIR)&0x40) {
				DEBUG(info) printf(KERN_DEBUG "Underrun HSCX %d.%d\n",dumb-dumbdata,hscx);
				ByteOutHSCX(dumb,hscx,CMDR,0x01);
				bufp->m_out_run = bufp->m_out;
				bufp->p_out = bufp->m_out->b_rptr;
			} else
#endif
			if(sendb != NULL) {
				ByteOutHSCX(dumb,hscx,CMDR, 0x08); /* XTF */
				DEBUG(hscxout) printf(",");
				bufp->m_out_run = sendb;
				bufp->p_out = sendp;
			} else {
				if(bufp->mode >= M_HDLC)
					ByteOutHSCX(dumb,hscx,CMDR, 0x0A); /* XTF|XME */
				else
					ByteOutHSCX(dumb,hscx,CMDR, 0x08); /* XTF */
				DEBUG(hscxout) printf(";");
				freemsg(bufp->m_out);
				bufp->m_out = bufp->m_out_run = NULL;
				if(bufp->q_out.nblocks < 2)
					isdn2_backenable(&dumb->card,hscx);
			}
		} else {
			short i;
			switch(bufp->mode) {
			default:
				goto exhopp;
			done:
				CEC(ByteInHSCX(dumb,hscx,STAR) & 0x04);
				ByteOutHSCX(dumb,hscx,CMDR, 0x08); /* XTF */
				DEBUG(hscxout) printf(",");
				break;
			case M_TRANS_HDLC:
				for (i=0;i<HSCX_W_FIFO_SIZE; i++)
					ByteOutHSCX(dumb,hscx,FIFO(i), 0x7E);
				goto done;
			case M_TRANS_ALAW:
				for (i=0;i<HSCX_W_FIFO_SIZE; i++)
					ByteOutHSCX(dumb,hscx,FIFO(i), 0x2A);
				goto done;
			case M_TRANS_V110:
				for(i=0;i<=HSCX_W_FIFO_SIZE-10; i+=10) {
					ByteOutHSCX(dumb,hscx,FIFO(i+0), 0x00);
					ByteOutHSCX(dumb,hscx,FIFO(i+1), 0xFF);
					ByteOutHSCX(dumb,hscx,FIFO(i+2), 0xFF);
					ByteOutHSCX(dumb,hscx,FIFO(i+3), 0xFF);
					ByteOutHSCX(dumb,hscx,FIFO(i+4), 0xFF);
					ByteOutHSCX(dumb,hscx,FIFO(i+5), 0x01);
					ByteOutHSCX(dumb,hscx,FIFO(i+6), 0xFF);
					ByteOutHSCX(dumb,hscx,FIFO(i+7), 0xFF);
					ByteOutHSCX(dumb,hscx,FIFO(i+8), 0xFF);
					ByteOutHSCX(dumb,hscx,FIFO(i+9), 0xFF);
				}
				goto done;
			}
			CEC(ByteInHSCX(dumb,hscx,STAR) & 0x04);
		}
#if 0
	} while (ByteInHSCX(dumb,hscx,STAR) & 0x40);
#endif
	DEBUG(hscxout) printf("\n");
  exhopp:;
	bufp->locked = 0;
}

#ifdef __GNUC__
/* inline */
#endif
static void IRQ_HSCX_(struct _dumb * dumb, u_char hscx, Byte Reason, Byte hasEX)
{
	hdlc_buf bufp = &dumb->chan[hscx];

	DEBUG(hscx) { printf(KERN_DEBUG "%c%d.%d %02x\n",(dumb->polled<0)?'X':(dumb->polled>0)?'P':'I',dumb-dumbdata,hscx, Reason); }
	if (hasEX) {
		Byte EXIR = ByteInHSCX(dumb,hscx,EXIR);
		DEBUG(hscx) { printf(". %x", EXIR); }
		if (EXIR & 0x80) { /* XMR */
			DEBUG(info) { printf(KERN_DEBUG "Msg Repeat HSCX %d.%d\n",dumb-dumbdata,hscx); }
			CEC(ByteInHSCX(dumb,hscx,STAR) & 0x04);
			if (ByteInHSCX(dumb,hscx,STAR) & 0x40) { /* XFW */
				Reason |= 0x10; /* also set XPR bit */
			} else {
				ByteOutHSCX(dumb,hscx,CMDR, 0x01); /* XRES -- cause XPR */
			}
			/* Restart */
			if((bufp->m_out_run = bufp->m_out) != NULL) 
				bufp->p_out = bufp->m_out->b_rptr;
		}
		if (EXIR & 0x40) { /* XDU */
			CEC(ByteInHSCX(dumb,hscx,STAR) & 0x04);
			if (bufp->mode >= M_HDLC)  {
				DEBUG(info) {
					printf(KERN_DEBUG "Xmit Underrun HSCX %d.%d\n",dumb-dumbdata,hscx); }
#if NEW_XMIT
				ByteOutHSCX(dumb,hscx,CMDR, 0x01); /* XRES */
				Reason |= 0x10;
#else /* old */
				if (ByteInHSCX(dumb,hscx,STAR) & 0x40) { /* XFW */
					Reason |= 0x10; /* set XPR bit */
				} else {
					ByteOutHSCX(dumb,hscx,CMDR, 0x01); /* XRES -- cause XPR */
				}
#endif
				/* Restart */
				if ((bufp->m_out_run = bufp->m_out) != NULL)
					bufp->p_out = bufp->m_out->b_rptr;
			} else {
				Reason |= 0x10; /* set XPR bit */
			}
		}
		DEBUG(hscx)if (EXIR & 0x20) { /* PCE */
			DEBUG(info) { printf(KERN_WARNING "ISDN .PCE\n"); }
		}
		if (EXIR & 0x10) { /* RFO */
			DEBUG(info) { printf(KERN_DEBUG "Recv overflow HSCX %d.%d\n",dumb-dumbdata,hscx); }
			CEC(ByteInHSCX(dumb,hscx,STAR) & 0x04);
			if(Reason & 0xC0) {
				ByteOutHSCX(dumb,hscx,CMDR, 0xC0); /* RMC|RHR */
				Reason &=~ 0xC0;
			} else
				ByteOutHSCX(dumb,hscx,CMDR, 0x80); /* RMC */
			if(bufp->m_in != NULL) {
				freemsg(bufp->m_in);
				bufp->m_in = bufp->m_in_run = NULL;
			}
		}
		DEBUG(hscx)if (EXIR & 0x08) { /* CSC */
			DEBUG(info) { printf(KERN_WARNING "ISDN .CSC\n"); }
		}
		DEBUG(hscx)if (EXIR & 0x04) { /* RFS */
			DEBUG(info) { printf(KERN_WARNING "ISDN .RFS\n"); }
		}
		/* 0x02 and 0x01 are empty */
	}

	if (Reason & 0x80) { /* RME */
		Byte RSTA = ByteInHSCX(dumb,hscx,RSTA);
		if ((RSTA & 0xF0) == 0xA0) {
			uchar_t *recvp;
			mblk_t *recvb;
			short xblen = ((ByteInHSCX(dumb,hscx,RBCH) & 0x0F) << 8) + ByteInHSCX(dumb,hscx,RBCL);
			short blen = ((xblen-1) & (HSCX_R_FIFO_SIZE-1));
			if(0)DEBUG(hscx)printf(":%d ",xblen);
			if(blen == 0) {
				if((recvb = bufp->m_in_run) != NULL) {
					mblk_t *msg = bufp->m_in;
					bufp->m_in = bufp->m_in_run = NULL;
					/* recvb->b_wptr --; */
					if(!isdn2_canrecv(&dumb->card,hscx))
						freemsg(msg);
					else if(isdn2_recv(&dumb->card,hscx,msg) != 0)
						freemsg(msg);
				}
                CEC(ByteInHSCX(dumb,hscx,STAR) & 0x04);
                ByteOutHSCX(dumb,hscx,CMDR, 0x80); /* RMC */
			} else {
				if ((recvb = bufp->m_in_run) != NULL) {
					if ((recvp = (uchar_t *)recvb->b_wptr) + blen > (uchar_t *)recvb->b_datap->db_lim)
						recvb = NULL;
				}
				if (recvb == NULL) {
					recvb = allocb(blen,BPRI_MED);
					if(recvb != NULL) {
						recvp = recvb->b_wptr;
						if(bufp->m_in_run == NULL) {
							bufp->m_in = recvb;
							bufp->m_in_run = recvb;
						} else {
							linkb(bufp->m_in_run,recvb);
							bufp->m_in_run = recvb;
						}
					}
				}
				if(recvb != NULL) {
					mblk_t *msg = bufp->m_in;
					bufp->m_in = bufp->m_in_run = NULL;
					if(msg == NULL) {
						printf(" :BufAllocErr%d:",hscx);
					} else {
						short i;
						for(i=0;i < blen; i++)
							*recvp++ = ByteInHSCX(dumb,hscx,FIFO(i));
						recvb->b_wptr = recvp;
						CEC(ByteInHSCX(dumb,hscx,STAR) & 0x04);
						ByteOutHSCX(dumb,hscx,CMDR, 0x80); /* RMC */
						if(!isdn2_canrecv(&dumb->card,hscx))
							freemsg(msg);
						else if(isdn2_recv(&dumb->card,hscx,msg) != 0)
							freemsg(msg);
					}
				} else {
					CEC(ByteInHSCX(dumb,hscx,STAR) & 0x04);
					ByteOutHSCX(dumb,hscx,CMDR, 0xC0); /* RMC|RHR */
				}
			}
		} else {
			DEBUG(info) { printf(KERN_DEBUG "Recv abort (%02x) HSCX %d.%d\n", RSTA,dumb-dumbdata,hscx); }
			if(bufp->m_in != NULL) {
				freemsg(bufp->m_in);
				bufp->m_in = bufp->m_in_run = NULL;
			}
			CEC(ByteInHSCX(dumb,hscx,STAR) & 0x04);
			ByteOutHSCX(dumb,hscx,CMDR, 0xC0); /* RMC|RHR */
		}
	} else if (Reason & 0x40) { /* RPF */
		uchar_t *recvp;
		mblk_t *recvb;

        if ((recvb = bufp->m_in_run) != NULL) {
            if ((recvp = (uchar_t *)recvb->b_wptr) + HSCX_R_FIFO_SIZE > (uchar_t *)recvb->b_datap->db_lim)
                recvb = NULL;
        }
		if(0)DEBUG(hscx)printf(":");
        if (recvb == NULL) {
            recvb = allocb(HSCX_R_FIFO_SIZE*4,BPRI_MED);
            if(recvb != NULL) {
                recvp = recvb->b_wptr;
            	if(bufp->m_in_run == NULL) {
                	bufp->m_in = bufp->m_in_run = recvb;
            	} else {
                	linkb(bufp->m_in_run, recvb);
                	bufp->m_in_run = recvb;
				}
            }
        }
        if(recvb == NULL) {
			CEC(ByteInHSCX(dumb,hscx,STAR) & 0x04);
			if(bufp->mode >= M_HDLC) {
				if(bufp->m_in != NULL) {
					freemsg(bufp->m_in);
					bufp->m_in = bufp->m_in_run = NULL;
				}
				ByteOutHSCX(dumb,hscx,CMDR, 0x40); /* RRESet */
			} else {
				ByteOutHSCX(dumb,hscx,CMDR, 0x80); /* RMC */
			}
		} else {
			short i;
			for (i=0; i < HSCX_R_FIFO_SIZE; i++)
				*recvp++ = ByteInHSCX(dumb,hscx,FIFO(i));
			recvb->b_wptr = recvp;
			CEC(ByteInHSCX(dumb,hscx,STAR) & 0x04);
			ByteOutHSCX(dumb,hscx,CMDR, 0x80); /* RMC */
			if(bufp->mode < M_HDLC && ++bufp->nblk > bufp->maxblk) {
				if(isdn2_canrecv(&dumb->card,hscx)) {
					mblk_t *msg = bufp->m_in;
					bufp->m_in = bufp->m_in_run = NULL;
					if(isdn2_recv(&dumb->card,hscx,msg) != 0)
						freemsg(msg);
					bufp->nblk = 0;
				} else if(bufp->nblk > 3*bufp->maxblk) {
					mblk_t *msg = bufp->m_in;
					bufp->m_in = bufp->m_in_run = NULL;
					if(msg != NULL)
						freemsg(msg);
					bufp->nblk = 0;
				}
			}
		}
	}
	
	DEBUG(hscx)if (Reason & 0x20) { /* RSC */
		DEBUG(info) { printf(KERN_WARNING "ISDN .RSC\n"); }
	}
	if ((Reason & 0x10) || (ByteInHSCX(dumb,hscx,STAR) & 0x40)) { /* XPR */
		HSCX_kick(dumb,hscx);
	}
	DEBUG(hscx)if (Reason & 0x08) { /* TIN */
		DEBUG(info) { printf(KERN_WARNING "ISDN .TIN\n"); }
	}
}

#ifdef __GNUC__
/* inline */
#endif
static void IRQ_ISAC(struct _dumb * dumb)
{
	Byte Reason;

  while((Reason = ByteInISAC(dumb,ISTA))) {
	DEBUG(isac) { printf(KERN_DEBUG "%c%d %02x\n",(dumb->polled<0)?'X':(dumb->polled>0)?'P':'I',dumb-dumbdata, Reason); }
	if (Reason & 0x04) { /* CISQ */
		Byte CIR = ByteInISAC(dumb,CIR0);
		
		if (CIR & 0x80) { /* SQC */
			DEBUG(info) { printf(KERN_WARNING "ISDN .SQC %x\n",ByteInISAC(dumb,SQRR)); }
		}
		if (CIR & 0x03) {
			CIR = ((CIR >> 2) & 0x0F);
			DEBUG(info) printf(KERN_DEBUG "ISDN CIR %01x",CIR);
			if (dumb->polled >= 0) {
				if ((CIR == 0x0C) || (CIR == 0x0D)) {
					DEBUG(info) printf(" up");
					isdn2_new_state(&dumb->card,1);
					dumb->circ = 0;
				} else if ((CIR == 0x00) || (CIR == 0x0F) || (CIR == 0x07))  {
					if ((CIR == 0x07) && (dumb->circ & 1)) {
						dumb->circ++;
					} else {
						dumb->circ = 0;
					}
					DEBUG(info) printf(" down count %d",dumb->circ);
					isdn2_new_state(&dumb->card,0);
				} else if (CIR == 0x04) {
					DEBUG(info) printf(" jitter count %d",dumb->circ);
					if(dumb->circ > 3)
						isdn2_new_state(&dumb->card,2);
					dumb->circ++;
				}
			}
			DEBUG(info) printf(KERN_DEBUG "\n");
#if 0
			ByteOutISAC(dumb,CMDR,0x41);
			Reason &=~ 0xC0;
#endif
		}
	}
	if (Reason & 0x01) {
		Byte EXIR = ByteInISAC(dumb,EXIR);
		DEBUG(isac) { printf(". %x", EXIR); }
		if (EXIR & 0x80) { /* XMR */
			DEBUG(info) { printf(KERN_DEBUG "MsgRepeat ISAC %d\n",dumb-dumbdata); }
			CEC(ByteInISAC(dumb,STAR) & 0x04);
			if (ByteInISAC(dumb,STAR) & 0x40) { /* XFW */
				Reason |= 0x10; /* also set XPR bit */
			} else {
				ByteOutISAC(dumb,CMDR, 0x01); /* XRES -- cause XPR */
			}
			/* Restart */
			if((dumb->chan[0].m_out_run = dumb->chan[0].m_out) != NULL) 
				dumb->chan[0].p_out = dumb->chan[0].m_out->b_rptr;
		}
		if (EXIR & 0x40) { /* XDU */
			CEC(ByteInISAC(dumb,STAR) & 0x04);
			if (dumb->chan[0].mode >= M_HDLC)  {
				DEBUG(info) { printf(KERN_DEBUG "Xmit Underrun ISAC %d\n",dumb-dumbdata); }
#if NEW_XMIT
				ByteOutISAC(dumb,CMDR, 0x01); /* XRES */
				Reason |= 0x10;
#else /* old */
				if (ByteInISAC(dumb,STAR) & 0x40) { /* XFW */
					Reason |= 0x10; /* set XPR bit */
				} else {
					ByteOutISAC(dumb,CMDR, 0x01); /* XRES -- cause XPR */
				}
#endif
				/* Restart */
				if ((dumb->chan[0].m_out_run = dumb->chan[0].m_out) != NULL)
					dumb->chan[0].p_out = dumb->chan[0].m_out->b_rptr;
			} else {
				Reason |= 0x10; /* set XPR bit */
			}
		}
		DEBUG(isac)if (EXIR & 0x20) { /* PCE */
			DEBUG(info) { printf(KERN_WARNING "ISDN .PCE\n"); }
		}
		if (EXIR & 0x10) { /* RFO */
			DEBUG(info) { printf(KERN_DEBUG "Recv overflow ISAC %d (%02x)\n",dumb-dumbdata, EXIR); }
			CEC(ByteInISAC(dumb,STAR) & 0x04);
			if(Reason & 0xC0) {
				ByteOutISAC(dumb,CMDR, 0xC0); /* RMC|RHR */
				Reason &=~ 0xC0;
			} else
				ByteOutISAC(dumb,CMDR, 0x80); /* RMC */
			if(dumb->chan[0].m_in != NULL) {
				freemsg(dumb->chan[0].m_in);
				dumb->chan[0].m_in = dumb->chan[0].m_in_run = NULL;
			}
		}
		DEBUG(isac)if (EXIR & 0x08) { /* CSC */
			DEBUG(info) { printf(KERN_WARNING "ISDN .CSC\n"); }
		}
		DEBUG(isac)if (EXIR & 0x04) { /* RFS */
			DEBUG(info) { printf(KERN_WARNING "ISDN .RFS\n"); }
		}
		/* 0x02 and 0x01 are empty */
	}

	if (Reason & 0x80) { /* RME */
		Byte RSTA = ByteInISAC(dumb,RSTA);
		if ((RSTA & 0xF0) == 0xA0) {
			uchar_t *recvp;
			mblk_t *recvb;
			short xblen;
			short blen;
			xblen = ByteInISAC(dumb,RBCL);
			blen = (xblen & (ISAC_R_FIFO_SIZE-1));
			xblen += (ByteInISAC(dumb,RBCH) & 0x0F) << 8;
			if(blen == 1) {
				if((recvb = dumb->chan[0].m_in_run) != NULL) {
					mblk_t *msg = dumb->chan[0].m_in;
					dumb->chan[0].m_in = dumb->chan[0].m_in_run = NULL;
					recvb->b_wptr --;
					if(!isdn2_canrecv(&dumb->card,0))
						freemsg(msg);
					else if(isdn2_recv(&dumb->card,0,msg) != 0)
						freemsg(msg);
				}
			} else {
				if(blen == 0) 
					blen = ISAC_R_FIFO_SIZE;
				DEBUG(isac) printf(KERN_DEBUG ".R %d\n",xblen);
				if ((recvb = dumb->chan[0].m_in_run) != NULL) {
					DEBUG(isac)printf("a%d ",dsize(dumb->chan[0].m_in));
					if ((recvp = (uchar_t *)recvb->b_wptr) + blen-1 > (uchar_t *)recvb->b_datap->db_lim)
						recvb = NULL;
				}
				if (recvb == NULL) {
					recvb = allocb(ISAC_R_FIFO_SIZE,BPRI_MED);
					if(recvb != NULL) {
						recvp = recvb->b_wptr;
						if(dumb->chan[0].m_in_run == NULL) {
							dumb->chan[0].m_in = dumb->chan[0].m_in_run = recvb;
						} else {
							linkb(dumb->chan[0].m_in_run,recvb);
							dumb->chan[0].m_in_run = recvb;
						}
					}
				}
				if(recvb != NULL) {
					short i;
					mblk_t *msg = dumb->chan[0].m_in;
					dumb->chan[0].m_in = dumb->chan[0].m_in_run = NULL;
					for(i=0;i < blen; i++)
						*recvp++ = ByteInISAC(dumb,FIFO(i));
					recvb->b_wptr = recvp;
					CEC(ByteInISAC(dumb,STAR) & 0x04);
					ByteOutISAC(dumb,CMDR, 0x80); /* RMC */
					if(!isdn2_canrecv(&dumb->card,0))
						freemsg(msg);
					else if(isdn2_recv(&dumb->card,0,msg) != 0)
						freemsg(msg);
				} else {
					CEC(ByteInISAC(dumb,STAR) & 0x04);
					ByteOutISAC(dumb,CMDR, 0xC0); /* RMC|RHR */
					DEBUG(isac) { printf("NoBit ISAC\n"); }
				}
			}
		} else {
			DEBUG(info) { printf(KERN_DEBUG "Recv abort (%x) %d\n",RSTA,dumb-dumbdata); }
			if(dumb->chan[0].m_in != NULL) {
				freemsg(dumb->chan[0].m_in);
				dumb->chan[0].m_in = dumb->chan[0].m_in_run = NULL;
			}
			CEC(ByteInISAC(dumb,STAR) & 0x04);
			ByteOutISAC(dumb,CMDR, 0xC0); /* RMC|RHR */
		}
	} else if (Reason & 0x40) { /* RPF */
		uchar_t *recvp;
		mblk_t *recvb;

        if ((recvb = dumb->chan[0].m_in_run) != NULL) {
            if ((recvp = (uchar_t *)recvb->b_wptr) + ISAC_R_FIFO_SIZE > (uchar_t *)recvb->b_datap->db_lim)
                recvb = NULL;
        }
        if (recvb == NULL) {
            recvb = allocb(ISAC_R_FIFO_SIZE*2,BPRI_MED);
            if(recvb != NULL) {
                recvp = recvb->b_wptr;
            	if(dumb->chan[0].m_in_run == NULL) {
                	dumb->chan[0].m_in = dumb->chan[0].m_in_run = recvb;
            	} else {
                	linkb(dumb->chan[0].m_in_run,recvb);
                	dumb->chan[0].m_in_run = recvb;
				}
            }
        }
        if(recvb == NULL) {
			CEC(ByteInISAC(dumb,STAR) & 0x04);
			if(dumb->chan[0].mode >= M_HDLC) {
				if(dumb->chan[0].m_in != NULL) {
					freemsg(dumb->chan[0].m_in);
					dumb->chan[0].m_in = dumb->chan[0].m_in_run = NULL;
				}
				ByteOutISAC(dumb,CMDR, 0x40); /* RRESet */
			} else
				ByteOutISAC(dumb,CMDR, 0x80); /* RMC */
		} else {
			short i;
			DEBUG(isac) printf(">%p",recvp);
			for(i=0;i < ISAC_R_FIFO_SIZE; i++)
				*recvp++ = ByteInISAC(dumb,FIFO(i));
			recvb->b_wptr = recvp;
			CEC(ByteInISAC(dumb,STAR) & 0x04);
			ByteOutISAC(dumb,CMDR, 0x80); /* RMC */
		}
	}
	if (Reason & 0x20) { /* RSC */
		DEBUG(info) { printf(KERN_WARNING "ISDN .RSC\n"); }
	}
	if (Reason & 0x08) { /* TIN */
		DEBUG(info) { printf(KERN_WARNING "ISDN .TIN\n"); }
#if 0
		ByteOutISAC(dumb,TIMR,0x11);
		ByteOutISAC(dumb,CMDR,0x10); /* start timer */
#endif
	}

	if ((Reason & 0x10) || (ByteInISAC(dumb,STAR) & 0x40)) { /* XPR */
		ISAC_kick(dumb);
	}
  }
}


#ifdef __GNUC__
/* inline */
#endif
static void IRQ_HSCX(struct _dumb * dumb)
{
	int i;
	for(i=1;i <= dumb->numHSCX; i += 2) {
		Byte Reason = ByteInHSCX(dumb,i+1,ISTA);
	
		if (Reason & 0x06)
			IRQ_HSCX_(dumb,i, ByteInHSCX(dumb,i,ISTA), Reason & 0x02);
		if (Reason & 0xF9) 
			IRQ_HSCX_(dumb,i+1, Reason, Reason & 0x01);
	}
}

#ifdef linux

static void
dumbintr(int irq, struct pt_regs *regs)
{
	struct _dumb *dumb;
	
	for(dumb=&dumbdata[dumb_num-1];dumb>=dumbdata;--dumb) {
		if(dumb->irq == irq) {
			dumb->polled --;
			IRQ_HSCX(dumb);
			IRQ_ISAC(dumb);
			PostIRQ(dumb);
			dumb->polled ++;
			toggle_off(dumb);
		}
	}
	for(dumb=&dumbdata[dumb_num-1];dumb>=dumbdata;--dumb) {
		if(dumb->irq == irq) {
			toggle_on(dumb);
		}
	}
}


#else
void NAME(CARDTYPE,intr)(int x)
{
	for(dumb=&dumbdata[dumb_num-1];dumb>=dumbdata;--dumb) {
		if(dumb->irq == x) {
			dumb->polled --;
			IRQ_HSCX(dumb);
			IRQ_ISAC(dumb);
			dumb->polled ++;
			toggle_off(dumb);
			toggle_on(dumb);
			return;
		}
	}
}
#endif

#ifdef linux
void NAME(CARDTYPE,poll)(struct _dumb *dumb)
#else
void NAME(CARDTYPE,poll)(void *nix)
#endif
{
#ifndef linux
	struct _dumb *dumb;
	for(i=dumb_num-1;i>=0;--i)
#endif
        {
		unsigned long ms;
		int j;
#ifndef linux
		dumb = &dumbdata[i];
#endif
		ms = SetSPL(dumb->ipl);
		IRQ_HSCX(dumb);
		IRQ_ISAC(dumb);

		if (dumb->chan[0].q_out.nblocks != 0)
			ISAC_kick(dumb);
		for(j=1;j <= dumb->numHSCX; j++) {
			if (dumb->chan[j].q_out.nblocks != 0)
				HSCX_kick(dumb,j);
		}
		toggle_off(dumb);
		toggle_on(dumb);
		splx(ms);
#if 0 /* def linux */
		if(dumb->irq != 0)
			unblock_irq(dumb->irq);
#endif
	}
}


#ifdef linux
static void dumbtimer(struct _dumb *dumb)
{
	NAME(CARDTYPE,poll)(dumb);
#if 0
	if(dumb->countme++ > 10) {
		dumb->countme = 0;
		printf(" -%d:%02x %02x %02x- ",dumb-dumbdata,ByteInISAC(dumb,STAR),ByteInISAC(dumb,ISTA),ByteInISAC(dumb,CIR0));
	}
#endif
#ifdef NEW_TIMEOUT
	dumb->timer =
#endif
		timeout((void *)dumbtimer,dumb,(dumb->irq == 0) ? HZ/DUMBTIME+1 : HZ/2);
}
#endif

void NAME(CARDTYPE,init)(void)
{
	extern caddr_t sptalloc();
	struct _dumb *dumb;
	int err;
	
	for(dumb=&dumbdata[dumb_num-1];dumb>=dumbdata;--dumb) {
		dumb->card.nr_chans = dumb->numHSCX * 2;
		dumb->card.ctl = dumb;
		dumb->card.chan = NULL;
		dumb->card.modes = (1<<M_HDLC)| (1<<M_HDLC_7H)| (1<<M_HDLC_7L)| (1<<M_TRANSPARENT)| (1<<M_TRANS_ALAW)| (1<<M_TRANS_V110)| (1<<M_TRANS_HDLC);
		dumb->card.ch_mode = dumb_mode;
		dumb->card.ch_prot = dumb_prot;
		dumb->card.send = dumb_data;
		dumb->card.flush = dumb_flush;
		dumb->card.cansend = dumb_candata;
		dumb->polled = -1;

		printf("ISDN: " STRING(CARDTYPE) " at mem %lx io 0x%x irq %d: ",dumb->memaddr,dumb->ioaddr,dumb->irq);

		if(!Init(dumb)) {
			printf("Card not initializable.\n");
			continue;
		}
		InitHSCX(dumb);
		InitISAC(dumb);
		InitHSCX(dumb);

		if(!ISACpresent(dumb)) {
			printf("Card not responding.\n");
			continue;
		}
#ifndef M_UNIX
#ifdef linux
		if((dumb->irq != 0) && request_irq(dumb->irq,dumbintr,SA_INTERRUPT,"ISDN")) {
			printf("IRQ not available.\n");
			continue;
		}
#endif
		NAME(CARDTYPE,poll)(dumb);
		if((err = isdn2_register(&dumb->card, dumb->ID)) != 0) {
			printf("not installed (ISDN_2), err %d\n",err);
			continue;
		}

		dumb->polled = 1;
#ifdef linux
		if(dumb->irq == 0) {
			printf("polling; ");
		}
		dumbtimer(dumb);
#endif
		printf("installed at ");
		if(dumb->memaddr != 0) 
			printf("mem 0x%lx ",dumb->memaddr);
		if(dumb->ioaddr != 0) 
			printf("io 0x%x ",dumb->ioaddr);
		if(dumb->irq != 0) 
			printf("irq %d.\n",dumb->irq);
		else
			printf("polled.\n");
#endif
	}
}


#ifdef M_UNIX
void NAME(CARDTYPE,start)(void)
{
	extern caddr_t sptalloc();
	short i;
	struct _dumb *dumb;
	int err;
	
	for(dumb=&dumbdata[dumb_num-1];dumb>=dumbdata;--dumb) {
		if((err = isdn2_register(&dumb->card, dumb->ID)) == 0)
			printcfg("dumb",0,dumb->ioaddr,dumb->irq,i, "unit %d",i);
		else
			printf("ISDN Driver %d/%d/%x not installed, err %d.\n",i,dumb->irq,dumb->ioaddr,err);
	}
}
#endif


void NAME(CARDTYPE,halt)(void)
{
	struct _dumb *dumb;

	for(dumb=&dumbdata[dumb_num-1];dumb>=dumbdata;--dumb) {
		int j;
		unsigned long ms = SetSPL(dumb->ipl);
		if(dumb->polled > 0) {
#ifdef NEW_TIMEOUT
			untimeout(dumb->timer);
#else
			untimeout(dumbtimer,dumb);
#endif
			ByteOutISAC(dumb,MASK,0xFF);
			for(j=1; j<=dumb->numHSCX;j++)
				ByteOutHSCX(dumb,j,MASK,0xFF);
			if(dumb->irq > 0)
				free_irq(dumb->irq);
			isdn2_unregister(&dumb->card);
		}
		splx(ms);
	}	
}


#ifdef MODULE
static int do_init_module(void)
{
	NAME(CARDTYPE,init)();
	return 0;
}

static int do_exit_module(void)
{
	NAME(CARDTYPE,halt)();
	return 0;
}
#endif
