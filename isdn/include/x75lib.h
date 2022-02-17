#ifndef __X75LIB_
#define __X75LIB_

/**
 ** X25/X75/Q921 support library
 **/

#include "smallq.h"

/*
 * Upstream state change indication
 */
typedef int (*P_state) (void *ref, uchar_t ind, ushort_t add);

/*
 * Can accept downstream/upstream data?
 */
typedef int (*P_candata) (void *ref);	/* Can accept data? */

/*
 * Send data downstream. Must prefix with address bytes. char&1 is command,
 * char&2 is broadcast.
 * 
 * Send data upstream. char&1 if UI frame, char&2 if broadcast.
 */
typedef int (*P_data) (void *ref, char, mblk_t * data);	/* Data */

/*
 * Flush downstream data, eg. when REJ comes in.
 */
typedef int (*P_flush) (void *ref);

/*
 * Bit 0 of the char parameter says "This is a command" when going down, and
 * "This is a UI frame" when going up. Bit 1 says "TEI Broadcast".
 */
typedef void (*P_backenable) (void *ref);		/* Continue sending */

typedef enum _x75_status {
		S_free, S_down, S_await_up, S_await_down, S_up, S_recover
} x75_status;

typedef struct _x75 {
	/* Operational parameters */
	struct _smallq I, UI;
	uchar_t v_a, v_s, v_r;
	uchar_t RC;
	x75_status status;

	ushort_t offset;	/* reserved message block header, downstream */
#ifdef NEW_TIMEOUT
	int timer_T1;
	int timer_T3;
#endif
	int L3_req:1;
	int RNR:1;
	int sentRR:1;
	int ack_pend:1;
	int inREJ:1;				  /* I have transmitted a REJ frame */
	int trypoll:1;				  /* RNR received */
	int T1:1;					  /* T200 */
	int T3:1;					  /* T203 */
	int asBroadcast:1;			  /* may overlap if more than one UI frame
								   * outstanding (downstream link busy or down */
	/* Configuration *//* Defaults. Timeouts in tenths */
	uchar_t k;					  /* 1 */
	uchar_t N1;					  /* 4 */
	ushort_t RUN_T1;			  /* 10 sec/10; also called T201 */
	ushort_t RUN_T3;			  /* 100 sec/10; T203 */
	ushort_t debug;				  /* debugging flags */
	short debugnr;				  /* to distinguish between debuggers */
	short errors;				  /* *10 if resetup, die if >100, -1 on data */
	int wide:1;					  /* use 7-bit sequence numbers */
	int poll:1;					  /* Aggressively poll if RNR */
	int ignoresabm:1;			  /* don't reset on SABM/SABME */
	int broadcast:1;			  /* Never establish a connection */

	/* Callbacks */
	void *ref;
	P_data send;				  /* Downstream */
	P_data recv;				  /* Upstream. Frees the mblock on error! */
	P_candata cansend;			  /* Downstream: can transmit X.75 blocks */
	P_candata canrecv;			  /* Upstream: queue non-full / receiver
								   * receiving */
	P_state state;				  /* Line going up/down/whatever */
	P_flush flush;
	P_backenable backenable;
} *x75;

/*
 * Initialization
 */
int x75_initconn (x75 state);

/*
 * Data space available?
 */
int x75_canrecv (x75 state);	  /* upstream */
int x75_cansend (x75 state, char isUI);	/* downstream */

/*
 * Data I/O.
 */
int x75_recv (x75 state, char cmd, mblk_t * mb);		/* Address recognition
														 * and removal is up to
														 * you. */
int x75_send (x75 state, char isUI, mblk_t * mb);		/* isUI&2 if broadcast */

#ifdef CONFIG_DEBUG_ISDN
#define x75_check_pending(a,b) deb_x75_check_pending(__FILE__,__LINE__,(a),(b))
int deb_x75_check_pending (const char *, unsigned int, x75 state, char fromLow);/* Check state */
#else
int x75_check_pending (x75 state, char fromLow);/* Check state */
#endif

/*
 * Mode change
 */
#ifdef CONFIG_DEBUG_ISDN
int deb_x75_changestate (const char *, unsigned int, x75 state, uchar_t ind, char isabort);
#define x75_changestate(a,b,c) deb_x75_changestate(__FILE__,__LINE__,(a),(b),(c))
#else
int x75_changestate (x75 state, uchar_t ind, char isabort);
#endif


#endif							/* __X75LIB_ */
