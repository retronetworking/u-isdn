#include "primitives.h"
#include "kernel.h"
#include <sys/file.h>
#include <sys/errno.h>
#include "f_signal.h"
#include <sys/stropts.h>
#include "f_termio.h"
#include "f_user.h"
#include <sys/uio.h>
#include <malloc.h>
#include "streams.h"
#include "streamlib.h"
#include "string.h"
#define NDEBUG
#include <assert.h>
#ifndef ASSERT
#define ASSERT(x) assert(x)
#endif
#include <syslog.h>

#ifdef kmalloc
#undef kmalloc
#undef kfree_s
#endif

#if 1
extern void chkall(void);
#else
#define chkall()
#endif
void sysdump(char *a,void *b,int c) { abort(); }

inline void *kmalloc(unsigned int x,int y) { return malloc(x); }
inline void kfree_s(void *x, int y) { free(x); }

#if defined(CONFIG_DEBUG_MALLOC) && !defined(KERNEL)
inline void *deb_kmalloc(const char *deb_file,unsigned int deb_line,size_t x,int y) { return malloc(x); }
inline void deb_kfree_s(const char *deb_file,unsigned int deb_line,void * x,size_t y) { free(x); }
#endif

#ifdef __KERNEL__
#error "Kernel ???"
#endif

#include "../streams/streams_sys.c"

#ifdef linux
unsigned long intr_count = 0;
unsigned long bh_mask = 0;
unsigned long bh_active = 0;
#endif

#ifdef CONFIG_DEBUG_STREAMS

#undef allocb
#define allocb(a,b) deb_allocb(__FILE__,__LINE__,a,b)
#undef freeb
#define freeb(a) deb_freeb(__FILE__,__LINE__,a)
#endif

unsigned long volatile jiffies;
struct timer_struct timer_table[32];
unsigned long timer_active;
#if 0
struct file_operations streams_fops;
int register_chrdev(unsigned int major, const char *name, struct file_operations *fops)
 { syslog(LOG_WARNING,"??? register_chrdev for %s\n",name); return 0; }
#endif

/*
 * Allocate a message and data block.
 */

#ifdef CONFIG_DEBUG_STREAMS
mblk_t * deb_allocb (const char *deb_file, unsigned int deb_line, ushort_t size, ushort_t pri)
#else
mblk_t * allocb (ushort_t size, ushort_t pri)
#endif
{
	dblk_t *databp;
	mblk_t *bp;

	chkall();
	databp = (dblk_t *)malloc (sizeof (dblk_t)+size+1);
	if (databp == NULL)
		return NULL;
	bp = (mblk_t *)malloc (sizeof (mblk_t));
	if (bp == NULL) {
		free (databp);
		return NULL;
	}
	databp->db_base = (streamchar *)(databp+1);
	databp->db_lim = databp->db_base + size;

	/*
	 * initialize message block and data block descriptors
	 */
	bp->b_next = NULL;
	bp->b_prev = NULL;
	bp->b_cont = NULL;
	bp->b_datap = databp;
	bp->b_rptr = databp->db_base;
	bp->b_wptr = databp->db_base;
#ifdef CONFIG_DEBUG_STREAMS
	bp->deb_magic = DEB_PMAGIC;
	bp->deb_queue = NULL;
	bp->deb_file = deb_file;
	bp->deb_line = deb_line;
	databp->deb_magic = DEB_DMAGIC;
#endif
	databp->db_type = M_DATA;
	databp->db_ref = 1;
	chkall();
	return (bp);
}

