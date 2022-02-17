/**
 ** Interface driver for the Leonardo card.
 **/

/* needs the "log" Stremas module, with log_printmsg visible, for now */

#define NLEO 6
#define SECTIMES 10
#define LEO_GRANT 3
#define LEO_DELAY_GRANT 100
#define LEO_TRIES 10			  /* Retries when the card's bus interface
								   * fails again */

#define MAXQBLOCKS 10
#define MAXBLOCKSIZE 4096

#include <sys/types.h>
#include <sys/time.h>
#include "f_signal.h"
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/user.h>
#include <sys/errno.h>
#include <sys/reg.h>
#include <sys/var.h>
#include <sys/ioctl.h>
#include <sys/termio.h>
#include <stddef.h>

#include "streamlib.h"
#include "isdn_12.h"
#include "smallq.h"
#include "streams.h"
#include "primitives.h"
#include "config.h"

#include "leo.h"

extern void log_printmsg (void *log, const char *text, mblk_t * mp, const char*);

#define PHYS	    0xf0000000
#define LEO_BASE(unit)  ((unsigned)PHYS+(unit << 24))
#define LEO_MEM(_pos)   (*(((unsigned char *)leoc) + (_pos)))
#define LEO_ADDR(_pos)  ((((_pos) + 0x18000) << 2) | 0x03)
#define LEO_GETBYTE(_pos)  LEO_MEM(LEO_ADDR(_pos))
#define LEO_GETWORD(_pos)  ((LEO_GETBYTE(_pos) << 8) + LEO_GETBYTE((_pos)+1))
#define LEO_GETLONG(_pos)  ((LEO_GETWORD(_pos) <<16) + LEO_GETWORD((_pos)+2))

#define LEO_PUTBYTE(_pos,_val) do { LEO_MEM(LEO_ADDR(_pos)) = _val; } while(0)
#define LEO_PUTWORD(_pos,_val) do { unsigned short _W_val = (_val); \
	LEO_PUTBYTE((_pos),  _W_val >> 8); \
	LEO_PUTBYTE((_pos)+1,_W_val & 0xFF); } while(0)
#define LEO_PUTLONG(_pos,_val) do { unsigned long _L_val = (_val); \
	LEO_PUTWORD((_pos),  _L_val >>16); \
	LEO_PUTWORD((_pos)+2,_L_val & 0xFFFF); } while(0)
#define osizeof(s, m) sizeof(((s *)0)->m)
#define LEO_R(_var) (((osizeof(mem,_var)) == 1) ? LEO_GETBYTE(0x8000+offsetof(mem,_var)) : \
		(((osizeof(mem,_var)) == 2) ? LEO_GETWORD(0x8000+offsetof(mem,_var)) : \
		 LEO_GETLONG(0x8000+offsetof(mem,_var))))
#define LEO_W(_var,_val) do { switch(osizeof(mem,_var)) { \
case 1: LEO_PUTBYTE(0x8000+offsetof(mem,_var),_val); break; \
case 2: LEO_PUTWORD(0x8000+offsetof(mem,_var),_val); break; \
case 4: LEO_PUTLONG(0x8000+offsetof(mem,_var),_val); break; \
default: printf("Stupid: %d at %s %d\n",osizeof(mem,_var),__FILE__, __LINE__); \
} } while(0)

#define LEO_DELAY 100000
#define LEO_DELAY_HDLC 2000000
#define LEO_DELAY_HDLC2 2000000
#define LEO_DELAY_SHORT 200000
#define LEO_DELAY_BUS 50000
#define LEO_DELAY_CMD   100
#define LEO_DELAY_CMD_RESET     2000
/* 90 seems to work, 80 sometimes works, 70 doesn't, 200 was OK */

typedef volatile struct leo_card {
	uchar_t _leo_port_a[4];
	uchar_t _leo_port_b[4];
	uchar_t _leo_port_c[4];
	uchar_t _leo_control[4];
} *leo_card;

#define leo_port_a      _leo_port_a[3]
#define leo_port_b      _leo_port_b[3]
#define leo_port_c      _leo_port_c[3]
#define leo_control		_leo_control[3]

/* misc */

#define STDCONFIG 0x90

/* Kommando Codes fuer Leonard CPU */

#define Leo_GetVersion 0x30		  /* Versionsnummer nach 8FD0+ */
#define Leo_Unknown1  0x40		  /* Reagiere auf EAZ 0; Flag in 9FF1 */
#define Leo_Semiperm 0x50		  /* semipermanente Verbindung; Flag in 9FF1 */
#define Leo_RealLAP 0x60		  /* mache vernuenftiges LAP */
#define Leo_DA_on	0x70		  /* DA ist da; auto-Abheben */
#define Leo_DA_off	0x80		  /* DA ist weg; nicht automatisch abheben */
#define Leo_EAZ		0x90		  /* Neue EAZ in 9FF1 */
#define Leo_Reset	0xA0		  /* Reset Leonardo Firmware */
#define Leo_Exec	0xA8		  /* Exec Leo RAM program */
#define Leo_Wait	0xB0		  /* Eingehende Anrufe werden angenommen */
#define Leo_Call	0xC0		  /* Die angegeben Nummer wird gewaehlt */
#define Leo_Status	0xD0		  /* Eine Statusmeldung wurde akzeptiert */
#define Leo_Cut		0xE0		  /* Eine Verbindung wird getrennt */

#define Leo_CmdIdle		0x80

/* Command, old */
#define Leo_CmdCommand		0x01
#define Leo_CmdWriteDone	0x02
#define Leo_CmdReadDone		0x04

#define Leo_Bus		0x00
#define Leo_Bus1	0x00
#define Leo_Bus2	0x00

/* Status, old */
#define Leo_CmdOK		0x01
#define Leo_WriteACK	0x02
#define Leo_ReadACK		0x04
#define Leo_HDLC		0x08

#define Leo_StatusIRQ	0x10
#define Leo_ReadIRQ		0x20
#define Leo_WriteIRQ	0x40
#define Leo_BusREQ		0x80

/* Command and status, new */
#define Leo_Written 0x01
#define Leo_Read 0x02
#define Leo_BreakHDLC 0x04

/* #define LEO_PORT_A _LEO_PORT_A(leop,leoc) */
#define LEO_PORT_A leoc->leo_port_a

#define LEO_WAITFOR(what) \
					for (delay = LEO_DELAY; delay > 0; delay --)			\
						if (LEO_PORT_A & what) 						\
							break;/**/
#define LEO_WAITFORBUS \
					for (delay = LEO_DELAY_BUS; delay > 0; delay --)		\
						if (! (LEO_PORT_A & Leo_BusREQ))				\
							break;
#define LEO_WAITFORHDLC \
					for (delay = LEO_DELAY_HDLC; delay > 0; delay --)		\
						if (! (LEO_PORT_A & Leo_HDLC))				\
							break;/**/
#define LEO_WAITFORHDLC2 \
					for (delay = LEO_DELAY_HDLC2; delay > 0; delay --)		\
						if (! (LEO_PORT_A & Leo_BreakHDLC))				\
							break;/**/
#define LEO_WAITFORCMD \
					for (delay = LEO_DELAY_SHORT; delay > 0; delay --)		\
						if (! (LEO_PORT_A & Leo_CmdOK))				\
							break;/**/

#define LEO_CMD(cmd) { 														\
					printf("Cmd %x: %x/",(cmd),LEO_PORT_A);					\
					LEO_MEM(0x87FC3) = (cmd); 								\
					printf("%x/",LEO_PORT_A);								\
					{ volatile int s = (((cmd) == Leo_Reset)				\
									 ? LEO_DELAY_CMD_RESET:LEO_DELAY_CMD);	\
						 for (;s;s--) ; } 									\
					leoc->leo_port_b = leop->LeoControl | Leo_CmdCommand;	\
					LEO_WAITFOR(Leo_CmdOK)									\
					leoc->leo_port_b = leop->LeoControl; 					\
					printf("%x\n",LEO_PORT_A);								\
					}			  /**/

static short leo_find ();
#if 1
short LL;
#else
#define LL 0
#endif

short LT;
short LI = 0;

short LLL = 0;					  /* 0x3000; */
short LLx = 0x0F;


/* *** BEGIN EXPER *** */

#define BLOCKSHIFT 5
#define BLOCKSIZE (1 << BLOCKSHIFT)

typedef unsigned short BlockNr;
typedef unsigned char Byte;

typedef struct _block_buf {
	volatile BlockNr in;
	volatile BlockNr out;
	volatile BlockNr work;		  /* number of buffers enqueued here */
} *block_buf;


typedef struct _block {
	Byte length;
	Byte type;
	BlockNr next;
} *block;

#define MAXBLOCKS 0x7FFF

typedef struct _hdlc_buf {
	struct _block_buf in, out;
	BlockNr sending;
	Byte mode;
	Byte lock, listen;
} *hdlc_buf;

/* Block types */
enum _block_type_ {
	BT_free,
	BT_d,
	BT_b1,
	BT_b2,
	BT_cmd,
	BT_console,
	BT_conscmd,
	BT_max = 0x1F,
	BT_write = 0x20,
	BT_run = 0x40,
	BT_last = 0x80,
};

typedef struct _mem {
	short flag;
	long addr1, addr2;
	Byte cHDLC;
	Byte memCorrupt;
	long f2;

	long Sig;
	short version;
	short release;

	ushort_t bufShift;
	ushort_t bufSize;
	void *memStart;
	block ptrStart;
	ushort_t maxmem;
	struct _hdlc_buf buf[4];	  /* command, D, B1, B2 */
} mem;


#define CMD_SHIFT 5
#define CMD_OUT  0				  /* Display */
#define CMD_OUT_UART 0
#define CMD_OUT_ECHO 1			  /* echo it back */
#define CMD_L1 1
#define CMD_ACK  0x00			  /* do nothing */
#define CMD_SET_D  0x01			  /* flush  */
#define CMD_SET_B1 0x02			  /* flush  */
#define CMD_SET_B2 0x03			  /* flush  */

#define CMD_L1_POKE 0x10		  /* poke low 4 bits */

#define CMD_R_D  2				  /* read chip register */
#define CMD_R_B1 3
#define CMD_R_B2 4
#define CMD_W_D  5				  /* write register */
#define CMD_W_B1 6
#define CMD_W_B2 7

