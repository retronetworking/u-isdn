#ifndef _SLIP_H
#define _SLIP_H

#include "primitives.h"
#include "f_ioctl.h"

#define NSLIP 4
#define SLIP_MAX 1550

struct slip_stats {
	int pkt_in;
	int pkt_out;
	int byte_in;
	int byte_out;
	int errors;
};

#define SLIP_STATE    _IOR('h',0,struct slip_stats)
#define SLIP_CLRSTATE _IO ('h',1)

#define SLIP_MSGSIG   _IO('h',2)
#define SLIP_MSGPROTO _IO('h',3)

#endif /* _SLIP_H */
