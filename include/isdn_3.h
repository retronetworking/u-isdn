#ifndef _ISDN_3
#define _ISDN_3

#include <sys/types.h>
#include <sys/param.h>
#include "streams.h"
#include "isdn_limits.h"
#include "config.h"

/**
 ** Interface of the Level 3 driver
 **
 ** Streams module isdn3_3
 **/

/* Forward decl's */
struct _isdn3_card;
struct _isdn3_conn;
struct _isdn3_talk;

/**
 ** One struct isdn3_card is allocated per connected card.
 **/

typedef struct _isdn3_card {		  /* One per card */
	struct _isdn3_talk *talk;	  /* Connections on the D channel. */
	long id;					  /* Card ID, identifies the card to L4. */
	struct _isdn3_card *next;
	mblk_t *info;				  /* Protocols to attach */
	ulong_t modes;				  /* Mask of permitted modes on this card. */
	uchar_t nr;					  /* Card number, for L2. */
	uchar_t TEI;				  /* For reference */
	uchar_t dchans;				  /* Number of D channels per card. */
	uchar_t bchans;				  /* Number of B channels per D channel. */
	unsigned is_up:2;
} *isdn3_card;

/**
 ** A Handler talks across a L2 D-channel connection.
 **
 ** Handlers are registered with isdn3_attach_hndl.
 ** You should never call a handler directly.
 **/

/*
 * Initialization
 */
typedef void (*P_hndl_init) (void);

/*
 * Called when a new card is registered. This routine would usually set up
 * protocol handlers for TEI management and for incoming connections.
 */
typedef void (*P_hndl_newcard) (struct _isdn3_card * card);

/*
 * Incoming data. isUI != zero if this was an UI frame. Bit 1 of isUI is set if
 * this is a broadcast packet.
 */
typedef int (*P_hndl_recv) (struct _isdn3_talk * talk, char isUI, mblk_t * data);

/*
 * Add protcol specific information to the mblk.
 */
typedef void (*P_hndl_report) (struct _isdn3_conn * conn, mblk_t * data);

/*
 * Data from the device. When talking through a D channel connection.
 */
typedef int (*P_hndl_send) (struct _isdn3_conn * conn, mblk_t * data);

/*
 * Config string for the connection. "id" is the device which sent the data and
 * presumably the one that should get the error message back. ;-)
 */
typedef int (*P_hndl_sendcmd) (struct _isdn3_conn * conn, ushort_t id, mblk_t * data);

/*
 * Card or connection changes state. See primitives.h.
 */
typedef int (*P_hndl_chstate) (struct _isdn3_talk * talk, uchar_t ind, short add);

/*
 * Notify when a talker is being killed (protocol down) if force == 1. If force
 * == 0, handler should check if the protocol can be deactivated
 * temporarily,eg. if no connections are active on it.
 */
typedef void (*P_hndl_kill) (struct _isdn3_talk * talk, char force);

/*
 * Notify when a connection is being killed. If "force" is set, the connection
 * must be aborted unconditionally. If not, shut it down normally.
 */
typedef void (*P_hndl_killconn) (struct _isdn3_conn * conn, char force);

/*
 * Notify when isdn3_setup_conn got called.
 */
typedef void (*P_hndl_hook) (struct _isdn3_conn * conn);

/*
 * Catch what's sent down to L2. If data returns NULL, forward nothing.
 */
typedef int (*P_hndl_proto) (struct _isdn3_conn * conn, mblk_t **data, char down);

typedef struct _isdn3_hndl {
	struct _isdn3_hndl *next;	  /* Next handler */
	uchar_t SAPI;
	uchar_t broadcast;

	P_hndl_init init;
	P_hndl_newcard newcard;		  /* New card shows up */
	P_hndl_chstate chstate;		  /* L2 goes up or down */
	P_hndl_report report;		  /* Report settings */
	P_hndl_recv recv;			  /* Data from L2 */
	P_hndl_send send;			  /* Data from the device, if attached to a D
								   * channel connection */
	P_hndl_sendcmd sendcmd;		  /* Config data */
	P_hndl_kill kill;			  /* Say goodbye */
	P_hndl_killconn killconn;	  /* terminate a connection */
	P_hndl_hook hook;			  /* isdn3_setup_conn was called */
	P_hndl_proto proto;           /* Send cmd; used for catching protocol lists */
} *isdn3_hndl;

