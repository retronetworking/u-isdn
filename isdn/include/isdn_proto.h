#ifndef _ISDN_PROTO
#define _ISDN_PROTO

#include "streams.h"
#include "msgtype.h"
#include "primitives.h"

/*
 * Special codes for protocols. Sent as MSG_PROTO messages to modules,
 * HDR_PROTOCMD betweel L2/L3, and CMD_PROT / CMD_CARDPROT between L3/L4.
 */

#define PROTO_SYS '*'			  /* System status */

/*
 * System status messages. Example: "*co". Forwarded by all modules. Some of
 * these are generated by L2 when attaching / detaching devices.
 */

#define PROTO_WANT_CONNECTED CHAR2('w','c')		/* down */
#define PROTO_CONNECTED CHAR2('c','o')	/* Connection established (up) */
#define PROTO_HAS_CONNECTED CHAR2('h','c')		/* down */
#define PROTO_LISTEN CHAR2('l','i')		/* Conn partially established (up) */
#define PROTO_HAS_LISTEN CHAR2('h','l')	/* down */

#define PROTO_INTERRUPT CHAR2('i','s')	/* Temporarily gone. (up/down) */
#define PROTO_DISCONNECT CHAR2('d','i')	/* Gone. (up/down) */
#define PROTO_WILL_INTERRUPT CHAR2('w','i')
#define PROTO_WILL_DISCONNECT CHAR2('w','d')	/* Please shutdown. (up) */
#define PROTO_HAS_INTERRUPT CHAR2('h','i')
#define PROTO_HAS_DISCONNECT CHAR2('h','d')		/* Did shutdown. (down) */

#define PROTO_ENABLE CHAR2('e','a')
#define PROTO_HAS_ENABLE CHAR2('e','h')
#define PROTO_DISABLE CHAR2('d','a')
#define PROTO_HAS_DISABLE CHAR2('d','h')

#define PROTO_INCOMING CHAR2('i','n')	/* Incoming call. */
#define PROTO_OUTGOING CHAR2('o','u')	/* Outgoing call. */

#define PROTO_OFFSET CHAR2('o','s')	/* do mblk preallocation */

#define PROTO_AT CHAR2('a','t')	  /* Command. "*at ATD9612521". */
#define PROTO_MODULE CHAR2('m','s')		/* Setup for a protocol */
#define PROTO_MODLIST CHAR2('m','l')	/* list of modules to be pushed */
#define PROTO_ERROR CHAR2('e','r')/* Error. */
#define PROTO_NOERROR CHAR2('o','k')	/* Error. */
/*
 * Connection setup on the data stream: LISTEN goes up, HAS_LISTEN goes down.
 * IN/OUT goes up. CONNECTED goes up and is forwarded by a module if its part
 * of connection setup is completed. HAS_CONNECTED goes down.
 * 
 * Disconnect request: WILL_DISCONNECT goes up, DISCONNECT goes down and is
 * forwarded by a module if its part of the protocol stack is down.
 * 
 * Disconnect request/indication: DISCONNECT goes up and is not delayed (because
 * the lower levels are already down); HAS_DISCONNECT goes down.
 * 
 * Interrupt works like Disconnect, except that a module may change an interrupt
 * request to disconnect or, if it can hold the protocol stack, do the upstream
 * echo by itself.
 */

/*
 * Module list. Example: "* log t70 x75"
 * 
 * Pop all modules off a device and push the named modules. These modules must not
 * sleep. u.u_error is saved, nothing else may be touched. This is very unsafe
 * but I don't know how to run an arbitrary procedure with the process context
 * of whoever has the device open.
 */

#define PROTO_MODE '-'

/*
 * Mode set command. Example: "- 1 3"
 * 
 * Sent by L3 to L2 to indicate that setup of the protocols is completed and that
 * the connection can be attached to its assigned B or D channel as soon as the
 * connection is established.
 * 
 * The first number tells which mode to switch the card to, the second (optional)
 * how many seconds to wait between sending PROTO_WILL_DISCONNECT upstream, and
 * taking down the connection.
 */

#endif							/* _ISDN_PROTO */