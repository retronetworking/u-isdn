#if defined(_ncp16_)
#define WIDE
#endif

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
#include "isdn_proto.h"
#include "stream.h"
#include "streamlib.h"
#include "kernel.h"
#ifdef SCO
#include <sys/immu.h>
#endif
#include <stddef.h>
#include "loader.h"

#ifdef linux
#include <asm/io.h>
#include <linux/ptrace.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/malloc.h>
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

#if defined(_teles3_) || defined(_creatixpnp_)
#define FIFO(x) Fifo
#else
#define FIFO(x) fifo[0] /* forget the address -- WARNING: side effects of x get lost */
#endif

#include "shell.h"

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
/* 1C */	volatile Byte __f5,__f6,Fifo,__f7;
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
/* 1C */	volatile Byte __f5,__f6,Fifo,__f7;
		} __w;
	} __rw;
} *__isac;

#ifdef WIDE
typedef struct _hscx {
	union {
		struct {
			volatile Byte fifo[HSCX_R_FIFO_SIZE];
/* 00 */	volatile Byte STAR,RSTA,MODE,TIMR;
/* 04 */	volatile Byte XAD1,XAD2,__f1,_f2;
/* 08 */	volatile Byte RAL1,RHCR,RBCL,RBCH;
/* 0C */	volatile Byte CCR0,CCR1,CCR2,CCR3;
/* 10 */	volatile Byte __f3,__f4,__f5,__f6;
/* 14 */	volatile Byte VSTR,__f7,PRE ,__f8;
/* 18 */	volatile Byte GISR,IPC ,ISR0,ISR1;
/* 1C */	volatile Byte PVR ,PIS ,Fifo,__f9; /* 0x1E is PCR, but ... */
		} __r;
		struct {
			volatile Byte fifo[HSCX_W_FIFO_SIZE];
/* 00 */	volatile Byte CMDR,__f0,MODE,TIMR;
/* 04 */	volatile Byte XAD1,XAD2,RAH1,RAH2;
/* 08 */	volatile Byte RAL1,RAL2,XBCL,XBCH;
/* 0C */	volatile Byte CCR0,CCR1,CCR2,CCR3;
/* 10 */	volatile Byte TSAX,TSAR,XCCR,RCCR;
/* 14 */	volatile Byte BGR ,RLCR,PRE ,__f8;
/* 18 */	volatile Byte IVA ,IPC ,IMR0,IMR1;
/* 1C */	volatile Byte PVR ,PIM ,Fifo,__f9; /* 0x1E is PCR, but ... */
		} __w;
	} __rw;
} *__hscx;
#else
typedef struct _hscx {
	union {
		struct {
			volatile Byte fifo[HSCX_R_FIFO_SIZE];
/* 00 */	volatile Byte ISTA,STAR,MODE,TIMR;
/* 04 */	volatile Byte EXIR,RBCL,_f1,RSTA;
/* 08 */	volatile Byte RAL1,RHCR,__f2,__f3;
/* 0C */	volatile Byte CCR2,RBCH,VSTR,CCR1;
/* 10 */	volatile Byte __f4,__f5,__f6,__f7;
/* 14 */	volatile Byte __f8,__f9,__fa,__fb;
/* 18 */	volatile Byte __fc,__fd,__fe,__ff;
/* 1C */	volatile Byte __fA,__fB,Fifo,__fC;
		} __r;
		struct {
			volatile Byte fifo[HSCX_W_FIFO_SIZE];
/* 00 */	volatile Byte MASK,CMDR,MODE,TIMR;
/* 04 */	volatile Byte XAD1,XAD2,RAH1,RAH2;
/* 08 */	volatile Byte RAL1,RAL2,XBCL,BGR;
/* 0C */	volatile Byte CCR2,XBCH,RLCR,CCR1;
/* 10 */	volatile Byte TSAX,TSAR,XCCR,RCCR;
/* 14 */	volatile Byte __f8,__f9,__fa,__fb;
/* 18 */	volatile Byte __fc,__fd,__fe,__ff;
/* 1C */	volatile Byte __fA,__fB,Fifo,__fC;
		} __w;
	} __rw;
} *__hscx;
#endif

#define ByteInISAC(_dumb,_what) InISAC((_dumb),offsetof(struct _isac,r._what))
#define ByteOutISAC(_dumb,_what,_data) OutISAC((_dumb),offsetof(struct _isac,w._what), (_data))

#define ByteInHSCX(_dumb,_hcr,_what) InHSCX((_dumb),(_hcr),offsetof(struct _hscx,r._what))
#define ByteOutHSCX(_dumb,_hcr,_what,_data) OutHSCX((_dumb),(_hcr),offsetof(struct _hscx,w._what),(_data))

#ifdef __linux__
#define SetSPL(x) spl(1)
#else
#define SetSPL(x) spl((x))
#endif

#ifdef linux
#ifdef _avm_
#include "avm_io.c"
#endif

#ifdef _bsc_
#include "bsc_io.c"
#endif

#ifdef _ncp16_
#include "ncp16_io.c"
#endif

#ifdef _ncp_
#include "ncp_io.c"
#endif

#ifdef _teles_
#include "teles_io.c"
#endif

#ifdef _teles3_
#include "teles3_io.c"
#endif

#ifdef _creatixpnp_
#include "creatixpnp_io.c"
#endif


#define DUMBTIME 300 /* poll: times per second */

void NAME(REALNAME,poll)(struct _dumb *dumb);
#else
void NAME(REALNAME,poll)(void *);
#endif



static struct _dumb *dumblist = NULL;