/**
 ** One struct isdn3_talk is created per L2 D-channel connection.
 **
 ** Calls managed by this connection are chained off the struct.
 **/

#define NITALK 11
#define NSTALK 5

typedef struct _isdn3_talk {		  /* one per card's D channel connection */
	struct _isdn3_conn *conn;	  /* connections managed on this channel */
	struct _isdn3_card *card;	  /* Back ref to my card */
	struct _isdn3_talk *next;	  /* next talker (chained card->talk->next) */
	struct _isdn3_hndl *hndl;	  /* Handler for this type of talk. */
	uchar_t state;				  /* . Zero: first call / uninitialized. Others
								   * depend on the talker. */
	long talki[NITALK];
	void *talks[NSTALK];
} *isdn3_talk;

/**
 ** One struct isdn3_conn represents one ISDN call.
 **/
#define STACK_LEN 10
#define NICONN 18
#define NSCONN 3
#define NBCONN 2

typedef struct _isdn3_conn {
	struct _isdn3_card *card;	  /* Card this call is running on */
	struct _isdn3_talk *talk;	  /* Back ref to management */
	struct _isdn3_conn *next;	  /* Next connection (chained talk->conn->next) */
	int conn_id;				  /* Sequence number to uniquely identify a connection */
	long call_ref;				  /* Protocol dependent. Uniquely identifies
								   * this connection if used together with
								   * card/protocol/subprotocol. */
	long subprotocol;			  /* Protocol dependent. Phone: Q931 vs Post
								   * vs... */
	char stack[STACK_LEN];		  /* Protocol stack to set up on this
								   * connection */
	int delay;					  /* don't get at it right away */
	long conni[NICONN]; /* additional variables, handler dependent */
	void *conns[NSCONN];
	mblk_t *connb[NBCONN];
#ifdef NEW_TIMEOUT				  /* Grumble */
	int start_timer;
	int delay_timer;
	int later_timer;
	int disc_timer;
#endif							/* Ignore this... */
	uchar_t state;				  /* Protocol dependent. Zero means
								   * free&unassigned. */
	uchar_t lockit;				  /* Lock against inadvertent free */
	uchar_t bchan;				  /* number of B channel. Zero if none or
								   * through D. */
	SUBDEV minor;				  /* minor number for data. Zero if none yet. */
	SUBDEV fminor;				  /* minor number for control. Zero if none yet. */
	ulong_t minorstate;			  /* Activity status. See below for flag bits. */
	uchar_t hupdelay;			  /* Delay between sending PROTO_DISC and
								   * actually disconnecting, in seconds */
	ushort_t id;				  /* If a protocol needs to delay a command */
	mblk_t *id_msg;				  /* for some UL action to complete */
	void *p_data;				  /* protocol specific */
	long timerflags;			  /* for controlling timers */
} *isdn3_conn;

/** Minorstate flags */
/* State */
#define MS_PROTO 01				  /* mirrors MINOR_PROTO flag */
#define MS_INCOMING 02			  /* This is an incoming connection */
#define MS_OUTGOING 04			  /* This is an outgoing connection */

#define MS_CONN_NONE 00
#define MS_CONN_SETUP 010		  /* B channel associated, but no data transfer yet */
#define MS_CONN_LISTEN 020		  /* B channel connected, but connection not fully established */
#define MS_CONN 030				  /* B channel connected */
#define MS_CONN_MASK 030

#define MS_CONN_TIMER 040		  /* Supervisory timer running. The connection
								   * is forcibly terminated if it is not
								   * completed within two minutes. */
#define MS_BCHAN 0100			  /* The B channel is known and set up. (if
								   * zero, it's connected through the D channel */
#define MS_WANTCONN 0200		  /* Set on CMD_DIAL/ANSWER, clear on CMD_OFF. */
#define MS_DETACHED 0400          /* temporarily not connected */
/* Actions */
#define MS_DIR_SENT 01000		  /* The directional info is sent to the
								   * device. */
#define MS_SETUP_MASK   06000     /* what attach has been sent */
#define MS_SETUP_NONE   00000
#define MS_SETUP_ATTACH 02000
#define MS_SETUP_LISTEN 04000
#define MS_SETUP_CONN   06000

