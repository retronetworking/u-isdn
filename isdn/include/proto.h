#ifndef _PROTO_H
#define _PROTO_H

#include "primitives.h"
#include "isdn_limits.h"

#define NIPROTO NCONN+5
#define PROTO_NAME "proto"

#define PROTO_CR CHAR2('c','r')	 	/* AT S3=x */
#define PROTO_LF CHAR2('l','f')	  	/* AT S4=x */
#define PROTO_BACKSPACE CHAR2('b','s')	/* AT S5=x */
#define PROTO_ABORT CHAR2('c','c')	/* No standard parameter; clears the current * line */
#define PROTO_CARRIER CHAR2('c','a')	/* AT &Cx */
#define PROTO_BREAK CHAR2('b','k')	/* Sending a break returns to command mode */
#define PROTO_ONLINE CHAR2('o','n')	/* We're on-line */
#define PROTO_OFFLINE CHAR2('o','f')	/* Command mode */
#define PROTO_BINCMD CHAR2('m','b')	/* commands are binary data */
#define PROTO_ASCIICMD CHAR2('m','a')	/* Commands are ASCII lines */
#define PROTO_SIGNALS CHAR2('s','g')	/* Send signals up? */
#define PROTO_PREFIX CHAR2('p','p')	/* prefix string */
	/* first char: data; second: commands to be sent down; third: LOCAL */

#define PREFIX_DATA 0
#define PREFIX_PROTO 1
#define PREFIX_LOCALPROTO 2
#define PREFIX_MAX 3
/*
 * Signals are: SIGUSR1 connect, SIGUSR2 disconnect, SIGPWR will_disconnect.
 * 
 * SIGHUP is controlled by the PROTO_CARRIER flag and generated with M_HANGUP.
 */

#endif							/* _PROTO_H */