/* *** END EXPER *** */


struct isdn_chan {
	char state;
	struct smallq q;
	int busy:1;					  /* Dequeueing packets. Not reentrant; blocks
								   * could get reordered. */
};

typedef struct leo {
	struct isdn1_card card;		  /* MUST BE FIRST */
	struct isdn_chan chan[3];
	mblk_t *rcvbuf[4];
	queue_t *qptr;				  /* read queue (i.e. going up) for Streams */
	mblk_t *ack;				  /* when sync, store conn/disconn ioctl here */
	leo_card leo_base;
	struct leo_stats leo_stats;
	struct termio tty;
	leo_nr nr;
	uchar_t LeoControl;
	uchar_t port_a;
	signed char eaz;
	uchar_t lockLevel;
	int oldms;
	char every;
	int isReading:1;
	int isWriting:1;
	int WritePending:1;
	int ReadPending:1;
	int ReadTimer:1;
	int WriteTimer:1;
	int DidWTimer:1;
	int Async:1;
	int CloseWait:1;
	int ResetOnClose:1;
	int ConnStateSent:1;
	int OpenListen:1;
	int OpenDone:1;
	int DoReport:1;
	int countWRT:4;
	int hasKick:2;
	int new_rom:1;
	int did_exec:1;
	ushort_t bufShift, bufSize;
	long memStart;
	long ptrStart;
	BlockNr maxmem;
	short curBT;
} *leo_data;

static struct leo leo_leo[NLEO];
extern int leo_cnt;
extern int leo_addr[];

static struct module_info leo_minfo =
{
		0, "leo", 0, LEO_MAXMSG, LEO_MAXINCORE, LEO_MAXMSG * 2
};

static int leo_open (), leo_close (), leo_wput ();
static int leo_wsrv (), leo_rsrv ();

static struct qinit leo_rinit =
{
		NULL, leo_rsrv, leo_open, leo_close, NULL, &leo_minfo, NULL
};

static struct qinit leo_winit =
{
		leo_wput, leo_wsrv, NULL, NULL, NULL, &leo_minfo, NULL
};

struct streamtab leo_info =
{&leo_rinit, &leo_winit, NULL, NULL};


inline uchar_t
_LEO_PORT_A (leo_data leop, leo_card leoc)
{
	uchar_t val = leoc->leo_port_a;

	leop->port_a |= val;
	return val;
}


static int leo_doRead (leo_data leop);
static int
leo_RW (leo_data leop)
{
	if (leop->qptr == NULL)
		return 0;
	leop->ReadPending = 1;
	leop->WritePending = 1;
	qenable (WR (leop->qptr));
	leo_doRead (leop);
	timeout (leo_RW, leop, LT);
	return 0;
}

#if 0
void
Leo_GetRW (leo_data leop)
{
	if (leop->WritePending && !leop->isWriting) {
		if (leop->qptr != NULL)
			qenable (WR (leop->qptr));
	}
	if (leop->ReadPending && !leop->isReading) {
		if (!leop->isWriting)
			leo_doRead (leop);
	}
}

#endif


static int
leo_buslock (leo_data leop)
{
	leo_card leoc = leop->leo_base;
	volatile int delay;
	int ms;
	int tries = 0;

#define MAXTRIES 20

	if (leoc == NULL)
		return 0;
	ms = splstr ();
	if (leop->lockLevel == 1) {
		splx (ms);
		return 0;
	}
	leop->lockLevel++;
	splx (ms);
	if (leop->lockLevel > 1) {
		if (LL & 4)
			printf ("L");
		return 1;
	}
	if (leop->new_rom) {
		leop->every = 0;
	  retry:
		ms = splstr ();
		leoc->leo_port_b = Leo_BusREQ | Leo_BreakHDLC;
		for (delay = 0; delay < 200; delay++) ;
		LEO_WAITFORHDLC2
				if (delay) {
			/* for (delay = 0; delay < 200; delay++) ; */
			leoc->leo_port_b = 0; /* ~Leo_BusREQ; */
			/* for (delay = 0; delay < 200; delay++) ; */
			LEO_WAITFORBUS
		}
		if (!delay) {
			leop->lockLevel = 1;
			splx (ms);
			leoc->leo_port_b = leop->LeoControl;
			printf ("Leo: Bus lock: lockout (%x)\n", leoc->leo_port_a);
			goto del;
		}
		/* for (delay = 0; delay < 200; delay++) ; */
		leop->oldms = ms;
		leop->lockLevel++;
		if (LL & 4)
			printf ("*L");
		if (((LEO_GETWORD (0x8000) ^ 0x4321) & 0xFFFE) == 0 && LEO_GETLONG (0x8010) == CHAR4 ('S', 'M', 'E', 'P')) {
			int channel;

			for (channel = 0; channel < 4; channel++) {
				if (LEO_R (buf[channel].in.in) > leop->maxmem ||
						LEO_R (buf[channel].out.out) > leop->maxmem) {
					if (LL & 0x80)
						printf (" %d: Bad L: %x %x\n", channel, LEO_R (buf[channel].in.in), LEO_R (buf[channel].out.out));
					goto none;
				}
			}

			if (LEO_R (memCorrupt)) {
				leop->lockLevel--;
				leoc->leo_port_b = leop->LeoControl;
				splx (ms);
				printf ("\nCard dead: MemCorrupt\n");
				goto del;
			}
			if (tries && (LL & 0x8000))
				printf ("\\");
			return 1;
		}
		/* Card dead. Problem? */
	  none:
		leop->lockLevel--;
		leoc->leo_port_b = leop->LeoControl;
		if (++tries < LEO_TRIES) {
			splx (ms);
			if (LL & 0x8000)
				printf ("/C");
			for (delay = 0; delay < 500 * tries; delay++) ;
			goto retry;
		}
		LEO_PUTWORD (0x8000, 0);
		LEO_PUTLONG (0x8010, 0);
		leop->new_rom = 0;
		splx (ms);
		printf ("\nCard dead.\n\n");
	  del:
		isdn2_unregister (&leop->card);
		goto Ex;
	}
	while (++tries < MAXTRIES) {
		LEO_WAITFORHDLC
				if (!delay) {
			printf ("Leo %d: Bus lock: no way (HDLC %x)\n", leop - leo_leo, leoc->leo_port_a);
			goto Ex;
		} else if ((tries & 0x03) == 0 && (LL & 1) && delay != LEO_DELAY_HDLC)
			printf ("BusWait Del HDLC %d\n", LEO_DELAY_HDLC - delay);

		leoc->leo_port_b = ~Leo_BusREQ;
		/* !!!          &=                */
		ms = splstr ();
		if (1 /* ! (LEO_PORT_A & Leo_HDLC) */ ){
			LEO_WAITFORBUS
					if (!delay) {
				splx (ms);
				printf ("Leo: Bus lock: no bus (%x)\n", leoc->leo_port_a);
				goto Ex;
			}
			leop->oldms = ms;
			leop->lockLevel++;
			if (LL & 4) {
				printf ("*L");
				if (tries > 1)
					printf (" %d tries ", tries);
			}
			return 1;
		} else {
			leoc->leo_port_b = leop->LeoControl;
			splx (ms);
			if (LL & 0x80)
				printf ("Leo: Bus lock: HDLC again (%x)\n", leoc->leo_port_a);
		}
	}
	printf ("Leo: Bus lock: HDLC stuck?.\n");
  Ex:
	if (leop->qptr != NULL)
		putctlx1 (leop->qptr, M_ERROR, ENXIO);
	return 0;
}

static void
leo_busunlock (leo_data leop, int dont)
{
	leo_card leoc = leop->leo_base;
	volatile int delay;
	int ms;

	if (leoc == NULL)
		return;
	ms = splstr ();
	if (leop->lockLevel < 2) {
		splx (ms);
		leop->lockLevel = 0;
		printf ("Leo %d: too many busunlock calls!\n", leop - leo_leo);
	} else if (leop->lockLevel > 2) {
		splx (ms);
		leop->lockLevel--;
		if (LL & 4)
			printf ("U");
		return;
	} else {
		if (dont == 2) {
			leop->lockLevel = 1;  /* block the card */
			splx (leop->oldms);
			printf ("*U Lock*");
			return;
		} else
			leop->lockLevel = 0;
		if (leop->new_rom)
			leop->LeoControl &= ~Leo_BreakHDLC;
		else
			leoc->leo_port_b = leop->LeoControl;
		if (LL & 4)
			printf ("*U");
		splx (leop->oldms);
		if (dont) {
			leoc->leo_port_b = leop->LeoControl;
			return;
		}
		if (LL & 1) {
			if (leop->isReading && leop->isWriting) {
				printf ("+");
			} else if (leop->isReading) {
				printf ("-");
			} else if (leop->isWriting) {
				printf ("=");
			}
		}
		if (leop->new_rom) {
			if (leop->isReading) {
				leop->LeoControl |= Leo_Read;
				leop->isReading = 0;
			} else
				leop->LeoControl &= ~Leo_Read;
			if (leop->isWriting) {
				leop->LeoControl |= Leo_Written;
				leop->isWriting = 0;
			} else
				leop->LeoControl &= ~Leo_Written;
			leoc->leo_port_b = leop->LeoControl;
		} else {
			if (leop->isReading) {
				leoc->leo_port_b = leop->LeoControl |= Leo_CmdReadDone;
				LEO_WAITFOR (Leo_ReadACK)
						leop->isReading = 0;
				leop->LeoControl &= ~Leo_CmdReadDone;
			}
			if (leop->isWriting) {
				leoc->leo_port_b = leop->LeoControl |= Leo_CmdWriteDone;
				LEO_WAITFOR (Leo_WriteACK)
						leop->isWriting = 0;
				leop->LeoControl &= ~Leo_CmdWriteDone;
			}
		}
	}
}

static int
leo_busgrant (leo_data leop)
{
	volatile int xx;

	leo_busunlock (leop, 1);
	for (xx = 0; xx < LEO_DELAY_GRANT; xx++) ;
	return leo_buslock (leop);
}



