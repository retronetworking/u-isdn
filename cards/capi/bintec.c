/* Teile dieses Codes: */
/******************************************************************************
*
*	(C)opyright 1993 BinTec Computersysteme GmbH
*	All Rights Reserved
*
******************************************************************************/

/* Der Rest: */
/*
 *
 * ISDN driver for CAPI cards: BINTEC.
 *
 * Copyright (c) 1993-1995 Matthias Urlichs <urlichs@noris.de>.
 */

#ifdef linux
#define SLOW_IO_BY_JUMPING
#endif

#include "f_module.h"
#include "primitives.h"
#include "streams.h"
#include "isdn_12.h"
#include "isdn_34.h" /* CMD_CARDPROT */
#include "isdn_proto.h"
#include "smallq.h"
#include "isdn_limits.h"
#include "stream.h"
#include "streamlib.h"
#include <sys/errno.h>
#include <sys/sysmacros.h>
#include <stddef.h>
#include "loader.h"
#include "kernel.h"

#ifdef linux
#include <asm/byteorder.h> /* htons and friends */
#include <linux/malloc.h>
#endif

#include "bri.h"
#include "bintec.h"
#include "capi.h"

#ifdef linux
#define SetSPL(x) spl5()
#else
#define SetSPL(x) spl((x))
#endif

extern void log_printmsg (void *log, const char *text, mblk_t * mp, const char*);
static void reset_card(struct _bintec *bp);
static void toss_unknown (struct _bintec *bp);
static void process_unknown (struct _bintec *bp);

#ifdef linux
void NAME(REALNAME,poll)(struct _bintec *bp);
#else
void NAME(REALNAME,poll)(void *);
#endif

#define MAXSEND 0x0D00 /* 3000 bytes. Split bigger blocks according to CAPI. */


static struct _bintec *bintecmap[16] = { NULL, };


/**** START of Bintec-provided code ******/


/*********************************************************************
	bd_memchk

	checks shared memory, for correct byte, word, dword access
**********************************************************************/
static int
bd_memchk(volatile void far *base, unsigned size)
{
    unsigned i;

    size /= sizeof(long);

	{
		volatile unchar far *p;

		p = (volatile unchar far *) base;
		for (i=0; i<256; ++i) {
			p[i] = (unchar) i;
			if (p[i] != i) {
				printf("board not present, addr %p, %d is %d\n",p,i,p[i]);
				return -EIO;
			}
		}
	}
	{
		volatile unchar far *p;

		p = (volatile unchar far *) base;
		for (i=0; i<8; ++i)
			p[i] = (1 << i);
		for (i=0; i<8; ++i) {
			if (p[i] != (1 << i)) {
				printf("byte access failed\n");
				return -EIO;
			}
		}
	}
	{
		volatile ushort far *p;

		p = (volatile ushort far *) base;
		for (i=0; i<16; ++i) 
			p[i] = (1 << i);
		for (i=0; i<16; ++i) {
			if (p[i] != (1 << i)) {
				printf("word access failed\n");
				return -EIO;
			}
		}
	}
	{
		volatile ulong far *p;

		p = (volatile ulong far *) base;
		for (i=0; i<32; ++i) 
			p[i] = (1 << i);
		for (i=0; i<32; ++i) {
			if (p[i] != (1 << i)) {
				printf("long access failed\n");
				return -EIO;
			}
		}
	}
	{
		volatile unchar far *b;
		volatile ushort far *s;

		b = (volatile unchar far *) base;
		s = (volatile ushort far *) base;

		b[0] = 0x12;
		b[1] = 0x34;
		if (s[0] != htons(0x1234)) {
			printf("word order mismatch\n");
			return -ENXIO;
		}
	}
	{
		ulong val;
		volatile ulong far *ptr;

		val = 0x31415926;               /*  set initial value  */
		ptr = base;                     /*  get memory pointer  */
		for (i=0; i < size; ++i) {      /*  loop thru memory  */
			*ptr++ = val;               /*  store value  */
			val += 0x12345678;          /*  adjust value  */
		}
		val = 0x31415926;               /*  set initial value  */
		ptr = base;                     /*  get memory pointer  */
		for (i=0; i<size; ++i) {        /*  loop thru memory  */
			if (*ptr++ != val) {        /*  check value  */
				printf("memory check failed: i 0x%x, val 0x%lx (wanted 0x%lx)\n",i,*--ptr,val);
				return -EIO;
			}
			val += 0x12345678;          /*  adjust value  */
		}
	}
    return 0;  /*  success  */
}

/*********************************************************************
	bd_check

	checks if board present
**********************************************************************/
static int
bd_check(struct _bintec *bp)
{
    unsigned size;

    if (!bp->info.memaddr) { /*  board not present  */
		printf("BINTEC: no memory address given!\n");
		return -EINVAL;
    }

    bp->base = (unchar far *) bp->info.memaddr;

    bp->ctrl  = (unchar far *) (bp->base + 0x3ffe);
    *bp->ctrl = 0xff;
	if ((*(bp->ctrl) >> 5) != BOARD_ID_PMX) {
		bp->type = *(bp->ctrl) >> 5;
		bp->ctrl  = (unchar far *) (bp->base + 0x3ffe);
		bp->state = (unchar far *) (bp->base + 0x3ffc);
		bp->debugtext = (unchar far *) (bp->base + 0x3ffd);
		bp->rcv.p = (unchar far *) (bp->base + 0x0008);
		bp->snd.p = (unchar far *) (bp->base + 0x2000);
		bp->rcv.d = (icinfo_t far *) (bp->base + 0x3ff0);
		bp->snd.d = (icinfo_t far *) (bp->base + 0x3ff6);
		size  = 16 * 1024 - 4;
	} else {
		bp->type = BOARD_ID_PMX;
		bp->ctrl  = (unchar far *) (bp->base + 0xfffe);
		bp->state = (unchar far *) (bp->base + 0x3ffc);
		bp->debugtext = (unchar far *) (bp->base + 0x3ffd);
		bp->rcv.p = (unchar far *) (bp->base + 0x0008);
		bp->snd.p = (unchar far *) (bp->base + 0x2000);
		bp->rcv.d = (icinfo_t far *) (bp->base + 0x3ff0);
		bp->snd.d = (icinfo_t far *) (bp->base + 0x3ff6);
		size  = 64 * 1024 - 4;
	}

    if (BOARD_TYPE(bp) != BOARD_ID_BRI
		&& BOARD_TYPE(bp) != BOARD_ID_BRI4
		&& BOARD_TYPE(bp) != BOARD_ID_PMX
		&& BOARD_TYPE(bp) != BOARD_ID_X21
    		) {
		printf( "BINTEC: board ctrl 0x%02x: ID unknown\n", *bp->ctrl);
		return -ENXIO;
    }
    CTRL_RESET(bp);             /*  reset board   */

    return bd_memchk((void *)bp->base, size);    /*  memory check  */
}



