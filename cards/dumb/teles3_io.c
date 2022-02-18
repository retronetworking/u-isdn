#include <linux/delay.h>

inline static void
PostIRQ(struct _dumb * dumb)
{
}

/*
 * Die Offsets sind alle +20 wegen der FIFO, deshalb wird überall 0x20
 * "zuviel" abgezogen.
 */


inline static Byte
InISAC(struct _dumb * dumb, char offset) {
	return ByteIn(dumb->info.ioaddr-0x420+offset);
}
inline static void
OutISAC(struct _dumb * dumb, char offset, Byte data) {
	ByteOut(dumb->info.ioaddr-0x420+offset,data);
}
inline static Byte
InHSCX(struct _dumb * dumb, unsigned char hscx, char offset) {
	return ByteIn(dumb->info.ioaddr+offset-((hscx&1)?0xC20:0x820));
}
inline static void
OutHSCX(struct _dumb * dumb, unsigned char hscx, char offset, Byte what) {
	ByteOut(dumb->info.ioaddr+offset-((hscx&1)?0xC20:0x820),what);
}
inline static Byte
Slot(struct _dumb * dumb, unsigned char hscx) {
	return (hscx&1) ? 0x2f : 0x03;
}


static int
Init(struct _dumb * dumb) {
Byte cfval;
long flags;
long timout;
	if(dumb->info.ioaddr == 0) return -EINVAL;
	dumb->numHSCX = 2;
	switch(dumb->info.irq) {
	default: printk("irq %d not possible: ",dumb->info.irq); return -EINVAL;
	case  9: 
	case  2: cfval = 0x00; break;
	case  5: cfval = 0x06; break;
	case 10: cfval = 0x08; break;
	case 12: cfval = 0x0C; break;
	case 15: cfval = 0x0E; break;
	}
	if(ByteIn(dumb->info.ioaddr+0) != 0x51) { return -EINVAL; }
	if(ByteIn(dumb->info.ioaddr+1) != 0x93) { return -EINVAL; }
	if((ByteIn(dumb->info.ioaddr+2) & 0xFC) != 0x1C) { return -EINVAL; }
	save_flags(flags);
	sti();
	ByteOut(dumb->info.ioaddr+4,cfval);
        timout=jiffies+(HZ/10+1);
        while(jiffies<timout);
	ByteOut(dumb->info.ioaddr+4,cfval|1);
        timout=jiffies+(HZ/10+1);
        printf("%steles3:HSCX0:%d HSCX1:%d ISAC:%d\n",KERN_INFO,
            ByteInHSCX(dumb,0,VSTR) &0xf,ByteInHSCX(dumb,1,VSTR) &0xf,ByteInISAC(dumb,RBCH));
	restore_flags(flags);
	return 0;
}

static void
InitISAC(struct _dumb * dumb)
{
	dumb->chan[0].mode = M_OFF;
	dumb->chan[0].listen = 0;
	ByteOutISAC(dumb, MASK, 0xFF);
	ByteOutISAC(dumb, ADF2, 0x80);
	ByteOutISAC(dumb, SQXR, 0x2F);
	ByteOutISAC(dumb, SPCR, 0x00);
	ByteOutISAC(dumb, ADF1, 0x02);
	ByteOutISAC(dumb, STCR, 0x70);
	ByteOutISAC(dumb, MODE, 0xC9);
	ByteOutISAC(dumb, TIMR, 0x00);
	ByteOutISAC(dumb, ADF1, 0x00);
	ByteOutISAC(dumb, CMDR, 0x41);
	ByteOutISAC(dumb, CIX0, 0x07);
	ByteOutISAC(dumb, MASK, 0xFF);
	ByteOutISAC(dumb, MASK, 0x00);
}

static void
InitHSCX_(struct _dumb * dumb, unsigned char hscx)
{
	ByteOutHSCX(dumb,hscx,CCR1, 0x85);
	ByteOutHSCX(dumb,hscx,XAD1, 0xFF);
	ByteOutHSCX(dumb,hscx,XAD2, 0xFF);
	ByteOutHSCX(dumb,hscx,RAH2, 0xFF);
	ByteOutHSCX(dumb,hscx,XBCH, 0);
	ByteOutHSCX(dumb,hscx,RLCR, 0);
	ByteOutHSCX(dumb,hscx,CCR2, 0x30);
	ByteOutHSCX(dumb,hscx,TSAX, 0xFF);
	ByteOutHSCX(dumb,hscx,TSAR, 0xFF);
	ByteOutHSCX(dumb,hscx,XCCR, 7);
	ByteOutHSCX(dumb,hscx,RCCR, 7);
	ByteOutHSCX(dumb,hscx,MODE, 0x84);
	ByteOutHSCX(dumb,hscx,MASK, 0x00);
}