static int
leo_neterr (leo_data leop)
{
	switch (leop->leo_stats.lastmsg) {
		/* invent a fancy error message */
	default:
		return EINVAL;
	case 4:
		return EPIPE;			  /* disconnect */
	case 2:
		return EIO;				  /* ISDN bad */
	case 5:
		return ENXIO;			  /* nobody home */
	case 6:
		return ETIMEDOUT;		  /* too many errors */
	case 7:
		return EIO;
	case 8:
		return EIO;
	case 9:
		return EBUSY;
	case 10:
		return EAGAIN;
	case 11:
		return EAGAIN;
	case 12:
		return EBUSY;
	case 13:
		return ENXIO;
	case 14:
		return EIO;
	}
}

static int
leo_wakeopen (leo_data leop)
{
	wakeup (((caddr_t) leop) + 1);
	return 0;
}

static int
leo_wakeclose (leo_data leop)
{
	leop->CloseWait = 0;

	wakeup (((caddr_t) leop) + 2);
	wakeup (leop);
	return 0;
}

static int
leo_timeout (leo_data leop)
{
	int ms = splstr ();

	if (leop->ack != NULL) {
		struct iocblk *iocb;
		mblk_t *mp = leop->ack;

		leop->ack = NULL;

		if (leop->qptr != NULL) {
			if (leop->leo_stats.state == CONN_CLOSING)
				mp->b_datap->db_type = M_IOCACK;
			else
				mp->b_datap->db_type = M_IOCNAK;
			iocb = (struct iocblk *) mp->b_rptr;
			iocb->ioc_count = 0;
			iocb->ioc_error = leo_neterr (leop);
			putq (leop->qptr, mp);
		} else
			freemsg (mp);
	}
	if (leop->leo_stats.state != CONN_NONE) {
		leo_card leoc = leop->leo_base;
		volatile int delay;

		printf ("Leo %d: disconnect on timeout\n", leop - leo_leo);
		if (leoc != NULL) {
			leoc->leo_port_b = Leo_Bus1;
			LEO_CMD (Leo_Cut);
			leop->leo_stats.state = CONN_NONE;
		}
	}
	splx (ms);
	return 0;
}



static int
leo_senddata (leo_data leop, mblk_t ** mbx, char channel)
{
	leo_card leoc = leop->leo_base;
	BlockNr bpos = 0;
	uchar_t btype;
	ushort_t nfree;
	char *fullmsg = "No";
	BlockNr bfirst, bnext = 0;
	mblk_t *mb = *mbx;

	if (LI)
		printf ("Leo_SendData\n");
	if (dsize (mb) == 0) {
		freemsg (mb);
		if (LI)
			printf ("-Leo_SendData\n");
		return 0;
	}
	channel++;

	if (channel < 0 || channel > 3) {
		printf ("Leo %d: Bad chan %d\n", leop - leo_leo, channel - 1);
		if (LI)
			printf ("-Leo_SendData\n");
		return ENXIO;
	}
	mb = pullupm (mb, 0);
	if (mb != NULL && channel == 0) {
		btype = *mb->b_rptr++;
		mb = pullupm (mb, 0);
	} else
		btype = channel;

	if (mb == NULL) {
		leo_busunlock (leop, 1);
		if (LI)
			printf ("-Leo_SendData\n");
		return 0;
	}
	if (!leo_buslock (leop)) {
		if (LI)
			printf ("-Leo_SendData\n");
		return ENXIO;
	}
	if (channel < 2)
		if (LL & 0x100)
			log_printmsg (NULL, "<<<", mb,KERN_DEBUG);

	bfirst = LEO_R (buf[channel].out.in);
	if ((LL & 0x1000) && LEO_R (buf[channel].out.out) != bfirst) {
		leo_busunlock (leop, 1);
		return EAGAIN;
	}
	bpos = LEO_R (buf[channel].out.work);
	if (bpos == bfirst && bpos != LEO_R (buf[channel].out.out)) {
		if (LL & 0x100)
			log_printmsg (NULL, "<.D", mb,KERN_DEBUG);
		fullmsg = "Fd";
	} else {
		while (mb != NULL) {
			long bcpos;
			ushort_t blen;

			bcpos = sizeof (struct _block) * bpos + leop->ptrStart;
			bnext = LEO_GETWORD (bcpos + offsetof (struct _block, next));

			if (bnext == bfirst) {
				bnext = bpos;
				if (LL & 0x100)
					log_printmsg (NULL, "< D", mb,KERN_DEBUG);
				fullmsg = "Fu";
				break;
			}
			blen = 0;
			{
				char *bp, *ep;
				uchar_t *lp;

				bp = mb->b_rptr;
				ep = mb->b_wptr;
				lp = &LEO_MEM (LEO_ADDR (leop->memStart + (bpos << leop->bufShift)));
				do {
					*lp = *bp++;
					lp += 4;
					blen++;
					while (bp >= ep) {
						mblk_t *mb2 = mb;

						mb = unlinkb (mb2);
						freeb (mb2);
						if (mb == NULL)
							break;
						else {
							bp = mb->b_rptr;
							ep = mb->b_wptr;
						}
					}
				} while (mb != NULL && blen < leop->bufSize);
				if (mb != NULL)
					mb->b_rptr = bp;
			}

			if (LL & 0x20)
				printf ("W @%d %d of %x @ %x N %x L %x:", channel, blen, btype, bpos, bnext, bfirst);
			LEO_PUTBYTE (bcpos + offsetof (struct _block, type), (mb == NULL) ? (btype | BT_last) : btype);
			LEO_PUTBYTE (bcpos + offsetof (struct _block, length), blen - 1);

			if (mb != NULL && ++leop->every > LEO_GRANT) {
				LEO_W (buf[channel].out.work, bnext);
				if (LL & 0x20)
					printf ("WU:");
				if (!leo_busgrant (leop)) {
					*mbx = mb;
					return ENXIO;
				}
				bfirst = LEO_R (buf[channel].out.in);
			}
			bpos = bnext;
		}

		if (mb == NULL)
			LEO_W (buf[channel].out.out, bnext);

		LEO_W (buf[channel].out.work, bnext);
	}
	if (LL & 0x800)
		printf ("W%d:%s%x/%x/%x:%x/%x/%x ", channel,
				(mb == NULL) ? "" : fullmsg,
				LEO_R (buf[channel].in.in),
				LEO_R (buf[channel].in.out),
				LEO_R (buf[channel].in.work),
				LEO_R (buf[channel].out.in),
				LEO_R (buf[channel].out.out),
				LEO_R (buf[channel].out.work));


	leo_busunlock (leop, 1);

	leop->isWriting = 0;

	if (mb == NULL) {
		if (LI)
			printf ("-Leo_SendData\n");
		return 0;
	} else {
		*mbx = mb;
		if (LI)
			printf ("-Leo_SendData\n");
		return EAGAIN;
	}
}

static int
leo_ch_mode (struct isdn1_card *card, short channel, char mode, char listen)
{
	leo_data leop = (leo_data) card;
	mblk_t *mb;
	int err = 0;

	if (0)
		printf ("Leo_ChMode %d %d\n", channel, mode);
	if (channel < -1 || channel > 2) {
		printf ("Leo %d: Mode change to %d: Bad channel %d\n", leop - leo_leo, mode, channel);
		if (LI)
			printf ("-Leo_ChMode\n");
		return ENXIO;
	}
	if (channel == -1) {
		if ((mb = allocb (2, BPRI_HI)) == NULL)
			return ENOMEM;
		*mb->b_wptr++ = BT_cmd | BT_last;
		*mb->b_wptr++ = (CMD_L1 << 5) | CMD_L1_POKE | (mode ? 0x09 : 0x0F);
	} else {
		if ((mb = allocb (3, BPRI_HI)) == NULL) {
			return ENOMEM;
		}
		*mb->b_wptr++ = BT_cmd | BT_last;
		*mb->b_wptr++ = (CMD_L1 << 5) | (channel + 1) | (listen ? 0x08 : 0);
		*mb->b_wptr++ = mode;
#ifndef LL
		if (LL & 0x2000) {
			if (mode <= M_TRANSPARENT)
				LL &= ~0x1000;
			else
				LL |= 0x1000;
		}
#endif
	}
	if ((err = leo_senddata (leop, &mb, -1)) != 0)
		freemsg (mb);

	if (LI)
		printf ("-Leo_ChMode\n");
	return err;
}

static int
leo_ch_prot (struct isdn1_card *card, short channel, mblk_t * proto)
{
	if (LI)
		printf ("/Leo_ChProt\n");
	return EINVAL;
}

static int
leo_cansend (struct isdn1_card *card, short channel)
{
	leo_data leop = (leo_data) ((long) card - offsetof (struct leo, card));
	struct isdn_chan *chan;

	if (channel < 0 || channel > 2) {
		printf ("Leo: Panic: cansend called with chan %x\n", channel);
		return 0;
	}
	chan = &leop->chan[channel];
	if (LL & 0x4000)
		printf ("LB%d %d ", channel, chan->q.nblocks);

	return (chan->q.nblocks < MAXQBLOCKS);
}

static int
leo_flush (struct isdn1_card *card, short channel)
{
	int ms;
	leo_data leop = (leo_data) ((long) card - offsetof (struct leo, card));
	leo_card leoc = leop->leo_base;
	struct isdn_chan *chan;


	if (channel < 0 || channel > 2) {
		printf ("Leo: Panic: send called with chan %x\n", channel);
		return EINVAL;
	}
	chan = &leop->chan[channel];
	ms = splstr ();
	if (chan->busy) {
		splx (ms);
		return EAGAIN;
	}
	chan->busy = 1;
	splx (ms);
	S_flush (&chan->q);

	if (!leo_buslock (leop))
		return ENXIO;
	{
		BlockNr bpos = LEO_R (buf[channel].out.in);
		BlockNr blast = LEO_R (buf[channel].out.out);

#ifndef LL
		BlockNr bposf = bpos;

#endif

		long bcpos;
		ushort_t blen;

		while (bpos != blast) {
			bcpos = sizeof (struct _block) * bpos + leop->ptrStart;

			LEO_PUTWORD (bcpos + offsetof (struct _block, type), BT_free);
			bpos = LEO_GETWORD (bcpos + offsetof (struct _block, next));
		}
		if (LL & 0x400)
			printf ("F%d:%x:%x/%x/%x:%x/%x/%x ", channel, bposf,
					LEO_R (buf[channel].in.in),
					LEO_R (buf[channel].in.out),
					LEO_R (buf[channel].in.work),
					LEO_R (buf[channel].out.in),
					LEO_R (buf[channel].out.out),
					LEO_R (buf[channel].out.work));
		LEO_W (buf[channel].out.work, blast);
	}
	leo_busunlock (leop, 1);
	chan->busy = 0;

	return 0;
}

