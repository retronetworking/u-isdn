#include "f_module.h"
#include "primitives.h"
#include <sys/errno.h>
#include "smallq.h"
#include "streams.h"
#include "streamlib.h"
#ifndef KERNEL
#include "kernel.h"
#endif

/**** extremely simple queue handling ****/
#define LI 0

#ifdef CONFIG_DEBUG_STREAMS
#define T_ENQUEUE 2
#define T_DEQUEUE 3
#endif

#ifdef CONFIG_DEBUG_STREAMS
int
cS_enqueue (const char *deb_file, unsigned int deb_line, smallq q, mblk_t * mb)
#else
int
S_enqueue (smallq q, mblk_t * mb)
#endif
{
	int ms = splstr ();

	if (LI)
		printf ("SE -%p %d\n", mb, dsize (mb));
#ifdef CONFIG_DEBUG_STREAMS
	if(mb->deb_queue != NULL) {
		printf("%s:%d enqueues %p to %p, was put on %p by %s:%d\n",
			deb_file,deb_line,mb,q,mb->deb_queue,mb->deb_file,mb->deb_line);
		return 0; /* toss it -- it's still(?) on that queue! */
	}
	if (!cS_check (deb_file, deb_line, q, mb)) {
		splx (ms);
		return -EIO;
	}
#endif
	if (q->last == NULL) {
		q->first = mb;
	} else {
		q->last->b_next = mb;
	}
	q->last = mb;
	mb->b_next = NULL;
	q->nblocks++;
#ifdef CONFIG_DEBUG_STREAMS
	(void) cS_check (deb_file, deb_line, q, NULL);
	mb->deb_queue = (queue_t *)q;
	mb->deb_file = deb_file;
	mb->deb_line = deb_line+100000;
#endif
	splx (ms);
	return 0;
}

#ifdef CONFIG_DEBUG_STREAMS
int
cS_requeue (const char *deb_file, unsigned int deb_line, smallq q, mblk_t * mb)
#else
int
S_requeue (smallq q, mblk_t * mb)
#endif
{
	int ms = splstr ();

	if (LI)
		printf ("SR -%p\n", mb);
#ifdef CONFIG_DEBUG_STREAMS
	if(mb->deb_queue != NULL) {
		printf("%s:%d enqueues %p to %p, was put on %p by %s:%d\n",
			deb_file,deb_line,mb,q,mb->deb_queue,mb->deb_file,mb->deb_line);
		return 0; /* toss it */
	}
	if (!cS_check (deb_file, deb_line, q, mb)) {
		splx (ms);
		return -EIO;
	}
#endif
	if (q->first == NULL) {
		q->last = mb;
	}
	mb->b_next = q->first;
	q->first = mb;
	q->nblocks++;
#ifdef CONFIG_DEBUG_STREAMS
	(void) cS_check (deb_file, deb_line, q, NULL);
	mb->deb_queue = (queue_t *)q;
	mb->deb_file = deb_file;
	mb->deb_line = deb_line+100000;
#endif
	splx (ms);
	return 0;
}

#ifdef CONFIG_DEBUG_STREAMS
mblk_t *
cS_dequeue (const char *deb_file, unsigned int deb_line, smallq q)
#else
mblk_t *
S_dequeue (smallq q)
#endif
{
	int ms = splstr ();
	mblk_t *mb = q->first;

#ifdef CONFIG_DEBUG_STREAMS
	if(mb != NULL && mb->deb_queue != (queue_t *)q) {
		printf("%s:%d dequeues %p from %p, but is on %p by %s:%d\n",
			deb_file,deb_line,mb,q,mb->deb_queue,mb->deb_file,mb->deb_line);
		return NULL;
	}
	if (!cS_check (deb_file, deb_line, q, NULL)) {
		splx (ms);
		return NULL;
	}
#endif

	if (mb != NULL) {
		if ((q->first = mb->b_next) == NULL)
			q->last = NULL;
		q->nblocks--;

		mb->b_prev = NULL;
		mb->b_next = NULL;
	}
	if (LI)
		printf ("SD +%p\n", mb);
#ifdef CONFIG_DEBUG_STREAMS
	if (!cS_check (deb_file, deb_line, q, mb)) {
		splx (ms);
		return NULL;
	}
	if(mb != NULL) {
		mb->deb_queue = NULL;
		mb->deb_file = deb_file;
		mb->deb_line = deb_line+100000;
	}
#endif
	splx (ms);
	return mb;
}