static void
toggle_off(struct _dumb * dumb)
{
	int i;
	ByteOutISAC(dumb,MASK,0xFF);
	for(i=1;i <= dumb->numHSCX; i++) {
#ifdef WIDE
		ByteOutHSCX(dumb,i,IMR0,0xFF);
		ByteOutHSCX(dumb,i,IMR1,0xFF);
#else
		ByteOutHSCX(dumb,i,MASK,0xFF);
#endif
	}
	
}
static void
toggle_on(struct _dumb * dumb)
{
	int i;
	for(i=1;i <= dumb->numHSCX; i++) {
#ifdef WIDE
		ByteOutHSCX(dumb,i,IMR0,0x00);
		ByteOutHSCX(dumb,i,IMR1,0x00);
#else
		ByteOutHSCX(dumb,i,MASK,0x00);
#endif
	}
	ByteOutISAC(dumb,MASK,0x00);
}

static void
fail_up(struct _dumb * dumb)
{
	if(dumb->do_uptimer) {
		dumb->do_uptimer = 0;
		DEBUG(info) printf("CARD NOT UP, FAIL\n");
		isdn2_new_state(&dumb->card,2);
	}
}

static int
mode (struct _isdn1_card * card, short channel, char mode, char listen)
{
	struct _dumb * dumb = (struct _dumb *) card;
	unsigned long ms = SetSPL(dumb->ipl);
	int err = 0;
	char do_uptimer = dumb->do_uptimer;
	static int modeloop = -1;

	modeloop++;
	switch(channel) {
	case 0:
		DEBUG(info) printf("%sISDN ISAC %s<%d>%s\n",KERN_INFO ,mode?(mode==1?"standby":"up"):"down",mode,listen?" listen":"");
		if(dumb->do_uptimer) {
			dumb->do_uptimer = 0;
#ifdef NEW_TIMEOUT
			untimeout(dumb->uptimer);
#else
			untimeout(fail_up,dumb);
#endif
		}
		if((dumb->chan[0].mode <= M_STANDBY) && (mode > M_STANDBY)) 
			do_uptimer = 1;
		else if(mode <= M_STANDBY)
			do_uptimer = 0;
		if(mode == M_ON)
			mode = M_HDLC;

		switch(mode) {
		case M_OFF:
			DEBUG(info) printk("%sISDN CIX1 0x3F\n",KERN_DEBUG );
			ByteOutISAC(dumb,CIX0,0x3F);
			if(!modeloop) isdn2_new_state(&dumb->card,0);
			dumb->chan[0].mode = mode;
			break;
		case M_STANDBY:
			switch((ByteInISAC(dumb,CIR0)>>2)&0x0F) {
			case 0x00:
			case 0x0F:
				DEBUG(info) printk("%sISDN CIX2 0x00\n",KERN_DEBUG );
				ByteOutISAC(dumb,CIX0,0x00);
				break;
			case 0x06:
				DEBUG(info) printk("%sISDN CIX3 0x3F\n",KERN_DEBUG );
				ByteOutISAC(dumb,CIX0,0x07);
				break;
			case 0x0C:
			case 0x0D:
				DEBUG(info) printk("%sISDN noCIX2 CIR %02x\n",KERN_DEBUG,ByteInISAC(dumb,CIR0));
				if(!modeloop) isdn2_new_state(&dumb->card,1);
				do_uptimer = 0;
				break;
			default:
				DEBUG(info) printk("%sISDN noCIX0 CIR %02x\n",KERN_DEBUG,ByteInISAC(dumb,CIR0));
				break;

			}
			ByteOutISAC(dumb,MODE,0xC9);
			ByteOutISAC(dumb,MASK,0x00);
			dumb->chan[0].mode = mode;
			dumb->chan[0].listen = 1;
			break;
		case M_HDLC:
			ByteOutISAC(dumb,MODE,0xC9);
			ByteOutISAC(dumb,MASK,0x00);
			switch((ByteInISAC(dumb,CIR0)>>2)&0x0F) {
			case 0x00:
			case 0x0F:
				if(dumb->chan[0].mode != M_HDLC) {
					DEBUG(info) printk("%sISDN CIX4 0x3F\n",KERN_DEBUG );
					ByteOutISAC(dumb,CIX0,0x00);
				}
				break;
			case 0x06:
				if(dumb->chan[0].mode != M_HDLC) {
					DEBUG(info) printk("%sISDN CIX5 0x3F\n",KERN_DEBUG );
					ByteOutISAC(dumb,CIX0,0x3F);
				}
				break;
			case 0x07:
				if(dumb->chan[0].mode != M_HDLC) {
					DEBUG(info) printk("%sISDN CIX6 0x27\n",KERN_DEBUG );
					ByteOutISAC(dumb,CIX0,0x27);
				}
				break;
			case 0x0C:
			case 0x0D:
				DEBUG(info) printk("%sISDN noCIX1 CIR %02x\n",KERN_DEBUG,ByteInISAC(dumb,CIR0));
				if(!modeloop) isdn2_new_state(&dumb->card,1);
				do_uptimer = 0;
				break;
			default:
				DEBUG(info) printk("%sISDN CIX9 0x07\n",KERN_DEBUG );
				ByteOutISAC(dumb,CIX0,0x07);
				break;
			}
			dumb->chan[0].mode = mode;
			dumb->chan[0].listen = 0;
			break;
		}
		if(mode == M_OFF) {
			int j;
			for(j=1;j <= dumb->numHSCX;j++)
				HSCX_mode(dumb,j,M_OFF,0);
		} else if (do_uptimer && !dumb->do_uptimer && !modeloop) {
#ifdef NEW_TIMEOUT
			dumb->uptimer =
#endif
				timeout((void *)fail_up,dumb,10*HZ);
			dumb->do_uptimer = 1;
		}
		break;
	default:
		if(channel > 0 && channel <= dumb->numHSCX) {
			DEBUG(info) printf("%sISDN HSCX%d %s<%d>%s\n",KERN_INFO ,channel,mode?"up":"down",mode,(listen&2)?"setup":((listen&1)?" listen":""));
			if(listen & 2) {
				err = HSCX_mode(dumb,channel,M_OFF,0);
			} else {
				if((mode > M_STANDBY) && (dumb->chan[channel].mode <= M_STANDBY)) {
					modeloop--;
					return -EAGAIN;
				}
				err = HSCX_mode(dumb,channel,(mode != M_ON) ? mode : dumb->chan[channel].mode,listen);
				if (err < 0) {
					printf("%sISDN err %d %d\n",KERN_WARNING ,channel, err);
					splx(ms);
					modeloop--;
					return err;
				}

				dumb->chan[channel].nblk = 0;
				dumb->chan[channel].maxblk = 10;
			}
			break;
		} else {
			printf("%sISDN badChan %d\n",KERN_WARNING ,channel);
			splx(ms);
			modeloop--;
			return -EINVAL;
		}
	}
	modeloop--;
	NAME(REALNAME,poll)(dumb);
	splx(ms);
	return err;
}