#define MS_END_TIMER 010000		  /* The delay timer to end the connection
								   * cleanly is running */
#define MS_SENTINFO 020000		  /* Sent pertinent data up */
#define MS_DELAYING 040000	  /* Wait a bit */
#define MS_INITPROTO 0100000 	  /* card protocols set */
#define MS_INITPROTO_SENT 0200000 /* asked for setup */
#define MS_NOMINOR 0400000
/*
 * if set and ms->delay == zero, we got killconn. This is a hack...
 */
#define MS_CONNDELAY_TIMER 02000000		/* Delay sending PROTO_CONN because the
										 * back channel may get delayed */
#define MS_FORWARDING 04000000	  /* Forwarding this call; in progress. */
#define MS_FORCING 010000000	  /* call is enforced. */

/*
 * Register a handler
 */
int isdn3_attach_hndl (isdn3_hndl hndl);

/*
 * send a reply back to a device. The bypass flag, if not set, causes the
 * message to be passed through L4 to be formatted according to the handler's
 * needs.
 */
int isdn3_at_send (isdn3_conn conn, mblk_t * data, char bypass);

/*
 * send data to this D channel connection
 */
int isdn3_send (isdn3_talk talk, char what, mblk_t * data);

#define AS_DATA 0				  /* I data */
#define AS_UIDATA 1				  /* UI data */
#define AS_UIBROADCAST 2		  /* UI broadcast data */

/*
 * send data to the device
 */
int isdn3_send_conn (SUBDEV minor, char what, mblk_t * data);

#define AS_PROTO 3				  /* Protocol setup */
#define AS_PROTO_NOERR 4		  /* Protocol setup, don't report errors */
#define AS_XDATA 5				  /* Data to be sent up to a device. When
								   * talking across the D channel. */

/*
 * change the state of this connection
 */
int isdn3_chstate (isdn3_talk talk, uchar_t ind, short add, char what);

#define CH_MSG 0				  /* send HDR_NOTIFY */
#define CH_OPENPROT 1			  /* send HDR_OPENPROT */
#define CH_CLOSEPROT 2			  /* send HDR_CLOSEPROT */

/*
 * Find the handler for a specific SAPI. Returns NULL if not found.
 */
isdn3_hndl isdn3_findhndl (uchar_t SAPI);

/*
 * Find the card with the appropriate ID.
 */
isdn3_card isdn3_findcard (uchar_t cardnr);

/*
 * Find the card with the appropriate ID.
 */
isdn3_card isdn3_findcardid (ulong_t cardid);

/*
 * Find the card with the appropriate name.
 */
isdn3_card isdn3_findcardsig (ulong_t cardsig);

/*
 * Find the talk structure for this card/handler pair. Creates a new talker if
 * not found and requested and info matches.
 */
isdn3_talk isdn3_findtalk (isdn3_card card, isdn3_hndl hndl, mblk_t *info, int create);

/*
 * Drop the talker. Usually called by a handler when the last connection
 * terminates.
 */
void isdn3_killtalk (isdn3_talk talk);

/*
 * Drop the connection. If force is clear, run an ordinary shutdown procedure.
 * If set, immediately forget the connection -- this is done when L2 dies.
 */
#ifdef CONFIG_DEBUG_ISDN
void deb_isdn3_killconn (isdn3_conn conn, char force, const char *deb_file, int deb_line);
#define isdn3_killconn(a,b) deb_isdn3_killconn(a,b,__FILE__,__LINE__)
#else
void isdn3_killconn (isdn3_conn conn, char force);
#endif

/*
 * Find the connection this device is associated with.
 */
isdn3_conn isdn3_findminor (ushort_t minor);

/*
 * Find the first connection this device is controlling.
 */
isdn3_conn isdn3_findatcmd (ushort_t minor);

/*
 * Find the connection associated with this call_ref (valid for this protocol).
 */
isdn3_conn isdn3_findconn (isdn3_talk talk, long protocol, long call_ref);

/*
 * Trigger connection chamge management.
 * 
 * Setup the state flags in conn->minorstate and call isdn3_setup_conn whenever
 * something changes.
 */
