#ifndef __IP_MON
#define __IP_MON

#include "primitives.h"
#include <netinet/in.h>
#ifndef linux
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#endif

#define IP_MON_TIMEOUT CHAR2('t','o')

#define IP_MON_NAME "/dev/ip_mon"

typedef struct _monitor {
	unsigned long local;	/* Address of "upstream" site */
	unsigned long remote;	/* Address of "downstream" site */
	ulong_t sofar_b;		/* Byte count */
	ulong_t cap_b;			/* Available byte count */
	ushort_t sofar_p;		/* Packet count */
	ushort_t cap_p;			/* Available packet count */
	time_t last;			/* Time of last packet */
	ushort_t p_local;
	ushort_t p_remote;
	uchar_t p_protocol;
	char known;				/* Was reported from above */
} monitor;

#endif							/* __IP_MON */
