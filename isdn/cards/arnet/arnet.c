/*
 * arnet 470i card driver
 *
 * Copyright (c) 1995, Matthias Urlichs <urlichs@noris.de>.
 *
 */

#define UAREA

#include "f_module.h"
#include "primitives.h"
#include <sys/time.h>
#include "f_signal.h"
#include "f_malloc.h"
#include <sys/sysmacros.h>
#include "streams.h"
#include <sys/stropts.h>
/* #ifdef DONT_ADDERROR */
#include "f_user.h"
/* #endif */
#include <sys/errno.h>
#include <sys/file.h>
#include <fcntl.h>
#include <stddef.h>
#include "streamlib.h"
#include "lap.h"
#include <sys/termios.h>

#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/tqueue.h>
#include <asm/io.h>

extern void log_printmsg (void *log, const char *text, mblk_t * mp, const char*);
extern void logh_printmsg (void *log, const char *text, mblk_t * mp);

#if 0
int dodebug = 1;
#define ddprintf(xx) printf(xx),({int x;for(x=0;x<1000;x++) udelay(1000);})
#else
#define ddprintf(xx) dprintf(xx)
#define dodebug 0
#endif
#define ldprintf if(0)printf
#if 1
#if 1
#define dprintf printf
#else
#define dprintf ({int x;for(x=0;x<300;x++) udelay(1000);}),printf
#endif
#else
#define dprintf if(0)printf
#endif

/*
 * Standard Streams driver information.
 */
static struct module_info arnet_minfo =
{
	0, "arnet", 0, INFPSZ, 8000, 3000
};

static struct module_info arnet_mtinfo =
{
	0, "tarnet", 0, INFPSZ, 8000, 3000
};

static qf_open arnet_open;
static qf_close arnet_close;
static qf_srv arnet_wsrv, arnet_rsrv;
static qf_put arnet_wput;

static struct qinit arnet_rinit =
{
		putq, arnet_rsrv, arnet_open, arnet_close, NULL, &arnet_minfo, NULL
};

static struct qinit arnet_winit =
{
		arnet_wput, arnet_wsrv, NULL, NULL, NULL, &arnet_minfo, NULL
};

static struct qinit arnet_rtinit =
{
		putq, arnet_rsrv, arnet_open, arnet_close, NULL, &arnet_mtinfo, NULL
};

static struct qinit arnet_wtinit =
{
		arnet_wput, arnet_wsrv, NULL, NULL, NULL, &arnet_mtinfo, NULL
};

struct streamtab arnet_info =
{&arnet_rinit, &arnet_winit, NULL, NULL};

struct streamtab arnet_tinfo =
{&arnet_rtinit, &arnet_wtinit, NULL, NULL};


#define ARNET_ADDR unsigned long

struct dmabuf {
	unsigned short cda;
	unsigned short bufptrA;
	unsigned short bufptrB;
	unsigned short buflen;
	unsigned char status;
	unsigned char fillup;
};
#define RBUFPTR(a)   ((a)->bufptrA | ((a)->bufptrB<<16))
#define WBUFPTR(a,b) (((a)->bufptrA = (b)), ((a)->bufptrB = (b)>>16))


struct arnet_port {
	queue_t *q;
	char portnr; char mode;
	struct arnet_info *inf;
	mblk_t *readmulti; /* read multi-segment messages */
	short nrmulti;
	/* num: nr buffers +1, cur: buf to free on IRQ, end: next buf to write to */
	unsigned short sendnum, sendcur, sendend; ARNET_ADDR dmasend; 
	unsigned short sendlen; /* single mode: length of sendbuf */
	char sendsingle;
	unsigned short recvnum, recvcur, recvend; ARNET_ADDR dmarecv;
	unsigned short recvlen; /* length of receive bufs */
	char recvsingle;
	/* stats */
	unsigned sendoverflow;
};

static unsigned short memsz[] = {
	16,32,48,64,256+16,1024+16,1024+512+16,2048+16,4096+16,8192+16,0
};
#define NRMEM (sizeof(memsz)/sizeof(memsz[0]))

struct arnet_info { unsigned char *membase; short irq; short ioaddr;
			unsigned short nports; short handshake;
			struct tq_struct queue; char usage;
			unsigned char ena01; unsigned char ena23; unsigned char irqf;
			struct arnet_port port[4];
			ARNET_ADDR memsize; ARNET_ADDR freemem[NRMEM]; ARNET_ADDR limmem; 
			ARNET_ADDR curpage; }
		arnet_list[3] = {{NULL,}};

short arnet_ncards = 0;

const char *astr(char x)
{
ldprintf(".%d.",__LINE__);
	switch(x) {
	default:  {
		static char xx[10];
		sprintf(xx,"??(%d)",x);
		return xx;
		}
	case 0: return "EIA-232";
	case 1: return "V.35 + EIA-232";
	case 2: return "EIA-530";
	case 3: return "X.21";
	case 6: return "EIA-530 / X.21";
	}
}


#define MEM(type,arn,offset) *(volatile unsigned type *)((void *)((arn)->membase + ((offset) & 0x3FFF)))

#define ADDR_SCA0 0xe110babe
#define ADDR_SCA1 0xca11babe
#define ADDR_NONE 0xdeadbeef
inline static void *
arnet_page(struct arnet_info *arn, ARNET_ADDR addr)
{
	SLOW_DOWN_IO; SLOW_DOWN_IO;
	switch(addr) {
	case ADDR_NONE:
if(dodebug) { if(arn->curpage != ADDR_NONE) { arn->curpage = ADDR_NONE; dprintf("C-_"); } }
		outb_p(0x00,arn->ioaddr+8);
		return NULL;
	case ADDR_SCA0:
if(dodebug) { if(arn->curpage != ADDR_NONE) { arn->curpage = ADDR_NONE; dprintf("C0_"); } }
		outb_p(0xC0,arn->ioaddr+8);
		break;
	case ADDR_SCA1:
if(dodebug) { if(arn->curpage != ADDR_NONE) { arn->curpage = ADDR_NONE; dprintf("C1_"); } }
		outb_p(0xE0,arn->ioaddr+8);
		break;
	default:
dprintf(" BadPage 0x%lx ",addr);
		return NULL;
	}
	SLOW_DOWN_IO; SLOW_DOWN_IO;
	return arn->membase;
}
inline static void *
arnet_mempage(struct arnet_info *arn, ARNET_ADDR addr)
{
	outb_p(((addr >> 14) & 0x0F) | 0x80, arn->ioaddr+8);
if(dodebug) { if(arn->curpage != (addr & ~0x3FFF)) { arn->curpage = addr & ~0x3FFF; dprintf("P%lx_",0x0F&(addr >> 14)); } }
	return arn->membase+(addr & 0x3FFF);
}
#define SEL_SCA(arp) arnet_page((arp)->inf,((arp)->portnr > 1) ? ADDR_SCA1 : ADDR_SCA0)


static void
arnet_memdump(struct arnet_info *arn, const char *s)
{
	int i,j;

	unsigned char *foo = arnet_page(arn,ADDR_SCA0);
	printf("\nARNET_Dump %s\n",s);
	for(i=256;i<512;i+=16) {
		printf("%03x:",i);
		for (j=0;j<16;j++) printf(" %02x", foo[i+j]);
		printf(": ");
		for (j=0;j<16;j++) printf("%c", ((foo[i+j] >= 0x20) && (foo[i+j] < 0x7F)) ? foo[i+j] : '.');
		printf("\n");
	}
}

static void
arnet_free(struct arnet_info *arn, ARNET_ADDR addr, unsigned short sz)
{
	unsigned short szoff = 0;
	unsigned long flags;
	ARNET_ADDR *nextaddr;

if(dodebug)dprintf(" <<F %d @ 0x%lx",sz,addr);
	while(memsz[szoff] < sz) {
		if(memsz[szoff] == 0)
			return;
		szoff++;
	}
	save_flags(flags); cli();
	nextaddr = arnet_mempage(arn, addr);
	*nextaddr = arn->freemem[szoff];
	arn->freemem[szoff] = addr;
	restore_flags(flags);
if(dodebug)dprintf(">> ");
}

