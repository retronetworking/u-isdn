#ifndef _DUMP_H_
#define _DUMP_H_

#include <sys/types.h>
#include "isdn_23.h"

#ifdef DEBUG
#define inc(_buf,_len,_inc) do { 			\
	if ((_len) < (_inc)) {					\
		printf("Need %d",_inc);				\
		dumpblock(" bytes",(_buf),(_len));	\
		return; }							\
	(_buf) += (_inc);						\
	(_len) -= (_inc);						\
  } while(0)
#else
#define inc(_buf,_len,_inc) do { 			\
	(_buf) += (_inc);						\
	(_len) -= (_inc);						\
  } while(0)
#endif

void dumpblock (char *name, uchar_t * buf, ushort_t len);
void dumpascii (uchar_t * buf, ushort_t len);
void dumphex (uchar_t * buf, ushort_t len);
void dumpaschex (uchar_t * buf, ushort_t len);

void hexm (uchar_t * tr, int *len);

char *conv_ind (unsigned char xx);
void dump_hdr (isdn23_hdr hdr, const char *what, uchar_t * data);

#endif							/* _DUMP_H_ */
