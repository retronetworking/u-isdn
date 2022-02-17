#ifndef __SMALLQ__
#define __SMALLQ__

#include <sys/types.h>
#include "streams.h"
#include "config.h"

typedef struct _smallq {
	ushort_t nblocks;
	mblk_t *first, *last;		  /* Pointer to enqueued data blocks */
} *smallq;

#ifdef CONFIG_DEBUG_STREAMS

int cS_enqueue (const char *deb_file, unsigned int deb_line, smallq q, mblk_t * mb);
int cS_requeue (const char *deb_file, unsigned int deb_line, smallq q, mblk_t * mb);
mblk_t *cS_dequeue (const char *deb_file, unsigned int deb_line, smallq q);
mblk_t *cS_nr (const char *deb_file, unsigned int deb_line, smallq q, int nr);
int cS_flush (const char *deb_file, unsigned int deb_line, smallq q);
int cS_check (const char *deb_file, unsigned int deb_line, smallq q, mblk_t * mb);

#define S_enqueue(q,mp) cS_enqueue(__FILE__,__LINE__,q,mp)
#define S_dequeue(q) cS_dequeue(__FILE__,__LINE__,q)
#define S_requeue(q,mp) cS_requeue(__FILE__,__LINE__,q,mp)
#define S_nr(q,nr) cS_nr(__FILE__,__LINE__,q,nr)
#define S_flush(q) cS_flush(__FILE__,__LINE__,q)
#define S_check(q,m) cS_check(__FILE__,__LINE__,q,m)
#else
int S_enqueue (smallq q, mblk_t * mb);
int S_requeue (smallq q, mblk_t * mb);

mblk_t *S_dequeue (smallq q);

mblk_t *S_nr (smallq q, int nr);

int S_flush (smallq q);

#endif

#endif							/* __SMALLQ__ */