static int
leo_send (struct isdn1_card *card, short channel, mblk_t * data)
{
	int ms;
	leo_data leop = (leo_data) ((long) card - offsetof (struct leo, card));
	struct isdn_chan *chan;
	int err = 0;
	mblk_t *mb;

#if 0
	if (data) {
		printf ("*** %d ", channel);
		log_printmsg (NULL, "Send", data,KERN_DEBUG);
	}
#endif
	if (channel < 0 || channel > 2) {
		printf ("Leo: Panic: send called with chan %x\n", channel);
		if (LI)
			printf ("-Leo_Send\n");
		return EINVAL;
	}
	chan = &leop->chan[channel];
	if (data != NULL)
		S_enqueue (&chan->q, data);

	ms = splstr ();
	if (chan->busy) {
		splx (ms);
		if (LI)
			printf (" -Leo_Send\n");
		return ((data == NULL) ? err : 0);
	}
	chan->busy = 1;
	splx (ms);

	while ((mb = S_dequeue (&chan->q)) != NULL) {
		if ((err = leo_senddata (leop, &mb, channel)) != 0) {
			if (err == EAGAIN)
				S_requeue (&chan->q, mb);
			else
				freemsg (mb);
			break;
		}
	}
	chan->busy = 0;
	if (LI)
		printf (" --Leo_Send\n");
	return ((data == NULL) ? err : 0);
}

static void
leo_sendagain (struct isdn1_card *card)
{
	int channel;

	for (channel = 0; channel <= card->nr_chans; channel++)
		leo_send (card, channel, NULL);
	isdn2_backenable (card, -1);
}

static int
leo_open (queue_t * q, int dev, int flag, int sflag)
{
	leo_data leop;
	leo_card leoc;
	volatile int delay;
	int ms;
	char listening;
	char already = 0;

	if (sflag == CLONEOPEN) {
		for (dev = 0; dev < leo_cnt; dev++)
			if (leo_leo[dev].qptr == NULL) {
				listening = 1;
				goto gotone;
			}
		printf ("Leo: No free card\n");
		u.u_error = ENOENT;
		return OPENFAIL;
	} else
		dev = minor (dev);

	listening = !(dev & LEO_ACTIVEOPEN);
	dev &= ~LEO_ACTIVEOPEN;

  gotone:
	if (dev >= leo_cnt) {
		printf ("Leo: No card %d\n", dev);
		u.u_error = ENOENT;
		return OPENFAIL;
	}
	leop = &leo_leo[dev];
	leoc = leop->leo_base;
	if (leoc == NULL) {
		printf ("Leo: No card at %d\n", dev);
		u.u_error = ENODEV;
		return OPENFAIL;
	}
	ms = splstr ();

  redo2:
	if (leop->CloseWait) {
		if (sleep (((char *) leop) + 2, (PZERO + 1) | PCATCH)) {
			printf ("Leo_open intr 1\n");
			u.u_error = EINTR;
			return OPENFAIL;
		}
	}
  redo:
	already = (leop->qptr != NULL);
	if (already && leop->new_rom) {
		splx (ms);
		printf ("Leo_open Already+NewROM\n");
		u.u_error = EBUSY;
		return OPENFAIL;
	}
	if (!already) {
		char eaz = leop->eaz;
		char unlocked = 1;
		char newrom = leop->new_rom;

		bzero (leop, sizeof (struct leo));

		leop->leo_base = leoc;
		leop->eaz = eaz;
		leop->new_rom = newrom;

		WR (q)->q_ptr = (caddr_t) leop;
		q->q_ptr = (caddr_t) leop;
		leop->qptr = q;

		leop->LeoControl = Leo_CmdIdle;

		leop->tty.c_iflag = IGNBRK | IGNPAR;
		leop->tty.c_cflag = B38400 | CS8 | CREAD | HUPCL;
		if (listening)
			leop->tty.c_cflag |= CLOCAL;
		if (leo_buslock (leop)) {
			unlocked = 0;
		}
		if (unlocked) {
			ms = splstr ();
			leoc->leo_control = STDCONFIG;
			leoc->leo_port_b = leop->LeoControl = Leo_CmdIdle;
			leop->new_rom = 0;
			splx (ms);
			if (leo_buslock (leop))
				unlocked = 0;
		}
		if (unlocked) {
			u.u_error = ENXIO;
			printf ("Leo_open No Lock\n");
			leop->qptr = NULL;
			leop->new_rom = 0;
			return OPENFAIL;
		}
		if (((LEO_GETWORD (0x8000) ^ 0x4321) & 0xFFFE) == 0 && LEO_GETLONG (0x8010) == CHAR4 ('S', 'M', 'E', 'P')) {
			int a = osizeof (mem, bufShift);

			printf ("Leo %d: new_rom!\n", leop - leo_leo);
			leop->bufShift = LEO_R (bufShift);
			leop->bufSize = LEO_R (bufSize);
			leop->memStart = LEO_R (memStart);
			leop->ptrStart = LEO_R (ptrStart);
			leop->maxmem = LEO_R (maxmem);
			printf ("mem %x, ptr %x\n in %x %x %x %x\nout %x %x %x %x\n", leop->memStart, leop->ptrStart,
					offsetof (mem, buf[0].in),
					offsetof (mem, buf[1].in),
					offsetof (mem, buf[2].in),
					offsetof (mem, buf[3].in),
					offsetof (mem, buf[0].out),
					offsetof (mem, buf[1].out),
					offsetof (mem, buf[2].out),
					offsetof (mem, buf[3].out));
			leop->curBT = -1;
			leop->new_rom = 1;
#ifndef LL
			LL = LLL;
#endif
		} else {
			printf ("Leo %d: W0 %x, L16 %x %x %x\n", leop - leo_leo,
					LEO_GETWORD (0x8000), LEO_GETLONG (0x8010), LEO_GETWORD (0x8014), LEO_GETWORD (0x8016));
#ifndef LL
			LL = LLx;
#endif
			leop->new_rom = 0;
		}
		leo_busunlock (leop, 0);

		if (!leop->new_rom && eaz < 0) {
			leop->eaz = eaz = -eaz;

			leoc->leo_port_b = Leo_Bus;
			LEO_WAITFORBUS
					if (!delay) {
				leoc->leo_port_b = leop->LeoControl;
				splx (ms);
				printf ("Leo %d: Bus_Problem %x\n", leop - leo_leo, leoc->leo_port_a);
				u.u_error = ENXIO;
				return OPENFAIL;
			}
			LEO_MEM (0x87FC3 + 4) = eaz;
			LEO_CMD (Leo_EAZ);
			leoc->leo_port_b = leop->LeoControl;
			leop->CloseWait = 1;
			timeout (leo_wakeclose, leop, v.v_hz / 10);
			goto redo2;
		}
	} else {
		WR (q)->q_ptr = (caddr_t) leop;
		q->q_ptr = (caddr_t) leop;
	}

	while (!leop->new_rom) {	  /* always true, unless. */
		if (leop->qptr == NULL)
			goto redo;
		if (listening) {
			if (leop->OpenListen && leop->leo_stats.state == CONN_OPEN)
				goto doit;
			if (!(already || leop->OpenDone || leop->OpenListen)) {
				switch (leop->leo_stats.state) {
				case CONN_OPEN:
					leop->OpenListen = 1;
					leop->WritePending = 1;
					goto doit;
				case CONN_LISTEN:
					break;
				case CONN_CALLING:
					printf ("Leo %d: Listening,OpenListen,CALLING!?!\n", dev);
					break;
				case CONN_CLOSING:
					continue;
				default:
					printf ("Leo %d: Waiting.\n", dev);
					leop->leo_stats.state = CONN_LISTEN;
					leop->OpenListen = 1;

					leoc->leo_port_b = Leo_Bus;
					LEO_CMD (Leo_Wait);
					if (delay)
						break;
					printf ("Leo %d: Err Wait\n", dev);
				  breakoff:
					u.u_error = ENXIO;
				  breakoff2:
					if (leop->qptr == q)
						leop->qptr = NULL;
					splx (ms);
					printf ("Leo_open Breakoff\n");
					return OPENFAIL;
				}
			}
			printf ("Leo %d: Sleep Open WaitConn\n", dev);
			if (sleep (leop, (PZERO + 1) | PCATCH)) {
				u.u_error = EINTR;
				goto breakoff2;
			}
		} else {				  /* I don't want to listen */
			if (leop->OpenListen && leop->OpenDone) {
				splx (ms);
				u.u_error = EBUSY;
				printf ("Leo_open NoListen\n");
				return OPENFAIL;
			}
			if (leop->OpenListen) {		/* cut off the listener */
				printf ("Leo %d: Listener Cut\n", dev);
				leop->leo_stats.state = CONN_NONE;
				leoc->leo_port_b = Leo_Bus1;
				LEO_CMD (Leo_Cut);
				if (!delay && !(flag & O_NDELAY))
					goto breakoff;
				leop->OpenListen = 0;
			}
			goto doit;
		}
	}

  doit:
	leop->qptr = q;

	if (!leop->OpenDone) {
		timeout (leo_wakeopen, leop, listening ? v.v_hz : (v.v_hz / 6));

		sleep (((caddr_t) leop) + 1, (PZERO + 1) | PCATCH);
		printf ("Leo %d: Open.\n", dev);
		leop->OpenDone = 1;
	}
	splx (ms);

	if (leop->new_rom) {
		long id = CHAR4 ('L', 'e', 'o', '0' + dev);

		leop->card.nr_chans = 2;
		leop->card.ch_mode = leo_ch_mode;
		leop->card.ch_prot = leo_ch_prot;
		leop->card.send = leo_send;
		leop->card.cansend = leo_cansend;
		leop->card.flush = leo_flush;
		leop->card.modes = 0x07;  /* Free/Trans/HDLC */
		isdn2_register (&leop->card, id);
		timeout (leo_RW, leop, LT);
	}
	printf ("Leo_open Done %s\n", leop->new_rom ? "new" : "old");
	return dev;
}


