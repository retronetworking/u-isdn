#ifndef _PRIM_H
#define _PRIM_H

#include <linux/config.h>
#include "compat.h"
#include "config.h"
#include "kernel.h"

#include "msgtype.h"
#include <sys/types.h>
#include <sys/param.h>
#ifdef linux
#include <linux/major.h>
#ifdef KERNEL
#include "compat.h"
#else
#include <stdio.h> /* printf */
#include <unistd.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#endif
#endif

#define bcopy(a,b,c) memcpy(b,a,c)
#define bzero(a,b) memset(a,0,b)

/**
 ** Primitives for L2<->L3 signalling.
 **
 ** Also necessary macros for L3<->L4 signalling.
 **
 ** Also basic channel modes.
 **
 ** Also basic omissions...
 **/

#ifndef NULL
#define NULL (void *)0
#endif

#if BYTE_ORDER == LITTLE_ENDIAN
#define CHAR2(b,a) (((a)<<8)|(b))
#define CHAR4(d,c,b,a) (((((((a)<<8)|(b))<<8)|(c))<<8)|(d))
#else
#define CHAR2(a,b) (((a)<<8)|(b))
#define CHAR4(a,b,c,d) (((((((a)<<8)|(b))<<8)|(c))<<8)|(d))
#endif

#define IND_UP_FIRST 0
#define DL_ESTABLISH_REQ 1
#define DL_ESTABLISH_IND 2
#define DL_ESTABLISH_CONF 3

#define PH_ACTIVATE_IND 11		  /* Bus goes up */
#define PH_ACTIVATE_REQ 12
#define PH_ACTIVATE_CONF 13
#define PH_ACTIVATE_NOTE 14		  /* saw activity */
#define IND_UP_LAST 20

#define DL_RELEASE_REQ 21
#define DL_RELEASE_IND 22
#define DL_RELEASE_CONF 23

#define PH_DEACTIVATE_REQ 31	  /* Bus goes down */
#define PH_DEACTIVATE_IND 32
#define PH_DEACTIVATE_CONF 33
#define PH_DISCONNECT_IND 34	  /* Bus isn't likely to come up any time soon */

#define MDL_ASSIGN_REQ 41		  /* no state for this one */
#define MDL_REMOVE_REQ 42
#define MDL_ERROR_IND 45

#define ERR_A 01
#define ERR_B 02
#define ERR_C 04
#define ERR_D 010
#define ERR_E 020
#define ERR_F 040
#define ERR_G 0100
#define ERR_H 0200
#define ERR_I 0400
#define ERR_J 01000
#define ERR_K 02000
#define ERR_L 04000
#define ERR_M 010000
#define ERR_N 020000
#define ERR_O 040000

#define M_FREE 0
#define M_OFF M_FREE
#define M_STANDBY 1
#define M_ON 2

/* These flags share a common space. PROT_* is for the isdn_2 interaction
   with the module list, PUSH_* is also for picking which modules to kick. */
#define PROT_FROMSTACK 01 /* travels from the channel to the master */
#define PROT_MODLIST 02 /* module list */
#define PROT_TOCARD 04 /* internal to isdn_2.c: send to card first, if there's a handler */
#define PUSH_UPDATE 010 /* Do not unload the old stack */
#define PUSH_BEFORE 020 /* Only unload the modules before reconn */
#define PUSH_AFTER  040 /* only do the modules after reconn */

#endif							/* _PRIM_H */