/*********************************************************************
	bd_msgout

	prints output from boards debug register
**********************************************************************/
static int
bd_msgout( struct _bintec *bp )
{
    unsigned char c;
    static char newline = 1;
	int count = 0;

    while ((c = *bp->debugtext) != 0) {
		*bp->debugtext = 0;   /*  clear byte  */
		if(!newline || (c != '\n')) { /* do not print empty lines */
			if (newline) printf("%sBINTEC: debug: ",KERN_DEBUG);
			newline = (c == '\n');
			printf("%c", c);  /*  display byte  */
		}
		if(++count > 1000) {
			printf("\n%sBINTEC: The board seems to be in an infinite loop.\n",KERN_ERR);
			if(bp->waitmsg == 0)
				isdn2_new_state(&bp->card,2);
			CTRL_RESET(bp);
			return -EIO;
		}
    }

    if (*bp->state & 0x80) {      /*  board failed  */
		CTRL_RESET(bp);           /*  reset board  */
		printf("BINTEC: msgout: board failed???\n");
		if(bp->waitmsg == 0)
			isdn2_new_state(&bp->card,2);
		return -EIO;
    }
    return 0;
}


/*********************************************************************
	bd_init

	initialize structures to point to boards memory
**********************************************************************/
static int
bd_init( struct _bintec *bp )
{
    if (!bp->info.memaddr) {
		printf("bd_init: board addr not present\n");
		return -EFAULT;
    }
    bp->base  = (unchar far *) bp->info.memaddr;
    bp->ctrl  = (unchar far *) (bp->base + 0x3ffe);
    *bp->ctrl = 0xff;
    if ((*(bp->ctrl) >> 5) != BOARD_ID_PMX) {
		bp->type = *(bp->ctrl) >> 5;
		bp->ctrl  = (unchar far *)   (bp->base + 0x3ffe);
		bp->state = (unchar far *)   (bp->base + 0x3ffc);
		bp->debugtext = (unchar far *)   (bp->base + 0x3ffd);
		bp->ctrl  = (unchar far *)   (bp->base + 0x3ffe);
		bp->rcv.p = (unchar far *)   (bp->base + 0x0008);
		bp->snd.p = (unchar far *)   (bp->base + 0x2000);
		bp->rcv.d = (icinfo_t far *) (bp->base + 0x3ff0);
		bp->snd.d = (icinfo_t far *) (bp->base + 0x3ff6);
    } else {
		bp->type = BOARD_ID_PMX;
		bp->ctrl  = (unchar far *)   (bp->base + 0xfffe);
		bp->state = (unchar far *)   (bp->base + 0x3ffc);
		bp->debugtext = (unchar far *)   (bp->base + 0x3ffd);
		bp->rcv.p = (unchar far *)   (bp->base + 0x0008);
		bp->snd.p = (unchar far *)   (bp->base + 0x2000);
		bp->rcv.d = (icinfo_t far *) (bp->base + 0x3ff0);
		bp->snd.d = (icinfo_t far *) (bp->base + 0x3ff6);
    }
	switch(bp->type) {
	case BOARD_ID_PMX:
		bp->card.nr_chans = 30;
		bp->card.nr_dchans = 1;
		break;
	case BOARD_ID_BRI:
		bp->card.nr_chans = 2;
		bp->card.nr_dchans = 1;
		break;
	case BOARD_ID_BRI4:
		bp->card.nr_chans = 8;
		bp->card.nr_dchans = 4;
		break;
	default:
		printf("BINTEC: unknown board ID %d\n",bp->type);
		return -EIO;
	}
	return 0;
}



/**** END of Bintec-provided code *****/

static void
reset_card(struct _bintec *bp)
{
	int i = 0;
	long s = splstr();

	*bp->state = 0; /* turn off interrupt generation */
	*bp->ctrl = 0; /* halt and reset CPU */

	for(i=0; i <= MAX_B; i++) {
		S_flush(&bp->chan[i].q_in);
		S_flush(&bp->chan[i].q_out);
		bp->chan[i].mode = M_OFF;
		if(bp->chan[i].in_more != NULL) {
			freemsg(bp->chan[i].in_more);
			bp->chan[i].in_more = NULL;
		}
	}
	S_flush(&bp->q_unknown);
	if(bp->unknown_timer) {
		bp->unknown_timer = 0;
#ifdef NEW_TIMEOUT
		untimeout(bp->timer_toss_unknown);
#else
		untimeout(toss_unknown, bp);
#endif
	}
	bp->sndoffset = -1;
	bp->rcvoffset = -1;
	bp->msgnr = 0;
	bp->waitmsg = 999;
	splx(s);
}