static int
leo_wput (q, mp)
	queue_t *q;
	mblk_t *mp;
{
	register leo_data leop = (leo_data) q->q_ptr;
	leo_card leoc = leop->leo_base;

	if (LI)
		printf ("Leo_WPut\n");

	if (leoc == NULL) {
		freemsg (mp);
		if (leop->qptr != NULL)
			putctlx1 (leop->qptr, M_ERROR, ENODEV);
		if (LI)
			printf ("-Leo_WPut\n");
		return 0;
	}
	switch (mp->b_datap->db_type) {
	case M_IOCTL:{
			struct iocblk *iocb;
			int error = EINVAL;

			iocb = (struct iocblk *) mp->b_rptr;

			if (leop->new_rom)
				goto iocnak;

			if (leop->ack != NULL) {
				int ms = splstr ();

				if (leop->ack != NULL) {
					mblk_t *mp2 = leop->ack;

					leop->ack = NULL;
					splx (ms);
					printf ("Leo %d: Stop Timer 4: leop %x, mblk %x, card %x\n", leop - leo_leo, leop, mp2, leoc);
					untimeout (leo_timeout, leop);
					printf ("IOCTL killed");
					freemsg (mp2);
				} else
					splx (ms);
			}
			switch (iocb->ioc_cmd) {
#define ASYNC(cmd) 											\
				if(!delay) { 								\
					splx(ms); 								\
					printf("Leo %d: BusProblem ASY\n",leop-leo_leo); 	\
					goto iocnak; 							\
				} 											\
				if(leop->Async) { 							\
					splx(ms); 								\
					goto iocack; 							\
				} else { 									\
					leop->ack = mp; 						\
					timeout(leo_timeout,leop, 				\
						v.v_hz*60*LEO_MINTIME-1); 			\
					splx(ms); 								\
					break; 									\
				}				  /**/

			case LEO_INFO:
				printf ("Port a: %x, b: %x, C: %x\n", leoc->leo_port_a, leoc->leo_port_b, leop->LeoControl);
				break;
			case LEO_ASYNC:
				leop->Async = 1;
				goto iocack;
			case LEO_SYNC:
				leop->Async = 0;
				goto iocack;
			case LEO_WAITING:{
					int waiting = 0;
					mblk_t *mb = q->q_first;

					while (mb != NULL) {
						waiting += msgdsize (mb);
						mb = mb->b_next;
					}
					iocb->ioc_rval = waiting;
					goto iocack;
				}
			case LEO_STATE:{
					mblk_t *m0 = allocb (sizeof (struct leo_stats), BPRI_MED);

					if (m0 == NULL) {
						error = ENOMEM;
						goto iocnak;
					}
					/*
					 * printf("state %d %d, port_a %x\n",
					 * leop->leo_stats.state, leop->leo_stats.lastmsg,
					 * leoc->leo_port_a);
					 */
					*(struct leo_stats *) (m0->b_wptr) = leop->leo_stats;
					if (leop->leo_stats.state == CONN_OPEN && !leop->ConnStateSent) {
						leop->ConnStateSent = 1;
						((struct leo_stats *) (m0->b_wptr))->lastmsg = 3;
					}
					m0->b_wptr += (iocb->ioc_count = sizeof (struct leo_stats));

					linkb (mp, m0);
					goto iocack;
				}
			case LEO_SETMEM:{
					int ms;
					volatile int delay;
					int memp, meml;
					char *memptr;

					if (iocb->ioc_uid != 0) {
						error = EPERM;
						goto iocnak;
					}
					if (mp->b_cont == NULL || iocb->ioc_count < 1) {
						goto iocnak;
					}
					memptr = mp->b_cont->b_rptr;
					memp = *((long *) memptr)++;
					meml = *((long *) memptr)++;

					printf ("Leo %d: Mem at $%x (%x) ", leop - leo_leo, memp, meml);
					memp = ((memp + 0x18000) << 2) | 0x03;

					leoc->leo_port_b = Leo_Bus;
					while (meml--) {
						LEO_MEM (memp) = *memptr++;
						memp += 4;
					}
					leoc->leo_port_b = leop->LeoControl;

					goto iocack;
				}
			case LEO_RESET:{
					int ms;
					volatile int delay;

					if (iocb->ioc_uid != 0) {
						error = EPERM;
						goto iocnak;
					}
					ms = splstr ();

					leoc->leo_control = STDCONFIG;
					leop->LeoControl = Leo_CmdIdle;

					leoc->leo_port_b = Leo_Bus1;
					LEO_CMD (Leo_Reset);
					leop->leo_stats.state = CONN_NONE;
					leoc->leo_port_b = leop->LeoControl;
					splx (ms);

					if (!delay) {
						error = ENXIO;
						goto iocnak;
					}
					goto iocack;
				}
			case LEO_EXEC:{
					int ms;
					volatile int delay;

					if (iocb->ioc_uid != 0) {
						error = EPERM;
						goto iocnak;
					}
					ms = splstr ();
					leoc->leo_port_b = Leo_Bus1;
					LEO_CMD (Leo_Exec);
					leoc->leo_port_b = leop->LeoControl;
					splx (ms);

					if (!delay) {
						error = ENXIO;
						goto iocnak;
					}
					leop->did_exec = 1;
					goto iocack;
				}
			case LEO_DIAL:{
					int nrlen = 0;
					volatile int delay;
					int memp = 0x84003;
					int ms;
					char *nrptr;

					if (mp->b_cont == NULL || iocb->ioc_count < 1
							|| iocb->ioc_count > LEO_NRLEN) {
						goto iocnak;
					}
					nrptr = mp->b_cont->b_rptr;

					printf ("Leo %d: Calling ", leop - leo_leo);
					while (nrlen < LEO_NRLEN) {
						if (nrlen == iocb->ioc_count)
							break;
						if (nrptr[nrlen] == '\0')
							break;
						printf ("%c", nrptr[nrlen]);
						nrlen++;
					}
					printf (".\n", nrlen);

					iocb->ioc_count = 0;
					ms = splstr ();
					if (leop->leo_stats.state != CONN_NONE) {
						splx (ms);
						error = EBUSY;
						goto iocnak;
					}
					leoc->leo_port_b = Leo_Bus2;
					LEO_MEM (memp) = nrlen;
					while (nrlen--) {
						memp += 4;
						LEO_MEM (memp) = *nrptr++;
					}
					LEO_CMD (Leo_Call);
					ASYNC (leop->leo_stats.state = CONN_CALLING)
				}
			case LEO_WAIT:{
					volatile int delay;
					int ms;

					if (leop->leo_stats.state != CONN_NONE) {
						error = EBUSY;
						goto iocnak;
					}
					ms = splstr ();
					leoc->leo_port_b = Leo_Bus1;
					LEO_CMD (Leo_Wait);
					ASYNC (leop->leo_stats.state = CONN_LISTEN)
				}
			case LEO_DA_ON:{
					volatile int delay;
					int ms;

					if (leop->leo_stats.state != CONN_NONE) {
						error = EALREADY;
						printf ("Leo %d: DA_ON not acceptable (line busy)\n", leop - leo_leo);
						goto iocnak;
					}
					ms = splstr ();
					leoc->leo_port_b = Leo_Bus1;
					LEO_CMD (Leo_DA_on);
					ASYNC (leop->leo_stats.state = CONN_CLOSING)
				}
			case LEO_DA_OFF:{
					volatile int delay;
					int ms;

					if (leop->leo_stats.state != CONN_NONE) {
						error = EALREADY;
						printf ("Leo %d: DA_OFF not acceptable (line busy)\n", leop - leo_leo);
						goto iocnak;
					}
					ms = splstr ();
					leoc->leo_port_b = Leo_Bus1;
					LEO_CMD (Leo_DA_off);
					ASYNC (leop->leo_stats.state = CONN_CLOSING)
				}
			case LEO_DISC:{
					volatile int delay;
					int ms;

					if (leop->leo_stats.state == CONN_NONE) {
						error = EALREADY;
						goto iocnak;
					}
					printf ("Leo %d: Disconnecting\n", leop - leo_leo);
					ms = splstr ();
					leoc->leo_port_b = Leo_Bus1;
					LEO_CMD (Leo_Cut);
					ASYNC (leop->leo_stats.state = CONN_CLOSING)
				}
			case LEO_EAZ:{
					volatile int delay;
					int memp = 0x84007;
					int ms;
					char nr;

					if (mp->b_cont == NULL || iocb->ioc_count != 1) {
						goto iocnak;
					}
					nr = (*mp->b_cont->b_rptr & 0x0F) | 0x30;
					printf ("Leo %d: EAZ %c\n", leop - leo_leo, nr);

					iocb->ioc_count = 0;
					ms = splstr ();
					leoc->leo_port_b = Leo_Bus;
					LEO_WAITFORBUS
							if (!delay) {
						leoc->leo_port_b = leop->LeoControl;
						splx (ms);
						printf ("Leo %d: Bus_Problem %x\n", leop - leo_leo, leoc->leo_port_a);
						goto iocnak;
					}
					LEO_MEM (0x87FC3 + 4) = nr;
					LEO_CMD (Leo_EAZ);
					leoc->leo_port_b = leop->LeoControl;
					splx (ms);
					if (!delay)
						goto iocnak;

					leop->eaz = nr;
					goto iocack;
				}
				/* The Fun Starts HERE */
			case TCGETA:{
					mblk_t *m0 = mp->b_cont;

					if (m0 == NULL) {
						m0 = allocb (sizeof (struct termio), BPRI_MED);

						if (m0 == NULL) {
							error = ENOMEM;
							goto iocnak;
						}
						linkb (mp, m0);
						} else if (m0->b_wptr - m0->b_rptr < sizeof (struct termio)) {
							goto iocnak;
						}
					*(struct termio *) m0->b_rptr = leop->tty;
					m0->b_wptr = m0->b_rptr + sizeof (struct termio);

					goto iocack;
				}
			case UIOCTTSTAT:{
					mblk_t *m0 = mp->b_cont;

					if (m0 == NULL) {
						m0 = allocb (3, BPRI_MED);
						if (m0 == NULL) {
							error = ENOMEM;
							goto iocnak;
						}
						linkb (mp, m0);
					} else if (m0->b_wptr - m0->b_rptr < 3) {
						goto iocnak;
					}
					m0->b_rptr[0] =
							m0->b_rptr[1] = 0;
					m0->b_rptr[2] = 1;
					m0->b_wptr = m0->b_rptr + 3;

					goto iocack;
				}
			case TCSETA:
			case TCSETAW:
			case TCSETAF:{
					int ms;

					if (mp->b_cont == NULL || iocb->ioc_count != sizeof (struct termio)) {
						goto iocnak;
					}
					ms = splstr ();
					leop->tty = *(struct termio *) mp->b_cont->b_rptr;
					leop->tty.c_iflag |= IGNBRK | IGNPAR;
					leop->tty.c_iflag &= ~(BRKINT | INPCK | IUCLC | IXON | IXANY | IXOFF);
					leop->tty.c_oflag &= ~(ONOCR | OLCUC | NLDLY | OFILL | OFDEL | CRDLY | TABDLY | BSDLY | VTDLY | FFDLY);
					leop->tty.c_cflag &= ~(CBAUD | CSIZE | CSTOPB | PARENB | PARODD | CLOCAL);
					leop->tty.c_cflag |= B38400 | CS8 | HUPCL;
					if (leop->OpenListen)
						leop->tty.c_cflag |= CLOCAL;
					/* bzero(leop->tty.c_cc,NCC); */
					splx (ms);

					goto iocack;

				}
			case TCXONC:
			case LDSETT:
			case UIOCMODEM:
			case UIOCNOMODEM:
			case UIOCEMODEM:
			case UIOCDTRFLOW:
			case UIOCFLOW:
			case UIOCNOFLOW:
			  iocack:
				mp->b_datap->db_type = M_IOCACK;
				qreply (q, mp);
				break;
			default:
				printf ("Leo %d: unknown ioctl %x\n", leop - leo_leo, iocb->ioc_cmd);
				/* FALL THRU */
			  iocnak:
				mp->b_datap->db_type = M_IOCNAK;
				iocb->ioc_error = error;
				qreply (q, mp);
			}
			break;
		}
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
	default:{
			log_printmsg (NULL, "Strange", mp,KERN_DEBUG);
			/* putctl1(RD(q)->q_next, M_ERROR, ENXIO); */
			freemsg (mp);
			break;
		}
	}
	if (LI)
		printf ("-Leo_WPut\n");
	return 0;
}