int Xisdn3_setup_conn (isdn3_conn conn, char established, const char*,unsigned);
#define isdn3_setup_conn(a,b) Xisdn3_setup_conn((a),(b),__FILE__,__LINE__)

#define EST_DISCONNECT 0		  /* The connection is not established. */
#define EST_NO_REAL_CHANGE 2	  /* Marker value. Do not use */
#define EST_WILL_DISCONNECT 4	  /* The connection will be disconnected shortly. */
#define EST_NO_CHANGE 6			  /* Something else happened (minorstate!) */
#define EST_SETUP 8				  /* Associate with card channel */
#define EST_LISTEN 9			  /* half-way communications, backwards */
#define EST_CONNECT 10			  /* End-to-end connection is established */

/*
 * Get a free call reference number.
 */
int isdn3_new_callref (isdn3_talk talk);

/*
 * Find the next free B channel
 */
uchar_t isdn3_free_b (isdn3_card card);

/*
 * get a new free connection and attach it to this talker.
 */
isdn3_conn isdn3_new_conn (isdn3_talk talk);

/*
 * Send a header down; see isdn3_23.h. Handle with care. Used by the TEI
 * handler.
 */
int isdn3_sendhdr (mblk_t * mb);

/*
 * Try this again when conditions change.
 */
void isdn3_repeat (isdn3_conn conn, ushort_t id, mblk_t * data);

/*
 * Add information about the connection to the mblk for reporting to L4.
 */
void conn_info (isdn3_conn conn, mblk_t * mb);

/*
 * Extract flags from conn info.
 */
long isdn3_flags(mblk_t *info, uchar_t protocol, uchar_t subprot);
#define FL_UPDELAY 017
#define FL_UPDELAY_SHIFT 3

#define FL_L2KEEP 020 /* default is to close L2 */
#define FL_TEI_IMMED 040 /* default is to do that later */

#define FL_POINTOPOINT 0100 /* point-to-point */
#define FL_MULTIPOINT1 0200 /* variable ID */
#define FL_MULTIPOINT2 0000 /* fixed ID; special -> default */
#define FL_MULTIPOINT3 0300 /* fixed TEI */
#define FL_POINTMASK 0300

#define FL_ANS_IMMED 0400 /* default is to delay */
#define FL_BUG1 01000
#define FL_BUG2 02000
#define FL_BUG3 04000

/**
 * Convenience macros for handling connection timeouts.
 *
 * // repeat for each timer XXX: //
 * #define RUN_XXX 0001 // flag bit for this timer //
 * #define VAL_XXX (xx * HZ) // timeout value for this timer //
 * static void XXX(isdn3_conn conn); // timeout handler //
 *
 * timer(XXX,conn) registers a timeout and starts it if (conn->timerflags & IS_UP)
 * untimer(XXX,conn) drops a timeout
 * rtimer(XXX,conn) starts a timeout registered with timer() after L2 comes up
 */
#define IS_UP 001

#define UNTIMER(T,c,w,f) do { if((c)->timerflags & f) { untimeout(w,(c)); (c)->timerflags &=~ f; } } while(0)
#define untimer(T,c) UNTIMER(T,c,T,RUN_##T)
#define TIMER(T,c,w,v,f) do { if(!((c)->timerflags & f)) { if((c)->talk->state & IS_UP) timeout(w,(c),v); (c)->timerflags |= f; } } while(0)
#define timer(T,c) TIMER(T,(c),T,VAL_##T,RUN_##T)
#define ntimer(T,c,n) TIMER(T,(c),T,(n),RUN_##T)
#define FTIMER(T,c,w,v,f) do { if(!((c)->timerflags & f)) { timeout(w,(c),v); (c)->timerflags |= f; } } while(0)
#define ftimer(T,c) FTIMER(T,(c),T,VAL_##T,RUN_##T)
#define fntimer(T,c) FTIMER(T,(c),T,(n),RUN_##T)
#define RTIMER(T,c,w,v,f) do { if((c)->timerflags & f) { untimeout(w,(c)); timeout(w,(c),v); } } while(0)
#define rtimer(T,c) RTIMER(T,(c),T,VAL_##T,RUN_##T)
#define rntimer(T,c,n) RTIMER(T,(c),T,(n),RUN_##T)

#endif							/* _ISDN_3 */