inline static void
setctl(struct _bintec *bp, int bit)
{
	bp->cflag |= (1<<bit);
	*bp->ctrl = bp->cflag;
}

inline static void
clrctl(struct _bintec *bp, int bit)
{
	bp->cflag &= ~(1<<bit);
	*bp->ctrl = bp->cflag;
}

inline static void
put8(struct _bintec *bp, u8 byt)
{
    bp->snd.p[bp->sndoffset] = byt;
	bp->sndoffset++;
	if(bp->sndoffset == bp->sndbufsize)
		bp->sndoffset = 0;
}

inline static void
put16(struct _bintec *bp, u16 byt)
{
	put8(bp, byt & 0xFF);
	put8(bp, byt >> 8);
}

inline static void
put32(struct _bintec *bp, u32 byt)
{
	put8(bp, byt & 0xFF);
	put8(bp, byt >>  8);
	put8(bp, byt >> 16);
	put8(bp, byt >> 24);
}

inline static void
putmb(struct _bintec *bp, mblk_t *mb, int len)
{
	while(mb != NULL && len > 0) {
		streamchar *pos = mb->b_rptr;
		while(pos < mb->b_wptr) {
			put8(bp,*pos++);
			if(--len == 0)
				break;
		}
		mb = mb->b_cont;
	}
}

static int
putstart(struct _bintec *bp, int len)
{
    int wi = ntohs(bp->snd.d->wi);  /*  get write index  */
    int ri = ntohs(bp->snd.d->ri);  /*  get read index  */
    int sz = ntohs(bp->snd.d->sz);  /*  get buffer size  */
	int space = (ri - wi + sz - 1) % sz;  /*  calc size  */
	int err;
	unsigned long ms = SetSPL(bp->ipl);

	if((err = bd_msgout(bp)) < 0)  {
		splx(ms);
		return err;
	}
    if ((sz < 2) || (wi >= sz) || (ri >= sz)) {
		printf( "BINTEC error: write: ri %d, wi %d, sz %d\n",ri,wi,sz);
		bp->sndoffset = sz;
		splx(ms);
		return -EIO;
    }
	if(len > sz-3) {
		printf("BINTEC error: write len %d > sz-3 %d\n",len,sz-3);
		bp->sndoffset = sz;
		splx(ms);
		return -ENXIO;
	}
	if(bp->sndoffset != -1) {
		if(bp->sndoffset != bp->sndbufsize)
			DEBUG(info) printf("BINTEC: busy sending\n");
		splx(ms);
		return -EAGAIN; /* busy */
	}
	if(len > space-2) {
		DEBUG(info) printf("BINTEC: buffer full, %d < %d\n",len,space-2);
		splx(ms);
		return -EAGAIN;
	}
	if(0)DEBUG(capiout) printf("BINTEC write %d bytes at %d, free %d\n",len,wi,space);
	bp->sndoffset = wi;
	bp->sndbufsize = sz;
	bp->sndend = (wi+len+2) % sz;
	splx(ms);
	put16(bp, htons(len));
	return 0;
}

static int
putend(struct _bintec *bp)
{
	int ms = SetSPL(bp->ipl);
	int err;

	if((err = bd_msgout(bp)) < 0)  {
		splx(ms);
		return err;
	}
	if(bp->sndoffset != bp->sndend) {
		DEBUG(info) printf("BINTEC error: send end %d, should be %d\n",bp->sndoffset,bp->sndend);
		bp->sndoffset = bp->sndbufsize;
		return -EIO;
	}
    bp->snd.d->wi = htons(bp->sndoffset);
	if(bp->type != BOARD_ID_PMX) {
		setctl(bp,4);
		clrctl(bp,4);
	}
	bp->sndoffset = -1;
	splx(ms);
	return 0;
}


inline static u8
get8(struct _bintec *bp)
{
	u8 byt;
	byt = bp->rcv.p[bp->rcvoffset];
	bp->rcvoffset++;
	if(bp->rcvoffset == bp->rcvbufsize)
		bp->rcvoffset = 0;
	return byt;
}

inline static void
getflush(struct _bintec *bp, int len)
{
	if(len > 0) {
		bp->rcvoffset += len;
		if (bp->rcvoffset >= bp->rcvbufsize)
			bp->rcvoffset -= bp->rcvbufsize;
	} else {
		bp->rcvoffset = bp->rcvend;
	}
}

inline static u16
get16(struct _bintec *bp)
{
	u16 byt;
	byt = get8(bp);
	byt |= get8(bp) << 8;
	return byt;
}

inline static u32
get32(struct _bintec *bp)
{
	u32 byt;
	byt  = get8(bp);
	byt |= get8(bp) << 8;
	byt |= get8(bp) << 16;
	byt |= get8(bp) << 24;
	return byt;
}

inline static void
getmb(struct _bintec *bp, mblk_t *mb, int len)
{
	while(len--)
		*mb->b_wptr++ = get8(bp);
}

