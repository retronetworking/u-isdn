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
	long uptimer;
#endif
	struct _dumb *next;
	int numHSCX;
	long countme; signed char polled; char circ;
	unsigned int do_uptimer:1;
} *__dumb;

extern struct _dumb dumbdata[];

#define M_TRANSPARENT 12			  /* no idle */
#define M_TRANS_ALAW 13			  /* a-law idle */
#define M_TRANS_V110 14			  /* V.110 idle */
#define M_TRANS_HDLC 15			  /* HDLC flags */
#define M_HDLC 20
#define M_HDLC_7L 21			  /* skip bit 8 */
#define M_HDLC_7H 22			  /* skip bit 0 */
#define M_HDLC_N0 23			  /* insert one after seven zeroes */
#define M_HDLC_16 24			  /* insert one after seven zeroes */

#endif