static ARNET_ADDR
arnet_malloc(struct arnet_info *arn, unsigned short sz)
{
	unsigned short szoff = 0;
	unsigned short lsz;
	unsigned long flags;
	ARNET_ADDR thisaddr;

if(dodebug)dprintf(" <<M %d ",sz);

	while(memsz[szoff] < sz) {
		if(memsz[szoff] == 0)
			return 0;
		szoff++;
	}
	save_flags(flags); cli();
	lsz = memsz[szoff];
	if(arn->freemem[szoff] != 0) {
		ARNET_ADDR *nextaddr;
		thisaddr = arn->freemem[szoff];
		nextaddr = arnet_mempage(arn, thisaddr);
		arn->freemem[szoff] = *nextaddr;
	} else {
		/* No free space with this size; allocate more if possible. */
		if(arn->limmem + lsz > arn->memsize) {
			restore_flags(flags);
			dprintf("ARNET: Try to allocate %d bytes, but limit is at %ld\n",lsz,arn->limmem);
			return 0;
		}
		if((arn->limmem ^ (arn->limmem + lsz - 1)) & ~0x3FFF) {
			/* The new block would straddle a 16k boundary. NO WAY. */
			/* Pass the skipped space to free(). */
			unsigned short nlsz = ((arn->limmem+lsz)&~0x3FFF) - arn->limmem;
			unsigned short nszoff = 0;
			while(memsz[nszoff] < nlsz) {
if(dodebug>1)ddprintf("t ");
				if(memsz[nszoff] == 0)
					return 0;
				nszoff++;
			}
			do {
				while(memsz[nszoff] > nlsz) {
if(dodebug>1)ddprintf("u ");
					if(nszoff == 0)
						break;
					nszoff--;
				}
				if(nszoff == 0)
					break;
				arnet_free(arn, arn->limmem, memsz[nszoff]);
				nlsz -= memsz[nszoff];
				arn->limmem += memsz[nszoff];
if(dodebug>1)ddprintf("v ");
			} while(nszoff > 0);

			arn->limmem = (arn->limmem + nlsz) & ~0x3FFF;
			printf("ARNET %d: mem limit at %ldk\n",arn-arnet_list, (arn->limmem+0x4000) >> 10);
		}
		thisaddr = arn->limmem;
		arn->limmem += lsz;
		/* DEBUG MALLOC ** arn->limmem += 0x4000; arn->limmem &= ~0x3FFF; */
	}
	restore_flags(flags);
if(dodebug)dprintf("@ 0x%lx>> ",thisaddr);
	return thisaddr;
}

static void
arnet_dmaexit(struct arnet_port *arp)
{
	struct arnet_info *arn = arp->inf;
	short dmaoff;
	unsigned short numbuf;
	struct dmabuf *buf;
ldprintf(".%d.",__LINE__);

	SEL_SCA(arp);

	dmaoff = ((arp->portnr & 1) ? 0xC0 : 0x80);

	MEM(char,arn,dmaoff+0x10)=0x00; /* stop DMA */
	MEM(char,arn,dmaoff+0x15)=0x01; /* software abort */
	MEM(char,arn,dmaoff+0x14) = 0x00; /* disable int rx */

	MEM(char,arn,dmaoff+0x30)=0x00; /* stop DMA */
	MEM(char,arn,dmaoff+0x35)=0x01; /* software abort */
	MEM(char,arn,dmaoff+0x34) = 0x00; /* disable int tx */

	MEM(char,arn,0x15) &= ~((arp->portnr & 1) ? 0xF0 : 0x0F);

	/* Clear stuff */
	if(arp->dmarecv) {
		buf = arnet_mempage(arn,arp->dmarecv);
		for(numbuf=0; numbuf <= arp->recvnum; numbuf++,buf++) {
			if(RBUFPTR(buf) != 0) {
				arnet_free(arn,RBUFPTR(buf),arp->recvlen);
				buf = arnet_mempage(arn,arp->dmarecv);
				buf += numbuf;
				WBUFPTR(buf, 0);
			}
		}
		arnet_free(arp->inf,arp->dmarecv,sizeof(struct dmabuf)*(arp->recvnum+1));
		arp->dmarecv = 0;
	}
	if(arp->dmasend) {
		buf = arnet_mempage(arn,arp->dmasend);
		for(numbuf=0; numbuf <= arp->sendnum; numbuf++,buf++) {
			if(RBUFPTR(buf) != 0) {
				arnet_free(arn,RBUFPTR(buf),arp->sendsingle ? arp->sendlen : buf->buflen);
				buf = arnet_mempage(arn,arp->dmasend);
				buf += numbuf;
				WBUFPTR(buf, 0);
			}
		}
		arnet_free(arp->inf,arp->dmasend,sizeof(struct dmabuf)*(arp->sendnum+1));
		arp->dmasend = 0;
	}
}

static int
arnet_dmainit(struct arnet_port *arp, int buflen, char recvlen, char sendlen)
{
	struct arnet_info *arn = arp->inf;
	ARNET_ADDR bufstart;
	short numbuf;
	struct dmabuf *buf;
	char gotempty, recvsingle, sendsingle;
	short dmaoff;
ldprintf(".%d.",__LINE__);

	SEL_SCA(arp);

	dmaoff = ((arp->portnr & 1) ? 0xC0 : 0x80);
	MEM(char,arn,dmaoff+0x10)=0x00; /* stop DMA */
	MEM(char,arn,dmaoff+0x15)=0x01; /* software abort */
	MEM(char,arn,dmaoff+0x30)=0x00; /* stop DMA */
	MEM(char,arn,dmaoff+0x35)=0x01; /* software abort */

	if(arp->dmarecv || arp->dmasend)
		arnet_dmaexit(arp);
	if(recvlen == 0)
		recvlen = -2;
	if(sendlen == 0)
		sendlen = -2;
	if(buflen <= 0)
		buflen = 512;
	if((sendsingle = (sendlen < 0)))
		sendlen = -sendlen;
	if((recvsingle = (recvlen < 0)))
		recvlen = -recvlen;
	
	/* init receiver */
	bufstart = arnet_malloc(arn,sizeof(struct dmabuf)*(recvlen+1));
	if(bufstart == 0)
		return -ENOBUFS;
	arp->dmarecv = bufstart; arp->recvlen = buflen;
	buf = arnet_mempage(arn,bufstart);
	for(gotempty=0, numbuf=0; numbuf<=recvlen; numbuf++,buf++) {
		buf->cda = ((numbuf == recvlen) ? 0 : (numbuf+1))*sizeof(struct dmabuf) + bufstart;
		if(gotempty)
			WBUFPTR(buf,0);
		else {
			ARNET_ADDR foo;
			foo = arnet_malloc(arn,buflen);
			if(foo == 0)
				gotempty = 1;
			buf = arnet_mempage(arn,bufstart);
			buf += numbuf;
if(0)dprintf(".Set bufstart %lx buf %d to %lx..",bufstart+numbuf*sizeof(struct dmabuf),numbuf,foo);
			WBUFPTR(buf, foo);
			buf->buflen = 12345+numbuf; /* Debugging */
		}
	}
	if(gotempty) {
		arnet_dmaexit(arp);
		return -ENOSPC;
	}
	arp->recvcur = 0; arp->recvend = (recvsingle ? 0 : recvlen);
	arp->recvnum = recvlen; arp->recvsingle = recvsingle;
	arp->recvlen = buflen;

	bufstart = arnet_malloc(arn,sizeof(struct dmabuf)*(sendlen+1));
	if(bufstart == 0) {
		arnet_dmaexit(arp);
		return -ENOSPC;
	}
	arp->dmasend = bufstart;
	buf = arnet_mempage(arn,bufstart);
	for(numbuf=0; numbuf<=(sendlen ? sendlen : 1); numbuf++,buf++)  {
		buf->cda = ((numbuf == sendlen) ? 0 : (numbuf+1))*sizeof(struct dmabuf) + bufstart;
		WBUFPTR(buf, 0);
		buf->buflen = 0;
	}
	arp->sendcur = 0; arp->sendend = 0; arp->sendnum = sendlen;
	arp->sendlen = (sendsingle ? buflen : 0); arp->sendsingle = sendsingle;

	/* Setup receiver 2 */
	if(!recvsingle) {
		SEL_SCA(arp);
		MEM(char ,arn,dmaoff+0x11) = 0x14; /* Mode: no FCT */
		MEM(long ,arn,dmaoff+0x04) = arp->dmarecv; /* buffer base */
		MEM(short,arn,dmaoff+0x08) = arp->dmarecv+sizeof(struct dmabuf)*arp->recvcur; /* current */
		MEM(short,arn,dmaoff+0x0A) = arp->dmarecv+sizeof(struct dmabuf)*arp->recvend; /* end */
		MEM(short,arn,dmaoff+0x0C) = arp->recvlen;
		MEM(char ,arn,dmaoff+0x14) = 0xF0; /* Interrupts */
		MEM(char ,arn,dmaoff+0x15) = 0x02; /* cmd clear FCT */
		MEM(char ,arn,dmaoff+0x10) = 0xF2; /* Status */
		if(0)dprintf("Now: conf %02x s%02x m%02x #%d c%x(%d) e%x(%d) buflen %d ", 
			MEM(char ,arn,dmaoff+0x11),
			MEM(char ,arn,dmaoff+0x10),
			MEM(char ,arn,dmaoff+0x14),
			MEM(short,arn,dmaoff+0x0E),
			MEM(short,arn,dmaoff+0x08),
			arp->recvcur,
			MEM(short,arn,dmaoff+0x0A),
			arp->recvend,
			MEM(short,arn,dmaoff+0x0C));
	} else {
		ARNET_ADDR foo;
		buf = arnet_mempage(arn,arp->dmarecv);
		foo = RBUFPTR(&buf[0]); /* pos zero */

		SEL_SCA(arp);
	printf("I %d @ %d:%lx / ",arp->recvlen,0,foo);
		MEM(char ,arn,dmaoff+0x11) = 0x00; /* Mode */
		MEM(long ,arn,dmaoff+0x00) = foo; /* buffer */
		MEM(short,arn,dmaoff+0x0E) = arp->recvlen; /* buffer length */
		MEM(char ,arn,dmaoff+0x14) = 0x80; /* Interrupts */
		MEM(char ,arn,dmaoff+0x10) = 0xF2; /* Status */
	}

	/* Setup transmitter */
	if(!sendsingle) {
		MEM(char ,arn,dmaoff+0x31) = 0x14; /* Mode -- no FCT for now */
		MEM(long ,arn,dmaoff+0x24) = arp->dmasend; /* buffer base */
		MEM(short,arn,dmaoff+0x28) = arp->dmasend+sizeof(struct dmabuf)*arp->sendcur; /* current */
		MEM(short,arn,dmaoff+0x2A) = arp->dmasend+sizeof(struct dmabuf)*arp->sendend; /* end */
		MEM(char ,arn,dmaoff+0x34) = 0xF0; /* Interrupts */
		MEM(char ,arn,dmaoff+0x35) = 0x02; /* cmd clear FCT */
		MEM(char ,arn,dmaoff+0x30) = 0xF1; /* Clear flags; do not send yet... */
	} else {
		MEM(char,arn,dmaoff+0x31) = 0x00; /* Mode */
		MEM(char,arn,dmaoff+0x34) = 0x80; /* Interrupts */
		MEM(char,arn,dmaoff+0x30) = 0xF1;
	}
	MEM(char,arn,0x15) |= ((arp->portnr & 1) ? 0xF0 : 0x0F);
dprintf("DMA (offset %02x) inited\n",dmaoff);
	arp->sendoverflow = 0;
	if(0)dprintf("Now4: conf %02x s%02x m%02x #%d c%x(%d) e%x(%d) buflen %d ", 
		MEM(char ,arn,dmaoff+0x11),
		MEM(char ,arn,dmaoff+0x10),
		MEM(char ,arn,dmaoff+0x14),
		MEM(short,arn,dmaoff+0x0E),
		MEM(short,arn,dmaoff+0x08),
		arp->recvcur,
		MEM(short,arn,dmaoff+0x0A),
		arp->recvend,
		MEM(short,arn,dmaoff+0x0C));
		
	return 0;
}