#ifdef CONFIG_DEBUG_STREAMS
mblk_t * deb_allocsb (const char *deb_file, unsigned int deb_line, ushort_t size, streamchar *data)
#else
mblk_t * allocsb (ushort_t size, streamchar *data)
#endif
{
	dblk_t *databp;
	mblk_t *bp;

	chkall();
	databp = (dblk_t *)malloc (sizeof (dblk_t));
	if (databp == NULL)
		return NULL;
	bp = (mblk_t *)malloc (sizeof (mblk_t));
	if (bp == NULL) {
		free (databp);
		return NULL;
	}
	databp->db_base = data;
	databp->db_lim = data + size;

	/*
	 * initialize message block and data block descriptors
	 */
	bp->b_next = NULL;
	bp->b_prev = NULL;
	bp->b_cont = NULL;
	bp->b_datap = databp;
	bp->b_rptr = databp->db_base;
	bp->b_wptr = databp->db_base + size;
#ifdef CONFIG_DEBUG_STREAMS
	bp->deb_magic = DEB_PMAGIC;
	bp->deb_queue = NULL;
	bp->deb_file = deb_file;
	bp->deb_line = deb_line;
	databp->deb_magic = DEB_DMAGIC;
#endif
	databp->db_type = M_DATA;
	databp->db_ref = 1;
	chkall();
	return (bp);
}


/*
 * test if block of given size can be allocated with a request of the given
 * priority.
 */
#ifdef CONFIG_DEBUG_STREAMS
int deb_testb (const char *deb_file, unsigned int deb_line, ushort_t size, ushort_t pri)
#else
int testb (ushort_t size, ushort_t pri)
#endif
{
	chkall();
	return 1;
}



/*
 * Free a message block and decrement the reference count on its data block. If
 * reference count == 0 also return the data block.
 */
#ifdef CONFIG_DEBUG_STREAMS
void deb_freeb (const char *deb_file, unsigned int deb_line, mblk_t * bp)
#else
void freeb (mblk_t * bp)
#endif
{
	int s;

	chkall();
	ASSERT (bp);

	s = splstr ();
#ifdef CONFIG_DEBUG_STREAMS
	if(bp->deb_magic != DEB_PMAGIC) {
		printf("Bad Magic of %p from %s:%d!\n",
			bp,deb_file,deb_line);
		splx(s);
		return;
	}
	if(bp->b_datap == NULL) {
		printf("Bad Magicn of %p from %s:%d!\n",
			bp,deb_file,deb_line);
		splx(s);
		return;
	}
	if(bp->b_datap->deb_magic != DEB_DMAGIC) {
		printf("Bad Magicd of %p from %s:%d!\n",
			bp,deb_file,deb_line);
		splx(s);
		return;
	}
	if(bp->deb_queue != NULL) {
		printf("Free %p from %s:%d, in queue %p by %s:%d\n",
			bp,deb_file,deb_line,bp->deb_queue,bp->deb_file,bp->deb_line);
		splx(s);
		return;
	}
#endif
	if (!--bp->b_datap->db_ref) 
		free (bp->b_datap);
	free (bp);
	chkall();
	splx (s);
	return;
}



/*
 * allocate a pair of queues
 */

#ifdef CONFIG_DEBUG_STREAMS
queue_t * deb_allocq (const char *deb_file, unsigned int deb_line)
#else
queue_t * allocq (void)
#endif
{
	int s;
	queue_t *qp;
	static queue_t zeroR =
	{NULL, NULL, NULL, NULL, NULL, NULL, 0, QREADR, 0, 0};
	static queue_t zeroW =
	{NULL, NULL, NULL, NULL, NULL, NULL, 0, 0, 0, 0};

	s = splstr ();

	qp = (queue_t *)malloc (2 * sizeof (queue_t));
	if (qp == NULL) {
		splx (s);
		return NULL;
	}
	*qp = zeroR;
	*WR (qp) = zeroW;
	splx (s);
	return (qp);
}


#ifdef CONFIG_DEBUG_STREAMS
void deb_freeq(const char *deb_file, unsigned int deb_line, queue_t *q)
#else
void freeq(queue_t *q)
#endif
{
	free(q);
}

/*
 * Qinit structure and Module_info structures for stream head read and write
 * queues
 */
void strrput (queue_t *, mblk_t *);
void strwsrv (queue_t *);

