inline static void
PostIRQ(struct _dumb * dumb)
{
}

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
	return ByteIn(dumb->info.ioaddr+offset-((hscx&1)?0x820:0xC20));
}
inline static void
OutHSCX(struct _dumb * dumb, unsigned char hscx, char offset, Byte what) {
	ByteOut(dumb->info.ioaddr+offset-((hscx&1)?0x820:0xC20),what);
}
inline static Byte
Slot(struct _dumb * dumb, unsigned char hscx) {
	printf(" Slot %d: ",hscx);
	return (hscx&1) ? 0x07 : 0x03;
}


static int
Init(struct _dumb * dumb) {
	int timout;
	long flags;

	if(dumb->info.ioaddr == 0)
		return -EINVAL;
	dumb->numHSCX = 2;
	save_flags(flags);
	if(dumb->info.ipl) {
		Byte cfval;
		switch(dumb->info.irq) {
		default: printk("irq %d not possible: ",dumb->info.irq); return -EINVAL;
		case  2: cfval = 0x00; break;
		case  3: cfval = 0x02; break;
		case  4: cfval = 0x04; break;
		case  5: cfval = 0x06; break;
		case 10: cfval = 0x08; break;
		case 11: cfval = 0x0A; break;
		case 12: cfval = 0x0C; break;
		case 15: cfval = 0x0E; break;
		}
		if(ByteIn(dumb->info.ioaddr+0) != 0x51) { return -EINVAL; }
		if(ByteIn(dumb->info.ioaddr+1) != 0x93) { return -EINVAL; }
		if((ByteIn(dumb->info.ioaddr+2) & 0xFE) != 0x1E) { return -EINVAL; }

		timout = jiffies+(HZ/10)+1;
		ByteOut(dumb->info.ioaddr+4,cfval);
		sti();
		while(jiffies <= timout) ;
		ByteOut(dumb->info.ioaddr+4,cfval|1);
		timout = jiffies+(HZ/10)+1;
		while(jiffies <= timout) ;
		restore_flags(flags);
	}
#if 0
	timout = jiffies+(HZ/5)+1;
	*(Byte *)(dumb->info.memaddr + 0x80) = 0;
	sti();
	while(jiffies <= timout) ;
	*(Byte *)(dumb->info.memaddr + 0x80) = 1;
	timout = jiffies+(HZ/5)+1;
	while(jiffies <= timout) ;
#endif
	restore_flags(flags);
	return 0;
}


static void
InitISAC(struct _dumb * dumb)
{
	dumb->chan[0].mode = M_OFF;
	dumb->chan[0].listen = 0;
	ByteOutISAC(dumb, ADF2, 0x80);
	ByteOutISAC(dumb, SQXR, 0x2F);
	ByteOutISAC(dumb, SPCR, 0x00);
	ByteOutISAC(dumb, ADF1, 0x02);
	ByteOutISAC(dumb, STCR, 0x70);
	ByteOutISAC(dumb, MODE, 0xC3);
	ByteOutISAC(dumb, TIMR, 0x00);
	ByteOutISAC(dumb, ADF1, 0x00);
	ByteOutISAC(dumb, CMDR, 0x41);
	ByteOutISAC(dumb, CIX0, 0x03);
}

static void
InitHSCX_(struct _dumb * dumb, unsigned char hscx)
{
	ByteOutHSCX(dumb,hscx,TSAX, Slot(dumb,hscx));
	ByteOutHSCX(dumb,hscx,TSAR, Slot(dumb,hscx));
	ByteOutHSCX(dumb,hscx,XCCR, 7);
	ByteOutHSCX(dumb,hscx,RCCR, 7);
	ByteOutHSCX(dumb,hscx,MODE, 0x06);
	ByteOutHSCX(dumb,hscx,CCR1, 0x85); /* 0x85 */
	ByteOutHSCX(dumb,hscx,CCR2, 0x32); /* 0x38 */
	ByteOutHSCX(dumb,hscx,XAD1, 0x01);
	ByteOutHSCX(dumb,hscx,XAD2, 0x03);
	ByteOutHSCX(dumb,hscx,RAL1, 0x03);
	ByteOutHSCX(dumb,hscx,RAL2, 0x01);
	ByteOutHSCX(dumb,hscx,RAH1, 0);
	ByteOutHSCX(dumb,hscx,RAH2, 0);
#if 0
	ByteOutHSCX(dumb,hscx,TIMR, 0x70);
#endif
	ByteOutHSCX(dumb,hscx,RLCR, 0x00);
	ByteOutHSCX(dumb,hscx,MASK, 0x00);
}

static int
HSCX_mode(struct _dumb * dumb, unsigned char hscx, Byte mode, Byte listen)
{
	unsigned long ms = SetSPL(dumb->info.ipl);
    if(dumb->chan[hscx].m_in != NULL) {
        freemsg(dumb->chan[hscx].m_in);
        dumb->chan[hscx].m_in = dumb->chan[hscx].m_in_run = NULL;
    }
    if(dumb->chan[hscx].m_out != NULL) {
        freemsg(dumb->chan[hscx].m_out);
        dumb->chan[hscx].m_out = dumb->chan[hscx].m_out_run = NULL;
    }

	ByteOutHSCX(dumb,hscx,CCR2, 0x30);
	ByteOutHSCX(dumb,hscx,TSAX, Slot(dumb,hscx));
	ByteOutHSCX(dumb,hscx,TSAR, Slot(dumb,hscx));
	ByteOutHSCX(dumb,hscx,XCCR, 7);
	ByteOutHSCX(dumb,hscx,RCCR, 7);
	ByteOutHSCX(dumb,hscx,CCR1, 0x05);

	switch(mode) {
	case M_OFF:
	case M_STANDBY:
		ByteOutHSCX(dumb,hscx,MASK, 0x00);
		ByteOutHSCX(dumb,hscx,MODE, 0x96);
		ByteOutHSCX(dumb,hscx,XAD1, 0xFF);
		ByteOutHSCX(dumb,hscx,XAD2, 0xFF);
		ByteOutHSCX(dumb,hscx,RAH2, 0xFF);
		ByteOutHSCX(dumb,hscx,CMDR, 0x41);
		dumb->chan[hscx].mode = mode;
		dumb->chan[hscx].locked = 0;
		dumb->chan[hscx].listen = listen;
		break;
	case M_TRANS_HDLC:
	case M_TRANS_ALAW:
	case M_TRANS_V110:
	case M_TRANSPARENT:
		ByteOutHSCX(dumb,hscx,MODE, 0xE6);
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
		/* FALL THRU */
	case M_HDLC_7L:
		ByteOutHSCX(dumb,hscx,XCCR, 6);
		ByteOutHSCX(dumb,hscx,RCCR, 6);
		/* FALL THRU */
	case M_HDLC:
		ByteOutHSCX(dumb,hscx,MODE, 0x8E);
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