static int
arnet_mode(struct arnet_port *arp, int mode)
{
	struct arnet_info *arn = arp->inf;
	unsigned char offset;
	long flags;
	int err;
	unsigned char inb3 = inb_p(arn->ioaddr+3);

dprintf("ARNET_MODE %d of %d: ",mode,arp->portnr);

	SEL_SCA(arp);

	save_flags(flags); cli();
	if(mode == -1) {
		switch(arp->portnr) {
		case 0:
			offset = 0x20;
			arn->ena01 = arn->ena01 & ~0x15;
			outb_p(arn->ena01,arn->ioaddr+0x09); break;
			break;
		case 1:
			offset = 0x40;
			arn->ena01 = arn->ena01 & ~0x2A;
			outb_p(arn->ena01,arn->ioaddr+0x09); break;
			break;
		case 2:
			offset = 0x20;
			arn->ena23 = arn->ena23 & ~0x15;
			outb_p(arn->ena23,arn->ioaddr+0x0E); break;
			break;
		case 3:
			offset = 0x40;
			arn->ena23 = arn->ena23 & ~0x2A;
			outb_p(arn->ena23,arn->ioaddr+0x0E); break;
			break;
		default:
			restore_flags(flags);
dprintf(" ... unknown portnr\n");
			return 0;
		}
		MEM(char,arn,0x14) &=~((arp->portnr & 1) ? 0xF0 : 0x0F); /* disable bit ints */
		MEM(char,arn,0x15) &=~((arp->portnr & 1) ? 0xF0 : 0x0F); /* disable DMA ints */
		MEM(char,arn,0x16) &=~((arp->portnr & 1) ? 0x0C : 0x03); /* disable timer ints */
		arp->mode = -1;
		MEM(char,arn,offset+0x0C) = 0x21; /* Reset */
		MEM(char,arn,offset+0x08) = 0x00; /* IRQ enable */
		MEM(char,arn,offset+0x09) = 0x00; /* IRQ enable */
		MEM(char,arn,offset+0x0A) = 0x00; /* IRQ enable */
		MEM(char,arn,offset+0x0B) = 0x00; /* IRQ enable */
		arnet_dmaexit(arp);
		restore_flags(flags);
		return 0;
	}

	switch(arp->portnr) {
	case 0:
		offset = 0x20;
		if(mode & 2) arn->ena01 &= ~0x10;
		outb_p(arn->ena01,arn->ioaddr+0x09); break;
		break;
	case 1:
		offset = 0x40;
		if(mode & 2) arn->ena01 &= ~0x20;
		outb_p(arn->ena01,arn->ioaddr+0x09); break;
		break;
	case 2:
		offset = 0x20;
		if(mode & 2) arn->ena23 &= ~0x10;
		outb_p(arn->ena23,arn->ioaddr+0x0E); break;
		break;
	case 3:
		offset = 0x40;
		if(mode & 2) arn->ena23 &= ~0x20;
		outb_p(arn->ena23,arn->ioaddr+0x0E); break;
		break;
	default:
dprintf(" ... unknown portnr\n");
		restore_flags(flags);
		return -ENXIO;
	}
	SEL_SCA(arp);
	MEM(char,arn,offset+0x0C) = 0x11; /* Rx reset */
	MEM(char,arn,offset+0x0C) = 0x01; /* Tx reset */

	MEM(char,arn,offset+0x1A) = 0x04; /* RRC on if bytecount > */
	MEM(char,arn,offset+0x18) = 0x10; /* TRC on if bytecount < */
	MEM(char,arn,offset+0x19) = 0x18; /* TRC off if bytecount > */
	if(mode) {
		MEM(char,arn,offset+0x0E) = 0x87; /* Bitsync HDLC, CRC C1 */
		MEM(char,arn,offset+0x0F) = 0x00; /* No addr check */
		MEM(char,arn,offset+0x10) = 0x00; /* NRZ */
		MEM(char,arn,offset+0x14) = 0x7E; /* Idle pattern */
		MEM(char,arn,offset+0x11) = 0x10; /* send idle */
if(dodebug>1)ddprintf("P");
	}

	switch(mode) {
	case 0: /* async */
		MEM(char,arn,offset+0x0E) = 0x00; /* Async */
		MEM(char,arn,offset+0x15) = 2;
		MEM(char,arn,offset+0x16) = 0x43; /* Clock RX : BRG, 9600 Baud */
		MEM(char,arn,offset+0x17) = 0x43; /* Clock TX : =RX */
		MEM(char,arn,offset+0x0F) = 0xC0; /* Clock, 1/64 */
		MEM(char,arn,offset+0x11) = 0x01; /* no RTS */
		break;
	case 1: /* slave clock */
		MEM(char,arn,offset+0x16) = 0x00; /* Clock RX : RXC */
		MEM(char,arn,offset+0x17) = 0x60; /* Clock TX : RX Clock */
		break;
	case 3: /* master clock */
	/* Clock: 9.8304 MHz, 128 kbps -> 76.8: 19.2*4 */
		MEM(char,arn,offset+0x15) = (dodebug>1) ? 219 : 19; /* go slowly for debugging; else 1 percent error */
		MEM(char,arn,offset+0x16) = 0x42; /* Clock RX : BRG */
		MEM(char,arn,offset+0x17) = 0x60; /* Clock TX : =RX */
		break;
	case 2: /* Tx clock */
		MEM(char,arn,offset+0x15) = dodebug ? 219 : 19; /* go slowly for debugging */
		MEM(char,arn,offset+0x16) = 0x00; /* Clock RX : RXC */
		MEM(char,arn,offset+0x17) = 0x42; /* Clock TX : BRG */
		break;
	default:
dprintf(" ... mode unknown\n");
		arnet_mode(arp,-1);
		restore_flags(flags);
		return -EIO;
	}

	switch(arp->portnr) {
	case 0:
		offset = 0x20;
		arn->ena01 |= 0x05;
		if(((inb3 >> 5) != 3) && !(mode & 2)) arn->ena01 |= 0x10;
		outb_p(arn->ena01,arn->ioaddr+0x09);
		break;
	case 1:
		offset = 0x40;
		arn->ena01 |= 0x0A;
		if(((inb3 >> 5) != 3) && !(mode & 2)) arn->ena01 |= 0x20;
		outb_p(arn->ena01,arn->ioaddr+0x09);
		break;
	case 2:
		offset = 0x20;
		arn->ena23 |= 0x05;
		if(((inb3 >> 5) != 3) && !(mode & 2)) arn->ena23 |= 0x10;
		outb_p(arn->ena23,arn->ioaddr+0x0E);
		break;
	case 3:
		offset = 0x40;
		arn->ena23 |= 0x0A;
		if(((inb3 >> 5) != 3) && !(mode & 2)) arn->ena23 |= 0x20;
		outb_p(arn->ena23,arn->ioaddr+0x0E);
		break;
	}
	arp->mode = mode;
	MEM(char,arn,offset+0x11) &=~ 0x01; /* RTS enable? */
	MEM(char,arn,offset+0x03) = 0x0F; /* status clear */
	MEM(char,arn,offset+0x09) = 0x1F; /* IRQ enable */
	if(mode)
		MEM(char,arn,offset+0x0C) = 0x02; /* Tx enable */
	if(1)err = arnet_dmainit(arp, 2048, mode ? 24 : -4, mode ? 8 : -3);  /* 32 rcv buffers would be too much */
	else err = arnet_dmainit(arp, 80, mode ? 10 : -2, mode ? 5 : -2); 
	if(err < 0) 
		arnet_mode(arp,-1);
	else {
		MEM(char,arn,offset+0x0C) = 0x12; /* Rx enable */

		if(0){
			int dmaoff = ((arp->portnr & 1) ? 0xC0 : 0x80);
			dprintf("Now5: conf %02x s%02x m%02x #%d c%x(%d) e%x(%d) buflen %d ", 
				MEM(char ,arn,dmaoff+0x11),
				MEM(char ,arn,dmaoff+0x10),
				MEM(char ,arn,dmaoff+0x14),
				MEM(short,arn,dmaoff+0x0E),
				MEM(short,arn,dmaoff+0x08),
				arp->recvcur,
				MEM(short,arn,dmaoff+0x0A),
				arp->recvend,
				MEM(short,arn,dmaoff+0x0C));

		}
		MEM(char,arn,0x14) |= ((arp->portnr & 1) ? 0xF0 : 0x0F); /* enable bit ints */
		MEM(char,arn,0x15) |= ((arp->portnr & 1) ? 0xF0 : 0x0F); /* enable DMA ints */
		MEM(char,arn,0x16) |= ((arp->portnr & 1) ? 0x0C : 0x03); /* enable timer ints */
	}

	restore_flags(flags);
dprintf(" ... done.\n");
	return err;
}

