#ifndef __IP_MON
#define __IP_MON

#include "primitives.h"
#include <netinet/in.h>
#ifndef linux
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#endif

#define IP_MON_TIMEOUT CHAR2('t','o')
#define IP_MON_EACHPACKET CHAR2('p','a')
#define IP_MON_FIRSTPACKET CHAR2('p','f')
#define IP_MON_SUMMARY CHAR2('p','s')

#define IP_MON_NAME "/dev/ip_mon"

typedef struct _monitor {
	unsigned long t_first;  /* kernel jiffies -- Timestamp of first packet */
	unsigned long t_last;   /* kernel jiffies -- Timestamp of last packet */
	unsigned long local;	/* Address of "upstream" site */
	unsigned long remote;	/* Address of "downstream" site */
	ulong_t bytes;			/* Byte count */
	ushort_t packets;		/* Packet count */
	ushort_t p_local;		/* local  port, or ICMP type */
	ushort_t p_remote;		/* remote port, or ICMP code */
	uchar_t p_protocol;		/* TCP/UDP/ICMP/??? */
	char dir;				/* incoming packet? */
} monitor;

#endif							/* __IP_MON */