struct module_info strhm_info =
{0, "strrhead", 0, INFPSZ, STRHIGH, STRLOW};
struct module_info stwhm_info =
{0, "strwhead", 0, INFPSZ, STRHIGH, STRLOW};
struct module_info stwlm_info =
{0, "strwtail", 0, INFPSZ, STRHIGH, STRLOW};
struct module_info strlm_info =
{0, "strrtail", 0, INFPSZ, STRHIGH, STRLOW};
struct qinit strhdata =
{strrput, NULL, NULL, NULL, NULL, &strhm_info, NULL};
struct qinit stwhdata =
{NULL, strwsrv, NULL, NULL, NULL, &stwhm_info, NULL};
struct qinit strldata =
{strrput, NULL, NULL, NULL, NULL, &strlm_info, NULL};
struct qinit stwldata =
{NULL, strwsrv, NULL, NULL, NULL, &stwlm_info, NULL};



/*
 * open a stream device
 */

struct xstream *
stropen (int flag)
{
	struct xstream *xs;

	xs = (struct xstream *)malloc (sizeof (struct xstream));

	if (xs == NULL)
		return NULL;
	bzero ((caddr_t) xs, sizeof (struct xstream));

	xs->rh.q_flag = QREADR | QWANTR;
	xs->wh.q_flag = 0;
	xs->wl.q_flag = QREADR;
	xs->rl.q_flag = QWANTR;

	setq (&xs->rh, &strhdata, &stwhdata);
	setq (&xs->wl, &stwldata, &strldata);
	xs->rh.q_ptr = (caddr_t) xs;
	xs->wh.q_ptr = (caddr_t) xs;
	xs->wl.q_ptr = (caddr_t) xs;
	xs->rl.q_ptr = (caddr_t) xs;

	xs->wh.q_next = &xs->rl;
	xs->wl.q_next = &xs->rh;
	return xs;
}




/*
 * Close a stream.  This is called from closef() on the last close of an open
 * stream. Strclean() will already have removed the siglist and pollist
 * information, so all that remains is to remove all multiplexor links for the
 * stream, pop all the modules (and the driver), and free the stream structure.
 */

void
strclose (struct xstream *xp, int flag)
{
	int s;
	queue_t *qp;

	qp = &xp->wh;
	while (qp->q_next->q_next)
		qdetach (RD (qp->q_next), 1, flag);
	s = splstr ();
	flushq (&xp->wh, FLUSHALL);
	flushq (&xp->rh, FLUSHALL);
	flushq (&xp->rl, FLUSHALL);
	flushq (&xp->wl, FLUSHALL);
	splx (s);

	free (xp);
}



/*
 * Read a stream according to the mode flags in sd_flag:
 * 
 * (default mode)              - Byte stream, msg boundries are ignored RMSGDIS
 * (msg discard)       - Read on msg boundries and throw away any data
 * remaining in msg RMSGNODIS (msg non-discard) - Read on msg boundries and put
 * back any remaining data on head of read queue
 * 
 * Consume readable messages on the front of the queue until u.u_count is
 * satisfied, the readable messages are exhausted, or a message boundary is
 * reached in a message mode.  If no data was read and the stream was not
 * opened with the NDELAY flag, block until data arrives. Otherwise return the
 * data read and update the count.
 * 
 * In default mode a 0 length message signifies end-of-file and terminates a read
 * in progress.  The 0 length message is removed from the queue only if it is
 * the only message read (no data is read).
 * 
 * Attempts to read an M_PROTO or M_PCPROTO message results in an EBADMSG error
 * return.
 */