static int
arnet_readbuf(struct arnet_port *arp, mblk_t **mbp)
{
	struct arnet_info *arn = arp->inf;
	unsigned short dmaoff, curlen;
	struct dmabuf *buf;
	streamchar *data;
	mblk_t *mb = *mbp;
ldprintf(".%d.",__LINE__);
if(dodebug>1)dprintf("S");

	if(arp->dmarecv == 0)
		return -EINVAL;
	dmaoff = ((arp->portnr & 1) ? 0xC0 : 0x80);
	SEL_SCA(arp);
	if(0)dprintf("NowR: conf %02x s%02x m%02x #%d c%x(%d) e%x(%d) buflen %d ", 
		MEM(char ,arn,dmaoff+0x11),
		MEM(char ,arn,dmaoff+0x10),
		MEM(char ,arn,dmaoff+0x14),
		MEM(short,arn,dmaoff+0x0E),
		MEM(short,arn,dmaoff+0x08),
		arp->recvcur,
		MEM(short,arn,dmaoff+0x0A),
		arp->recvend,
		MEM(short,arn,dmaoff+0x0C));
	if(!arp->recvsingle) { /* chained mode */
		unsigned char fstate;
		unsigned short cur;
if(dodebug>1)dprintf("O");
		if(mbp == NULL)
			return -EINVAL;
		SEL_SCA(arp);
		cur = MEM(short,arn,dmaoff+0x08);
		buf = arnet_mempage(arn,arp->dmarecv);
		if(((cur ^ (unsigned long) &buf[arp->recvcur]) & 0x3FFF) == 0) {
if(dodebug>1)dprintf("X");
			SEL_SCA(arp);
			MEM(char,arn,0x15) |= ((arp->portnr & 1) ? 0x30 : 0x03); /* reenable, just to be sure */
			return -EAGAIN;
		}
		if(buf == NULL)
			return -EIO;
if(dodebug>1)dprintf("F %d:",arp->recvcur);
		if(0)arnet_memdump(arn,"");
		fstate = buf[arp->recvcur].status;
		if((fstate & 0x3F) != 0x00) { /* We ignore "short frame" errors. Those are fine! */
dprintf("M");
			dprintf(" ** Frame at %d (%p) is 0x%02x, dropped!\n",arp->recvcur,&buf[arp->recvcur].status,fstate);
			arp->recvcur = (arp->recvcur == arp->recvnum) ? 0 : (arp->recvcur + 1);
			if(arp->readmulti != NULL) {
				freemsg(arp->readmulti);
				arp->readmulti = NULL;
			}
			arp->nrmulti = 0;
			*mbp = NULL;
			return 0;
		}
		if(arp->nrmulti >= 0) {
if(dodebug>1)dprintf("L");
			curlen = buf[arp->recvcur].buflen;
			data = arnet_mempage(arn, RBUFPTR(&buf[arp->recvcur]));
			if(arp->nrmulti > (arp->recvnum<<1)) { /* Too long. Kill. */
				MEM(char,arn,(arp->portnr & 1) ? 0x4C : 0x2C) = 0x11; /* RX reset */
				arp->recvcur = 0; arp->recvend = arp->recvnum;
				MEM(short,arn,dmaoff+0x08) = arp->dmarecv+sizeof(struct dmabuf)*arp->recvcur; /* current */
				MEM(short,arn,dmaoff+0x0A) = arp->dmarecv+sizeof(struct dmabuf)*arp->recvend; /* end */
				MEM(char ,arn,dmaoff+0x15) = 0x02; /* cmd clear FCT */
				MEM(char ,arn,dmaoff+0x10) = 0xF2; /* Clear stuff; restart */
				MEM(char ,arn,(arp->portnr & 1) ? 0x4C : 0x2C) = 0x12; /* RX enable */
			} else {
				mb = allocb(curlen,BPRI_MED);
				if(mb == NULL) {
					dprintf(" * No room for %d bytes\n",curlen);
					arp->recvcur = (arp->recvcur == arp->recvnum) ? 0 : (arp->recvend + 1);
					if(arp->readmulti != NULL) {
						freemsg(arp->readmulti);
						arp->readmulti = NULL;
						arp->nrmulti = (fstate & 0x80) ? 0 : -1;
					}
					return -ENOBUFS;
				}
				memcpy(mb->b_wptr,data,curlen);
				mb->b_wptr += curlen;
			}
		} else {
			mb = NULL;
			if(fstate & 0x80)
				arp->nrmulti = 0;
		}
		buf = arnet_mempage(arn,arp->dmarecv);
		arp->recvend = arp->recvcur;
		arp->recvcur = (arp->recvcur == arp->recvnum) ? 0 : (arp->recvcur + 1);

		SEL_SCA(arp);
		MEM(short,arn,dmaoff+0x0A) = arp->dmarecv + arp->recvend * sizeof(struct dmabuf);
		MEM(char ,arn,dmaoff+0x10) = 0x41; /* one frame is processed */
		if(mb != NULL) {
			if(fstate & 0x80) { /* finished */
if(dodebug>1)dprintf("K");
				if(arp->readmulti != NULL) {
					linkb(arp->readmulti,mb);
					mb = arp->readmulti;
					arp->readmulti = NULL;
					arp->nrmulti = 0;
				}
			} else { /* more frames */
if(dodebug>1)dprintf("J");
				if(arp->readmulti != NULL)
					linkb(arp->readmulti,mb);
				else 
					arp->readmulti = mb;
				mb = NULL;
				arp->nrmulti++;
			}
		}
		/* XXX TODO: Figure out if transfer halted, and restart */
	} else {	/* single mode */
		ARNET_ADDR thebuf;
		unsigned short foo ;
ddprintf("Ioff");
		SEL_SCA(arp);
		foo = MEM(short,arn,dmaoff+0x0E);
		MEM(char ,arn,dmaoff+0x10) = 0x00; /* halt DMA */

		buf = arnet_mempage(arn,arp->dmarecv);
		if(arp->recvcur == arp->recvend) { /* current buffer */
			/* Store and switch */
			curlen = arp->recvlen - foo;
dprintf("H %d-%d @ %d ",arp->recvlen,foo,arp->recvcur);
			if(curlen == 0)
				return -EAGAIN;
			if(curlen > arp->recvlen)
				curlen = arp->recvlen; /* Overflow?? */

			buf = arnet_mempage(arn,arp->dmarecv);
			buf[arp->recvcur].buflen = curlen;
		} else {
			buf = arnet_mempage(arn,arp->dmarecv);
dprintf("G %d @ %d ",buf[arp->recvcur].buflen,arp->recvcur);
		}

		if ((arp->recvcur == arp->recvend)
			 || ((MEM(short,arn,dmaoff+0x0E) < (arp->recvlen>>1))
			  && (((arp->recvend == arp->recvnum) ? 0 : (arp->recvend + 1)) != arp->recvcur))) {
			/* More buffer space available -- either it's the first buffer, or another buf is more than half full. */

			arp->recvend = (arp->recvend == arp->recvnum) ? 0 : (arp->recvend + 1);
			thebuf = RBUFPTR(&buf[arp->recvend]);
dprintf("F %d @ %ld:%x ",arp->recvlen,arp->recvend,thebuf);
			SEL_SCA(arp);
			MEM(long ,arn,dmaoff+0x00) = thebuf;
			MEM(short,arn,dmaoff+0x0E) = arp->recvlen;
			MEM(char ,arn,dmaoff+0x10) = 0x02; /* start DMA */
			MEM(char ,arn,((arp->portnr & 1) ? 0x40 : 0x20) +0x11) = 0x00; /* RTS up */
		} else { /* stay with the current buffer -- pause the other side if necessary */
ddprintf("E");
			SEL_SCA(arp);
			MEM(char ,arn,dmaoff+0x10) = 0x02; /* start DMA */
			if (((arp->recvend == arp->recvnum) ? 0 : (arp->recvend + 1)) == arp->recvcur) 
				MEM(char,arn,((arp->portnr & 1) ? 0x40 : 0x20) +0x11) = 0x01; /* RTS down */
			else
				MEM(char,arn,((arp->portnr & 1) ? 0x40 : 0x20) +0x11) = 0x00; /* RTS up */
		}
ddprintf("D");
		if(mbp == NULL)
			return 0;
		buf = arnet_mempage(arn,arp->dmarecv);
		curlen = buf[arp->recvcur].buflen;
		mb = allocb(curlen,BPRI_MED);
		if(mb == NULL) 
			return -ENOSPC;
		
dprintf("C %d @ %d:%x ",curlen,arp->recvcur,RBUFPTR(&buf[arp->recvcur]));
		data = arnet_mempage(arn, RBUFPTR(&buf[arp->recvcur]));
		memcpy(mb->b_wptr, data, curlen);
		mb->b_wptr += curlen;

		arp->recvcur = (arp->recvcur == arp->recvnum) ? 0 : (arp->recvcur + 1);
dprintf("B");
	}
	*mbp = mb;
	return 0;
}

