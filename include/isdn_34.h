#ifndef _ISDN_34
#define _ISDN_34

#include <sys/types.h>
#include <sys/ioctl.h>
#include "config.h"

/**
 ** Interface between Level 3 and the Level 4 handler
 **
 ** Device: /dev/isdnmaster, push the isdn_3 module.
 **/

/*
 * Messages are textual. Set stream head to message mode(discard) for best
 * results.
 */

/*
 * Message format: See streamlib.h. In the samples below, replace names in
 * capitals with the two-letter sequences they stand for and lower-case names
 * with types with appropriate arguments.
 */

#define PREF_NOERR '='	/* Prefix to disable error replies */

#define CMD_LOADFILE CHAR2('l','F')     /* download part of a file */

#define CMD_FAKEOPEN CHAR2('f','o')		/* Indicate that a /dev/isdn device has
										 * been opened. This is used to avoid a
										 * possible race condition */
#define CMD_CLOSE CHAR2('f','c')  /* Kick a port. */
#define CMD_LIST   CHAR2 ('l','i')/* List cards "CMD_LIST :ARG_CARD" Should not
								   * be necessary in normal cases because for
								   * each card, IND_CARD is sent up
								   * automatically when the driver is opened. */
#define CMD_PROT   CHAR2 ('p','r')/* Protocol info for modules. "CMD_PROT
								   * :ARG_MINOR minor ::string" The meaning of
								   * the string argument is described in
								   * isdn_proto.h. */
#define CMD_CARDSETUP CHAR2 ('s','c')	/* Protocol info for cards done. */
#define CMD_CARDPROT CHAR2 ('p','c')	/* Protocol info for cards.
										 * "CMD_CARDPROT :ARG_CARD card
										 * :ARG_CHANNEL channel ::string" */
#define CMD_NOPROT CHAR2 ('n','p')/* Protocol info was insufficient -- try
								   * again */
#define CMD_OFF    CHAR2 ('d','i')/* Take a connection down. "CMD_OFF
								   * :ARG_MINOR minor" */
#define CMD_INFO   CHAR2 ('-','-')/* Send a response text to the device.
								   * "CMD_INFO :ARG_FMINOR minor ::text" */
#define CMD_ATPARAM CHAR2 ('a','p')		/* Set AT command interpreter
										 * parameters. "CMD_ATPARAM CR LF BS BR
										 * Ca Br" Defaults: "ap 13 10 8 3 1 1".
										 * If BS and BR are both zero, commands
										 * will be interpreted directly. This
										 * way, specialized drivers can do
										 * their own call management. */
#define CMD_NOCARD CHAR2('q','C') /* (active) card is to be presumed dead */

/* Commands for ISDN-B access. */
#define CMD_DIAL   CHAR2 ('d','o')/* Dial out. */
#define CMD_ANSWER CHAR2 ('a','n')/* Answer a call. */
#define CMD_PREPANSWER CHAR2 ('a','p')
#define CMD_FORWARD CHAR2('f','w')/* Forward this call */

/* Replies. */
#define IND_PROTO  CHAR2 ('i','p')/* Set up a protocol stack. */
#define IND_PROTO_AGAIN  CHAR2 ('i','a')/* Set up a protocol stack. */
#define IND_CARDPROTO  CHAR2 ('i','c')/* Set up a protocol stack. */
#define IND_ERR    CHAR2 ('e','r')/* Command error. */
#define IND_NOERR    CHAR2 ('o','k')/* Command error. */
#define IND_INCOMING CHAR2 ('i','n')	/* Incoming call. */
#define IND_CONN   CHAR2 ('c','o')/* Connected. */
#define IND_CONN_ACK CHAR2('C','A')
#define IND_DISC   CHAR2 ('D','i')/* Disconnected. */
#if 0
#define IND_PACKET CHAR2 ('p','a')/* Unanalysed packet data from below. */
#endif
#define IND_ATCMD  CHAR2 ('d','s')/* Command to analyse. */
#define IND_ATRESP CHAR2 ('d','r')/* Repackage this into a reply the other side
								   * can understand. */