static int
HSCX_mode(struct _dumb * dumb, unsigned char hscx, Byte mode, Byte listen)
{
	unsigned long ms = SetSPL(1);
    if(dumb->chan[hscx].m_in != NULL) {
        freemsg(dumb->chan[hscx].m_in);
        dumb->chan[hscx].m_in = dumb->chan[hscx].m_in_run = NULL;
	}
	if(dumb->chan[hscx].m_out != NULL) {
		freemsg(dumb->chan[hscx].m_out);
		dumb->chan[hscx].m_out = dumb->chan[hscx].m_out_run = NULL;
	}

	ByteOutHSCX(dumb,hscx,CCR1, 0x85);
	ByteOutHSCX(dumb,hscx,XAD1, 0xFF);
	ByteOutHSCX(dumb,hscx,XAD2, 0xFF);
	ByteOutHSCX(dumb,hscx,RAH2, 0xFF);
	ByteOutHSCX(dumb,hscx,XBCH, 0);
	ByteOutHSCX(dumb,hscx,RLCR, 0);
	
	switch(mode) {
	case M_OFF:
	case M_STANDBY:
		ByteOutHSCX(dumb,hscx,CCR2, 0x30);
		ByteOutHSCX(dumb,hscx,TSAX, 0xFF);
		ByteOutHSCX(dumb,hscx,TSAR, 0xFF);
		ByteOutHSCX(dumb,hscx,XCCR, 0x07);
		ByteOutHSCX(dumb,hscx,RCCR, 0x07);
		ByteOutHSCX(dumb,hscx,MODE, 0x84);
		dumb->chan[hscx].mode = mode;
		dumb->chan[hscx].locked = 0;
		dumb->chan[hscx].listen = listen;
		break;
	case M_TRANS_HDLC:
	case M_TRANS_ALAW:
	case M_TRANS_V110:
	case M_TRANSPARENT:
		ByteOutHSCX(dumb,hscx,CCR2, 0x30);
		ByteOutHSCX(dumb,hscx,TSAX, Slot(dumb,hscx));
		ByteOutHSCX(dumb,hscx,TSAR, Slot(dumb,hscx));
		ByteOutHSCX(dumb,hscx,XCCR, 0x07);
		ByteOutHSCX(dumb,hscx,RCCR, 0x07);
		ByteOutHSCX(dumb,hscx,MODE, 0xE4);
		ByteOutHSCX(dumb,hscx,CMDR, 0x41);
		ByteOutHSCX(dumb,hscx,MASK, 0x00);
		dumb->chan[hscx].mode = mode;
		dumb->chan[hscx].locked = 0;
		dumb->chan[hscx].listen = listen;
		break;
	case M_HDLC_7H:
		ByteOutHSCX(dumb,hscx,CCR2, 0x02);
		ByteOutHSCX(dumb,hscx,TSAX, Slot(dumb,hscx)+1);
		ByteOutHSCX(dumb,hscx,TSAR, Slot(dumb,hscx)+1);
		ByteOutHSCX(dumb,hscx,XCCR, 6);
		ByteOutHSCX(dumb,hscx,RCCR, 6);
		ByteOutHSCX(dumb,hscx,MODE, 0x8C);
		ByteOutHSCX(dumb,hscx,CMDR, 0x41);
		ByteOutHSCX(dumb,hscx,MASK, 0x00);
		break;
	case M_HDLC_7L:
		ByteOutHSCX(dumb,hscx,CCR2, 0x02);
		ByteOutHSCX(dumb,hscx,TSAX, Slot(dumb,hscx));
		ByteOutHSCX(dumb,hscx,TSAR, Slot(dumb,hscx));
		ByteOutHSCX(dumb,hscx,XCCR, 6);
		ByteOutHSCX(dumb,hscx,RCCR, 6);
		ByteOutHSCX(dumb,hscx,MODE, 0x8C);
		ByteOutHSCX(dumb,hscx,CMDR, 0x41);
		ByteOutHSCX(dumb,hscx,MASK, 0x00);
		break;
	case M_HDLC:
		ByteOutHSCX(dumb,hscx,CCR2, 0x30);
		ByteOutHSCX(dumb,hscx,TSAX, Slot(dumb,hscx));
		ByteOutHSCX(dumb,hscx,TSAR, Slot(dumb,hscx));
		ByteOutHSCX(dumb,hscx,XCCR, 0x07);
		ByteOutHSCX(dumb,hscx,RCCR, 0x07);
		ByteOutHSCX(dumb,hscx,MODE, 0x8C);
		ByteOutHSCX(dumb,hscx,CMDR, 0x41);
		ByteOutHSCX(dumb,hscx,MASK, 0x00);
		dumb->chan[hscx].mode = mode;
		dumb->chan[hscx].locked = 0;
		dumb->chan[hscx].listen = listen;
		break;
	default:
		printf("HSCX unknown mode %x\n",mode);
		splx(ms);
		return -EIO;
	}

	splx(ms);
	return 0;
}

