inline static Byte InISAC(struct _dumb * dumb, char offset) {
	if(offset >= 0x20)
		return ByteIn(dumb->ioaddr+0x1400-0x20+offset);
	else
		return ByteIn(dumb->ioaddr+0x1000+offset); /* ?? */
}
inline static void OutISAC(struct _dumb * dumb, char offset, Byte data) {
	if(offset >= 0x20)
		ByteOut(dumb->ioaddr+0x1400-0x20+offset,data);
	else
		ByteOut(dumb->ioaddr+0x1000+offset,data); /* ?? */
}

inline static Byte InHSCX(struct _dumb * dumb, u_char hscx, char offset) {
	if(hscx&1) {
		if(offset >= 0x20)
			return ByteIn(dumb->ioaddr+0x400-0x20+offset);
		else
			return ByteIn(dumb->ioaddr+0x000+offset);
	} else {
		if(offset >= 0x20)
			return ByteIn(dumb->ioaddr+0xC00-0x20+offset);
		else
			return ByteIn(dumb->ioaddr+0x800+offset);
	}
}

inline static void OutHSCX(struct _dumb * dumb, u_char hscx, char offset, Byte data) {
	if(hscx&1) {
		if(offset >= 0x20)
			ByteOut(dumb->ioaddr+0x400-0x20+offset, data);
		else
			ByteOut(dumb->ioaddr+0x000+offset, data);
	} else {
		if(offset >= 0x20)
			ByteOut(dumb->ioaddr+0xC00-0x20+offset, data);
		else
			ByteOut(dumb->ioaddr+0x800+offset, data);
	}
}

inline static Byte Slot(struct _dumb * dumb, u_char hscx) {
	printf(" Slot %d: ",hscx);
	return (hscx&1) ? 0x2F : 0x03;
}

static int Init(struct _dumb * dumb) {
	int timout;
	long flags;
	Byte foo;
	unsigned int step = 0;

	if(dumb->ioaddr == 0)
		return -EINVAL;
	dumb->numHSCX = 2;
	save_flags(flags);
	sti();
	timout = jiffies+1;

	ByteOut(dumb->ioaddr+0x1800,0x00); while(jiffies <= timout) ; timout = jiffies;
	ByteOut(dumb->ioaddr+0x1800,0x10); while(jiffies <= timout) ; timout = jiffies;
	ByteOut(dumb->ioaddr+0x1800,0x00); while(jiffies <= timout) ; timout = jiffies;

	ByteOut(dumb->ioaddr+0x1800,0x00); while(jiffies <= timout) ; timout = jiffies;
	ByteOut(dumb->ioaddr+0x1800,0x01); while(jiffies <= timout) ; timout = jiffies;
	ByteOut(dumb->ioaddr+0x1800,0x00); while(jiffies <= timout) ;

	timout = jiffies+(HZ/20);
	ByteOut(dumb->ioaddr+0x1800,0x01); 
	while(jiffies <= timout) ;
	ByteOut(dumb->ioaddr+0x1800,0x00); 
	timout = jiffies+(HZ/20);
	while(jiffies <= timout) ;
	restore_flags(flags);
  	
/*  1  2  3  4  5  6  7  8  9 */
/* 17 07 13 03 17 07 13 03  */
	while(++step) {
		ByteOut(dumb->ioaddr+0x1800,(step&1)?0x10:0x00); 
		switch((foo = ByteIn(dumb->ioaddr+0x1800)) & 0xFE) {
		case 0x06:
			if(step & 1)
				goto def;
			if (step == 4)
				step += 2;
			break;
		case 0x16:
			if (!(step & 1))
				goto def;
			if (step == 3)
				step += 2;
			break;
		case 0x02:
			if(step & 1)
				goto def;
			if (step == 2)
				step += 2;
			else if (step >= 10)
				goto Exit;
			break;
		case 0x12:
			if (!(step & 1))
				goto def;
			if (step == 1)
				step += 2;
			break;
		default:
		def:
			printf(" AIRQR %02x, step %d  ",foo,step);
			return -EIO;
		}
	}
  Exit:

	save_flags(flags);
	timout = jiffies+(HZ/20)+1;
	ByteOut(dumb->ioaddr+0x1800,0x01); 
	sti();
	while(jiffies <= timout) ;
	ByteOut(dumb->ioaddr+0x1800,0x00); 
	timout = jiffies+(HZ/20)+1;
	while(jiffies <= timout) ;
	restore_flags(flags);

	ByteOut(dumb->ioaddr+0x1800,0x04); 
	ByteOut(dumb->ioaddr+0x1800,0x08); 
	return 0;
}

static void InitISAC(struct _dumb * dumb)
{
	dumb->chan[0].mode = M_OFF;
	dumb->chan[0].listen = 0;
	ByteOutISAC(dumb, ADF2, 0x80);
	ByteOutISAC(dumb, SPCR, 0x00);
	ByteOutISAC(dumb, ADF1, 0x00);
	ByteOutISAC(dumb, STCR, 0x70);
	ByteOutISAC(dumb, TIMR, 0xFF);
	ByteOutISAC(dumb, MASK, 0x00);
	ByteOutISAC(dumb, MODE, 0xC9);

	ByteOutISAC(dumb, SQXR, 0x0F);
	ByteOutISAC(dumb, CIX0, 0x03);
}

static void InitHSCX_(struct _dumb * dumb, u_char hscx)
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
			ByteOutISAC(dumb,MODE,0xC9);
			printk(KERN_DEBUG "CIX0 0x03\n");
			ByteOutISAC(dumb,CIX0,0x03);
		}
else printk(KERN_DEBUG "NoCIX0 %d\n",dumb->chan[0].mode);
		ByteOutISAC(dumb,MASK,0x00);
		dumb->chan[0].mode = mode;
		dumb->chan[0].listen = 1;
		break;
	case M_HDLC:
		ByteOutISAC(dumb,MODE,0xC9);
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

static int HSCX_mode(struct _dumb * dumb, u_char hscx, Byte mode, Byte listen)
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

	if (mode > M_OFF && !(hscx & 1) && (dumb->chan[hscx-1].mode >= M_HDLC_16))
		return -EIO;

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
	case M_HDLC_16:
		if(!(hscx & 1))
			return -EIO;
		if(dumb->chan[hscx+1].mode != M_OFF)
			return -ENXIO;
		ByteOutHSCX(dumb,hscx,XCCR, 15);
		ByteOutHSCX(dumb,hscx,RCCR, 15);
		goto HDLC_common;
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
	HDLC_common:
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
	return -EIO;
}

inline static void PostIRQ(struct _dumb * dumb)
{
	Byte foo = ByteIn(dumb->ioaddr+0x1800);
	unsigned int doagain=0;
	do {
		switch(foo) {
		case 0x07: return;
		case 0x03:
			if(doagain & 1) {
				ByteOut(dumb->ioaddr+0x1800,0x00);
				ByteOut(dumb->ioaddr+0x1800,0x10);
				ByteOut(dumb->ioaddr+0x1800,0x00);
			} else {
				ByteOut(dumb->ioaddr+0x1800,0x04);
				ByteOut(dumb->ioaddr+0x1800,0x08);
				ByteOut(dumb->ioaddr+0x1800,0x00);
			}
			printf(".");
			if(doagain < 10)
				break;
			/* FALL THRU */
		default:
			printf (" AIRQ %d ISAC %d  HSCX %x %x: %02x ",doagain,ByteInISAC(dumb,ISTA),ByteInHSCX(dumb,1,ISTA),ByteInHSCX(dumb,2,ISTA),foo);
			return;
		}
	} while(++doagain); /* always true */
}