#define IND_PRRESP CHAR2 ('p','r')/* Protocol packet from below. Indication
								   * that the D channel stack is open or
								   * closed. */
#if 0
#define IND_EXPAND  CHAR2 ('e','x')		/* Expand this */
#endif
#define IND_CLOSE   CHAR2 ('c','p')		/* Port closed. */
#define IND_OPEN    CHAR2 ('o','p')		/* Port opened. */
#define IND_INFO	CHAR2 ('-','-')		/* Informational message */
#define CMD_DOCARD  CHAR2 ('!','C')		/* Set up this card */
#define IND_CARD	CHAR2 ('C','d')		/* Card coming online. IND_CARD
										 * arg_card arg_bchan arg_modemask. */
#define IND_NOCARD	CHAR2 ('C','n')		/* Card going offline. IND_NOCARD
										 * arg_card. */
#define IND_TRACE CHAR2('t','r')

/*
 * Arguments. short,long,uchar are encoded decimally, ushort and ulong as
 * unsigned hex, ident as four letters, strings as-is.
 */
#define ARG_PREFOUT CHAR2('x','i') /* drop an incoming call if we're also calling out */
#define ARG_FORCEOUT CHAR2('y','i') /* always call back instead of accepting */
#define ARG_BACKCALL CHAR2('b','i') /* call back if incoming call rejected because busy */
#define ARG_NOREJECT CHAR2('n','j') /* don't send REJ code */
#define ARG_FASTDROP CHAR2('f','X')
#define ARG_FASTREDIAL CHAR2('f','r')
#define ARG_CHANBUSY CHAR2('i','b')
#define ARG_IGNORELIMIT CHAR2('i','l')

#define ARG_FORCE  CHAR2('f','d') /* In CMD_OFF: Force DISC */
#define ARG_NOCONN CHAR2('N','C') /* don't infer a connection */
#define ARG_NODISC CHAR2('N','D') /* don't disconnect */
#define ARG_INT		CHAR2('d','I') /* don't hangup when disconnecting */
#define ARG_NOINT	CHAR2('n','I') /* do hangup when disconnecting */
#define ARG_CAUSE    CHAR2('c','x') /* Cause of error, ID */
#if 0
#define ARG_EXPAND   CHAR2 ('e','x')	/* Punt to L4 for expansion. Example:
										 * ":exnr smurf" -> ":nr
										 * 49.721.961252.1". May depend on the
										 * presence of other arguments. */
#endif
#define ARG_ERRNO    CHAR2 ('e','r')	/* short errno */
#if 0
#define ARG_MAGIC    CHAR2 ('m','g')	/* long */
#endif
#define ARG_CONNREF  CHAR2 ('C','r')	/* uchar refnum for a connection.  */
#define ARG_CARD     CHAR2 ('c','d')	/* ident Card */
#define ARG_SUBCARD  CHAR2 ('c','s')	/* D channel on card */
#define ARG_CHANNEL  CHAR2 ('b','c')	/* B channel to use */
#define ARG_ASSOC    CHAR2 ('a','=')	/* associate card IDs */
#define ARG_LISTEN   CHAR2 ('l','p')    /* listen params */

#define ARG_MINOR    CHAR2 ('m','i')	/* uchar minor number of data
										 * connection */
#define ARG_FMINOR   CHAR2 ('m','f')	/* uchar minor number of command
										 * connection */