static int
prot (struct _isdn1_card * card, short channel, mblk_t * mp, int flags)
{
	struct _dumb * dumb = (struct _dumb *)card;
	streamchar *origmp = mp->b_rptr;
	ushort_t id;
	int err = 0;

	DEBUG(info)printf("%sDumbProt chan %d flags 0%o\n",KERN_DEBUG,channel,flags);

#define isspace(x) (((x)==' ') || ((x)=='\t'))
	if(flags & CHP_MODLIST) {
		if(!(flags & PUSH_AFTER)) {
			streamchar *s1,*s2,sx;

			s1 = mp->b_rptr;
			while(s1 < mp->b_wptr && isspace(*s1))
				s1++;
			s2 = s1;
			while(s2 < mp->b_wptr && !isspace(*s2))
				s2++;
			sx = *s2; *s2 = '\0';
			if(!strcmp(s1,"trans"))
				dumb->chan[channel].mode = M_TRANSPARENT;
			else if(!strcmp(s1,"transalaw"))
				dumb->chan[channel].mode = M_TRANS_ALAW;
			else if(!strcmp(s1,"transv110"))
				dumb->chan[channel].mode = M_TRANS_V110;
			else if(!strcmp(s1,"transframe"))
				dumb->chan[channel].mode = M_TRANS_HDLC;
			else if(!strcmp(s1,"frame"))
				dumb->chan[channel].mode = M_HDLC;
			else if(!strcmp(s1,"framelow"))
				dumb->chan[channel].mode = M_HDLC_7L;
			else if(!strcmp(s1,"framehigh"))
				dumb->chan[channel].mode = M_HDLC_7H;
			else if(!strcmp(s1,"framezero"))
				dumb->chan[channel].mode = M_HDLC_N0;
			else if(!strcmp(s1,"frame16"))
				dumb->chan[channel].mode = M_HDLC_16;
			else {
				mp->b_rptr = origmp;
				return -ENOENT;
			}
			origmp = s2+1;
		}
		err = -ERESTART;
	} else if(!(flags & ~CHP_FROMSTACK)) {
		if ((err = m_getid (mp, &id)) != 0)
			goto err;
		switch (id) {
		default:
			err = -ERESTART;
			break;
		case PROTO_OFFSET:
			{
				long z;
				if ((err = m_geti (mp, &z)) != 0)
					goto err;
				if (z < 0 || z >= 1024) {
					err = -EINVAL;
					goto err;
				}
				if(flags & CHP_FROMSTACK) /* down */
					dumb->chan[channel].offset = z;
			}
			err = -ERESTART;
			break;
		}
	} else
		err = -ERESTART;
	if(err == 0) {
		freemsg(mp);
		return 0;
	}
  err:
	mp->b_rptr = origmp;
	return ((err != -ERESTART) ? err : isdn2_chprot(card,channel,mp,flags));
}

/*
 * Check if buffer space is available
 */
static int
candata (struct _isdn1_card * card, short channel)
{
	struct _dumb * dumb = (struct _dumb *)card;
	int ret = (dumb->chan[channel].q_out.nblocks < 4);
	if(channel == 0) { DEBUG(isac) printf("%sCQ D %d...",KERN_DEBUG,ret); }
	else { DEBUG(hscxout) printf("%sCQ %d %d...",KERN_DEBUG,channel,ret); }
	return ret;
}

/*
 * Enqueue the data.
 */
static int
data (struct _isdn1_card * card, short channel, mblk_t * data)
{
	struct _dumb * dumb = (struct _dumb *)card;
	if(channel == 0) { DEBUG(isac) printf("%sQ D %d...",KERN_DEBUG,msgdsize(data)); }
	else { DEBUG(hscxout) printf("%sQ %d %d...",KERN_DEBUG,channel,msgdsize(data)); }
	S_enqueue(&dumb->chan[channel].q_out, data);
	NAME(REALNAME,poll)((struct _dumb *) card);
	return 0;
}

/*
 * Flush the send queue.
 */
