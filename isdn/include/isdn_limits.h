#ifndef _ISDN_LIMITS
#define _ISDN_LIMITS

#define MINOR_MAX 240

#define MAX_B 2					  /* max # of B channels per card */
#define MAX_D 3					  /* max # D channel data connections, i.e. X25 */
#define MAXCHAN (MAX_B+MAX_D)
#define NMINOR MINOR_MAX
#define NPORT MINOR_MAX
#define MAXNR 31

#define MAXTEST 50

#endif							/* _ISDN_LIMITS */
