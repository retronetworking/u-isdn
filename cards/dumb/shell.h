#ifndef CONF_H
#define CONF_H
#include "isdn_limits.h"
#include "isdn_12.h"
#include "smallq.h"
#include <stream.h>
#include "loader.h"

extern int dumb_num;

#define DEBUG(_x) if((DEBUG_##_x) & dumb->info.debug)
#define DEBUG_memory  0x01
#define DEBUG_uart    0x00
#define DEBUG_isac    0x04
#define DEBUG_hscx    0x08
#define DEBUG_hscxout 0x02
#define DEBUG_cpu     0x00
#define DEBUG_check   0x20
#define DEBUG_info    0x40
#define DEBUG_main    0x80

typedef unsigned char Byte;
typedef struct _hdlc_buf {
	mblk_t *m_in, *m_in_run;
	unsigned short inlen;
	struct _smallq q_out;
	mblk_t *m_out, *m_out_run;
	unsigned char *p_out;
	Byte mode;
	Byte locked,listen;
	u_short nblk,maxblk,offset;
} *hdlc_buf;

typedef struct _dumb {
	struct _isdn1_card card; /* must be first */
	struct cardinfo info, *infoptr;

	struct _hdlc_buf chan[MAX_B+1];
#ifdef NEW_TIMEOUT
	long timer;
#endif
	struct _dumb *next;
	int numHSCX;
	long countme; signed char polled; char circ;
} *__dumb;

extern struct _dumb dumbdata[];
#endif