static int
flush (struct _isdn1_card * card, short channel)
{
	struct _dumb * dumb = (struct _dumb *)card;
	S_flush(&dumb->chan[channel].q_out);
	return 0;
}

static int
ISACpresent(struct _dumb *dumb)
{
	unsigned char Bt;
	if(((Bt = ByteInISAC(dumb,STAR)) & ~0x4F) != 0) {
		printf("<STAR %02x> ",Bt);
		return -ENXIO;
	}
	if(((Bt = ByteInISAC(dumb,EXIR)) & ~0x10) != 0) {
		printf("<EXIR %02x> ",Bt);
		return -ENXIO;
	}
	if(((Bt = ByteInISAC(dumb,RBCH)) & 0x8F) != 0x00) {
		printf("<RBCH %02x> ",Bt);
		return -ENXIO;
	}
	ByteOutISAC(dumb,MODE,0xD1);
	if((Bt = ByteInISAC(dumb,MODE)) != 0xD1) {
		printf("<MODE %02x> ",Bt);
		return -ENXIO;
	}
#if 0
	if((Bt = ByteInISAC(dumb,RBCL)) != 0) {
		printf("<RBCL %02x> ",Bt);
		return -ENXIO;
	}
#endif
	ByteOutISAC(dumb,MODE,0xC1);
	if((Bt = ByteInISAC(dumb,MODE)) != 0xC1) {
		printf("<MODE %02x> ",Bt);
		return -ENXIO;
	}
	return 1;
}

static void
InitHSCX(struct _dumb * dumb)
{
	int i;
	for (i=1; i <= dumb->numHSCX; i++) 
		InitHSCX_ (dumb,i);
}


static void
ISAC_kick(struct _dumb * dumb)
{
	mblk_t *sendb;
	uchar_t *sendp = NULL;
	
	unsigned long ms = SetSPL(dumb->ipl);
	DEBUG(isac) printf("%sK ",KERN_DEBUG );
	if(dumb->chan[0].locked) {
		DEBUG(isac) printf("lck ");
		splx(ms);
		return;
	}

	if(dumb->chan[0].mode < M_ON) {
		if(0)DEBUG(isac) { printf("Flush Off\n"); }
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
			for(;thisb > 0; thisb--) {
				ByteOutISAC(dumb,FIFO(numb),*sendp);
				numb++; sendp++;
			}
			while(sendp >= (uchar_t *)sendb->b_wptr) {
				sendb = sendb->b_cont;
				if(sendb != NULL)
					sendp = (uchar_t *)sendb->b_rptr;
				else
					goto dEnd; /* shortcut */
			}
		} while((numb < ISAC_W_FIFO_SIZE) && (sendb != NULL));
		if(sendb != NULL) {
			ByteOutISAC(dumb,CMDR, 0x08); /* XTF */
		    DEBUG(isac)printf(",");
			dumb->chan[0].m_out_run = sendb;
			dumb->chan[0].p_out = sendp;
		} else {
		  dEnd:
			ByteOutISAC(dumb,CMDR, 0x0A); /* XTF|XME */
		    DEBUG(isac)printf(";");
			freemsg(dumb->chan[0].m_out);
			dumb->chan[0].m_out = dumb->chan[0].m_out_run = NULL;
			if(dumb->chan[0].q_out.nblocks < 2)
				isdn2_backenable(&dumb->card,0);
		}
	} else DEBUG(isac) printf("nothing");
	dumb->chan[0].locked = 0;
	DEBUG(isac) printf("\n");
}



#ifdef __GNUC__
/* inline */
#endif
static void
HSCX_kick(struct _dumb * dumb, u_char hscx)
{
	mblk_t *sendb;
	uchar_t *sendp = NULL;
	hdlc_buf bufp = &dumb->chan[hscx];

	unsigned long ms = SetSPL(dumb->ipl);
	DEBUG(hscxout) printf("%sK.%d ",KERN_DEBUG ,hscx);
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
				printf("%sPNull! %p %p %p %p %p %d\n",KERN_WARNING ,sendb,sendp,bufp->m_out,bufp->m_out_run,bufp->p_out,numb);
				goto exhopp;
			}
#endif
			DEBUG(hscxout) printf(".s");
			do {
				short thisb = (uchar_t *)sendb->b_wptr-sendp;
				if(thisb > HSCX_W_FIFO_SIZE-numb) thisb = HSCX_W_FIFO_SIZE-numb;
				DEBUG(hscxout) printf(">%d ",thisb);
				for(;thisb > 0; thisb --) {
					ByteOutHSCX(dumb,hscx,FIFO(numb),*sendp);
					numb++; sendp++;
				}
				while(sendp >= (uchar_t *)sendb->b_wptr) {
					sendb = sendb->b_cont;
					DEBUG(hscxout)printf("=%p ",sendb);
					if(sendb != NULL)
						sendp = (uchar_t *)sendb->b_rptr;
					else
						goto bEnd; /* shortcut */
				}
			} while((numb < HSCX_W_FIFO_SIZE) && (sendb != NULL));
#if 1
			if(
#ifdef WIDE
					ByteInHSCX(dumb,hscx,ISR1)&0x10
#else
					ByteInHSCX(dumb,hscx,EXIR)&0x40
#endif
						) { /* XDU */
				DEBUG(info) printf("%sUnderrun HSCX.%d\n",KERN_DEBUG ,hscx);
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
			  bEnd:
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
	} while (ByteInHSCX(dumb,hscx,STAR) & 0x40); /* XFW */
#endif
	DEBUG(hscxout) printf("\n");
  exhopp:;
	bufp->locked = 0;
}