int
strread (struct xstream *xp, streamchar *data, int *len, int usehq)
{
	mblk_t *bp, *nbp;
	queue_t *q;
	int n;
	int nlen = 0;
	char rflg;

	if (usehq)
		q = &xp->rh;
	else
		q = &xp->rl;

	/* loop terminates when *len == 0 */
	for (;;) {
		if ((bp = getq (q)) == NULL) {
			*len = 0;
			return 0;
		}
		runqueues ();

		switch (bp->b_datap->db_type) {

		CASE_DATA

			if ((bp->b_wptr - bp->b_rptr) == 0) {
				/*
				 * if already read data put zero length message back on queue
				 * else free msg and return 0.
				 */
				if (nlen)
					putbq (q, bp);
				else
					freemsg (bp);
				*len = nlen;
				return 0;
			}
			rflg = 1;
			while (bp && *len) {
				if ((n = min (*len, bp->b_wptr - bp->b_rptr)) > 0) {
					bcopy (bp->b_rptr, data, n);
					bp->b_rptr += n;
					data += n;
					nlen += n;
					*len -= n;
				}
				while (bp && (bp->b_rptr >= bp->b_wptr)) {
					nbp = bp;
					bp = bp->b_cont;
					freeb (nbp);
				}
			}

			if (bp)
				putbq (q, bp);

			if (*len && !usehq)
				printf ("NotEnoughData\n");
			*len = nlen;
			return 0;

		case M_HANGUP:
			*len = 0;
			putbq (q, bp);
			return 0;
		case M_ERROR:
			*len = 0;
			putbq (q, bp);
			return *bp->b_rptr;
		default:
			/*
			 * Garbage on stream head read queue
			 */
			ASSERT (0);
			freemsg (bp);
			break;
		}
	}
}



/*
 * Stream read put procedure.  Called from downstream driver/module with
 * messages for the stream head.  Data, protocol, and in-stream signal messages
 * are placed on the queue, others are handled directly.
 */

void
strrput (queue_t * q, mblk_t * bp)
{
	struct xstream *xq = (struct xstream *) q->q_ptr;

	chkall();
	switch (bp->b_datap->db_type) {

	CASE_DATA
		putq (q, bp);
		return;

	case M_ERROR:
		putq (q, bp);
		return;

	case M_HANGUP:
		putq (q, bp);
		return;

	case M_FLUSH:
		/*
		 * Flush queues.  The indication of which queues to flush is in the
		 * first byte of the message.  If the read queue is specified, then
		 * flush it.
		 */
		if (q == &xq->rh) {
			if (*bp->b_rptr & FLUSHR)
				flushq (q, FLUSHALL);
			if (*bp->b_rptr & FLUSHW) {
				*bp->b_rptr &= ~FLUSHR;
				qreply (q, bp);
				return;
			}
		} else {
			if (*bp->b_rptr & FLUSHW)
				flushq (q, FLUSHALL);
			if (*bp->b_rptr & FLUSHR) {
				*bp->b_rptr &= ~FLUSHW;
				qreply (q, bp);
				return;
			}
		}
		freemsg (bp);
		return;

	default:
		ASSERT (0);
		freemsg (bp);
		return;
	}
	return;
}


int
strwrite (struct xstream *xp, streamchar * data, int *len, int usehq)
{
	mblk_t *mp;
	queue_t *q;

	chkall();
	if (usehq)
		q = &xp->wh;
	else
		q = &xp->wl;

	mp = allocb (*len, BPRI_LO);
	if (mp == NULL)
		return ENOMEM;
	bcopy (data, mp->b_wptr, *len);
	mp->b_wptr += *len;
	*len = 0;

	(*q->q_next->q_qinfo->qi_putp) (q->q_next, mp);
	runqueues ();
	return 0;
}


int
strwritev (struct xstream *xp, struct iovec *iov, int iovlen, int usehq)
{
	mblk_t *mp, *bp = NULL;
	queue_t *q;

	chkall();
	if (usehq)
		q = &xp->wh;
	else
		q = &xp->wl;

	while (iovlen--) {
		mp = allocb (iov->iov_len, BPRI_LO);
		if (mp == NULL) {
			if (bp != NULL)
				freemsg (bp);
			return ENOMEM;
		}
		bcopy (iov->iov_base, mp->b_wptr, iov->iov_len);
		mp->b_wptr += iov->iov_len;
		if (bp == NULL)
			bp = mp;
		else
			linkb (bp, mp);
		iov++;
	}
	(*q->q_next->q_qinfo->qi_putp) (q->q_next, bp);
	runqueues ();
	return 0;
}



