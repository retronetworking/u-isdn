inline static void PostIRQ(struct _dumb * dumb)
{
}

static inline Byte InISAC(struct _dumb * dumb, u_char offset) {
	return *(Byte *)(dumb->info.memaddr+0x100+((offset&1)?0x1FF:0)+offset);
}
static inline void OutISAC(struct _dumb * dumb, u_char offset, Byte data) {
	*(Byte *)(dumb->info.memaddr+0x100+((offset&1)?0x1FF:0)+offset) = data;
}

static inline Byte InHSCX(struct _dumb * dumb, u_char hscx, u_char offset) {
	return *(Byte *)(dumb->info.memaddr+0x180+((offset&1)?0x1FF:0)+((hscx&1)?0:0x40)+offset);
}
static inline void OutHSCX(struct _dumb * dumb, u_char hscx, u_char offset, Byte data) {
	*(Byte *)(dumb->info.memaddr+0x180+((offset&1)?0x1FF:0)+((hscx&1)?0:0x40)+offset) = data;
}
static inline Byte Slot(struct _dumb * dumb, u_char hscx) {
	return (hscx&1) ? 0x03 : 0x03; /* was 3 / 7 */
}

static int
Init(struct _dumb * dumb) {
	int timout;
	long flags;

	if(dumb->info.memaddr == 0)
		return -EINVAL;
	dumb->numHSCX = 2;
	save_flags(flags);
	if(dumb->info.ipl) {
		int ioaddr;
		Byte cfval;
		switch(dumb->info.ipl) {
		default: printk("ipl %d unknown: ",dumb->info.ipl); return -EINVAL;
		case 1: ioaddr = 0xd80; break;
		case 2: ioaddr = 0xe80; break;
		case 3: ioaddr = 0xf80; break;
		case 4: ioaddr = 0xc80; break; /* may cause conflicts (motherboard range) */
		}
		switch(dumb->info.irq) {
		default: printk("irq %d not possible: ",dumb->info.irq); return -EINVAL;
		case  2:
		case  9: cfval = 0x00; break;
		case  3: cfval = 0x02; break;
		case  4: cfval = 0x04; break;
		case  5: cfval = 0x06; break;
		case 10: cfval = 0x08; break;
		case 11: cfval = 0x0A; break;
		case 12: cfval = 0x0C; break;
		case 15: cfval = 0x0E; break;
		}
		if(dumb->info.memaddr & ~0xDE000) { printk("info.memaddr %lx not possible: ",dumb->info.memaddr); return -EINVAL; }
		if(~dumb->info.memaddr & 0xC0000) { printk("info.memaddr %lx not possible: ",dumb->info.memaddr); return -EINVAL; }
		cfval |= ((dumb->info.memaddr >> 9) & 0xF0);
		if(ByteIn(ioaddr+0) != 0x51) { return -EINVAL; }
		if(ByteIn(ioaddr+1) != 0x93) { return -EINVAL; }
		if((ByteIn(ioaddr+2) & 0xFC) != 0x1C) { return -EINVAL; }

		timout = jiffies+(HZ/10)+1;
		ByteOut(ioaddr+4,cfval);
		sti();
		while(jiffies <= timout) ;
		ByteOut(ioaddr+4,cfval|1);
		timout = jiffies+(HZ/10)+1;
		while(jiffies <= timout) ;
		restore_flags(flags);
	}
	timout = jiffies+(HZ/5)+1;
	*(Byte *)(dumb->info.memaddr + 0x80) = 0;
	sti();
	while(jiffies <= timout) ;
	*(Byte *)(dumb->info.memaddr + 0x80) = 1;
	timout = jiffies+(HZ/5)+1;
	while(jiffies <= timout) ;
	restore_flags(flags);
	return 0;
}