#ifdef __GNUC__
/* inline */
#endif
static void
IRQ_HSCX_(struct _dumb * dumb, u_char hscx, 
#ifdef WIDE
						Byte isr0, Byte isr1
#else
						Byte Reason, Byte hasEX
#endif
							)
{
	hdlc_buf bufp = &dumb->chan[hscx];

#ifdef WIDE
	DEBUG(hscx) { printf("%s%c.%d %02x:%02x\n",KERN_DEBUG ,(dumb->polled<0)?'X':'I',hscx, isr0,isr1); }
#else
	DEBUG(hscx) { printf("%s%c.%d %02x\n",KERN_DEBUG ,(dumb->polled<0)?'X':'I',hscx, Reason); }
	if (hasEX)
#endif
	{
#ifndef WIDE
		Byte EXIR = ByteInHSCX(dumb,hscx,EXIR);
		DEBUG(hscx) { printf(". %x", EXIR); }
#endif
		if (
#ifdef WIDE
				isr1 & 0x02
#else
				EXIR & 0x80
#endif
					) { /* XMR */
			DEBUG(info) { printf("%sMsg Repeat HSCX.%d\n",KERN_DEBUG ,hscx); }
			CEC(ByteInHSCX(dumb,hscx,STAR) & 0x04);
			if (ByteInHSCX(dumb,hscx,STAR) & 0x40) { /* XFW */
#ifdef WIDE
				isr1 |= 0x01; /* also set XPR */
#else
				Reason |= 0x10; /* also set XPR bit */
#endif
			} else {
				ByteOutHSCX(dumb,hscx,CMDR, 0x01); /* XRES -- cause XPR */
			}
			/* Restart */
			if((bufp->m_out_run = bufp->m_out) != NULL) 
				bufp->p_out = bufp->m_out->b_rptr;
		}
		if (
#ifdef WIDE
				isr1 & 0x10
#else
				EXIR & 0x40
#endif
					) { /* XDU */
			CEC(ByteInHSCX(dumb,hscx,STAR) & 0x04);
			if (bufp->mode >= M_HDLC)  {
				DEBUG(info) {
					printf("%sXmit Underrun HSCX.%d\n",KERN_DEBUG ,hscx); }
#if NEW_XMIT
				ByteOutHSCX(dumb,hscx,CMDR, 0x01); /* XRES */
#ifdef WIDE
				isr1 |= 0x01; /* set XPR */
#else
				Reason |= 0x10; /* set XPR bit */
#endif
#else /* old */
				if (ByteInHSCX(dumb,hscx,STAR) & 0x40) { /* XFW */
#ifdef WIDE
					isr1 |= 0x01; /* set XPR */
#else
					Reason |= 0x10; /* set XPR bit */
#endif
				} else {
					ByteOutHSCX(dumb,hscx,CMDR, 0x01); /* XRES -- cause XPR */
				}
#endif
				/* Restart */
				if ((bufp->m_out_run = bufp->m_out) != NULL)
					bufp->p_out = bufp->m_out->b_rptr;
			} else {
#ifdef WIDE
				isr1 |= 0x01; /* set XPR */
#else
				Reason |= 0x10; /* set XPR bit */
#endif
			}
		}
#ifdef WIDE
		DEBUG(hscx)if (isr0 & 0x08) { /* PLLA */
			DEBUG(info) { printf("%sISDN .PLLA\n",KERN_WARNING ); }
		}
		DEBUG(hscx)if (isr0 & 0x20) { /* RSC */
			DEBUG(info) { printf("%sISDN .RSC\n",KERN_WARNING ); }
		}
		DEBUG(hscx)if (isr0 & 0x10) { /* PCE */
			DEBUG(info) { printf("%sISDN .PCE\n",KERN_WARNING ); }
		}
		DEBUG(hscx)if (isr0 & 0x40) { /* RFS */
			DEBUG(info) { printf("%sISDN .RFS\n",KERN_WARNING ); }
		}
#else
		DEBUG(hscx)if (EXIR & 0x08) { /* CSC */
			DEBUG(info) { printf("%sISDN .CSC\n",KERN_WARNING ); }
		}
		DEBUG(hscx)if (EXIR & 0x04) { /* RFS */
			DEBUG(info) { printf("%sISDN .RFS\n",KERN_WARNING ); }
		}
		/* 0x02 and 0x01 are empty */
		DEBUG(hscx)if (EXIR & 0x20) { /* PCE */
			DEBUG(info) { printf("%sISDN .PCE\n",KERN_WARNING ); }
		}
#endif
		if (
#ifdef WIDE
				isr0 & 0x02
#else
				EXIR & 0x10
#endif
					) { /* RFO */
			DEBUG(info) { printf("%sRecv overflow HSCX.%d\n",KERN_DEBUG ,hscx); }
			CEC(ByteInHSCX(dumb,hscx,STAR) & 0x04);
			if(
#ifdef WIDE
					isr0 & 0x81
#else
					Reason & 0xC0
#endif
						) { /* RME|RPF */
				ByteOutHSCX(dumb,hscx,CMDR, 0xC0); /* RMC|RHR */
#ifdef WIDE
				isr0 &=~ 0x81;
#else
				Reason &=~ 0xC0;
#endif
			} else
				ByteOutHSCX(dumb,hscx,CMDR, 0x80); /* RMC */
			if(bufp->m_in != NULL) {
				freemsg(bufp->m_in);
				bufp->m_in = bufp->m_in_run = NULL;
			}
		}
	}

	if (
#ifdef WIDE
			isr0 & 0x80
#else
			Reason & 0x80
#endif
				) { /* RME */
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
					/* TODO: defer processing of incoming frames */
					if(!isdn2_canrecv(&dumb->card,hscx))
						freemsg(msg);
					else if(isdn2_recv(&dumb->card,hscx,msg) != 0)
						freemsg(msg);
				}
                CEC(ByteInHSCX(dumb,hscx,STAR) & 0x04);
                ByteOutHSCX(dumb,hscx,CMDR, 0x80); /* RMC */
			} else {
				if ((recvb = bufp->m_in_run) != NULL) {
					if ((recvp = (uchar_t *)recvb->b_wptr) + blen > (uchar_t *)DATA_END(recvb))
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
						/* TODO: defer processing of incoming frames */
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
			DEBUG(info) { printf("%sRecv abort (%02x) HSCX.%d\n",KERN_DEBUG , RSTA,hscx); }
			if(bufp->m_in != NULL) {
				freemsg(bufp->m_in);
				bufp->m_in = bufp->m_in_run = NULL;
			}
			CEC(ByteInHSCX(dumb,hscx,STAR) & 0x04);
			ByteOutHSCX(dumb,hscx,CMDR, 0xC0); /* RMC|RHR */
		}
	} else if (
#ifdef WIDE
			isr0 & 0x01
#else
			Reason & 0x40
#endif
				) { /* RPF */
		uchar_t *recvp;
		mblk_t *recvb;

        if ((recvb = bufp->m_in_run) != NULL) {
            if ((recvp = (uchar_t *)recvb->b_wptr) + HSCX_R_FIFO_SIZE > (uchar_t *)DATA_END(recvb))
                recvb = NULL;
        }
		if(0)DEBUG(hscx)printf(":");
        if (recvb == NULL) {
			if(bufp->m_in_run == NULL) { /* first block */
				recvb = allocb(HSCX_R_FIFO_SIZE*2+bufp->offset,BPRI_MED);
				if(recvb != NULL) {
					recvb->b_wptr += bufp->offset;
					recvb->b_rptr += bufp->offset;
					recvp = recvb->b_wptr;
					bufp->m_in = bufp->m_in_run = recvb;
				}
			} else {
				recvb = allocb(HSCX_R_FIFO_SIZE*4,BPRI_MED);
				if(recvb != NULL) {
					recvp = recvb->b_wptr;
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
#ifdef WIDE
	DEBUG(hscx)if (isr1 & 0x08) { /* TIN */
		DEBUG(info) { printf("%sISDN .TIN\n",KERN_WARNING ); }
	}
	DEBUG(hscx)if (isr1 & 0x20) { /* AOLP */
		DEBUG(info) { printf("%sISDN .AOLP\n",KERN_WARNING ); }
	}
#else
	DEBUG(hscx)if (Reason & 0x20) { /* RSC */
		DEBUG(info) { printf("%sISDN .RSC\n",KERN_WARNING ); }
	}
	DEBUG(hscx)if (Reason & 0x08) { /* TIN */
		DEBUG(info) { printf("%sISDN .TIN\n",KERN_WARNING ); }
	}
#endif

	if ((
#ifdef WIDE
			isr1 & 0x01
#else
			Reason & 0x10
#endif
			) || (ByteInHSCX(dumb,hscx,STAR) & 0x40)) { /* XPR */
		HSCX_kick(dumb,hscx);
	}
}


#ifdef __GNUC__
inline
#endif
static void DoCIR (struct _dumb * dumb, Byte CIR)
{
	switch(CIR) {
	case 0x0C:
	case 0x0D:
		DEBUG(info) printf(" up");
		if(dumb->do_uptimer) {
			dumb->do_uptimer = 0;
#ifdef NEW_TIMEOUT
			untimeout(dumb->uptimer);
#else
			untimeout(fail_up,dumb);
#endif
		}
		isdn2_new_state(&dumb->card,1);
		break;
	case 0x07:
		if(dumb->chan[0].mode >= M_ON) {
			DEBUG(info) printk("%sISDN CIX7 0x27\n",KERN_DEBUG );
			ByteOutISAC(dumb,CIX0,0x27);
		}
		/* FALL THRU */
	case 0x04:
		if((dumb->chan[0].mode >= M_ON) && !dumb->do_uptimer) {
#ifdef NEW_TIMEOUT
			dumb->uptimer =
#endif
				timeout((void *)fail_up,dumb,2*HZ);
			dumb->do_uptimer = 1;
		}
		break;
	case 0x00:
	case 0x06:
	case 0x0F:
		if(dumb->chan[0].mode >= M_ON) {
			DEBUG(info) printk("%sISDN CIX8 0x03\n",KERN_DEBUG );
			ByteOutISAC(dumb,CIX0,0x03);
		}
		if((CIR != 0x0F) && (dumb->chan[0].mode == M_OFF)) {
			DEBUG(info) printk("%sISDN CIX9 0x3F\n",KERN_DEBUG );
			ByteOutISAC(dumb,CIX0,0x3F);
		}
		isdn2_new_state(&dumb->card,0);
	}
}

#ifdef __GNUC__
inline
#endif
static void
IRQ_ISAC(struct _dumb * dumb)
{
	Byte Reason;

  while((Reason = ByteInISAC(dumb,ISTA))) {
	DEBUG(isac) { printf("%s%c %02x\n",KERN_DEBUG ,(dumb->polled<0)?'X':'I',Reason); }
	if (Reason & 0x04) { /* CISQ */
		Byte CIR = ByteInISAC(dumb,CIR0);
		
		if (CIR & 0x80) { /* SQC */
			DEBUG(info) { printf("%sISDN .SQC %x\n",KERN_WARNING ,ByteInISAC(dumb,SQRR)); }
		}
		if (CIR & 0x03) {
			CIR = ((CIR >> 2) & 0x0F);
			DEBUG(info) printf("%sISDN CIR %01x",KERN_DEBUG ,CIR);
			if (dumb->polled >= 0)
				DoCIR(dumb,CIR);
			DEBUG(info) printf("\n");
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
			DEBUG(info) { printf("%sMsgRepeat ISAC\n",KERN_DEBUG ); }
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
				DEBUG(info) { printf("%sXmit Underrun ISAC\n",KERN_DEBUG ); }
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
			DEBUG(info) { printf("%sISDN .PCE\n",KERN_WARNING ); }
		}
		if (EXIR & 0x10) { /* RFO */
			DEBUG(info) { printf("%sRecv overflow ISAC (%02x)\n",KERN_DEBUG , EXIR); }
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
			DEBUG(info) { printf("%sISDN .CSC\n",KERN_WARNING ); }
		}
		DEBUG(isac)if (EXIR & 0x04) { /* RFS */
			DEBUG(info) { printf("%sISDN .RFS\n",KERN_WARNING ); }
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
#if 0 /* if this causes problems, TELL ME. */
			if(blen == 1) {
				DEBUG(isac) printf("%s.R-%d\n",KERN_DEBUG ,xblen);
				if((recvb = dumb->chan[0].m_in_run) != NULL) {
					mblk_t *msg = dumb->chan[0].m_in;
					dumb->chan[0].m_in = dumb->chan[0].m_in_run = NULL;
					recvb->b_wptr --;
					if(!isdn2_canrecv(&dumb->card,0))
						freemsg(msg);
					else if(isdn2_recv(&dumb->card,0,msg) != 0)
						freemsg(msg);
				}
			} else
#endif
					{
				if(blen == 0) 
					blen = ISAC_R_FIFO_SIZE;
				DEBUG(isac) printf("%s.R=%d",KERN_DEBUG ,xblen);
				if ((recvb = dumb->chan[0].m_in_run) != NULL) {
					DEBUG(isac)printf("a%d ",dsize(dumb->chan[0].m_in));
					if ((recvp = (uchar_t *)recvb->b_wptr) + blen > (uchar_t *)DATA_END(recvb))
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
					DEBUG(isac) printf(">%p\n",recvp);
					for(i=0;i < blen; i++)
						*recvp++ = ByteInISAC(dumb,FIFO(i));
					recvb->b_wptr = recvp;
					CEC(ByteInISAC(dumb,STAR) & 0x04);
					ByteOutISAC(dumb,CMDR, 0x80); /* RMC */
					/* TODO: defer processing of incoming frames */
					if(!isdn2_canrecv(&dumb->card,0))
						freemsg(msg);
					else if(isdn2_recv(&dumb->card,0,msg) != 0)
						freemsg(msg);
				} else {
					CEC(ByteInISAC(dumb,STAR) & 0x04);
					ByteOutISAC(dumb,CMDR, 0xC0); /* RMC|RHR */
					DEBUG(isac) { printf(".NoBit ISAC\n"); }
				}
			}
		} else {
			DEBUG(info) { printf("%sRecv abort (%x)\n",KERN_DEBUG ,RSTA); }
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
            if ((recvp = (uchar_t *)recvb->b_wptr) + ISAC_R_FIFO_SIZE > (uchar_t *)DATA_END(recvb))
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
		DEBUG(info) { printf("%sISDN .RSC\n",KERN_WARNING ); }
	}
	if (Reason & 0x08) { /* TIN */
		DEBUG(info) { printf("%sISDN .TIN\n",KERN_WARNING ); }
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
inline
#endif
static void
IRQ_HSCX(struct _dumb * dumb)
{
	int i;
	for(i=1;i <= dumb->numHSCX; i += 2) {
#ifdef WIDE
		Byte isr = ByteInHSCX(dumb,i,GISR);

		if(isr & 0x0C) {
			Byte isr0 = ByteInHSCX(dumb,i,ISR0);
			Byte isr1 = ByteInHSCX(dumb,i,ISR1);
			IRQ_HSCX_(dumb,i, isr0,isr1);
		}
		if(isr & 0x03) {
			Byte isr0 = ByteInHSCX(dumb,i+1,ISR0);
			Byte isr1 = ByteInHSCX(dumb,i+1,ISR1);
			IRQ_HSCX_(dumb,i+1, isr0,isr1);
		}
#else
		Byte Reason = ByteInHSCX(dumb,i+1,ISTA);
	
		if (Reason & 0x06)
			IRQ_HSCX_(dumb,i, ByteInHSCX(dumb,i,ISTA), Reason & 0x02);
		if (Reason & 0xF9) 
			IRQ_HSCX_(dumb,i+1, Reason, Reason & 0x01);
#endif
	}
}


static void
intr(int irq, void *dev, struct pt_regs *regs)
{
	struct _dumb *dumb = dev;

	DEBUG(info)printf(" Pi%d ",dumb->polled);
	if(!dumb->polled++) {
		IRQ_HSCX(dumb);
		IRQ_ISAC(dumb);
		PostIRQ(dumb);
		toggle_off(dumb);
		dumb->polled--;
	} else if(dumb->polled < 0)
		dumb->polled--;
	if(dumb->polled == 0) {
		toggle_on(dumb);
	}
	DEBUG(info)printf(" -Pi%d ",dumb->polled);
}


void NAME(REALNAME,poll)(struct _dumb *dumb)
{
	long s;
	int j;
	DEBUG(info)printf(" Pl%d ",dumb->polled);
	s = splstr();
	if(!dumb->polled++) {
		splx(s);
		IRQ_HSCX(dumb);
		IRQ_ISAC(dumb);

		if (dumb->chan[0].q_out.nblocks != 0)
			ISAC_kick(dumb);
		for(j=1;j <= dumb->numHSCX; j++) {
			if (dumb->chan[j].q_out.nblocks != 0)
				HSCX_kick(dumb,j);
		}
		splstr();
		while(--dumb->polled) {
			splx(s);
			IRQ_HSCX(dumb);
			IRQ_ISAC(dumb);
			splstr();
		}
		toggle_off(dumb);
		toggle_on(dumb);
	} else
		dumb->polled--;
	splx(s);
	DEBUG(info)printf(" -Pl%d ",dumb->polled);
}


#ifdef linux
static void dumbtimer(struct _dumb *dumb)
{
	NAME(REALNAME,poll)(dumb);
#if 0
	if(dumb->countme++ < 10) {
		printf(" -(%d):%02x %02x %02x- ",dumb->info.irq,ByteInISAC(dumb,STAR),ByteInISAC(dumb,ISTA),ByteInISAC(dumb,CIR0));
	}
#endif
#ifdef NEW_TIMEOUT
	dumb->timer =
#endif
		timeout((void *)dumbtimer,dumb,(dumb->info.irq == 0) ? HZ/DUMBTIME+1 : HZ/2);
}
#endif

int NAME(REALNAME,init)(struct cardinfo *inf)
{
	int err;
	struct _dumb *dumb;
	
	dumb = kmalloc(sizeof(*dumb),GFP_KERNEL);
	if(dumb == NULL) {
		printf("???: No Memory\n");
		return -ENOMEM;
	}
	bzero(dumb,sizeof(*dumb));

	dumb->numHSCX = 1;
	dumb->info = *inf;
	dumb->infoptr = inf;
	dumb->card.ctl = dumb;
	dumb->card.modes = 0;
	dumb->card.ch_mode = mode;
	dumb->card.ch_prot = prot;
	dumb->card.send = data;
	dumb->card.flush = flush;
	dumb->card.cansend = candata;
	dumb->card.poll = NULL;
	dumb->polled = -99;

	printf("%sISDN: " STRING(REALNAME) " at mem 0x%lx io 0x%x irq %d: ",KERN_DEBUG, dumb->info.memaddr,dumb->info.ioaddr,dumb->info.irq);

	if((err = Init(dumb)) < 0) {
		printf("Card not initializable.\n");
		kfree(dumb);
		return err;
	}
	dumb->card.nr_chans = dumb->numHSCX;

	InitHSCX(dumb);
	InitISAC(dumb);
	InitHSCX(dumb);

	if((err = ISACpresent(dumb)) < 0) {
		printf("Card not responding.\n");
		kfree(dumb);
		return err;
	}
#ifdef linux
	if((dumb->info.irq != 0) && request_irq(dumb->info.irq,intr,SA_SAMPLE_RANDOM|SA_INTERRUPT,STRING(REALNAME),dumb)) {
		printf("IRQ not available.\n");
		kfree(dumb);
		return -EEXIST;
	}
#endif
	NAME(REALNAME,poll)(dumb);
	if((err = isdn2_register(&dumb->card, dumb->info.ID)) != 0) {
		printf("not installed (ISDN_2), err %d\n",err);
		kfree(dumb);
		return err;
	}

	dumb->polled = 0;
	printf("installed at ");
	if(dumb->info.memaddr != 0) 
		printf("mem 0x%lx ",dumb->info.memaddr);
	if(dumb->info.ioaddr != 0) 
		printf("io 0x%x ",dumb->info.ioaddr);
	if(dumb->info.irq != 0) 
		printf("irq %d.\n",dumb->info.irq);
	else
		printf("polled.\n");
	dumb->next = dumblist;
	dumblist = dumb;
#ifdef linux
	dumbtimer(dumb);
#endif
	MORE_USE;
	return 0;
}


void
NAME(REALNAME,exit)(struct cardinfo *inf)
{
	int j;
	unsigned long ms = SetSPL(inf->ipl);
	struct _dumb *dumb = NULL;
	struct _dumb **ndumb = &dumblist;
	while(*ndumb != NULL) {
		if((*ndumb)->infoptr == inf) {
			dumb = *ndumb;
			*ndumb = dumb->next;
			break;
		}
		ndumb = &((*ndumb)->next);
	}
	if(dumb == NULL) {
		printf("%s??? No entry\n",KERN_DEBUG);
		return;
	}
#ifdef NEW_TIMEOUT
	untimeout(dumb->timer);
#else
	untimeout(dumbtimer,dumb);
#endif
	ByteOutISAC(dumb,MASK,0xFF);
	for(j=1; j<=dumb->numHSCX;j++) {
#ifdef WIDE
		ByteOutHSCX(dumb,j,IMR0,0xFF);
		ByteOutHSCX(dumb,j,IMR1,0xFF);
#else
		ByteOutHSCX(dumb,j,MASK,0xFF);
#endif
	}
	if(dumb->info.irq > 0)
		free_irq(dumb->info.irq,dumb);
	isdn2_unregister(&dumb->card);
	splx(ms);
	kfree(dumb);
	LESS_USE;
}


#ifdef MODULE
static int do_init_module(void)
{
	return 0;
}

static int do_exit_module(void)
{
	return 0;
}
#endif

