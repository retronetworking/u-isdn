#ifndef _ISDN_2
#define _ISDN_2

#include "config.h"
#include <sys/types.h>
#include <sys/param.h>
#include "x75lib.h"
#include "smallq.h"
#include "isdn_limits.h"
#include "msgtype.h"

/* D channel state machine. */
typedef struct _isdn2_state {
	struct _isdn2_state *next;
	struct _isdn2_card *card;
	uchar_t SAPI;
#ifdef DO_MULTI_TEI
	uchar_t bchan;
#endif
	struct _x75 state;
} *isdn2_state;

enum M_state {
	M_free, M_blocked, M_D_ctl, M_D_conn, M_B_conn,
};

typedef struct _isdn2_chan {
	queue_t *qptr;				  /* read queue (i.e. going up) for Streams */
	struct _isdn2_card *card;	  /* NULL if not assigned to a card */
#if 0
	struct _isdn_chan *chan;		  /* B channel: data buffer */
#endif
	mblk_t *bufx;
#ifdef NEW_TIMEOUT
	int timer_unblock;
#endif
	int oflag;					  /* status flag when opening (O_NDELAY ??) */
	int dev;
	enum M_state status;
	uchar_t channel;			  /* Bi / Dconn */
} *isdn2_chan;

enum C_state {
	C_down,						  /* Link is down */
	C_await_up,					  /* Link going up */
	C_await_down,				  /* Link going down */
	C_up,						  /* Link up */
	C_lock_up,					  /* Link up, locked because the wire
	                                 doesn't want to stay down */
	C_wont_down,				  /* Link doesn't go down, eg because the bus
								   * is in use */
	C_wont_up,					  /* Link doesn't go up, eg. because the card
								   * isn't connected */
};

/* internal data, one per card */
typedef struct _isdn2_card {
	struct _isdn1_card *card;
	struct _isdn2_card *next;
	struct _isdn2_chan *chan[MAXCHAN+1];	/* pointer to B/D channels */
#ifdef DO_MULTI_TEI
	struct _isdn2_state *state[MAX_B];
#else
	struct _isdn2_state *state[1];	  /* attached state machines */
#endif
#ifdef NEW_TIMEOUT
	int timer_not_up;
	int timer_takedown;
#endif
	long id;
	int flags;
	enum C_state status;		  /* physical link state */
#ifdef DO_MULTI_TEI
	uchar_t TEI[MAX_B];
	uchar_t bchan;
#else
	uchar_t TEI[1];
#endif
	uchar_t nr;
	int timedown:1;
	int timeup:1;
	int offline:1;			/* temporarily off -- no cable */
} *isdn2_card;

#endif							/* _ISDN_2 */
