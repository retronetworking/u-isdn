#ifndef BINTEC_H
#define BINTEC_H

#include "isdn_limits.h"
#include "isdn_12.h"
#include "smallq.h"
#include <stream.h>
#include "loader.h"

#include "bri.h"

#define DEBUG(_x) if((DEBUG_##_x) & bp->info.debug)
#define DEBUG_memory  0x01
#define DEBUG_uart    0x00
/* #define DEBUG_isac    0x04 */
#define DEBUG_capi    0x08
#define DEBUG_capiout 0x02
#define DEBUG_cpu     0x00
#define DEBUG_check   0x20
#define DEBUG_info    0x40
#define DEBUG_main    0x80

typedef unsigned char Byte;
typedef struct _hdlc_buf {
	struct _smallq q_in, q_out;
	mblk_t *in_more;
	ushort_t offset;
	ushort_t appID, PLCI, NCCI;
	uchar_t dblock; /* sent data block number, incremented, mostly ignored */
	uchar_t mode;
	uchar_t waitflow;
} *hdlc_buf;

typedef struct _bintec {
	struct _isdn1_card card; /* must be first */
	struct cardinfo info,*infoptr;

	struct _hdlc_buf chan[MAX_B+1];

#ifdef NEW_TIMEOUT
	long timer, timer_toss_unknown;
#endif

	struct _smallq q_unknown;

	char unknown_timer; char registered;
	signed char polled; char lastout; int maxoffset;

	int sndoffset; /* buffer position where we send */
	int sndbufsize; /* size of send buffer */
	int sndend; /* last byte of block, for count */
	int rcvoffset; /* buffer position at which to receive */
	int rcvbufsize; /* size of receive buffer */
	int rcvend; /* last byte of block, for flushing / count */

	int msgnr; /* 0x0000-3FFF: Treiber; 4000-7FFF: CAPI;
	              0x8000-FFFF: Board */
	int waitmsg; /* messages to skip before coming online */

    volatile uchar_t *base;	/*  base address of shared memory  */
    volatile uchar_t *state;	/*  state flag			   */
    volatile uchar_t *debugtext;	/*  debug output		   */
    volatile uchar_t *ctrl;	/*  address of control register	   */

    intercom_t rcv;			/*  incoming buffer	   */
    intercom_t snd;			/*  outgoing buffer	   */
    int cflag;				/*  control flag		   */

    unsigned long ctrlmask;		/*  controller bitmask		   */
    unsigned char 	type;   	/*  board type			   */

	struct _bintec *next;
} *__bintec;

#endif /* BINTEC_H */
