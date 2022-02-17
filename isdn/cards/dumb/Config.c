#include "Config.h"
#include "primitives.h"

#if 0
struct _dumb {
	struct _isdn1_card card;
	long memaddr;
	short ioaddr;
	char irq, ipl;
	/* Linux: IRQ wird nicht automatisch erkannt bei ladbaren Treibern! */
	/* IPL ist 0 für Teles/8-Karten, 1..3 für Teles/16-Karten an der 1..3.
	   Adresse. */
	char numHCRX;
	long ID;
	int debug;
	struct _hdlc_buf chan[MAX_B_CHAN+1];
};
#endif

struct _dumb dumbdata[] =  {
#ifdef _avm_
	{ { }, 0,0x300, 3, 0,2, CHAR4('A','V','M','0'),
	  DEBUG_isac|DEBUG_hscx|DEBUG_hscxout|DEBUG_info, },
#endif
#ifdef _ncp_
	{ { }, 0,0x350, 5, 0,2, CHAR4('n','c','p','0'),
	  DEBUG_hscx|DEBUG_hscxout|DEBUG_info, },
	{ { }, 0,0x250, 7, 0,2, CHAR4('n','c','p','1'),
	  DEBUG_hscx|DEBUG_hscxout|DEBUG_info, },
#endif
#ifdef _ncp16_
	{ { }, 0,0x350,12, 0,2, CHAR4('N','c','p','0'),
	  DEBUG_hscx|DEBUG_hscxout|DEBUG_info, },
	{ { }, 0,0x250,15, 0,2, CHAR4('N','c','p','1'),
	  DEBUG_hscx|DEBUG_hscxout|DEBUG_info, },
#endif
#ifdef _bsc_
	{ { }, 0,0x3E0, 5, 5,2, CHAR4('B','S','C','0'),
	  DEBUG_isac|DEBUG_hscx|DEBUG_hscxout|DEBUG_info, },
#endif
#ifdef _teles_
	{ { }, 0xD4000,0,12, 1,2, CHAR4('T','e','l','0'),
	  DEBUG_info| 0, },
	{ { }, 0xD5000,0, 3, 0,2, CHAR4('T','e','l','1'),
	  DEBUG_info| 0, },
	{ { }, 0xD6000,0, 5, 0,2, CHAR4('T','e','l','2'),
	  DEBUG_info| 0, },
	{ { }, 0xD7000,0, 4, 0,2, CHAR4('T','e','l','3'),
	  DEBUG_info| 0, },
#endif
};

int dumb_num = sizeof(dumbdata)/sizeof(dumbdata[0]);