static int
getstart(struct _bintec *bp)
{
    int wi = ntohs(bp->rcv.d->wi);  /*  get write index  */
    int ri = ntohs(bp->rcv.d->ri);  /*  get read index  */
    int sz = ntohs(bp->rcv.d->sz);  /*  get buffer size  */
	int space = (wi - ri + sz) % sz;  /*  calc size  */
	int err, len;
	unsigned long ms = SetSPL(bp->ipl);

	if((err = bd_msgout(bp)) < 0)  {
		splx(ms);
		return err;
	}
	if(bp->rcvoffset != -1) {
		if(bp->rcvoffset != sz)
			DEBUG(info) printf("BINTEC: read: receiver busy\n");
		return -EAGAIN; /* busy */
	}

    if (sz < 2) {              /*  invalid buffer size  */
		printf( "BINTEC error: read: invalid buffer size %d\n", sz);
		bp->rcvoffset = sz;
		splx(ms);
		return -EIO;
    }
	if(space == 0) {
		splx(ms);
		/* no message to read */
		return -ENODATA;
	}
    if (space > sz || space < 2) {     /* we have a problem */
		printf("BINTEC error: read: invalid space %d\n", space);
		bp->rcvoffset = sz;
		splx(ms);
		return -EFAULT;
    }

	bp->rcvoffset = ri;
	bp->rcvbufsize = sz;
	splx(ms);

	len = ntohs(get16(bp));
	if(len > space-2) {
		printf("BINTEC error: read: len %d > space-2 %d\n",len,space-2);
		bp->rcv.d->ri = bp->rcv.d->wi;
		bp->rcvoffset = sz;
		return -EFAULT;
	}
	if(0)DEBUG(capi) printf("BINTEC: reading %d bytes at %d\n",len,ri);
	bp->rcvend = (ri + len + 2) % sz;
	return len;
}

static int
getend(struct _bintec *bp)
{
	if(bp->rcvoffset != bp->rcvend) {
		printf("BINTEC error: read: now at %d instead of %d!\n",bp->rcvoffset,bp->rcvend);
		bp->rcvoffset = bp->rcvbufsize;
		return -EIO;
	}
    bp->rcv.d->ri = htons(bp->rcvoffset);  /*  update read index  */
	bp->rcvoffset = -1;
	if(bp->type != BOARD_ID_PMX) {
		setctl(bp,4);
		clrctl(bp,4);
	}
	return 0;
}



static int
init1 (struct _bintec *bp)
{
	int err;
	err = bd_init(bp);
	if(err < 0)
		return err;
	err = bd_check(bp);
	if(err < 0)
		return err;
	bp->waitmsg = 999;
	return 0;
}

static int
boot(struct _isdn1_card * card, int step, int offset, mblk_t *data)
{
	struct _bintec * bp = (struct _bintec *) card;
	int err, i;
	mblk_t *origdata = data;

   	if ((err = bd_msgout(bp)) < 0) {
		return err;
   	}
	DEBUG(info) if(offset == 0 || msgdsize(data) < 100) printf("BINTEC boot: step %d offset %d bytes %d for type %d\n",step,offset,msgdsize(data),bp->type);
	switch(step) {
	case 1:
		if(offset == 0) {
			volatile uchar_t *ptr;
			{
				mblk_t *mp;

				if(msgdsize(data) < 2) {
					err = -ENXIO;
					break;
				}
				mp = pullupm(data,2);
				if(mp == NULL)
					return -ENOMEM;
				data = mp;
			}

			reset_card(bp);

			err = bd_check(bp);
			if(err < 0)
				return err;

			bzero((void *)bp->base, 0x3ffe);   	/*  clear shared memory  	*/
			bp->rcv.d->sz = htons(0x1ff8);  	/*  set size  			*/
			bp->snd.d->sz = htons(0x1ff0);  	/*  set size  			*/
			ptr = bp->base;             	/*  get pointer to reset vector */
			*((u32 *)ptr)++ = htonl(0x2000); /* SP */
			*((u32 *)ptr)++ = htonl(0x0200); /* PC */
			for (i=0; i<504; ++i) {     	/*  clear vector table  	*/
				*ptr++ = 0xff;
			}

			data->b_rptr[0] = 0x70;
			data->b_rptr[1] = bp->type;
		}
		if(msgdsize(data) != 0) {
			if(0)DEBUG(capiout) {
				printf("BINTEC: write %d bytes to card at %x",msgdsize(data),0x200+offset);
				log_printmsg(NULL,": ",data,">>");
			}
			while(data != NULL) {
				bcopy(data->b_rptr,(char *)(bp->info.memaddr) +0x200 + offset,data->b_wptr-data->b_rptr);
				offset += data->b_wptr-data->b_rptr;

				data = data->b_cont;
			}
		} else {
    		if (BOARD_TYPE(bp) != BOARD_ID_PMX) {
				CTRL_SET(bp, 0x0e);
    		} else {
				CTRL_SET(bp, 0x03);
    		}
			DEBUG(info) printf("BINTEC: Boot phase 1 complete; card started\n");
			err = bd_msgout(bp);
		}
		break;
	case 2:
		if(offset == 0) {
			{
				mblk_t *mp;

				if(msgdsize(data) < 2) {
					err = -ENXIO;
					break;
				}
				mp = pullupm(data,2);
				if(mp == NULL)
					return -ENOMEM;
				data = mp;
			}

			data->b_rptr[0] = 0x70;
			data->b_rptr[1] = bp->type;
		}
		{
			int len = msgdsize(data);
			int err = putstart(bp,len);

			if(0)DEBUG(capiout) {
				printf("BINTEC: write %d bytes to card",len);
				log_printmsg(NULL,": ",data,">>");
			}

			if(err < 0)
				return err;
			putmb(bp,data,len);
			err = putend(bp);
			if(err < 0)
				return err;
		}
		break;
	case 3:
		if(msgdsize(data) != 0) {
			err = -EIO;
			break;
		}
		bp->waitmsg = 2;
		if(bp->info.irq != 0) {
			setctl(bp,0); /* enable interrupts */
			*bp->state = 3; /* ... send and receive */
		}
		break; /* boot is finished */
	default:
		return -EFAULT;
	}
	if(err == 0)
		freemsg(origdata);
	return err;
}