static int
arnet_writebuf(struct arnet_port *arp, mblk_t **mbp)
{
	struct arnet_info *arn = arp->inf;
	unsigned short dmaoff, curlen;
	struct dmabuf *buf;
	streamchar *data, *database;
	mblk_t *mb = mbp ? *mbp : NULL;
ldprintf(".%d.",__LINE__);

	if(arp->dmasend == 0)
		return -EINVAL;
	dmaoff = ((arp->portnr & 1) ? 0xC0 : 0x80);

	if(!arp->sendsingle) { /* chained mode */
		ARNET_ADDR thebuf;
		if(mb == NULL) {
			if(arp->sendcur != arp->sendend) {
				SEL_SCA(arp);
					
				MEM(char,arn,dmaoff+0x30) = 0x02; /* Start DMA if it was stopped */
				MEM(char,arn,0x15) |= ((arp->portnr & 1) ? 0x40 : 0x04);
			}
			return 0;
		}
		if(((arp->sendend == arp->sendnum) ? 0 : (arp->sendend + 1)) == arp->sendcur)
			return -ENOSPC;
		buf = arnet_mempage(arn,arp->dmasend);
		if(buf == NULL)
			return -EIO;
		if(RBUFPTR(&buf[arp->sendend]) != 0) {
			arnet_free(arn, RBUFPTR(&buf[arp->sendend]), buf[arp->sendend].buflen);
			buf = arnet_mempage(arn,arp->dmasend);
			WBUFPTR(&buf[arp->sendend], 0);
		}
		thebuf = arnet_malloc(arn, curlen = msgdsize(mb));
		if(thebuf == 0)
			return -ENOBUFS;
		data = arnet_mempage(arn, thebuf);
		while(mb != NULL) {
			int xlen = mb->b_wptr - mb->b_rptr;
if(dodebug>1)ddprintf("w ");
			if(xlen > 0) {
				memcpy(data,mb->b_rptr,xlen);
				mb->b_rptr += xlen;
				data += xlen;
			}
			{
				mblk_t *mb2 = mb->b_cont;
				freeb(mb);
				mb = mb2;
			}
		}
		buf = arnet_mempage(arn,arp->dmasend);
		WBUFPTR(&buf[arp->sendend], thebuf);
		buf[arp->sendend].buflen = curlen;
		buf[arp->sendend].status = 0x80; /* EOM */
if(dodebug>1)dprintf("write buf %d: ",arp->sendend);
		arp->sendend = (arp->sendend == arp->sendnum) ? 0 : (arp->sendend + 1);
		SEL_SCA(arp);
		MEM(short,arn,dmaoff+0x2A) = arp->dmasend + arp->sendend * sizeof(struct dmabuf);
		MEM(char ,arn,dmaoff+0x30) = 0xC2; /* Start DMA if it was stopped */
		MEM(char ,arn,0x15) |= ((arp->portnr & 1) ? 0xC0 : 0x0C); /* reenable sender interrupts */
	} else {	/* single mode */
		ARNET_ADDR thebuf;
      AgainS:
		buf = arnet_mempage(arn,arp->dmasend);
		if(buf == NULL)
			return -EIO;
		if(RBUFPTR(&buf[arp->sendend]) == 0) {
			thebuf = arnet_malloc(arn, arp->sendlen);
			if(thebuf == 0)
				return -ENOBUFS;
			buf = arnet_mempage(arn,arp->dmasend);
			WBUFPTR(&buf[arp->sendend], thebuf);
			curlen = 0;
		} else
			curlen = buf[arp->sendend].buflen;
		database = data = arnet_mempage(arn, thebuf = RBUFPTR(&buf[arp->sendend]));
dprintf("buf %d: cur %d, full %d; ",arp->sendend,curlen,arp->sendlen);
		if(curlen == arp->sendlen) {
			if (((arp->sendend == arp->sendnum) ? 0 : (arp->sendend + 1)) != arp->sendcur) {
				arp->sendend = (arp->sendend == arp->sendnum) ? 0 : (arp->sendend + 1);
				goto AgainS;
			}
			return -EAGAIN;
		}
		data += curlen;
		curlen = arp->sendlen - curlen;
		while((curlen > 0) && (mb != NULL)) {
			int thislen = mb->b_wptr - mb->b_rptr;
ddprintf("x ");
dprintf("this %d, cur %d: ",thislen,curlen);
			if(thislen > curlen)
				thislen = curlen;
			if(thislen > 0) {
				memcpy(data,mb->b_rptr,thislen);
				mb->b_rptr += thislen;
				data += thislen;
				curlen -= thislen;
			}
			if(mb->b_rptr >= mb->b_wptr) {
				mblk_t *mb2 = mb->b_cont;
				freeb(mb);
				mb = mb2;
			}
		}
dprintf("f");
		buf = arnet_mempage(arn,arp->dmasend);
		buf[arp->sendend].buflen = data - database;
		if(arp->sendcur == arp->sendend) { /* kick it start..? */
			if((data - database) == 1) {
dprintf("e");
				SEL_SCA(arp);
				if(!(MEM(char,arn,((arp->portnr & 1) ? 0x40 : 0x20) + 0x02) & 0x02)) /* TX ready? */  {
dprintf("d");
					goto Up;
				}
				if(MEM(char,arn,((arp->portnr & 1) ? 0x40 : 0x20) + 0x05) & 0x04) /* CTS enabled? */  {
dprintf("c");
					goto Up;
				}

				MEM(char,arn,((arp->portnr & 1) ? 0x40 : 0x20) + 0x00) = *database;
dprintf("b");
				buf = arnet_mempage(arn,arp->dmasend);
				buf[arp->sendend].buflen = 0;
				goto Up;
			} else if(data == database) {
dprintf("a");
				goto Up;
			}
dprintf("g");
			arp->sendend = (arp->sendend == arp->sendnum) ? 0 : (arp->sendend + 1);
			buf[arp->sendend].buflen = 0;
			SEL_SCA(arp);
			MEM(long ,arn,dmaoff+0x24) = thebuf;
			MEM(short,arn,dmaoff+0x2E) = data - database;
			if(!(MEM(char,arn,((arp->portnr & 1) ? 0x40 : 0x20) + 0x05) & 0x04)) /* CTS enabled? */ {
				MEM(char,arn,dmaoff+0x30) = 0x02;
dprintf("Ena ");
			}
else dprintf("!Ena ");
			MEM(char,arn,0x15) |= ((arp->portnr & 1) ? 0x40 : 0x04);
			MEM(char,arn,((arp->portnr & 1) ? 0x40 : 0x20) +0x0C) = 0x02; /* Tx enable */
dprintf("DMA (off %x) stat %02x, mode %02x, ier %02x: isr1 %02x, ier1 %02x: ",dmaoff,
			MEM(char,arn,dmaoff+0x30),
			MEM(char,arn,dmaoff+0x31),
			MEM(char,arn,dmaoff+0x34),
			MEM(char,arn,0x11),
			MEM(char,arn,0x15)
			);
dprintf("startup %lx\n",thebuf);
		} else {
dprintf("continue: sending %d\n",arp->sendcur);
			if (arp->sendcur != ((arp->sendend == arp->sendnum) ? 0 : (arp->sendend + 1))) 
				arp->sendend = (arp->sendend == arp->sendnum) ? 0 : (arp->sendend + 1);
		}
	}
  Up:
	*mbp = mb;
	return 0;
}