/*
 * Stream head write service routine. Its job is to wake up any sleeping
 * writers when a queue downstream needs data (part of the flow control in putq
 * and getq). It also must wake anyone sleeping on a poll(). For stream head
 * right below mux module, it must also invoke put procedure of next downstream
 * module
 */

void
strwsrv (queue_t * q)
{
	return;
}


/*
 * ioctl for streams
 */

int
strioctl (struct xstream *xp, long cmd, long arg)
{
	queue_t *q;

	if (1 /* usehq */ )
		q = &xp->wh;
	else
		q = &xp->wl;
	switch (cmd) {

	case I_NREAD:
		/*
		 * return number of bytes of data in first message in queue in "arg"
		 * and return the number of messages in queue in return value
		 */
		{
			int size = 0;
			mblk_t *bp;

			if ((bp = RD (q)->q_first) != NULL)
				size = msgdsize (bp);
			(int *) arg = size;
			return qsize (RD (q));
		}

	case I_FIND:
		/*
		 * get module name
		 */
		{
			char mname[FMNAMESZ + 1];
			int i;

			strncpy (mname, (char *) arg, FMNAMESZ + 1);
			/*
			 * find module in fmodsw
			 */
			if ((i = findmod (mname)) < 0) {
				return EINVAL;
			}
			/* look downstream to see if module is there */
			for (q = q->q_next;
					q && q->q_next && (fmod_sw[i].f_str->st_wrinit != q->q_qinfo);
					q = q->q_next) ;

			return ((q && q->q_next) ? 1 : 0);
		}

	case I_PUSH:
		/*
		 * Push a module
		 */

		{
			char mname[FMNAMESZ + 1];
			int i;

			/*
			 * get module name and look up in fmodsw
			 */
			strncpy (mname, (char *) arg, FMNAMESZ + 1);
			if ((i = findmod (mname)) < 0) {
				return EINVAL;
			}
			/*
			 * push new module and call its open routine via qattach
			 */
			if (!qattach (fmod_sw[i].f_str, RD (q), 0,0))
				return ENXIO;

			/*
			 * If flow control is on, don't break it - enable first back queue
			 * with svc procedure
			 */
			if (RD (q)->q_flag & QWANTW) {
				for (q = backq (RD (q->q_next));
						q && !q->q_qinfo->qi_srvp; q = backq (q)) ;
				if (q) {
#ifdef CONFIG_DEBUG_STREAMS
					const char deb_file[] = __FILE__;
					int deb_line = __LINE__+2;
#endif
					qenable (q);
				}
			}
			return 0;
		}


	case I_POP:
		/*
		 * Pop module ( if module exists )
		 */
		if (q->q_next->q_next &&
				!(q->q_next->q_next->q_flag & QREADR)) {
			qdetach (RD (q->q_next), 1, 0);
			return 0;
		}
		return EINVAL;



	case I_LOOK:
		/*
		 * Get name of first module downstream If no module (return error)
		 */
		{
			int i;

			for (i = 0; i < fmodcnt; i++)
				if (fmod_sw[i].f_str->st_wrinit == q->q_next->q_qinfo) {
					strncpy ((char *) arg, fmod_sw[i].f_name, FMNAMESZ + 1);
					return 0;
				}
			return ENOENT;
		}



	case I_FLUSH:
		/*
		 * send a flush message downstream flush message can indicate FLUSHR -
		 * flush read queue FLUSHW - flush write queue FLUSHRW - flush
		 * read/write queue
		 */
		if (arg & ~FLUSHRW) {
			return EINVAL;
		}
		if (!putctl1 (q->q_next, M_FLUSH, arg))
			return ENOMEM;

		runqueues ();
		return 0;
	}
	return 0;
}

