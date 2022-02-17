#ifndef _ISDN3_PHONE
#define _ISDN3_PHONE

/* PD */
#define PD_Q931 8
#define PD_N0 0x40
#define PD_N1 0x41

/*
 * Interface for different phone protocol handlers.
 */

/*
 * Startup.
 */
typedef void (*P_prot_init) (void);

/*
 * Incoming data. isUI != zero if this was an UI frame. Bit 1 of isUI is set if
 * this is a broadcast packet.
 */
typedef int (*P_prot_recv) (isdn3_conn conn, uchar_t msgtype, char isUI, uchar_t * data, ushort_t len);

/*
 * Data from the device. When talking through a D channel connection.
 */
typedef int (*P_prot_send) (isdn3_conn conn, mblk_t * data);

/*
 * Config string for the connection. "id" is the device which sent the data and
 * presumably the one that should get the error message back. ;-)
 */
typedef int (*P_prot_sendcmd) (isdn3_conn conn, ushort_t id, mblk_t * data);

/*
 * Report my current state.
 */
typedef void (*P_prot_report) (isdn3_conn conn, mblk_t *mb);

/*
 * Card or connection changes state. See primitives.h.
 */
typedef int (*P_prot_chstate) (isdn3_conn conn, uchar_t ind, short add);

/*
 * Notify when a talker is being killed (protocol down)
 */
typedef void (*P_prot_kill) (isdn3_talk talk);

/*
 * Notify when a connection is being killed. If "force" is set, the connection
 * must be aborted unconditionally. If not, shut it down normally.
 */
typedef void (*P_prot_killconn) (isdn3_conn conn, char force);

/*
 * Notify when isdn3_setup_conn got called
 */
typedef void (*P_prot_hook) (isdn3_conn conn);

typedef struct _isdn3_prot {
	struct _isdn3_prot *next;	  /* Next handler */
	uchar_t protocol;

	P_prot_init init;
	P_prot_chstate chstate;		  /* L2 goes up or down */
	P_prot_report report;		  /* what's going on? */
	P_prot_recv recv;			  /* Data from L2 */
	P_prot_send send;			  /* Data from the device, if attached to a D
								   * channel connection */
	P_prot_sendcmd sendcmd;		  /* Config data */
	P_prot_killconn killconn;	  /* terminate a connection */
	P_prot_hook hook;			  /* isdn3_setup_conn got called */
} *isdn3_prot;

/*
 * Register a handler
 */
int isdn3_attach_prot (isdn3_prot prot);

/*
 * Find the protocol; "info" has a list of allowable protocols
 */
isdn3_prot isdn3_findprot (mblk_t *info, uchar_t protocol);

/*
 * Extract data into a conn vector.
 */
int phone_get_vector (isdn3_conn conn, uchar_t * data, int len, int vnr, uchar_t dict, uchar_t key);

/*
 * Put data into a vector.
 */
int phone_put_vector (isdn3_conn conn, uchar_t * data, int len, int vnr, uchar_t dict, uchar_t key);


/*
 * Create a message header and send the data with it.
 */
int phone_sendback (isdn3_conn conn, uchar_t msgtype, mblk_t * data);


#define SAPI_PHONE 0

/* Talker */
#define PHONE_UP IS_UP
#define PHONE_DOWNTIMER 02

/* Connection */

#endif							/* _ISDN3_PHONE */