static void
arnet_intr1(struct arnet_port *arp)
{
	struct arnet_info *arn = arp->inf;
	unsigned char irq1,irq2,irq3;
	unsigned char msk1,msk2,msk3;
	unsigned short dmaoff;
ldprintf(".%d.",__LINE__);

	dmaoff = ((arp->portnr & 1) ? 0xC0 : 0x80);

	SEL_SCA(arp);
		
	irq1 = MEM(char,arn,0x10);
	irq2 = MEM(char,arn,0x11);
	irq3 = MEM(char,arn,0x12);
	msk1 = MEM(char,arn,0x14);
	msk2 = MEM(char,arn,0x15);
	msk3 = MEM(char,arn,0x16);
	if(0)if(irq1||irq3||(irq2&0x11))if(arp->mode != -1) dprintf("intr %02x/%02x %02x/%02x %02x/%02x: ",irq1,msk1,irq2,msk2,irq3,msk3);
	if(arp->mode == 0) {
		unsigned char errs = MEM(char,arn,dmaoff+0x10);
		dprintf("Poll%d, conf %02x s%02x m%02x #%d c%x(%d) e%x(%d) buflen %d ", 
			arp->portnr,
			MEM(char,arn,dmaoff+0x11),
			errs,
			MEM(char ,arn,dmaoff+0x14),
			MEM(short,arn,dmaoff+0x0E),
			MEM(short,arn,dmaoff+0x08),
			arp->recvcur,
			MEM(short,arn,dmaoff+0x0A),
			arp->recvend,
			MEM(short,arn,dmaoff+0x0C));
	}

	if(arp->portnr & 1) {
		irq1 >>= 4;
		irq2 >>= 4;
		irq3 >>= 2;
		msk1 >>= 4;
		msk2 >>= 4;
		msk3 >>= 2;
	}
	if(irq1 & 0x01) {
		dprintf(":RXRDY "); 
		MEM(char,arn,0x14) &=~ ((arp->portnr & 1) ? 0x10 : 0x01);
	}
	if(irq1 & 0x02) {
		dprintf(":TXRDY ");
		MEM(char,arn,0x14) &=~ ((arp->portnr & 1) ? 0x20 : 0x02);
	}
	if(irq1 & 0x04) {
		dprintf(":RXINT ");
		MEM(char,arn,0x14) &=~ ((arp->portnr & 1) ? 0x40 : 0x04);
	}
	if(irq1 & 0x08) {
		dprintf(":TXINT ");
		MEM(char,arn,0x14) &=~ ((arp->portnr & 1) ? 0x80 : 0x08);
	}
	if(irq2 & 0x01) {
		unsigned char errs = MEM(char,arn,dmaoff+0x10);
		dprintf(":DMI err rcv, s%02x m%02x ", errs,
			MEM(char ,arn,dmaoff+0x14),
			MEM(short,arn,dmaoff+0x0E));
		if(msk2 & 0x01) {
			errs &= MEM(char,arn,dmaoff+0x14);
			if(errs & 0x20) { /* buffer overrun receive -- fatal */
				printf(" * ARNET %d.%d: buffer overrun\n",arp->inf-arnet_list,arp->portnr);
				if(!arp->recvsingle) {
					if(0) {
						struct dmabuf *buf = arnet_mempage(arn,arp->dmarecv);
						unsigned short blocklen = buf[arp->recvcur].buflen;
						ARNET_ADDR dataptr = RBUFPTR(&buf[arp->recvcur]);
						unsigned char *data = arnet_mempage(arn, dataptr);
						dprintf("DStart %d: %d @ %lx: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x ; ",
							arp->recvcur,blocklen,dataptr,
							data[0x0], data[0x1], data[0x2], data[0x3],
							data[0x4], data[0x5], data[0x6], data[0x7],
							data[0x8], data[0x9], data[0xA], data[0xB],
							data[0xC], data[0xD], data[0xE], data[0xF]);
					}
					dprintf("Rx stat %02x,%02x,%02x, ",
						MEM(char,arn,(arp->portnr & 1) ? 0x43 : 0x23),
						MEM(char,arn,(arp->portnr & 1) ? 0x44 : 0x24),
						MEM(char,arn,(arp->portnr & 1) ? 0x45 : 0x25));
					
					SEL_SCA(arp);
					MEM(char,arn,(arp->portnr & 1) ? 0x4C : 0x2C) = 0x11; /* RX reset */
					arp->recvcur = 0; arp->recvend = arp->recvnum;
					MEM(short,arn,dmaoff+0x08) = arp->dmarecv+sizeof(struct dmabuf)*arp->recvcur; /* current */
					MEM(short,arn,dmaoff+0x0A) = arp->dmarecv+sizeof(struct dmabuf)*arp->recvend; /* end */
					MEM(char ,arn,dmaoff+0x15) = 0x02; /* cmd clear FCT */
					MEM(char ,arn,dmaoff+0x10) = 0xF2; /* Clear errors; restart */
					MEM(char ,arn,(arp->portnr & 1) ? 0x4C : 0x2C) = 0x12; /* RX enable */
					if(0)arnet_memdump(arn,"AfterReset");
				} else {
					/* TODO */
				}
			}
			else if(errs & 0x10) { /* counter overflow */
				printf(" * ARNET %d.%d: counter overrun\n",arp->inf-arnet_list,arp->portnr);
			}
		} else 
			dprintf(" RE-masked-; ");
	}
	if(irq2 & 0x02) {
		unsigned char errs = MEM(char,arn,dmaoff+0x10);
		if(dodebug>1)dprintf(":DMI int rcv, s%02x m%02x #%d ", errs,
			MEM(char ,arn,dmaoff+0x14),
			MEM(short,arn,dmaoff+0x0E));
		if(msk2 & 0x02) {
			errs &= MEM(char,arn,dmaoff+0x14);
			MEM(char,arn,0x15) &=~ ((arp->portnr & 1) ? 0x20 : 0x02);
			qenable(arp->q);
			if(!(errs & 0xC0)) dprintf("  Foo! Errs is 0x%x !  ",errs);
			MEM(char,arn,dmaoff+0x10) = (errs & ~0x30) | 0x02; /* clear error flags */
			/* TODO -- debugging only! */
		} else 
			if(dodebug)dprintf(" RI-masked-; ");
	}
	if(irq2 & 0x04) {
		unsigned char errs = MEM(char,arn,dmaoff+0x30);
		if(dodebug>1)dprintf(":DMI err send, s%02x m%02x #%d ", errs,
			MEM(char ,arn,dmaoff+0x34),
			MEM(short,arn,dmaoff+0x2E));
		if(msk2 & 0x04) {
			if(errs & 0x20) { /* buffer over/underrun */
				if(dodebug>1)printf(" * ARNET %d.%d: buffer underrun\n",arp->inf-arnet_list,arp->portnr);
				if(0)arnet_memdump(arn,"Buf Underrun");
				if(arp->sendoverflow > 1) {
				} else {
					arp->sendoverflow++;
				}
			}
			if(errs & 0x10) { /* counter overflow */
				printf(" * ARNET %d.%d: counter underrun\n",arp->inf-arnet_list,arp->portnr);
				if(arp->sendsingle) {
				} else {
				}
			}
			MEM(char,arn,dmaoff+0x30) = errs & ~0xC0; /* clear error flags */
		} else 
			dprintf(" TI-masked-; ");
	}
	if(irq2 & 0x08) {
		unsigned char errs = MEM(char,arn,dmaoff+0x30);
		if(dodebug)dprintf(":iS, s%02x c%x(%d) e%x(%d) ", 
			errs,
			MEM(short,arn,dmaoff+0x28),
			arp->sendcur,
			MEM(short,arn,dmaoff+0x2A),
			arp->sendend);
		if(msk2 & 0x08) {
			if(arp->sendcur != arp->sendend) {
				unsigned short cur;
				struct dmabuf *buf;
				errs &= MEM(char,arn,dmaoff+0x34);
				if(dodebug>1)dprintf("Tx stat %02x,%02x,%02x, ",
					MEM(char,arn,(arp->portnr & 1) ? 0x43 : 0x23),
					MEM(char,arn,(arp->portnr & 1) ? 0x44 : 0x24),
					MEM(char,arn,(arp->portnr & 1) ? 0x45 : 0x25));
				cur = MEM(short,arn,dmaoff+0x28);
				buf = arnet_mempage(arn,arp->dmasend);
				if(dodebug)dprintf(" sc@ %lx:  ", (unsigned long) &buf[arp->sendcur]);
				while(((cur ^ (unsigned long) &buf[arp->sendcur]) & 0x3FFF) != 0) {
					ARNET_ADDR foo; unsigned int foolen;
					buf = arnet_mempage(arn,arp->dmasend);
					foo = RBUFPTR(&buf[arp->sendcur]);
					foolen = buf[arp->sendcur].buflen;
					WBUFPTR(&buf[arp->sendcur],0);
					arnet_free(arn, foo,(arp->sendsingle ? arp->sendlen : foolen));
					arp->sendcur = (arp->sendcur == arp->sendnum) ? 0 : (arp->sendcur + 1);
					SEL_SCA(arp);
					if(arp->sendcur == arp->sendend) {
						qenable(WR(arp->q));
						break;
					}
				}
			} else {
				/* Temporarily turn off this interrupt... */
if(dodebug)dprintf("TxI Off; ");
				MEM(char,arn,0x15) &=~ ((arp->portnr & 1) ? 0x80 : 0x08);
			}
			if(errs & 0x80) { /* EOT */
				(void) arnet_writebuf(arp, NULL);
			}
			SEL_SCA(arp);
			MEM(char,arn,dmaoff+0x30) = (errs & 0xC0) | 0x01; /* clear flags */
		} else 
			if(dodebug)dprintf(" TE-masked-; ");
	}
	if(irq3 & 0x10) {
		dprintf(":T0IRQ ");
		MEM(char,arn,0x16) &=~ ((arp->portnr & 1) ? 0x40 : 0x10);
	}
	if(irq3 & 0x20) {
		dprintf(":T1IRQ ");
		MEM(char,arn,0x16) &=~ ((arp->portnr & 1) ? 0x80 : 0x20);
	}
	irq1 = MEM(char,arn,0x10);
	irq2 = MEM(char,arn,0x11);
	irq3 = MEM(char,arn,0x12);
	msk1 = MEM(char,arn,0x14);
	msk2 = MEM(char,arn,0x15);
	msk3 = MEM(char,arn,0x16);
	if(dodebug>1)dprintf(" ");
	if(dodebug && (arp->mode>=0))dprintf(":sE s%02x c%x(%d) e%x(%d) ", 
		MEM(char,arn,dmaoff+0x30),
		MEM(short,arn,dmaoff+0x28),
		arp->sendcur,
		MEM(short,arn,dmaoff+0x2A),
		arp->sendend);
	if(irq1||irq3||(irq2&0x11))if(arp->mode != -1) dprintf("now %02x/%02x %02x/%02x %02x/%02x.\n",irq1,msk1,irq2,msk2,irq3,msk3);
}

