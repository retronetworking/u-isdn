#ifndef __STREAMS_H
#define __STREAMS_H

#include "kernel.h"

#if !(defined(__sys_stream_h) || defined(_SYS_STREAM_H) || defined(_LINUX_STREAM_H))

#ifdef KERNEL
#include "stream.h"
#else
#ifdef linux
#include "stream.h"
#else
#include <sys/aux-stream.h>
#endif							/* KERNEL */
#endif

#endif							/* SYS_xxx */

#ifndef KERNEL


/*
 * Stream control. Use setq to change the behavior of the stream endpoints.
 */
struct xstream {
	queue_t rh, wh, wl, rl;		  /* Order matters. Note that the lower queue
								   * names are  inverted. */
};

#endif							/* KERNEL */


/* Standard STREAMS prototypes et al. */

#if (!defined(KERNEL) || !defined(SVR4)) && !defined(linux) /* System prototypes */
mblk_t *allocb (register int size, int pri);
int testb (register int size, int pri);
void freeb (register mblk_t * bp);
void freemsg (register mblk_t * bp);
mblk_t *dupb (register mblk_t * bp);
mblk_t *dupmsg (register mblk_t * bp);
mblk_t *copyb (register mblk_t * bp);
mblk_t *copymsg (register mblk_t * bp);
mblk_t *copybufb (register mblk_t * bp);
mblk_t *copybufmsg (register mblk_t * bp);
void linkb (register mblk_t * mp, register mblk_t * bp);
mblk_t *unlinkb (register mblk_t * bp);
mblk_t *rmvb (register mblk_t * mp, register mblk_t * bp);
int pullupmsg (mblk_t * mp, register int len);
int adjmsg (mblk_t * mp, register int len);
int xmsgsize (register mblk_t * bp);
int msgdsize (register mblk_t * bp);
mblk_t *getq (register queue_t * q);
void rmvq (register queue_t * q, register mblk_t * mp);
void flushq (register queue_t * q, int flag);
int canput (queue_t * q);
/* void */ int putq (register queue_t * q, register mblk_t * bp);
void putbq (register queue_t * q, register mblk_t * bp);
void insq (register queue_t * q, register mblk_t * emp, register mblk_t * mp);
int putctl (queue_t * q, uchar_t type);
int putctl1 (queue_t * q, uchar_t type, char info);
queue_t *allocq (void);
queue_t *backq (register queue_t * q);
void qreply (register queue_t * q, mblk_t * bp);
int qenable (register queue_t * q);
/* Nobody knows why qenable() returns an int. */
void queuerun (void);
void runqueues (void);
int qsize (register queue_t * qp);
int qattach (register struct streamtab *qinfo, register queue_t * qp, int flag);
void qdetach (register queue_t * qp, int clmode, int flag);
void setq (queue_t * rq, struct qinit *rinit, struct qinit *winit);
int findmod (register char *name);

#endif


#ifndef KERNEL
/*
 * New or modified routines
 */
#ifdef CONFIG_DEBUG_STREAMS
mblk_t *deb_allocsb (const char *deb_file, unsigned int deb_line, ushort_t size, streamchar *data);
#define allocsb(a,b) deb_allocsb(__FILE__,__LINE__,a,b)
#else
mblk_t *allocsb (ushort_t size, streamchar *data);
#endif
void modregister (struct streamtab *info);
struct xstream *stropen (int flag);
void strclose (struct xstream *xp, int flag);
int strread (struct xstream *xp, streamchar *data, int len, int usehq);
#ifdef CONFIG_DEBUG_STREAMS
#define strwrite(a,b,c,d) deb_strwrite(__FILE__,__LINE__,(a),(b),(c),(d))
int deb_strwrite (const char *deb_file,unsigned int deb_line, struct xstream *xp, streamchar *data, int len, int usehq);
#define strwritev(a,b,c,d) deb_strwritev(__FILE__,__LINE__,(a),(b),(c),(d))
int deb_strwritev (const char *deb_file,unsigned int deb_line, struct xstream *xp, struct iovec *iov, int iovlen, int usehq);
#else
int strwrite (struct xstream *xp, streamchar *data, int len, int usehq);
int strwritev (struct xstream *xp, struct iovec *iov, int iovlen, int usehq);
#endif
int strioctl (struct xstream *xp, long cmd, long arg);

#endif							/* KERNEL */

#endif							/* __STREAMS_H */