static void
leo_reader (leo_data leop)
{
	leop->ReadTimer = 0;
	if (leop->new_rom)
		return;
	if (!leop->ReadPending)
		return;
	if (leop->isReading || leop->isWriting)
		return;
	leo_doRead (leop);
}

static void
leo_writer (leo_data leop)
{
	leop->WriteTimer = 0;
	if (leop->new_rom)
		return;
	leop->DidWTimer = 1;
	if (!leop->WritePending)
		return;
	if (leop->isWriting)
		return;
	qenable (WR (leop->qptr));
}

static int
leo_doRead (leo_data leop)
{
	leo_card leoc = leop->leo_base;
	volatile int delay;
	int dataLen = 0, XdataLen = 0;
	char didLock;
	char problem = 0;
	int ms;


	if (leop->new_rom) {
		unsigned short bpos;
		char channel;

		if (LI)
			printf ("Leo_doRead\n");

		leop->isReading = 1;
		if (LL & 0x10)
			printf ("R:");
		if (!leo_buslock (leop))
			return 0;

		for (channel = 0; channel < 4; channel++) {
			unsigned short bfirst;
			unsigned short blast;
			unsigned short bfi = 0;
			int bcpos = 0;
			unsigned short nblock = 0;
			uchar_t btype = 0;
			ushort_t blen;
			mblk_t *mb = leop->rcvbuf[channel];
			mblk_t *mb2;
			char did = 0;

			if(mb == NULL) {
				if((mb = allocb(channel == 0 ? 16 : channel == 1 ? 512 : MAXBLOCKSIZE,BPRI_MED)) == NULL) {
					printf("N%d ",channel);
					continue;
				}
				leop->rcvbuf[channel] = mb;
			}
			if (channel > 0 && !isdn2_canrecv (&leop->card, channel - 1)) {
				if (LL & 0x400)
					printf ("nR %d ", channel);
				continue;
			}
			bpos = LEO_R (buf[channel].in.in);

			if (bpos > leop->maxmem) {
				printf ("R %d: bad1 %d\n", channel, bpos);
				leo_busunlock (leop, 2);
				if (LI)
					printf ("-Leo_doRead\n");
				return 0;
			}
			blast = LEO_R (buf[channel].in.out);
			while (bpos != blast) {
				BlockNr bnext;

				did = 1;
				if (bpos > leop->maxmem) {
					printf ("R%d: bad2 %d\n", channel, bpos);
					leo_busunlock (leop, 2);
					if (LI)
						printf ("-Leo_doRead\n");
					return 0;
				}
				bcpos = sizeof (struct _block) * bpos + leop->ptrStart;
				btype = LEO_GETBYTE (bcpos + offsetof (struct _block, type));
				blen = LEO_GETBYTE (bcpos + offsetof (struct _block, length)) + 1;
				bnext = LEO_GETWORD (bcpos + offsetof (struct _block, next));

#define MPUT(_l) 														\
				do {													\
					if(channel<2&&(LL&0x200))log_printmsg(NULL,">>>",mb,KERN_DEBUG);\
					if(channel > 0 && (mb2=allocb(blen = mb->b_wptr-mb->b_rptr,BPRI_MED))!=NULL){ \
						bcopy(mb->b_rptr,mb2->b_wptr,blen); 			\
						mb2->b_wptr += blen; 							\
						if(isdn2_recv(&leop->card,channel-1,mb2) != 0)	\
							freemsg(mb2); 								\
					} else { 											\
						printf("D%d %d ",channel,mb->b_wptr-mb->b_rptr);\
					} 													\
					mb->b_wptr=mb->b_rptr; 								\
				} while(0)

#if 0
					if(channel == 0) {									\
						if(0 && (leop->qptr != NULL) && canput(leop->qptr))	\
							putq(leop->qptr,mb2);						\
						else											\
							freemsg(mb2);								\
					} else if ((LL&0x100) && (channel == 1)) {			\
						mblk_t *mbx = dupmsg(mb2);						\
						if (mbx != NULL) {								\
							if(isdn2_recv(&leop->card,channel-1,mbx) != 0)\
								freemsg(mbx);							\
						}												\
						if ((leop->qptr == NULL) || !canput(leop->qptr))\
							freemsg(mb2);								\
						else  											\
							putq(leop->qptr,mb2);						\
					} else {											\
						int err = isdn2_recv(&leop->card,channel-1,mb2);\
						if (err != 0) {									\
							printf("dR %d ",err);						\
							freemsg(mb2);								\
						}												\
					}													\
					mb = mb2 = NULL;									\

#endif
				if (LL & 0x10)
					printf ("R @%d %d of %x @ %x N %x L %x :", channel, blen, btype, bpos, bnext, blast);

				if (mb->b_wptr + blen > mb->b_datap->db_lim) {
					printf("O%d %d+%d ",channel,mb->b_wptr-mb->b_rptr,blen);
					MPUT((btype &~ BT_last) | BT_run);
				}
				{
					char *bp = mb->b_wptr;
					uchar_t *lp = &LEO_MEM (LEO_ADDR (leop->memStart + (bpos << leop->bufShift)));

					while (blen-- > 0) {
						*bp++ = *lp;
						lp += 4;
					}
					mb->b_wptr = bp;
				}

				if (btype & BT_last || ++leop->every > 3) {

					LEO_W (buf[channel].in.in, bpos);
					leo_busunlock (leop, 1);
					if (LL & 0x10)
						printf ("RU:");

					if (btype == 0x84 && mb->b_rptr + 1 == mb->b_wptr &&
							(*mb->b_rptr & 0xF0) == 0x30) {
						if (*mb->b_rptr == 0x30 || *mb->b_rptr == 0x3F)
							isdn2_new_state (&leop->card, 0);
						else if (*mb->b_rptr == 0x3C || *mb->b_rptr == 0x3D)
							isdn2_new_state (&leop->card, 1);
					}
					if (!leo_buslock (leop)) {
						freemsg (mb2);
						return EIO;
					}
					if (btype & (BT_last | BT_run)) {
						MPUT (btype & (BT_last | BT_run));
						if (LL & 0x400)
							printf ("R%d:%x:%x/%x/%x:%x/%x/%x ", channel, bfi,
									LEO_R (buf[channel].in.in),
									LEO_R (buf[channel].in.out),
									LEO_R (buf[channel].in.work),
									LEO_R (buf[channel].out.in),
									LEO_R (buf[channel].out.out),
									LEO_R (buf[channel].out.work)
									);
					}
					blast = LEO_R (buf[channel].in.out);
				}
				bpos = bnext;
			}
			if (did)
				LEO_W (buf[channel].in.in, bpos);
			if ((btype & BT_run) && mb->b_rptr!=mb->b_wptr)
				MPUT (BT_run);
		}

		leo_busunlock (leop, 0);

		if (LI)
			printf ("-Leo_doRead\n");
		return 0;
	} else {
		mblk_t *mb;
		if (leop->ReadTimer) {
			untimeout (leo_reader, leop);
			leop->ReadTimer = 0;
		}
		if (leoc == NULL || leop->qptr == NULL || leop->leo_stats.state != CONN_OPEN) {
			leoc->leo_port_b = leop->LeoControl |= Leo_CmdReadDone;
			leop->ReadPending = 0;
			LEO_WAITFOR (Leo_ReadACK)
					leoc->leo_port_b = leop->LeoControl &= ~Leo_CmdReadDone;
			return -1;
		}
		ms = splstr ();
		if (leop->isReading || !leop->ReadPending) {
			splx (ms);
			return 0;
		}
		leop->isReading = 1;
		splx (ms);

		didLock = (leo_buslock (leop) != 0);
		if (!didLock) {
			printf ("Can't lock the bus ");
			problem = 1;
		}
		if (!problem) {
			if (!canput (leop->qptr)) {
				printf ("Queue full! ");
				problem = 1;
			}
		}
		if (!problem) {
			XdataLen = dataLen = (LEO_MEM (0x80003) << 8)
					+ LEO_MEM (0x80007);

			if (XdataLen > LEO_MAXMSG || XdataLen <= 0) {
				printf ("Read Len %d (%x)", XdataLen, XdataLen & 0x0FFFF);
				goto AckOff;
			}
			mb = allocb (dataLen, BPRI_MED);
			if (mb == NULL) {
				printf ("Leo Read: no %d-byte block. ", dataLen);
				problem = 1;
			}
		}
		if (!problem) {
			int mpos = 0x8000B;

			if (leop->tty.c_iflag & (ICRNL | INLCR | ISTRIP | IGNCR)) {
				unsigned char c;
				unsigned short flag = leop->tty.c_iflag;

				while (dataLen--) {
					c = LEO_MEM (mpos);
					mpos += 4;
	#if 0
					if (!(dataLen & LEO_GRANTBITS))
						leo_busgrant (leop);
	#endif

					if (flag & ISTRIP)
						c &= 0x7F;
					switch (c) {
					case '\r':
						if (flag & IGNCR)
							continue;
						else if (flag & ICRNL)
							c = '\n';
						break;
					case '\n':
						if (flag & INLCR)
							c = '\r';
						break;
					default:;
					}
					*mb->b_wptr++ = c;
				}
			} else {
				while (dataLen--) {
					*mb->b_wptr++ = LEO_MEM (mpos);
					mpos += 4;
	#if 0
					if (!(dataLen & LEO_GRANTBITS))
						leo_busgrant (leop);
	#endif
				}
			}
		}
		if (!problem) {
			if (leop->tty.c_cflag & CREAD) {
				putq (leop->qptr, mb);
			} else {
				printf ("Leo %d: Discarding %d chars (!CREAD)\n", mb->b_wptr - mb->b_rptr);
				freemsg (mb);
			}
			leop->ReadPending = 0;

		} else {
			leop->isReading = 0;
		}
	  AckOff:
		if (didLock)
			leo_busunlock (leop, 0);
		if (problem) {
			printf ("Leo %d: Read Problem\n", leop - leo_leo);
		} else {
			leop->leo_stats.pkt_in++;
			leop->leo_stats.byte_in += XdataLen;
		}
		return 0;
	}
}