static void
arnet_intr(int irq, struct pt_regs *foo)
{
	int arc = arnet_ncards;
	struct arnet_info *arn = arnet_list;
	unsigned char irqf;
ldprintf(".%d.",__LINE__);

	while(arc > 0) {
		unsigned long flags;

		save_flags(flags); cli();
		if(arn->usage++) {
			if(irq != 0) {
				if(dodebug>1)dprintf(".D.");
				queue_task(&arn->queue,&tq_timer);
			}
			restore_flags(flags);
			goto next;
		}
		restore_flags(flags);

		if((irq != 0) && (arn->irq != irq))
			goto next;
		irqf = inb_p(arn->ioaddr+0x07);
		if((irq != 0) && !(irqf & 1)) {
			if(0)dprintf("stray interrupt!  ");
			goto next;
		}
		while(irqf & 0x01) {
if(dodebug>1)ddprintf("k ");
			arnet_intr1(&arn->port[0]);
			if(arn->nports > 1)
				arnet_intr1(&arn->port[1]);
			if(arn->nports > 2)
				arnet_intr1(&arn->port[2]);
			if(arn->nports > 3)
				arnet_intr1(&arn->port[3]);
			irqf = inb_p(arn->ioaddr+0x07);
		}
		if((irqf ^ arn->irqf) & 0x1E) {
			dprintf("ARNET: DCD changed: from %02x to %02x\n",
				irqf,arn->irqf);
			/* TODO */
		}
		arn->irqf = irqf;
	  next:
	  	arn->usage--;
		arc--;
		arn++;
	}
}

static void
arnet_exit1(struct arnet_info *arn)
{
	int cnt;
	struct arnet_port *arp = arn->port;
ldprintf(".%d.",__LINE__);
	arn->usage = 99;

	for(cnt=0;cnt < arn->nports; arp++,cnt++) {
dprintf("Port %d: ",cnt);
		if(arp->inf != arn) {
			printf("Oops -- bad board ptr %p, should be %p!\n",arp->inf,arn);
			break;
		}
		arnet_mode(arp,-1);
		arnet_dmaexit(arp);
	}
	MEM(char,arn, 0x14) = 0x00; /* IRQ off */
	MEM(char,arn, 0x15) = 0x00; /* IRQ off */
	MEM(char,arn, 0x16) = 0x00; /* IRQ off */
	if(arn->irq != 0)
		free_irq(arn->irq);
	release_region(arn->ioaddr,16);
}

static int
arnet_init1(struct arnet_info *arn)
{
	uchar_t inb3, inb4, inb5, inb6, inbc;
	unsigned short x;
	struct arnet_port *arp;
ldprintf(".%d.",__LINE__);
	printf("AR: ");

	arn->usage = 99;
	bzero(&arn->queue,sizeof(arn->queue));
	arn->queue.routine = (void *)&arnet_intr;
	arn->queue.data = (void *)(long)arn->irq;

	for(x=0;x<NRMEM;x++) 
		arn->freemem[x] = 0;
	arn->limmem = 256;
	
	inb3 = inb_p(arn->ioaddr+3);
	inb4 = inb_p(arn->ioaddr+4);
	inb5 = inb_p(arn->ioaddr+5);
	inb6 = inb_p(arn->ioaddr+6);
	if(check_region(arn->ioaddr,16)) {
		printf("*** region 0x%x..0x%x blocked\n",arn->ioaddr,arn->ioaddr+15);
		return 0;
	}
	request_region(arn->ioaddr,16,"arnet");
	arn->memsize = 1<<(((inb3&0x1C)>>2)+16);
	printf("cf %02x: %d kByte, memory at %p, bus %d, adapter %s; %d ports; rev %02x; ", inb3, arn->memsize>>10, arn->membase, inb3&3, astr(inb3>>5), inb5, inb4);
	if((unsigned long)arn->membase & 0xFF003FFF) {
		printf("*** invalid membase: 0x%p\n",arn->membase);
		release_region(arn->ioaddr,16);
		return 0;
	}
	{
		unsigned long i;
		for(i=0;i <= arn->memsize-0x1000; i += 0x1000) {
			unsigned int j;
			unsigned long *mem = arnet_mempage(arn,i);
			for(j=0;j<0x1000/sizeof(unsigned long);j++) {
				*mem++=0xefbeadde; /* 0xdeadbeef, reversed */
			}
		}

	}
	arn->nports = inb5;
	arn->handshake = inb6;
	if(inb6 == 0) 
		printf("no control/status lines; ");
	else {
		printf("Control:");
		if(inb6 & 0x80) printf(" DCD");
		if(inb6 & 0x40) printf(" RI");
		if(inb6 & 0x20) printf(" DSR");
		if(inb6 & 0x10) printf(" CTS");
		if(inb6 & 0x08) printf(" ?08");
		if(inb6 & 0x04) printf(" ?04");
		if(inb6 & 0x02) printf(" RTS");
		if(inb6 & 0x01) printf(" DTR");
		printf("; ");
	}

	bzero(&arn->port,sizeof(arn->port));
	for(x=0,arp=arn->port; x < arn->nports; x++,arp++) {
		arp->portnr = x;
		arp->inf = arn;
		arp->mode = -1;
	}

	outb_p(0x00,arn->ioaddr+0x09);
	if(arn->nports > 2)
		outb_p(0x00,arn->ioaddr+0x0E);
	udelay(2);
	outb_p(0x40,arn->ioaddr+0x09);
	udelay(500);
	arn->ena01 = arn->ena23 = 0x70;
	switch(arn->irq) {
	case  0: inbc = 0; break;
	case  3: inbc = 1; break;
	case  5: inbc = 2; break;
	case  7: inbc = 3; break;
	case 10: inbc = 4; break;
	case 11: inbc = 5; break;
	case 12: inbc = 6; break;
	case 15: inbc = 7; break;
	default:
		printf("*** unknown IRQ %d\n",arn->irq);
		release_region(arn->ioaddr,16);
		return 0;
	}
    if((arn->irq != 0) && request_irq(arn->irq,arnet_intr,0 /* SA_INTERRUPT */,"arnet")) {  
		printf("*** IRQ %d not available\n",arn->irq);
		release_region(arn->ioaddr,16);
		return 0;
	}
	inbc <<= 1;
	inbc |= 1 | (((long)arn->membase >> 10) & 0x30);
	outb_p((long)arn->membase >> 16, arn->ioaddr+13);
	outb_p(inbc, arn->ioaddr+12);
	arn->irqf = inb_p(arn->ioaddr+7);
	printf("inbc %02x, inbd %02lx; irqf %02x",inbc,(((long)arn->membase)>>16)&0xFF, arn->irqf);

	/* Now checking memory access */

	arnet_page(arn,ADDR_SCA0);
	MEM(char,arn,0x09) = 0x80; /* DMA Master */
	MEM(char,arn,0x08) = 0x14; /* DMA priority */
	MEM(char,arn,0x18) = 0xD0; /* IRQ vectors */
	arnet_page(arn,ADDR_SCA1);
	MEM(char,arn,0x09) = 0x80; /* DMA Master */
	MEM(char,arn,0x08) = 0x14; /* DMA priority */
	MEM(char,arn,0x18) = 0xD0; /* IRQ vectors */

	arnet_page(arn,ADDR_SCA0);
	MEM(char,arn,0x33) = 0xAA;
	MEM(char,arn,0x53) = 0x55;
	if(arn->nports > 2) {
		arnet_page(arn,ADDR_SCA1);
		MEM(char,arn,0x33) = 0x5A;
		MEM(char,arn,0x53) = 0xA5;
	}

	arnet_page(arn,ADDR_SCA0);
	if(MEM(char,arn,0x33) != 0xAA) {
		if((arn->nports <= 2) || (MEM(char,arn,0x33) != 0x5A)) {
			printf("ERROR: Readback A, %02x\n", MEM(char,arn,0x33));
			goto GetOut;
		}
		printf("ERROR: second chip unselectable A, %02x\n", MEM(char,arn,0x33));
		arn->nports -= 1;
	}
	if(MEM(char,arn,0x53) != 0x55) {
		if((arn->nports <= 2) || (MEM(char,arn,0x53) != 0xA5)) {
			printf("ERROR: Readback B, %02x\n", MEM(char,arn,0x53));
			goto GetOut;
		}
		printf("ERROR: second chip unselectable B, %02x\n", MEM(char,arn,0x53));
		arn->nports -= 1;
	}
	if(arn->nports == 3)
		arn->nports --;
	if(arn->nports > 2) {
		arnet_page(arn,ADDR_SCA1);
		if(MEM(char,arn,0x33) != 0x5A) {
			printf("ERROR: Readback C, %02x\n", MEM(char,arn,0x33));
			arn->nports -= 2;
		} else if(MEM(char,arn,0x53) != 0xA5) {
			printf("ERROR: Readback D, %02x\n", MEM(char,arn,0x53));
			arn->nports -= 2;
		}
	}

	arnet_page(arn,ADDR_SCA0);
	MEM(char,arn,0x33) = 0xFF;
	MEM(char,arn,0x53) = 0xFF;
	if(arn->nports > 2) {
		arnet_page(arn,ADDR_SCA1);
		MEM(char,arn,0x33) = 0xFF;
		MEM(char,arn,0x53) = 0xFF;
	}

  	printf("\n");
	arn->usage = 0;
	return 1;

  GetOut:
	if(arn->irq != 0)
		free_irq(arn->irq);
	release_region(arn->ioaddr,16);
	return 0;
}