static int
HSCX_mode(struct _dumb * dumb, u_char hscx, Byte mode, Byte listen)
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

	if (mode > M_OFF && !(hscx & 1) && (dumb->chan[hscx-1].mode >= M_HDLC_16))
		return -EIO;

	ByteOutHSCX(dumb,hscx,CCR2, 0x30); /* 0x38 */
	ByteOutHSCX(dumb,hscx,TSAX, Slot(dumb,hscx));
	ByteOutHSCX(dumb,hscx,TSAR, Slot(dumb,hscx));
	ByteOutHSCX(dumb,hscx,XCCR, 7);
	ByteOutHSCX(dumb,hscx,RCCR, 7);

	switch(mode) {
	case M_OFF:
		ByteOutHSCX(dumb,hscx,MASK, 0x00);
		ByteOutHSCX(dumb,hscx,MODE, 0x94);
		ByteOutHSCX(dumb,hscx,CCR1, 0x85);
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
		ByteOutHSCX(dumb,hscx,MODE, 0xE4);
		ByteOutHSCX(dumb,hscx,CCR1, 0x85);
		ByteOutHSCX(dumb,hscx,CMDR, 0x41);
		ByteOutHSCX(dumb,hscx,MASK, 0x00);
		dumb->chan[hscx].mode = mode;
		dumb->chan[hscx].locked = 0;
		dumb->chan[hscx].listen = listen;
		break;
	case M_HDLC_16:
		if(!(hscx & 1))
			return -EIO;
		if(dumb->chan[hscx+1].mode != M_OFF)
			return -ENXIO;
		ByteOutHSCX(dumb,hscx,XCCR, 15);
		ByteOutHSCX(dumb,hscx,RCCR, 15);
		goto HDLC_common;
	case M_HDLC_7L:
		ByteOutHSCX(dumb,hscx,CCR2, 0x00); /* 0x38 */
		ByteOutHSCX(dumb,hscx,TSAX, Slot(dumb,hscx));
		ByteOutHSCX(dumb,hscx,TSAR, Slot(dumb,hscx));
		/* FALl THRU */
	case M_HDLC_7H:
		ByteOutHSCX(dumb,hscx,XCCR, 6);
		ByteOutHSCX(dumb,hscx,RCCR, 6);
		/* FALl THRU */
	case M_HDLC:
	HDLC_common:
		ByteOutHSCX(dumb,hscx,MODE, 0x8C);
		ByteOutHSCX(dumb,hscx,CCR1, 0x8D);
		ByteOutHSCX(dumb,hscx,CMDR, 0x41);
		ByteOutHSCX(dumb,hscx,MASK, 0x00);
		dumb->chan[hscx].mode = mode;
		dumb->chan[hscx].locked = 0;
		dumb->chan[hscx].listen = listen;
		break;
	default:
		printf("%sHSCX unknown mode %x\n",KERN_DEBUG,mode);
	}

	splx(ms);
	return 0;
}
static void
InitISAC(struct _dumb * dumb)
{
	dumb->chan[0].mode = M_OFF;
	dumb->chan[0].listen = 0;
	ByteOutISAC(dumb, ADF2, 0x00);
	ByteOutISAC(dumb, SPCR, 0x0A);
#if 0
	ByteOutISAC(dumb, TIMR, 0x6E);
#endif
	ByteOutISAC(dumb, MODE, 0xC1);
	ByteOutISAC(dumb, ADF1, 0x02);
	ByteOutISAC(dumb, STCR, 0x70);
	ByteOutISAC(dumb, MASK, 0x00);
	ByteOutISAC(dumb, CIX0, 0x07);
}

static void
InitHSCX_(struct _dumb * dumb, u_char hscx)
{
	ByteOutHSCX(dumb,hscx,CCR2, 0x30); /* 0x38 */
	ByteOutHSCX(dumb,hscx,TSAX, Slot(dumb,hscx));
	ByteOutHSCX(dumb,hscx,TSAR, Slot(dumb,hscx));
	ByteOutHSCX(dumb,hscx,XCCR, 7);
	ByteOutHSCX(dumb,hscx,RCCR, 7);
	ByteOutHSCX(dumb,hscx,MODE, 0x06); /* 0x14; */
	ByteOutHSCX(dumb,hscx,CCR1, 0x85);
	ByteOutHSCX(dumb,hscx,XAD1, 0x01);
	ByteOutHSCX(dumb,hscx,XAD2, 0x03);
	ByteOutHSCX(dumb,hscx,RAL1, 0x03);
	ByteOutHSCX(dumb,hscx,RAL2, 0x01);
	ByteOutHSCX(dumb,hscx,RAH1, 0);
	ByteOutHSCX(dumb,hscx,RAH2, 0);
#if 0
	ByteOutHSCX(dumb,hscx,TIMR, 0x70);
#endif
	ByteOutHSCX(dumb,hscx,MASK, 0x00);
}