#define ARG_CALLREF  CHAR2 ('c','r')	/* long call reference number */
#define ARG_PBX      CHAR2 ('p','b')    /* behave like a PBX */
#define ARG_EAZ      CHAR2 ('e','a')	/* map EAZ to local phone nr */
#define ARG_LNUMBER  CHAR2 ('l','r')	/* local phone nr */
#define ARG_NUMBER   CHAR2 ('n','r')	/* remote phone nr */
#define ARG_OUTNUMS  CHAR2 ('o','m')    /* outgoing, for build */
#define ARG_INNUMS   CHAR2 ('i','m')    /* incoming, for match */
#define ARG_BOTHNUMS CHAR2 ('b','m')    /* in+out, default */
#define ARG_NEEDLOCAL CHAR2('v','l')    /* local numer required, incoming */
#define ARG_NEEDREMOTE CHAR2('v','r')   /* remote numer required, incoming */
#define ARG_NEEDNOLOCAL CHAR2('V','l')  /* local numer missing; incoming */
#define ARG_NEEDNOREMOTE CHAR2('V','r') /* remote numer missing; incoming */
#define ARG_SERVICE  CHAR2 ('s','v')	/* ulong service type. */
#define ARG_BEARER   CHAR2 ('v','B')	/* vector Bearer Capability. */
#define ARG_LLC      CHAR2 ('v','L')	/* vector Lower Level Compat. */
#define ARG_ULC      CHAR2 ('v','U')	/* vector Upper Level Compat. */
#define ARG_MODE     CHAR2 ('m','o')	/* uchar Mode to switch B channel to. */
#define ARG_MODEMASK CHAR2 ('m','m')	/* ulong Mask of supported modes */
#define ARG_SUPPRESS CHAR2 ('s','n')	/* Suppress calling number */
#define ARG_PROTOCOL CHAR2 ('p','r')	/* uchar L3 protocol */
#define ARG_ERRHDR   CHAR2 ('e','H')    /* reported error type */
#define ARG_SPV		 CHAR2 ('p','v')	/* semipermanent */
#define ARG_FORCETALK CHAR2 ('F','t')	/* force talker */
#define ARG_SUBPROT  CHAR2 ('s','p')	/* long Subprotocol to use. For SAPI 0:
										 * "65" for calls according to 1TR6,
										 * "0" for Q.931. */
#define ARG_DELAY    CHAR2 ('d','l')	/* Answer delay */
#define ARG_STACK    CHAR2 ('s','t')	/* uchar Protocol stack to establish,
										 * as defined by L4 config files. */
#define ARG_FLAGS    CHAR2('f','l')		/* Flags, with open() */
#define ARG_UID      CHAR2('u','i')		/* uid of whoever opened the device */
#define ARG_GID      CHAR2('g','i')		/* gid of whoever opened the device */
#define ARG_CHARGE	 CHAR2('C','I')		/* what's it cost */

#define ARG_SEQNUM   CHAR2('s','N') /* number of the file */
#define ARG_OFFSET   CHAR2('o','F') /* offset within the file */
#define ARG_LENGTH   CHAR2('l','N') /* offset within the file */

#define ARG_UPDELAY CHAR2('u','d') /* Delay the data exchange */

#define ARG_POINTOPOINT CHAR2('p','p') /* point-to-point link */
#define ARG_MULTIPOINT1 CHAR2('m','p') /* multipoint link, variable ID */
#define ARG_MULTIPOINT2 CHAR2('m','f') /* multipoint link, fixed ID */
#define ARG_MULTIPOINT3 CHAR2('m','t') /* multipoint link, fixed TEI */

#define ARG_ANSWER_IMMED CHAR2('a','i') /* grab the TEI on startup */
#define ARG_ANSWER_DELAY CHAR2('a','d') /* grab the TEI when needed */

#define ARG_TEI_IMMED CHAR2('t','i') /* grab the TEI on startup */
#define ARG_TEI_DELAY CHAR2('t','d') /* grab the TEI when needed */

#define ARG_L2KEEP CHAR2('k','l') /* keep L2 open */
#define ARG_L2CLOSE CHAR2('c','l') /* close L2 when no connection is pending */
#define ARG_DEBUG CHAR2('d','e')
#define ARG_EVENT CHAR2('e','v')
#define ARG_SUBEVENT CHAR2('e','s')

/* Other arguments are protocol dependent. See the appropriate include files. */

#define LISTARG_CONN CHAR2 ('L','C')
#define LISTARG_CARD CHAR2 ('L','K')

#endif							/* _ISDN_34 */