static int
leo_wsrv (queue_t * q)
{
	mblk_t *mp, *mp2;
	register leo_data leop = (leo_data) q->q_ptr;
	leo_card leoc = leop->leo_base;
	int dataLen;
	uchar_t didLock;

	if (leoc == NULL) {
		printf ("LeoWrite: leoc == NULL");
		while ((mp = getq (q)) != NULL)
			freemsg (mp);
		putctlx1 (leop->qptr, M_ERROR, ENODEV);
		printf ("!\n");
	}
	if (leop->new_rom) {
		int err = 0;

		if (LL & 0x10)
			printf ("W:");
		if (LI)
			printf ("Leo_WSrv\n");
		while ((mp = getq (q)) != NULL) {
			char channel = *mp->b_rptr & BT_max;

			if (LL & 0x10)
				printf ("C %x ", *mp->b_rptr);
			switch (channel) {
			case 1:
			case 2:
			case 3:
				channel--;
				mp->b_rptr++;
				break;
			default:
				channel = -1;
				break;
			}
			if ((err = leo_senddata (leop, &mp, channel)) != 0) {
				if (err == EAGAIN)
					putbq (q, mp);
				else
					freemsg (mp);
				break;
			}
		}
		if (LI)
			printf ("-Leo_WSrv\n");
		return err;
	}
	if (leop->leo_stats.state == CONN_OPEN &&
			!leop->DidWTimer && !(leop->WritePending && !leop->WriteTimer))
		return 0;

	while ((mp = getq (q)) != NULL) {
		volatile int delay;
		int ms;
		char problem = 0;

		if (leop->leo_stats.state != CONN_OPEN) {
			if (leop->leo_stats.state == CONN_NONE) {
				int sofar;
				mblk_t *m0;

				for (sofar = 0; sofar < LEO_NRLEN; sofar++)
					if (leop->nr[sofar] == '\0')
						break;
				for (m0 = mp; m0 != NULL; m0 = m0->b_cont) {
					while (m0->b_rptr < m0->b_wptr) {
						char c = *m0->b_rptr++;

						if (c >= '0' && c <= '9' && sofar < LEO_NRLEN)
							leop->nr[sofar++] = c;
						else if ((c == '\n' || c == '\r') && sofar > 0) {
							int memp = 0x84003;
							char *nrptr = leop->nr;

							printf ("Leo %d: Calling %s.\n", leop - leo_leo, leop->nr);

							ms = splstr ();
							leoc->leo_port_b = Leo_Bus2;
							LEO_MEM (memp) = sofar;
							while (sofar--) {
								memp += 4;
								LEO_MEM (memp) = *nrptr++;
							}
							LEO_CMD (Leo_Call);
							leop->leo_stats.state = CONN_CALLING;
							splx (ms);
							bzero (leop->nr, LEO_NRLEN);
							if (delay)
								leop->DoReport = 1;
							else
								leop->leo_stats.state = CONN_NONE;
						}
					}
				}
			}
			freemsg (mp);
			continue;
		}
		ms = splstr ();
		if (leop->isWriting || !leop->WritePending) {	/* not ready */
			splx (ms);
			if (leop->countWRT++ == 15)
				printf ("Leo %d: Write IRQ lost?\n", leop - leo_leo);
			putbq (q, mp);
			goto final;
		}
		leop->isWriting = 1;
		leop->countWRT = 0;
		if (leop->hasKick)
			leop->hasKick = 1;
		splx (ms);

		dataLen = 0;
		didLock = (0 != leo_buslock (leop));
		if (!didLock)
			problem = 1;

		if (!problem) {
			int mpos = 0x8400B;
			mblk_t *m0 = mp;
			mblk_t *m1;

#define ADD(ch)							\
			do {						\
				LEO_MEM(mpos) = (ch);	\
				mpos += 4;				\
				dataLen ++;				\
			} while(0)			  /**/

			while (m0 != NULL) {
				if (leop->tty.c_oflag & OPOST) {
					unsigned short flag = leop->tty.c_oflag;

					while (m0->b_rptr < m0->b_wptr) {
						unsigned char c;

						if (dataLen >= LEO_MAXMSG) {
							goto putout;
						}
						c = *m0->b_rptr++;
						switch (c) {
						case '\n':
							if (flag & ONLCR) {
								if (dataLen == LEO_MAXMSG - 1) {
									--m0->b_rptr;
									goto putout;
								}
								ADD ('\r');
							}
							goto out;
						case '\r':
							if (flag & OCRNL)
								c = '\n';
							goto out;
						default:
						  out:
							ADD (c);
						}
					}
					m1 = m0;
					m0 = m1->b_cont;
					freeb (m1);
					if (m0 == NULL) {
						m0 = getq (q);
						if (LL & 8 && m0 != NULL)
							printf ("/");
					}
				} else {
					while (m0->b_rptr < m0->b_wptr) {
						if (dataLen >= LEO_MAXMSG) {
							goto putout;
						}
						ADD (*m0->b_rptr++);
					}
					m1 = m0;
					m0 = m1->b_cont;
					freeb (m1);
				}
			}
		  putout:

			if (m0 != NULL)
				putbq (q, m0);
			LEO_MEM (0x84003) = dataLen >> 8;
			LEO_MEM (0x84007) = dataLen & 0xFF;
			leop->WritePending = 0;

			if (leop->ReadPending)
				leo_doRead (leop);
		} else {
			putbq (q, mp);
			leop->isWriting = 0;
		}
		if (didLock)
			leo_busunlock (leop, 0);

		if (problem) {
			printf ("Leo %d: Write Problem\n", leop - leo_leo);
			return 0;
		} else {
			leop->leo_stats.pkt_out++;
			leop->leo_stats.byte_out += dataLen;
		}
	}
  final:
	if (leop->ReadPending)
		leo_doRead (leop);
	return 0;
}

static int
leo_close (queue_t * q)
{
	leo_data leop = (leo_data) q->q_ptr;
	int ms = 0;

	printf ("Leo %d: Closing.\n", leop - leo_leo);

	leop->lockLevel = 0;

	if (leop->new_rom) {
		untimeout (leo_RW, leop);
		isdn2_unregister (&leop->card);
	} else {
		ms = splstr ();
		if (leop->leo_stats.state != CONN_NONE || leop->ResetOnClose) {
			leo_card leoc = leop->leo_base;
			volatile int delay;

			if (leoc != NULL) {
				leoc->leo_port_b = Leo_Bus1;
				LEO_CMD (leop->ResetOnClose ? Leo_Reset : Leo_Cut);
				leop->leo_stats.state = CONN_NONE;
			}
		}
	}

	if (leop->ack != NULL) {
		mblk_t *mp = leop->ack;

		leop->ack = NULL;
		untimeout (leo_timeout, leop);
		printf ("IOCTL killed");
		freemsg (mp);
	}
	leop->qptr = NULL;
	leop->OpenDone = 0;
	leop->DoReport = 0;

	if (!leop->new_rom) {
		if (leop->did_exec) {
			leop->new_rom = 1;
			leop->did_exec = 0;
		} else {
			leop->CloseWait = 1;
			timeout (leo_wakeclose, leop, v.v_hz);
		}
		splx (ms);
	} else if (leo_buslock (leop)) {
		leo_card leoc = leop->leo_base;

		LEO_PUTWORD (0x8000, 0);
		LEO_PUTLONG (0x8010, 0);
		leo_busunlock (leop, 1);
		leop->new_rom = 0;
	}
	return 0;
}