#ifdef CONFIG_DEBUG_STREAMS
mblk_t *
cS_nr (const char *deb_file, unsigned int deb_line, smallq q, int nr)
#else
mblk_t *
S_nr (smallq q, int nr)
#endif
{
	int ms = splstr ();
	mblk_t *mb;
	int onr = nr;

#ifdef CONFIG_DEBUG_STREAMS
	if (!cS_check (deb_file, deb_line, q, NULL) || nr >= q->nblocks) {
		splx (ms);
		return NULL;
	}
#endif
	mb = q->first;
	while (mb != NULL && nr-- > 0)
		mb = mb->b_next;
#ifdef CONFIG_DEBUG_STREAMS
	if(mb->deb_queue != (queue_t *)q) {
		printf("%s:%d gets %p (#%d) from %p, but is on %p by %s:%d\n",
			deb_file,deb_line,mb,onr,q,mb->deb_queue,mb->deb_file,mb->deb_line);
		return NULL;
	}
#endif
	mb = dupmsg (mb);
	if (LI)
		printf ("SN  %d %d\n", onr, dsize (mb));
#ifdef CONFIG_DEBUG_STREAMS
	if(mb != NULL) {
		mb->deb_queue = NULL;
		mb->deb_file = deb_file;
		mb->deb_line = deb_line+100000;
	}
#endif
	splx (ms);
	return mb;
}

#ifdef CONFIG_DEBUG_STREAMS
int
cS_flush (const char *deb_file, unsigned int deb_line, smallq q)
#else
int
S_flush (smallq q)
#endif
{
	int ms = splstr ();

#ifdef CONFIG_DEBUG_STREAMS
	(void)cS_check (deb_file, deb_line, q, NULL);
	if (q->first == NULL) {
		splx (ms);
		return -ENOENT;
	}
#endif
	if (LI)
		printf ("SF\n");
	while (q->first != NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		freemsg (cS_dequeue (deb_file, deb_line, q));
#else
		freemsg (S_dequeue (q));
#endif
	}
	if (LI)
		printf ("SF end\n");
	q->nblocks = 0;
	splx (ms);
	return 0;
}

#ifdef CONFIG_DEBUG_STREAMS
int
cS_check (const char *deb_file, unsigned int deb_line, smallq q, mblk_t * ms)
{
	int nx = 0;
	mblk_t *x = q->first;

#if 0
	if (ms != NULL && isanywhere (ms, deb_file, deb_line))
		return 0;
#endif
	while (x != NULL && nx < 100) {
		nx++;
		if(deb_msgdsize(deb_file,deb_line, x) < 0)
			return 0;
		if (x == ms) {
			printf ("ALARM q %s:%d %p: QueueHas %p; f %p, l %p; %s:%d\n",
				deb_file, deb_line, q, ms, q->first, q->last, x->deb_file,x->deb_line);
			return 0;
		}
		if ((x->b_next == NULL) != (x == q->last)) {
			printf ("ALARM q %s:%d %p: NotEnd %p %p; f %p, l %p; %s:%d\n",
				deb_file, deb_line, q, x, x->b_next, q->first, q->last,x->deb_file,x->deb_line);
			q->last = x; x->b_next = NULL;
			return 0;
		}
		x = x->b_next;
	}
	if (nx != q->nblocks)
		printf ("ALARM q %s %d %p: nblocks %d (is %d); f %p, l %p\n", deb_file, deb_line, q, q->nblocks, nx, q->first, q->last);
	if (nx == 200) {
		printf (" q %s:%d %p: Killing link (N>200); f %p, l %p.\n", deb_file, deb_line, q, q->first, q->last);
		q->last->b_next = NULL;
	}
	return 1;
}

#endif


#ifdef MODULE
static int do_init_module(void)
{
	return 0;
}

static int do_exit_module(void)
{
	return 0;
}
#endif