static int
bintec_mode (struct _isdn1_card * card, short channel, char mode, char listen)
{
	struct _bintec * bp = (struct _bintec *) card;
	unsigned long ms = SetSPL(bp->ipl);
	int err = 0;

	switch(channel) {
	case 0:
		if(mode == M_OFF) {
			reset_card(bp);		
			splx(ms);
			return 0;
		}
		splx(ms);
		return -ENXIO;
	default:
		if(channel > 0 && channel <= bp->card.nr_chans) {
			DEBUG(info) printf("%sBINTEC: Chan%d %s<%d>%s\n",KERN_INFO ,channel,mode?"up":"down",mode,listen?" listen":"");
			bp->chan[channel].mode = mode;
			if(mode == M_OFF) {
				bp->chan[channel].appID = 0;
				bp->chan[channel].PLCI = 0;
				bp->chan[channel].NCCI = 0;
			}
			splx(ms);
			return 0;
		} else {
			printf("%sBintec badChan %d\n",KERN_WARNING ,channel);
			splx(ms);
			return -EINVAL;
		}
	}
	NAME(REALNAME,poll)(bp);
	splx(ms);
	return err;
}

static int
bintec_prot (struct _isdn1_card * card, short channel, mblk_t * mp, int flags)
{
    struct _bintec * bp = (struct _bintec *)card;
    streamchar *origmp = mp->b_rptr;
    ushort_t id;
    int err = 0;
	hdlc_buf chan = &bp->chan[channel];

    DEBUG(info) printf("Prot chan %d flags 0%o\n",channel,flags);

    if(!(flags & ~CHP_FROMSTACK)) { /* Nothing else set? */
        if ((err = m_getid (mp, &id)) != 0)
            goto err;
        if(flags & CHP_FROMSTACK) { /* from the application */
			switch(id) {
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
					if(bp->maxoffset < z)
						bp->maxoffset = z;
					bp->chan[channel].offset = z;
				}
				err = -ERESTART;
				break;
			}
		} else { /* from the system */
			switch (id) {
			default:
				err = -ERESTART;
				break;
			case CMD_CARDPROT:
				{
					while(m_getsx(mp,&id) == 0) {
						switch(id) {
						case ARG_ASSOC:
							{ /* appID plci ncci */
								long a,n,p;
								if((err = m_geti(mp,&a)) < 0)
									break;
								if((err = m_geti(mp,&p)) < 0)
									break;
								if((err = m_geti(mp,&n)) < 0)
									break;
								chan->appID = a;
								chan->PLCI = p;
								chan->NCCI = n;
								DEBUG(capi) printf("BINTEC: chan %d: assoc %04lx %04lx %04lx\n",channel,a,p,n);
								process_unknown(bp);
							}
							break;
						}
					}
				}
				if(err == 0) {
					mblk_t *mz = make_reply(err);
					if(mz != NULL) {
    					mp->b_rptr = origmp;
						mp->b_datap->db_type = mz->b_datap->db_type;
						linkb(mz,mp);
						if((err = isdn2_chprot(card,channel,mz,flags|CHP_FROMSTACK)) < 0)
							freeb(mz);
						else 
							mp = NULL;
					}
				}
				break;
			}
		}
    }
	if(err == 0) {
		if(mp != NULL)
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
bintec_candata (struct _isdn1_card * card, short channel)
{
	struct _bintec * bp = (struct _bintec *)card;
	if(bp->waitmsg)
		return 0;
	return (bp->chan[channel].q_out.nblocks < 4);
}

/*
 * Enqueue the data.
 */
static int
bintec_data (struct _isdn1_card * card, short channel, mblk_t * data)
{
	struct _bintec * bp = (struct _bintec *)card;
	if(bp->waitmsg)
		return -ENXIO;
	S_enqueue(&bp->chan[channel].q_out, data);
	NAME(REALNAME,poll)((struct _bintec *) card);
	return 0;
}

/*
 * Flush the send queue.
 */
static int
bintec_flush (struct _isdn1_card * card, short channel)
{
	struct _bintec * bp = (struct _bintec *)card;
	if(channel > 0)
		S_flush(&bp->chan[channel].q_out);
	return 0;
}


static int
sendone(struct _bintec *bp, int thechan)
{
	int len, hlen, err;
	struct _hdlc_buf *chan = &bp->chan[thechan];
	mblk_t *mb;

	if(thechan != 0 && chan->waitflow > 5)
		return 0; /* XXX  -EAGAIN ? */
	mb = S_dequeue(&chan->q_out);
	if(mb == NULL)
		return 0;

	len = msgdsize(mb);

	err = bd_msgout(bp);
	if(err < 0) {
		freemsg(mb);
		return err;
	}
	if(thechan != 0)
		hlen = 21;
	else
		hlen = 2;
	DEBUG(capiout) {
		if(thechan == 0) {
			struct CAPI_every_header *capi;
			capi = ((typeof(capi))mb->b_rptr);
			if(capi->PRIM_type == CAPI_ALIVE_RESP)
				goto foo;
		}
		printf("BINTEC: Send %d bytes on chan %d",len,thechan);
		if(thechan == 0)
			log_printmsg(NULL,": ",mb,">>");
		else
			printf("\n");
		foo:;
	}
	err = putstart(bp,hlen + ((len > MAXSEND) ? MAXSEND : len)); /* auto-puts the length */
	if(err >= 0) {
		put16(bp, htons(1));
		if(thechan != 0) { /* data */
			put16(bp, 19); /* msg len, including header */
			put16(bp, chan->appID); /* AppID */
			put16(bp, CAPI_DATAB3_REQ);
			put16(bp, bp->msgnr++ & 0x3FFF);
			put16(bp, chan->NCCI); 
			put16(bp, (len > MAXSEND) ? MAXSEND : len); 
			put32(bp, 0);
			put8 (bp, ++chan->dblock);
			put16(bp, (len > MAXSEND) ? CAPI_MORE : 0); /* flags */
		}
		putmb(bp,mb,MAXSEND);
		err = putend(bp);
		if(err < 0) 
			S_requeue(&chan->q_out,mb);
		else {
			if(len > MAXSEND) {
				adjmsg(mb,len-MAXSEND);
				mb = pullupm(mb,0);
				S_requeue(&chan->q_out,mb);
			} else
				freemsg(mb);
			if(thechan != 0)
				chan->waitflow++;
			if(BOARD_TYPE(bp) != BOARD_ID_PMX) {
				*bp->ctrl = bp->cflag | 0x10;
				*bp->ctrl = bp->cflag;
			} else
				CTRL_SET(bp,1);
		}
	} else if(err == -EAGAIN) {
		S_requeue(&chan->q_out,mb);
	} else {
		freemsg(mb);
		/* TODO: Kill the board? */
	}
	return err;
}


static int
pushone(struct _bintec *bp, int thechan)
{
	int err;
	struct _hdlc_buf *chan = &bp->chan[thechan];
	mblk_t *mb;

	if(!isdn2_canrecv(&bp->card,thechan)) {
		return -EAGAIN;
	}
	mb = S_dequeue(&chan->q_in);
	if(mb->b_cont == NULL) {
		freeb(mb);
		return -EINVAL;
	}
	{
		struct CAPI_every_header *capi, *capi2;
		struct CAPI_datab3_ind *ci;
		struct CAPI_datab3_resp *cr;
		mblk_t *mr = allocb(sizeof(*capi)+sizeof(*cr),BPRI_HI);
		if(mr != NULL) {
			capi  = ((typeof(capi ))mb->b_rptr);
			capi2 = ((typeof(capi2))mr->b_wptr)++;
			cr = ((typeof(cr))mr->b_wptr)++;
			ci =  (typeof(ci))(capi+1);
			capi2->len = mr->b_wptr-mr->b_rptr;
			capi2->appl = capi->appl;
			capi2->PRIM_type = CAPI_DATAB3_RESP;
			capi2->messid = capi->messid;
			cr->ncci = ci->ncci;
			cr->blknum = ci->blknum;

			if(ci->flags & CAPI_MORE) {
				if(chan->in_more == NULL) 
					chan->in_more = mb->b_cont;
				else
					linkb(chan->in_more,mb->b_cont);
				err = 0;
			} else {
				if(chan->in_more != NULL) {
					linkb(chan->in_more,mb->b_cont);
					mb->b_cont = chan->in_more;
					chan->in_more = NULL;
				}
				err = isdn2_recv(&bp->card,thechan,mb->b_cont);
			}
			if(err == 0) {
				freeb(mb);
				S_enqueue(&bp->chan[0].q_out,mr);
				sendone(bp,0);
			} else if(err == -EAGAIN) {
				S_requeue(&chan->q_in, mb);
				freemsg(mr);
			} else {
				/* XXX TODO: send a toss message, or close the channel */
				freemsg(mb);
				freemsg(mr);
			}
		} else {
			S_requeue(&chan->q_in, mb);
			freemsg(mb);
			err = -ENOMEM;
		}
	}
	return err;
}

static int
postproc(struct _bintec *bp, mblk_t *mb, int ch)
/* ch: pre-found channel; -1: not found; 0: unknown */
{
	int err;
	struct CAPI_every_header *capi;

	capi = (typeof(capi))mb->b_rptr;

	DEBUG(capi) if(capi->PRIM_type != CAPI_ALIVE_IND) log_printmsg(NULL,"BINTEC read packet:",mb,"> ");

	switch(capi->PRIM_type) {
	case CAPI_ALIVE_IND:
		err = 0;
		if(!isdn2_canrecv(&bp->card,0)) 
			goto def;
		capi->PRIM_type = CAPI_ALIVE_RESP;
		if(bp->chan[0].q_out.nblocks < 100)
			S_enqueue(&bp->chan[0].q_out,mb);
		else {
			isdn2_chstate(&bp->card,MDL_ERROR_IND,0);
			return -EIO;
		}
		sendone(bp,0);
		break;
	case CAPI_DATAB3_IND:
		{
			struct _hdlc_buf *chan = &bp->chan[1];
			if(ch == 0) {
				struct CAPI_datab3_ind *c2;
				int i;

				ch = -1;
				c2 =  (typeof(c2))(capi+1);
				for(i=1;i<bp->card.nr_chans;i++) {
					if(chan->appID == capi->appl && chan->NCCI == c2->ncci) {
						ch = i;
						break;
					}
					chan++;
				}
				if(ch == -1) 
					return -ERESTART;
			} else if(ch > 0)
				chan = &bp->chan[ch];
			else
				return -ERESTART;
			if(chan->q_in.nblocks < 200)
				S_enqueue(&chan->q_in,mb);
			else {
				freemsg(mb);
				/* TODO: Throw the connection away. Right now we kill the
				   card instead. */
				isdn2_chstate(&bp->card,MDL_ERROR_IND,0);
			}
			pushone(bp,ch);
		}
		break;
	case CAPI_DATAB3_CONF:
		{
			struct _hdlc_buf *chan = &bp->chan[1];
			if(ch == 0) {
				struct CAPI_datab3_ind *c2;
				int i;

				ch = -1;
				c2 = (typeof(c2))(capi+1);
				for(i=1;i<bp->card.nr_chans;i++) {
					if(chan->appID == capi->appl && chan->NCCI == c2->ncci) {
						ch = i;
						break;
					}
					chan++;
				}
				if(ch == -1) 
					return -ERESTART;
			} else if(ch > 0)
				chan = &bp->chan[ch];
			else
				return -ERESTART;
			if(chan->waitflow) 
				chan->waitflow--;
			sendone(bp,ch);
		}
		break;
	default:
	def:
		if(!isdn2_canrecv(&bp->card,0)) {
			printf("BINTEC read: cannot accept packet\n");
			if(bp->chan[0].q_in.nblocks < 100)
				S_enqueue(&bp->chan[0].q_in,mb);
			else {
				printf("BINTEC: incoming queue full\n");
				isdn2_chstate(&bp->card,MDL_ERROR_IND,0);
			}
			return 0;
		} else if((err = isdn2_recv(&bp->card,0,mb)) != 0) {
			printf("BINTEC read error: err %d\n",err);
			return err;
		}
		break;
	}

	return 0;
}

static void
toss_unknown (struct _bintec *bp)
{
	mblk_t *mb;
	while((mb = S_dequeue(&bp->q_unknown)) != NULL)
		freemsg(mb);
}

static void
process_unknown (struct _bintec *bp)
{
	if(bp->q_unknown.nblocks == 0)
		return;
	
	if(bp->unknown_timer) {
		bp->unknown_timer = 0;
#ifdef NEW_TIMEOUT
		untimeout(bp->timer_toss_unknown);
#else
		untimeout(toss_unknown, bp);
#endif
	}
	{
		struct _smallq sq = bp->q_unknown;
		mblk_t *mb;

		bzero(&bp->q_unknown,sizeof(bp->q_unknown));
		while((mb = S_dequeue(&sq)) != NULL) {
			switch(postproc(bp,mb,0)) {
			case 0:
				break;
			default:
				{
					struct CAPI_every_header *capi, *capi2;
					struct CAPI_datab3_ind *ci;
					struct CAPI_datab3_resp *cr;
					mblk_t *mr = allocb(sizeof(*capi)+sizeof(*cr),BPRI_HI);
					if(mr != NULL) {
						capi  = ((typeof(capi ))mr->b_rptr);
						capi2 = ((typeof(capi2))mr->b_wptr)++;
						cr = ((typeof(cr))mr->b_wptr)++;
						ci =  (typeof(ci))(capi+1);
						*capi2 = *capi;
						capi2->PRIM_type = CAPI_DATAB3_RESP;
						cr->ncci = ci->ncci;
						cr->blknum = ci->blknum;
						S_enqueue(&bp->chan[0].q_out,mr);
						sendone(bp,0);
					}
					freemsg(mb);
				}
			case -EAGAIN:
			case -ERESTART:
				S_enqueue(&bp->q_unknown,mb);
				bp->unknown_timer = 1;
				break;
			}
		}
		
	}
	if(bp->unknown_timer) {
#ifdef NEW_TIMEOUT
		bp->timer_toss_unknown =
#endif
			timeout((void *)toss_unknown,bp,10*HZ);
	}
}

static void
DoIRQ(struct _bintec *bp)
{
	int lastpos;
	int err = bd_msgout(bp);
	if(err < 0)
		return; /* HW fail */
	if(bp->waitmsg > 9)
		return; /* not yet */

	while(err >= 0) {
		err = getstart(bp);
		if(err >= 0) {
			int len = err;
			if((err < 4) || (bp->waitmsg > 0)) {
				if((err == 1) && (bp->waitmsg > 0)) {
					if(!--bp->waitmsg) {
						DEBUG(info) printf("BINTEC: card is online\n");
						isdn2_new_state(&bp->card,1);
					}
				}
				getflush(bp,len);
				err = getend(bp);
				continue;
			}
			err = get16(bp);
			len -= 2;
			if(err == htons(1)) {
				int ch = 0;
				int capilen = get16(bp);
				mblk_t *mb = allocb(capilen,BPRI_HI);
				struct CAPI_every_header *capi;
				if(mb == NULL) {
					DEBUG(info)printf("BINTEC:read: no mem for %d bytes\n",err),
					getflush(bp,len);
					getend(bp);
					err = -ENOMEM;
					break;
				}
				*((ushort_t *)mb->b_wptr)++ = capilen;
				getmb(bp,mb,capilen-2);
				capi = (typeof(capi))mb->b_wptr;

				if(len > capilen) { /* Data msg? */
					int offset = -1;
					switch(capi->PRIM_type) {
					case CAPI_DATAB3_IND:
						{
							struct CAPI_datab3_ind *c2;
							int i;
							struct _hdlc_buf *chan = &bp->chan[1];

							c2 = (typeof(c2))(capi+1);
							for(i=1;i<bp->card.nr_chans;i++) {
								if(chan->appID == capi->appl && chan->NCCI == c2->ncci) {
									offset = chan->offset;
									ch = i;
									break;
								}
								chan++;
							}
							if(offset < 0) {
								ch = -1;
								offset = bp->maxoffset; /* Not yet */
							}
						}
						break;
					default:
						offset = 0;
						break;
					}
					{
						mblk_t *m2 = allocb(len-capilen+offset,BPRI_HI);
						if(m2 == NULL) {
							getflush(bp,len-capilen);
							printf("%sBINTEC error: No memory for %d bytes\n",KERN_ERR,len-capilen);
						} else {
							m2->b_rptr += offset;
							m2->b_wptr += offset;
							getmb(bp,m2,len-capilen);
							linkb(mb,m2);
						}
					}
				} else if(len < capilen) {
					DEBUG(info)printf("BINTEC:read: want %d bytes, got %d\n",capilen,len),
					freemsg(mb);
					continue;
				}
				switch(capi->PRIM_type) {
				case CAPI_DATAB3_CONF:
					break;
				}
				if(err < 0)
					getend(bp);
				else
					err = getend(bp);
				if(err >= 0) {
					err = postproc(bp,mb,ch);
					if(err == -ERESTART) {
						static int prcnt;
						if(bp->q_unknown.nblocks > 100) {
							if(prcnt < 3) {
								prcnt++;
								DEBUG(info)printf("BINTEC:read: 'unknown data' queue full\n");
							}
							freemsg(mb);
						} else {
							S_enqueue(&bp->q_unknown,mb);
							if(prcnt)
								prcnt--;
						}
						err = 0;
					}
				}
				if (err < 0)
					freemsg(mb);
			} else {
				mblk_t *mb = allocb(len+2,BPRI_LO);
				if(mb != NULL) {
					*((ushort_t *)mb->b_wptr)++ = ntohs(err);
					getmb(bp,mb,len);
					log_printmsg(NULL,"BINTEC error: msgtype",mb,KERN_WARNING);
					freemsg(mb);
				} else {
					printf("BINTEC error: msg type %04x\n",ntohs(err));
					getflush(bp,len);
				}
				err = getend(bp);
			}
		}
	}

	lastpos = bp->lastout;
	do {
		err = sendone(bp,0);
		if(err < 0)
			break;

		err = sendone(bp,bp->lastout);

		if(bp->lastout == bp->card.nr_chans)
			bp->lastout = 1;
		else
			bp->lastout++;
	} while((err >= 0) && (bp->lastout != lastpos));
}

#ifdef linux
static void
bintecintr(int irq, struct pt_regs *regs)
{
	struct _bintec *bp;
	
	for(bp=bintecmap[irq];bp != NULL; bp = bp->next) {
		if(bp->info.irq == irq) {
			bp->polled --;
			DoIRQ(bp);
			bp->polled ++;
		}
	}
}


#else
void NAME(REALNAME,intr)(int x)
{
	struct _bintec *bp;

	for(bp=bintecmap[x];bp != NULL; bp = bp->next) {
		if(bp->info.irq == x) {
			bp->polled --;
			DoIRQ(bp);
			bp->polled ++;
		}
	}
}
#endif

#ifdef linux
void NAME(REALNAME,poll)(struct _bintec *bp)
#else
void NAME(REALNAME,poll)(void *nix)
#endif
{
#ifndef linux
	struct _bintec *bp;
	for(i=bintec_num-1;i>=0;--i)
#endif
        {
		unsigned long ms;
#ifndef linux
		bp = &bintecdata[i];
#endif
		ms = SetSPL(bp->ipl);
		DoIRQ(bp);
		splx(ms);
#if 0 /* def linux */
		if(bp->info.irq != 0)
			unblock_irq(bp->info.irq);
#endif
	}
}


#ifdef linux
static void
bintectimer(struct _bintec *bp)
{
	NAME(REALNAME,poll)(bp);
#ifdef NEW_TIMEOUT
	bp->timer =
#endif
		timeout((void *)bintectimer,bp, (bp->info.irq == 0) ? (HZ/100+1) : (HZ/2));
}
#endif

int NAME(REALNAME,init)(struct cardinfo *inf)
{
	struct _bintec *bp;
	int err;

	bp = kmalloc(sizeof(*bp),GFP_KERNEL);
	if(bp == NULL) {
		printf("BINTEC: no memory!\n");
		return -ENOMEM;
	}
	bzero(bp,sizeof(*bp));
	bp->info = *inf;
	bp->infoptr = inf;
	bp->card.ctl = bp;
	bp->card.modes = CHM_INTELLIGENT;
	bp->card.ch_mode = bintec_mode;
	bp->card.ch_prot = bintec_prot;
	bp->card.send = bintec_data;
	bp->card.flush = bintec_flush;
	bp->card.cansend = bintec_candata;
	bp->card.poll = NULL;
	bp->card.boot = boot;
	bp->polled = -1;
	bp->lastout = 1;
	printf("ISDN: " STRING(REALNAME) " at mem 0x%lx irq %d: ",bp->info.memaddr,bp->info.irq);
	if((err = init1(bp)) < 0) {
		printf("Card not initializable.\n");
		kfree(bp);
		return err;
	}
#ifdef linux
	if((bp->info.irq != 0) && (bintecmap[bp->info.irq] == NULL) && request_irq(bp->info.irq,bintecintr,SA_INTERRUPT,"Bintec")) {
		printf("IRQ not available.\n");
		kfree(bp);
		return -EIO;
	}
#endif
	NAME(REALNAME,poll)(bp);
	if((err = isdn2_register(&bp->card, bp->info.ID)) != 0) {
		printf("not installed (ISDN_2), err %d\n",err);
		kfree(bp);
		return err;
	}

	bp->polled = 1;
#ifdef linux
	if(bp->info.irq == 0) {
		printf("polling; ");
	}
	bintectimer(bp);
#endif
	bp->next = bintecmap[bp->info.irq];
	bintecmap[bp->info.irq] = bp;
	printf("installed at ");
	if(bp->info.memaddr != 0) 
		printf("mem 0x%lx ",bp->info.memaddr);
	if(bp->info.irq != 0) 
		printf("irq %d.\n",bp->info.irq);
	else
		printf("polled.\n");
	MORE_USE;
	return 0;
}


void NAME(REALNAME,exit)(struct cardinfo *inf)
{
    unsigned long ms = SetSPL(inf->ipl);
    struct _bintec *bp = NULL, **nbp;
	
	nbp = &bintecmap[(int)inf->irq]; /* grumble, GCC */
	while(*nbp != NULL) {
		
		if((*nbp)->infoptr == inf) {
			bp = *nbp;
			*nbp = bp->next;
			break;
		}
		nbp = &((*nbp)->next);
	}

	if(bp == NULL) {
		printf("BINTEC error: exit: info record not found!\n");
		return;
	}
#ifdef NEW_TIMEOUT
    untimeout(bp->timer);
#else
    untimeout(bintectimer,bp);
#endif
	reset_card(bp);

	isdn2_unregister(&bp->card);
	if((bp->info.irq != 0) && (bintecmap[bp->info.irq] == NULL))
		free_irq(bp->info.irq);
	splx(ms);
	kfree(bp);
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