void
leo_init (void)
{
	int unit;

#ifndef LL
	LL = 0x0F;
#endif
	LT = 60;
	for (unit = 0; unit < leo_cnt; unit++) {
		leo_data leop = &leo_leo[unit];
		leo_card leoc;
		volatile int delay;

		bzero ((char *) leop, sizeof (struct leo));

		leoc = leop->leo_base = (struct leo_card *) (LEO_BASE (leo_addr[unit]));

		leoc->leo_control = STDCONFIG;
		leoc->leo_port_b = Leo_Bus1;
		/* LEO_PUTWORD (0x8000, 0); */
		leoc->leo_port_b = leop->LeoControl = Leo_CmdIdle;

		LEO_CMD (Leo_Reset);

		if (delay && leo_buslock (leop)) {

			LEO_PUTWORD (0x8000, 0);
			LEO_PUTLONG (0x8010, 0);

			leo_busunlock (leop, 1);

			printf ("Leonardo ISDN card %d in slot #%d\n", unit + 1, leo_addr[unit]);
		} else {
			leop->leo_base = NULL;
			printf ("Leonardo ISDN card %d in slot #%d does not react.\n",
					unit + 1, leo_addr[unit]);
		}
	}
}

static short
leo_find (int slot)
{
	register int dev;

	for (dev = 0; dev < leo_cnt; dev++) {
		if (leo_addr[dev] == slot)
			return dev;
	}
	return -1;
}

#define REPORT_OK "CONNECT\r\n"
#define REPORT_NONE "NO CARRIER\r\n"
#define REPORT_ERR "ERROR\r\n"

void
leo_int (struct args *args)
{
	int dev = leo_find (args->a_dev);
	leo_data leop;
	leo_card leoc;

	/* printf("IRQ Leo %d: ", dev+1); */

	if (dev >= 0 && (leoc = (leop = &leo_leo[dev])->leo_base) != NULL) {
		uchar_t what;

		if (leoc == NULL) {
			printf ("Leo %d: Interrupt from bad card %d, trying to fix that\n", leop - leo_leo, dev);
			leoc = (struct leo_card *) (LEO_BASE (leo_addr[dev]));
		}
		what = leoc->leo_port_a;

		if (leop->new_rom) {
			if (LL & 0x40)
				printf (" I:%x ", what);
#if 1
			if (1 || (what & Leo_Written)) {
				leop->WritePending = 1;
				if (leop->qptr != NULL) {
					qenable (WR (leop->qptr));
					leo_sendagain (&leop->card);
				}
			}
			if (1 || (what & Leo_Read)) {
				leop->ReadPending = 1;
				leo_doRead (leop);
			}
#endif
			return;
		}
		if (what & Leo_ReadIRQ)
			leop->ReadPending = 1;
		if (what & Leo_WriteIRQ)
			leop->WritePending = 1;

		if (what & Leo_StatusIRQ) {
			uchar_t status;
			volatile int delay;
			uchar_t oldState = leop->LeoControl;

			leop->port_a &= ~Leo_StatusIRQ;

			leoc->leo_port_b = leop->LeoControl = oldState & ~Leo_BusREQ;

			LEO_WAITFORBUS
					if (!delay) {
				printf ("Leo %d: ctl: no bus\n", leop - leo_leo);
			} else if (LL & 4 && delay != LEO_DELAY_BUS)
				printf ("Int Del Bus %d\n", LEO_DELAY_BUS - delay);

			status = LEO_MEM (0x83FC3);
			printf ("Leo %d: Status %d\n", leop - leo_leo, status);
			/* if this is deleted, uncomment the "State" printf below */
			if (status != 1 && status != 10 && status != 11)
				leop->leo_stats.lastmsg = status;
			else
				leop->leo_stats.lastmsg = 1;

			switch (status) {
			case 15:{
#if 1
					short nrLen;
					int memp = 0x83EC3;

					nrLen = LEO_MEM (memp);
					if (nrLen < LEO_NRLEN)
						leop->leo_stats.telnr[nrLen] = '\0';
					else
						nrLen = LEO_NRLEN;		/* max! */
					memp += 4 * nrLen;
					while (nrLen--) {
						leop->leo_stats.telnr[nrLen] = LEO_MEM (memp);
						memp -= 4;
					}
#endif
					printf ("Leo %d: Call from %s\n", leop - leo_leo, leop->leo_stats.telnr);
					break;
				}
			case 3:{
					struct iocblk *iocb = NULL;

					leop->leo_stats.state = CONN_OPEN;

					if (leop->qptr != NULL && leop->DoReport) {
						static char report[]= REPORT_OK;
						mblk_t *mb = allocb (sizeof (REPORT_OK), BPRI_LO);

						if (mb != NULL) {
							bcopy (report, mb->b_wptr, sizeof (report) - 1);
							mb->b_wptr += sizeof (report) - 1;
							putq (leop->qptr, mb);
						}
					}
					leop->WritePending = 1;
					qenable (WR (leop->qptr));
					leop->ConnStateSent = 0;
					if (leop->ack != NULL) {
						mblk_t *mp = leop->ack;

						leop->ack = NULL;

						untimeout (leo_timeout, leop);
						printf ("IOCTL killed");

						mp->b_datap->db_type = M_IOCACK;
						iocb = (struct iocblk *) mp->b_rptr;
						iocb->ioc_count = 0;
						putq (leop->qptr, mp);
					}
					leop->leo_stats.pkt_in =
							leop->leo_stats.pkt_out =
							leop->leo_stats.byte_in =
							leop->leo_stats.byte_out =
							leop->leo_stats.conn_cost = 0;
					wakeup (leop);
					break;
				}
			case 16:{
					short nrLen;
					int memp = 0x83F43;
					int charge = 0;

					nrLen = LEO_MEM (memp);
					while (nrLen--) {
						memp += 4;
						charge = charge * 10 + LEO_MEM (memp) - '0';
					}
					leop->leo_stats.conn_cost = charge;
					printf ("Cost %d\n", charge);
					break;
				}
			case 17:{			  /* Version */
					short versLen;
					int memp = 0x83F43;
					char *nr = leop->leo_stats.version;

					versLen = LEO_MEM (memp);
					while (versLen--) {
						memp += 4;
						*nr++ = LEO_MEM (memp);
					}
					*nr = '\0';
				}
			case 0:
			case 10:
			case 11:
				break;

			case 1:
				if (leop->leo_stats.state != CONN_OPEN)
					break;
			default:
				status = 8;
			case 8:
				/* printf("Leo %d: State %d\n",leop-leo_leo,status); */
				leop->leo_stats.state = CONN_NONE;
				leop->ResetOnClose = 1;

				LEO_CMD (Leo_Cut);

				if (leop->eaz < 0)
					leop->eaz = -leop->eaz;
			case 2:
			case 4:
			case 5:
			case 6:
			case 7:
			case 9:
			case 12:
			case 13:
			case 14:
				{
					mblk_t *mp;
					struct iocblk *iocb = NULL;

					/* alert text-only users */
					if (leop->qptr != NULL && leop->DoReport) {
						char *report;
						mblk_t *mb;
						int sz = 0;

						switch (leop->leo_stats.state) {
						case CONN_LISTEN:
							report = REPORT_NONE;
							break;
						case CONN_OPEN:
							report = (status == 4 ? REPORT_NONE : REPORT_ERR);
							break;
						default:
							report = "";
							break;
						}
						{
							char *r = report;

							while (*r++)
								sz++;
						}

						mb = allocb (sz, BPRI_LO);
						if (mb != NULL && sz > 0) {
							bcopy (report, mb->b_wptr, sz);
							mb->b_wptr += sz;
							putq (leop->qptr, mb);
						}
					}
					/* alert ioctl callers */
					if (leop->ack != NULL) {
						mp = leop->ack;
						leop->ack = NULL;
						untimeout (leo_timeout, leop);
						printf ("IOCTL killed");
						if (leop->leo_stats.state == CONN_CLOSING)
							mp->b_datap->db_type = M_IOCACK;
						else
							mp->b_datap->db_type = M_IOCNAK;
						iocb = (struct iocblk *) mp->b_rptr;
						iocb->ioc_count = 0;
						iocb->ioc_error = leo_neterr (leop);
						putq (leop->qptr, mp);
					}
					leop->leo_stats.state = CONN_NONE;

					/* alert upper level, if running */
					if (leop->OpenDone && leop->qptr != NULL) {
						if (status == 8) {
							/* leop->leo_base = NULL; */
							putctlx1 (leop->qptr, M_ERROR, ENODEV);
						} else	  /* if (status != 4 && status != 5 && status
								   * != 9 && status != 12 && status != 13) */
							putctlx (leop->qptr, M_HANGUP);
					}
					/* alert upper level, if waiting for open et al. */
					if (!leop->CloseWait) {
						leop->CloseWait = 1;
						timeout (leo_wakeclose, leop, v.v_hz);
					}
					wakeup (leop);
					break;
				}				  /* case something_or_other */
			}					  /* switch */
			leoc->leo_port_b = leop->LeoControl = oldState;
		}						  /* statusIRQ */
		if (leop->leo_stats.state == CONN_OPEN) {
			if (LL & 0x08) {
				if (leop->ReadPending && leop->WritePending)
					printf ("B");
				else if (leop->ReadPending)
					printf ("R");
				else if (leop->WritePending)
					printf ("W");
			}
			if (leop->WritePending && leop->ReadPending) {
				if (leop->ReadTimer) {
					leop->ReadTimer = 0;
					untimeout (leo_reader, leop);
				}
				if (leop->WriteTimer) {
					leop->WriteTimer = 0;
					untimeout (leo_writer, leop);
					leop->DidWTimer = 1;
				}
				qenable (WR (leop->qptr));
			} else {
				if (leop->ReadPending && !leop->ReadTimer) {
					leop->ReadTimer = 1;
					timeout (leo_reader, leop, v.v_hz / SECTIMES);
				}
				if (leop->WritePending && !leop->WriteTimer) {
					leop->WriteTimer = 1;
					timeout (leo_writer, leop, v.v_hz / SECTIMES);
				}
			}
		}
	}
}


static int
leo_rsrv (queue_t * q)
{
	mblk_t *mp;
	register leo_data leop = (leo_data) q->q_ptr;

	if (LI)
		printf ("Leo_RSrv\n");
	while ((mp = getq (q)) != NULL) {
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
	if (leop->ReadPending && !leop->isReading)
		leo_doRead (leop);
	if (LI)
		printf ("-Leo_RSrv\n");
	return 0;
}