static int
arnet_init (void)
{
ldprintf(".%d.",__LINE__);
	arnet_ncards = 0;

	arnet_list[arnet_ncards].membase = (void *)0xCC000;
	arnet_list[arnet_ncards].irq = 15;
	arnet_list[arnet_ncards].ioaddr = 0x300;
	if(arnet_init1(&arnet_list[0])) arnet_ncards++;

	printf ("ARNET: %d cards found\n",arnet_ncards);
	if(arnet_ncards == 0)
		return -ESRCH;
	else
		return 0;
}

static void
arnet_exit(void)
{
	int arc = arnet_ncards;
	struct arnet_info *arn = arnet_list;
ldprintf(".%d.",__LINE__);

	while(arc > 0) {
dprintf("Exit, %d ports: ",arn->nports);
ddprintf("l ");
		arnet_exit1(arn);
		arc--;
		arn++;
	}

}

/* Streams code to open the driver. */
static int
arnet_open (queue_t * q, dev_t dev, int flag, int sflag
#ifdef DO_ADDERROR
		,int *err
#define U_ERROR *err
#else
#define U_ERROR u.u_error
#endif
)
{
	struct arnet_info *arn = arnet_list;
	struct arnet_port *arp;
	int mdev = arnet_ncards;
	int cdev;
	int err;
ldprintf(".%d.",__LINE__);

	dev = minor (dev);
	cdev = dev & 0x0F;
	while(mdev > 0 && cdev >= arn->nports) {
ddprintf("n ");
		cdev -= arn->nports;
		arn++;
		mdev --;
	}
	if(mdev == 0) {
		U_ERROR = ENXIO;
		return OPENFAIL;
	}
	arn->usage++;
	arp = &arn->port[cdev];
	if(arp->q != NULL) {
		printf("ARNET dev 0x%x: already open, %p\n",dev,arp->q);
		arn->usage--;
		return 0;
	}
	dprintf("Opening ARNET port %d(=%d): ",cdev,arp->portnr);
	
	arp->q = q;
	arp->mode = dev >> 4;

	MORE_USE;
	WR (q)->q_ptr = (caddr_t) arp;
	q->q_ptr = (caddr_t) arp;

	if((err = arnet_mode(arp,arp->mode)) < 0) {
		arnet_dmaexit(arp);
		LESS_USE;
		arn->usage--;
		q->q_ptr = NULL;
		WR(q)->q_ptr = NULL;
		arp->q = NULL;
		U_ERROR = -err;
		return OPENFAIL;
	}
	arn->usage = 0;
	if(0)arnet_memdump(arn,"Open");
	if(0) {
		short dmaoff = (arp->portnr & 1) ? 0xC0 : 0x80;
		dprintf("Now3: conf %02x s%02x m%02x #%d c%x(%d) e%x(%d) buflen %d ", 
			MEM(char ,arn,dmaoff+0x11),
			MEM(char ,arn,dmaoff+0x10),
			MEM(char ,arn,dmaoff+0x14),
			MEM(short,arn,dmaoff+0x0E),
			MEM(short,arn,dmaoff+0x08),
			arp->recvcur,
			MEM(short,arn,dmaoff+0x0A),
			arp->recvend,
			MEM(short,arn,dmaoff+0x0C));
	}
	return dev;
}

/* Streams code to close the driver. */
static void
arnet_close (queue_t *q, int dummy)
{
	struct arnet_port *arp = (struct arnet_port *) q->q_ptr;
	struct arnet_info *arn = arp->inf;

	arn->usage++;

	if(0)arnet_memdump(arn,"Close");

	if(arp->readmulti != NULL) {
		freemsg(arp->readmulti);
		arp->readmulti = NULL;
	}
	if(arp->q == NULL)
		return;
	arnet_mode(arp,-1);
	arnet_dmaexit(arp);
	arp->q = NULL;
	arn->usage--;

	LESS_USE;
	return;
}


/* Streams code to write data. */
static void
arnet_wput (queue_t *q, mblk_t *mp)
{
	struct arnet_port *arp = (struct arnet_port *) q->q_ptr;
	struct arnet_info *arn = arp->inf;
ldprintf(".%d.",__LINE__);

#ifdef CONFIG_DEBUG_STREAMS
	if(msgdsize(mp) < 0)
		return;
#endif
	if(arp->q == NULL)  {
		freemsg(mp);
		return;
	}
	switch (mp->b_datap->db_type) {
	case M_IOCTL:
		mp->b_datap->db_type = M_IOCNAK;
		((struct iocblk *)mp->b_rptr)->ioc_error = EINVAL;
		qreply (q, mp);
		break;
	CASE_DATA
		putq (q, mp);
		break;
	case M_FLUSH:
		if (*mp->b_rptr & FLUSHW) 
			flushq (q, 0);

		if (*mp->b_rptr & FLUSHR) {
			flushq (RD (q), 0);
			*mp->b_rptr &= ~FLUSHW;
			qreply (q, mp);
		} else
			freemsg (mp);
		break;
	default:
		log_printmsg (NULL, "Strange ARNET", mp, KERN_WARNING);
		/* putctl1(RD(q)->b_next, M_ERROR, ENXIO); */
		freemsg (mp);
		break;
	}
	return;
}

/* Streams code to scan the write queue. */
static void
arnet_wsrv (queue_t *q)
{
	struct arnet_port *arp = (struct arnet_port *) q->q_ptr;
	struct arnet_info *arn;
	mblk_t *mp;
ldprintf(".%d.",__LINE__);

	if(arp == NULL || arp->q == NULL) {
		flushq(q,FLUSHALL);
		return;
	}
	arn = arp->inf;
	if(arn->usage > 0) {
		dprintf("  * ARNET Write: Stop; try again *  ");
		q->q_flag |= QWANTR;
		return;
	}
	arn->usage++;
	while ((mp = getq (q)) != NULL) {
		int err;
if(dodebug)ddprintf("o ");
		err = arnet_writebuf(arp,&mp);
		if(err < 0) {
			if(dodebug || (err != -ENOSPC && err != -ENOBUFS))dprintf("ARNET %d: error %d\n",arp->portnr,err);
			putbq(q,mp);
			goto Out;
		} else if(mp != NULL) {
			if(dodebug)dprintf("ARNET %d: buffer full\n",arp->portnr);
			putbq(q,mp);
			goto Out;
		}
	}
  Out:
if(dodebug>1)dprintf("EndWrite, %d\n",arn->usage);
	if(dodebug) {
		short dmaoff = ((arp->portnr & 1) ? 0xC0 : 0x80);
		dprintf(":EW, s%02x c%x(%d) e%x(%d) ", 
			MEM(char,arn,dmaoff+0x30),
			MEM(short,arn,dmaoff+0x28),
			arp->sendcur,
			MEM(short,arn,dmaoff+0x2A),
			arp->sendend);
	}
	arn->usage--;
	return;
}



static void
arnet_rsrv (queue_t * q)
{
	struct arnet_port *arp = (struct arnet_port *) q->q_ptr;
	struct arnet_info *arn = arp->inf;
	mblk_t *mp;
ldprintf(".%d.",__LINE__);

	if(arp->q == NULL) {
		flushq(q,FLUSHALL);
		return;
	}
	if(arn->usage > 0) {
		dprintf("  * ARNET Read: Stop; try again *  ");
		q->q_flag |= QWANTR;
	} else {
		arn->usage++;
		while (arnet_readbuf(arp,&mp) == 0) {
if(dodebug>1)ddprintf("p ");
			if(mp != NULL)
				putq(q,mp);
		}
		arn->usage--;
	}
	while ((mp = getq (q)) != NULL) {
if(dodebug>1)ddprintf("q ");
		if (q->q_next == NULL) {
			freemsg (mp);
			continue;
		}
		if (mp->b_datap->db_type >= QPCTL || canput (q->q_next)) {
			putnext (q, mp);
			continue;
		} else {
			putbq (q, mp);
			break;
		}
	}
	return;
}



#ifdef MODULE
static int devmajor1 = 0;
static int devmajor2 = 0;

static int do_init_module(void)
{
	int err;
ldprintf(".%d.",__LINE__);

	if((err = arnet_init()) < 0)
		return err;
	err = register_strdev(0,&arnet_info,0);
	if(err < 0) {
		arnet_exit();
		return err;
	}
	devmajor1 = err;
	err = register_strdev(0,&arnet_tinfo,arnet_ncards);
	if(err < 0) {
		unregister_strdev(devmajor1,&arnet_info,0);
		arnet_exit();
		return err;
	}
	devmajor2 = err;
	return 0;
}

static int do_exit_module(void)
{
	int err1 =  unregister_strdev(devmajor1,&arnet_info,0);
	int err2 =  unregister_strdev(devmajor2,&arnet_tinfo,arnet_ncards);
ldprintf(".%d.",__LINE__);
	arnet_exit();
	return err1 || err2;
}
#endif
