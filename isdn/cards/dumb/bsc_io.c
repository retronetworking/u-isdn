inline static void PostIRQ(struct _dumb * dumb)
{
}

inline static Byte InISAC(struct _dumb * dumb, char offset) {
	ByteOut(dumb->ioaddr,offset);
	return ByteIn(dumb->ioaddr+1);
}
inline static void OutISAC(struct _dumb * dumb, char offset, Byte data) {
	ByteOut(dumb->ioaddr,offset);
	ByteOut(dumb->ioaddr+1,data);
}

inline static Byte InHSCX(struct _dumb * dumb, unsigned char hscx, char offset) {
	ByteOut(dumb->ioaddr,offset+((hscx&1)?0:0x40));
	return ByteIn(dumb->ioaddr+2);
}
inline static void OutHSCX(struct _dumb * dumb, unsigned char hscx, char offset, Byte what) {
	ByteOut(dumb->ioaddr,offset+((hscx&1)?0:0x40));
	ByteOut(dumb->ioaddr+2,what);
}
inline static Byte Slot(struct _dumb * dumb, unsigned char hscx) {
	printf(" Slot %d: ",hscx);
	return (hscx&1) ? 0x2F : 0x03;
}

static int Init(struct _dumb * dumb) {
	int timout;
	long flags;

	save_flags(flags);
	timout = jiffies+(HZ/20)+1;
	ByteOut(dumb->ioaddr,0x80); 
	sti();
	while(jiffies <= timout) ;
	ByteOut(dumb->ioaddr,0x00); 
	timout = jiffies+(HZ/20)+1;
	while(jiffies <= timout) ;
	restore_flags(flags);
	return 1;
}
static void InitISAC(struct _dumb * dumb)
{
	dumb->chan[0].mode = M_OFF;
	dumb->chan[0].listen = 0;
	ByteOutISAC(dumb, ADF2, 0x80);
	ByteOutISAC(dumb, SPCR, 0x00);
	ByteOutISAC(dumb, SQXR, 0x0F);

	ByteOutISAC(dumb, STCR, 0x70);
	ByteOutISAC(dumb, MODE, 0xC3);
	ByteOutISAC(dumb, CIX0, 0x03);
	ByteOutISAC(dumb, ADF1, 0x00);
	ByteOutISAC(dumb, TIMR, 0xE9);
	ByteOutISAC(dumb, MASK, 0x00);
}

static void InitHSCX_(struct _dumb * dumb, unsigned char hscx)
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
	ByteOutHSCX(dumb,hscx,MASK, 0x00);
}

static void ISAC_mode(struct _dumb * dumb, Byte mode, Byte listen)
{
	unsigned long ms = SetSPL(dumb->ipl);
	
	if(dumb->chan[0].m_in != NULL) {
		freemsg(dumb->chan[0].m_in);
		dumb->chan[0].m_in = dumb->chan[0].m_in_run = NULL;
	}
	if(dumb->chan[0].m_out != NULL) {
		freemsg(dumb->chan[0].m_out);
		dumb->chan[0].m_out = dumb->chan[0].m_out_run = NULL;
	}
	ByteOutISAC(dumb,CMDR,0x41);

	switch(mode) {
	case M_OFF:
		printk(KERN_DEBUG "CIX0 0x3F\n");
		ByteOutISAC(dumb,CIX0,0x3F);
		if(dumb->polled>0) isdn2_new_state(&dumb->card,0);
		dumb->chan[0].mode = mode;
		break;
	case M_STANDBY:
		if(dumb->chan[0].mode != M_STANDBY) {
			ByteOutISAC(dumb,MODE,0xCA);
			printk(KERN_DEBUG "CIX0 0x03\n");
			ByteOutISAC(dumb,CIX0,0x03);
		}
else printk(KERN_DEBUG "NoCIX0 %d\n",dumb->chan[0].mode);
		ByteOutISAC(dumb,MASK,0x00);
		dumb->chan[0].mode = mode;
		dumb->chan[0].listen = 1;
		break;
	case M_HDLC:
		ByteOutISAC(dumb,MODE,0xCA);
		ByteOutISAC(dumb,MASK,0x00);
		if(dumb->chan[0].mode != M_HDLC) {
			printk(KERN_DEBUG "CIX0 0x27\n");
			ByteOutISAC(dumb,CIX0,0x27);
		} else {
printk(KERN_DEBUG "NoCIX0 %d\n",dumb->chan[0].mode);
			if(dumb->polled>0) isdn2_new_state(&dumb->card,1);
		}
#if 0
		ByteOutISAC(dumb,TIMR,0x11);
		ByteOutISAC(dumb,CMDR,0x10);
#endif
		dumb->chan[0].mode = mode;
		dumb->chan[0].listen = 0;
		break;
	default:
		printf("ISAC unknown mode %x\n",mode);
	}
	splx(ms);
}

static void HSCX_mode(struct _dumb * dumb, unsigned char hscx, Byte mode, Byte listen)
{
	unsigned long ms = SetSPL(dumb->ipl);
    if(dumb->chan[hscx].m_in != NULL) {
        freemsg(dumb->chan[hscx].m_in);
        dumb->chan[hscx].m_in = dumb->chan[hscx].m_in_run = NULL;
    }
    if(dumb->chan[hscx].m_out != NULL) {
        freemsg(dumb->chan[hscx].m_out);
        dumb->chan[hscx].m_out = dumb->chan[hscx].m_out_run = NULL;
    }

	ByteOutHSCX(dumb,hscx,CCR2, 0x32);
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
	}

	splx(ms);
}
