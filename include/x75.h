#ifndef _X75_H
#define _X75_H

#include "primitives.h"

#define NX75 16

#define X75_K CHAR2('n','k')	  /* Max number of outstanding I frames */
#define X75_WIDE CHAR2('w','d')	  /* modulo 127 */
#define X75_NOTWIDE CHAR2('n','w')/* modulo 7 */
#define X75_WIDEADDR CHAR2('w','a')	  /* modulo 127 */
#define X75_NOTWIDEADDR CHAR2('n','a')/* modulo 7 */
#define X75_IGNORESABM CHAR2('i','s')
#define X75_NOIGNORESABM CHAR2('n','i')
#define X75_POLL CHAR2('p','o')	  /* In case the other side forgets to send RR
								   * after RNR. Hello, wherever you are... */
#define X75_NOPOLL CHAR2('n','p') /* Not forgetting... */
#define X75_N1 CHAR2('n','1')
#define X75_T1 CHAR2('t','1')
#define X75_T3 CHAR2('t','3')
#define X75_ADR CHAR2('a','d')
#define X75_CONCAT CHAR2('c','d')
#define X75_CONNMODE CHAR2('c','m')
#define X75_DEBUG CHAR2('d','e')
#define X75_SENDINTR CHAR2('i','I')
#define X75_SENDDISC CHAR2('i','D')

/* When to establish the connection. Zero means to assume established. */
#define X75CONN_OUT  01			  /* Establish when dialling out */
#define X75CONN_IN   02			  /* Establish when incoming */
#define X75CONN_DATA 04			  /* Establish when sending the first data
								   * packet */
#define X75CONN_UI	 010		  /* Forget establishing anything -- use UI
								   * frames. */
#define X75CONN_PPP  020		  /* Do PPP AC compression. */
#define X75CONN_DONE 0			  /* The connection is assumed to be
								   * established.  This is a compatibility mode
								   * (also called "stupidity hack") for
								   * connecting to people who think they don't
								   * need to read protocol specs, much less
								   * adhere to them. */

#endif							/* _X75_H */
