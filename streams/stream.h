#if !defined(_LINUX_STREAMS_H)
#define _LINUX_STREAMS_H

#include <linux/config.h>
#include <linux/types.h>
#include "compat.h"
#include "stropts.h"
#ifdef __KERNEL__
#include <linux/fs.h>
#include <linux/major.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>

#define MAX_STRDEV MAX_CHRDEV /* don't change */
#define MAX_STRMOD MAX_CHRDEV /* change if necessary */
#else
#define MAX_STRMOD 32
#endif

#undef STREAMS_TTY	/* Not yet */

#ifdef linux
#define ERR_DECL
#define ERR_RETURN(x) return (x)
#else
#define OPENFAIL (-1)	/* compatibility */
#endif

#ifdef CONFIG_DEBUG_STREAMS
#define DEB_PMAGIC 0x13572469
#define DEB_DMAGIC 0x14582368
#endif

#define STREAM_MAGIC 0x53AA

#define DATA_BLOCK(m) ((m)->b_datap)
#define DATA_START(m) ((m)->b_datap->db_base)
#define DATA_END(m) ((m)->b_datap->db_lim)
#define DATA_REFS(m) ((m)->b_datap->db_ref)
#define DATA_TYPE(m) ((m)->b_datap->db_type)

/*
 * Grumble. Some Streams subsystems use signed characters, some unsigned.
 */
typedef unsigned char streamchar;

/*
 * The following data structures are culled from various header files.
 */

/*
 * Q_Flag bits
 */
#define	QENAB	01			/* Queue is scheduled */
#define	QWANTR	02			/* Someone wants to read */
#define	QWANTW	04			/* Someone wants to write */
#define	QFULL	010			/* Queue is full */
#define	QREADR	020			/* This is the reader Queue */
#define	QNOENB	040			/* Don't enable queue in putq */
#define	QRETRY	0100		/* Timer to re-qenable() a queue if we're 
							   temporarily out of resources */


/*
 * module information
 */
struct module_info {
	short	mi_idnum;		/* module id number */
	char 	*mi_idname;		/* module name */
	char	dummy1;			/* Compatibility with old drivers */
	char	dummy2;
	short	mi_hiwat;		/* hi-water mark */
	short 	mi_lowat;		/* lo-water mark */
};


/*
 * flag bits
 */
#define	SF_HANGUP         01	/* Hangup */
#define SF_MSGDISCARD	  02	/* Read messages: discard rest */
#define SF_MSGKEEP        04	/* Read messages: keep rest */
#define SF_WAITOPEN	     010	/* Wait for open to complete */
#define SF_WAITCLOSE     020	/* Wait for open to complete */
#define SF_PRIORITY	     040	/* Reading a priority message */
#define SF_CTTY	        0100	/* Create a controlling TTY on open? */
#define SF_IOCTL	    0200	/* Waiting for ioctl to finish */
#define SF_PARTIALREAD	0400	/* partially read message in front */

/*
 *  Data block descriptor
 */
typedef struct datab {
	streamchar	*db_base;
	streamchar	*db_lim;
	uchar_t		db_ref;
	uchar_t		db_type;
#ifdef CONFIG_DEBUG_STREAMS
	long		deb_magic;
#endif
} dblk_t;


/*
 * Message block descriptor
 */
typedef struct	msgb {
	struct	msgb	*b_next;
	struct	msgb	*b_prev;
	struct	msgb	*b_cont;
	struct	datab 	*b_datap;
#ifdef CONFIG_DEBUG_STREAMS
	long		deb_magic;
	struct queue	*deb_queue;
	const char	*deb_file;
	unsigned int	deb_line;
#endif
	streamchar	*b_rptr;
	streamchar	*b_wptr;
} mblk_t;


/*
 * stream information
 */

struct streamtab {
	struct qinit	*st_rdinit;
	struct qinit	*st_wrinit;
	void		 *dummy1;
	void		 *dummy2;
};


/*
 * stream header -- interface to the system. This is Linux specific.
 */

struct stream_header {
	int magic;
	struct stream_header *prev;
	struct stream_header *next;
	struct	queue *write_queue;	/* write queue */
	struct	msgb *iocblk;		/* data block for ioctl */
	struct	inode *inode;		/* One of the inodes this was opened with */
#if 0
	struct  streamtab *strtab;	/* driver's data -- needed for muxing */
#endif
	struct  wait_queue *waiting;	/* Waiting for open/close to complete */
	struct  wait_queue *reading;	/* Waiting for data in RD queue */
	struct  wait_queue *writing;	/* Waiting for room in WR queue */
	struct  wait_queue *ioctling;	/* Waiting for IOCTL to finish */
	struct  tty_struct *tty;		/* terminal data */
	struct fasync_struct *fasync;   /* async calls */
	mblk_t *write_buf;
	long	ioctl_id;			/* ioctl id */
	int     error;			/* hangup or error to set u.u_error */
	ushort_t push_count;		/* # modules */
	ushort_t count;			/* # times this has been opened */
	ushort_t flag;			/* state/flags */
	ushort_t write_offset;		/* write offset for downstream data */
	ushort_t ref;			/* reference count */
	uchar_t  wait;			/* time to timeout */
};

/*
 * data queue
 */
typedef struct	queue {
	struct	qinit	*q_qinfo;	/* queue limits and procedures */
	struct	msgb	*q_first;	/* first data block */
	struct	msgb	*q_last;	/* last data block */
	struct	queue	*q_next;	/* next stream to send data to */
	struct	queue	*q_link;	/* next scheduled stream */
	caddr_t		q_ptr;		/* private data structure for driver/module */
#ifdef NEW_TIMEOUT
	long		q_retry;	/* redo a service proc when temp-out-of-space */
#endif
	short		q_count;	/* number of queued bytes */
	ushort_t	q_flag;		/* queue state */
	short		q_hiwat;	/* queue high water mark */
	short		q_lowat;	/* queue low water mark */
} queue_t;

/*
 * queue information
 */
typedef int  (qf_open)(queue_t *,dev_t,int,int);
typedef void (qf_close)(queue_t *,int);
typedef void (qf_put)(queue_t *,mblk_t *);
typedef void (qf_srv)(queue_t *);

struct qinit {
	qf_put	*qi_putp;
	qf_srv	*qi_srvp;
	qf_open	*qi_qopen;
	qf_close *qi_qclose;
	void *dummy1; /* (*qadmin)() */
	struct module_info *qi_minfo;		/* module information */
	void *dummy2;
};

/****************
 * Message types.
 */

/*
 * Regular priority, data
 */
#define	M_DATA		00		/* regular data */
#define M_PROTO		01		/* protocol control */
#define M_SPROTO	02		/* strippable protocol control */

/*
 * , control
 */
#define	M_BREAK		010		/* line break */
#define	M_SIG		013		/* generate signal */
#define	M_DELAY		014		/* real-time xmit delay (1 param) */
#define M_CTL		015		/* device-specific control message */
#define	M_IOCTL		016		/* ioctl; set/get params */
#define M_SETOPTS	020		/* set various stream head options */
#define M_ADMIN		030		/* administration (out of stream) */

/*
 * Expedited messages
 */
#define M_EXPROTO	0100		/* expedited protocol control */
#define M_EXDATA	0101		/* expedited data */
#define M_EXSPROTO	0102		/* strippable expedited protocol control */
#define	M_EXSIG		0103		/* generate process signal */

/*
 * Priority messages: no flow control
 */
#define M_PCPROTO	0200		/* priority protocol control */
#define	M_IOCACK	0201		/* acknowledge ioctl */
#define	M_IOCNAK	0202		/* negative ioctl acknowledge */
#define	M_PCSIG		0203		/* generate process signal */
#define	M_FLUSH		0206		/* flush your queues */
#define	M_STOP		0207		/* stop transmission immediately */
#define	M_START		0210		/* restart transmission after stop */
#define	M_HANGUP	0211		/* line disconnect */
#define M_ERROR		0212		/* fatal error used to set u.u_error */


/*
 * Misc message type defines
 */
#define QNORM    0			/* normal messages */
#define QEXP  0100			/* expedited messages */
#define QPCTL 0200			/* priority cntrl messages */


/*
 *  IOCTL structure - this structure is the format of the M_IOCTL message type.
 */
struct iocblk {
	int 	ioc_cmd;		/* ioctl command type */
	ushort_t ioc_uid;		/* effective uid of user */
	ushort_t ioc_gid;		/* effective gid of user */
	int	ioc_id;			/* ioctl id */
	int	ioc_count;		/* count of bytes in data field */
	int	ioc_error;		/* error code */
	int	ioc_rval;		/* return value  */
};


/*
 * Options structure for M_SETOPTS message.  This is sent upstream
 * by driver to set stream head options.
 */
struct stroptions {
	short so_flags;		/* options to set */
	short so_readopt;	/* read option */
	short so_wroff;		/* write offset */
	short so_hiwat;		/* read queue high water mark */
	short so_lowat;		/* read queue low water mark */
};

/* flags for stream options set message */

#define SO_ALL		077	/* set all options */
#define SO_READOPT	 01	/* set read opttion */
#define SO_WROFF	 02	/* set write offset */
#define SO_HIWAT	020	/* set high water mark */
#define SO_LOWAT	040	/* set low water mark */



/********************************************************************************/
/*		Miscellaneous parameters and flags				*/
/********************************************************************************/

/*
 * Default timeout in seconds for ioctls and close
 */
#define STRTIMOUT 15

/*
 * Stream head default high/low water marks 
 */
#define STRHIGH 512
#define STRLOW	256

/* Compatibility... */
#define INFPSZ	-1

/*
 * Flags for flushq()
 */
#define FLUSHALL        1       /* flush all messages */
#define FLUSHDATA       0       /* don't flush control messages */

/*
 * maximum ioctl data block size
 */
#define MAXIOCBSZ	1024


#if 0
/*
 * Copy modes for tty and I_STR ioctls
 */
#define	U_TO_K 	01			/* User to Kernel */
#define	K_TO_K  02			/* Kernel to Kernel */
#endif


/*
 * Values for dev in open to indicate module open, clone open;
 * return value for failure.
 */
#define DEVOPEN 	0		/* open as a device */
#define MODOPEN 	1		/* open as a module */
#define CLONEOPEN	2		/* open for clone, pick own minor device */
#if 0
#define FWDPUSH		3		/* forwarder push */
#define FWDPOP		4		/* forwarder pop */
#define FWDMNAME	5		/* forwarder I_MNAME */
#define FWDFIND		6		/* forwarder I_FIND */
#define FWDLOOK		7		/* forwarder I_LOOK */


/*
 *	Forwarder module ID range (do not use for non-forwarder modules)
 */

#define FORWARDERMIN	5000
#define FORWARDERMAX	5299
#endif

/*
 * Block allocation priorities. Currently ignored.
 */ 
#define BPRI_LO		1
#define BPRI_MED	2
#define BPRI_HI		3


/*
 * Turn off interrupt and Streams processing
 */
#define splstr() spl(1)

/*
 * noenable - prevent queue from running.
 * enableok - Let queue be runnable again.
 * canenable - check if queue is runnable. DO NOT call qenable() unless canenable is true.
 */
#define noenable(p_queue)	(p_queue)->q_flag |= QNOENB
#define enableok(p_queue)	(p_queue)->q_flag &= ~QNOENB
#define canenable(p_queue)	!((p_queue)->q_flag & QNOENB)

/*
 * Getting related queues. RDQ and WRQ are safe versions of RD and WR.
 */
#define	OTHERQ(p_queue)	((p_queue)->q_flag&QREADR? (p_queue)+1: (p_queue)-1)
#define	RDQ(p_queue)	((p_queue)->q_flag&QREADR? (p_queue): (p_queue)-1)
#define	WRQ(p_queue)	((p_queue)->q_flag&QREADR? (p_queue)+1: (p_queue))
#define	WR(p_queue)	((p_queue)+1)
#define	RD(p_queue)	((p_queue)-1)

/*
 * Message priority.
 */
#define queclass(p_msg) (DATA_TYPE(p_msg) & (QPCTL | QEXP))


/*
 * Free a queue (pair). WARNING: Call flushq() on both queues before doing this!
 */

/*
 * The usual, except this is ANSI and we might be debugging.
 */
#ifdef CONFIG_DEBUG_STREAMS

#ifdef linux
#define STREAM_INVAL 0x0400	/* Linux */
#else
#define STREAM_INVAL 0x12345687
#endif
/* add invalid addresses appropriate for other kernels */

extern mblk_t *deb_rmvb(const char *, unsigned int, mblk_t *, mblk_t *);
#define rmvb(a,b) deb_rmvb(__FILE__,__LINE__,a,b)
extern mblk_t *deb_allocb(const char *, unsigned int, ushort,ushort);
#define allocb(a,b) deb_allocb(__FILE__,__LINE__,a,b)
extern int deb_testb(const char *, unsigned int, ushort,ushort);
#define testb(a,b) deb_testb(__FILE__,__LINE__,a,b)
extern void deb_freeb(const char *, unsigned int, mblk_t *);
#define freeb(a) deb_freeb(__FILE__,__LINE__,a)
extern void deb_freemsg(const char *, unsigned int, mblk_t *);
#define freemsg(a) deb_freemsg(__FILE__,__LINE__,a)
extern mblk_t *deb_dupb(const char *, unsigned int, mblk_t *);
#define dupb(a) deb_dupb(__FILE__,__LINE__,a)
extern mblk_t *deb_dupmsg(const char *, unsigned int, mblk_t *);
#define dupmsg(a) deb_dupmsg(__FILE__,__LINE__,a)
extern mblk_t *deb_copyb(const char *, unsigned int, mblk_t *);
#define copyb(a) deb_copyb(__FILE__,__LINE__,a)
extern mblk_t *deb_copymsg(const char *, unsigned int, mblk_t *);
#define copymsg(a) deb_copymsg(__FILE__,__LINE__,a)
extern mblk_t *deb_copybufb(const char *, unsigned int, mblk_t *);
#define copybufb(a) deb_copybufb(__FILE__,__LINE__,a)
extern mblk_t *deb_copybufmsg(const char *, unsigned int, mblk_t *);
#define copybufmsg(a) deb_copybufmsg(__FILE__,__LINE__,a)
extern int deb_pullupmsg(const char *, unsigned int, mblk_t *, short);
#define pullupmsg(a,b) deb_pullupmsg(__FILE__,__LINE__,a,b)
extern int deb_adjmsg(const char *, unsigned int, mblk_t *, short);
#define adjmsg(a,b) deb_adjmsg(__FILE__,__LINE__,a,b)
extern int deb_msgdsize(const char *, unsigned int, mblk_t *);
#define msgdsize(a) deb_msgdsize(__FILE__,__LINE__,a)
extern int deb_msgsize(const char *, unsigned int, mblk_t *);
#define msgsize(a) deb_msgsize(__FILE__,__LINE__,a)
extern int deb_xmsgsize(const char *, unsigned int, mblk_t *);
#define xmsgsize(a) deb_xmsgsize(__FILE__,__LINE__,a)
extern mblk_t *deb_getq(const char *, unsigned int, queue_t *);
#define getq(a) deb_getq(__FILE__,__LINE__,a)
extern void deb_rmvq(const char *, unsigned int, queue_t *, mblk_t *);
#define rmvq(a,b) deb_rmvq(__FILE__,__LINE__,a,b)
extern void deb_flushq(const char *, unsigned int, queue_t *, int);
#define flushq(a,b) deb_flushq(__FILE__,__LINE__,a,b)
extern void deb_putq(const char *, unsigned int, queue_t *, mblk_t *);
static inline void putq(queue_t *q, mblk_t *p) { deb_putq("PUTQ",0,q,p); }
#define putq(a,b) deb_putq(__FILE__,__LINE__,a,b)
extern void deb_putbq(const char *, unsigned int, queue_t *, mblk_t *);
#define putbq(a,b) deb_putbq(__FILE__,__LINE__,a,b)
extern void deb_insq(const char *, unsigned int, queue_t *, mblk_t *, mblk_t *);
#define insq(a,b,c) deb_insq(__FILE__,__LINE__,a,b,c)
extern void deb_appq(const char *, unsigned int, queue_t *, mblk_t *, mblk_t *);
#define appq(a,b,c) deb_appq(__FILE__,__LINE__,a,b,c)
extern int deb_putctl(const char *, unsigned int, queue_t *, uchar_t);
#define putctl(a,b) deb_putctl(__FILE__,__LINE__,a,b)
extern int deb_putctlerr(const char *, unsigned int, queue_t *, int);
#define putctlerr(a,b) deb_putctlerr(__FILE__,__LINE__,a,b)
extern int deb_putctl1(const char *, unsigned int, queue_t *, uchar_t, streamchar);
#define putctl1(a,b,c) deb_putctl1(__FILE__,__LINE__,a,b,c)
extern int deb_canput(const char *, unsigned int, queue_t *);
#define canput(a) deb_canput(__FILE__,__LINE__,a)
extern queue_t *deb_backq(const char *, unsigned int, queue_t *);
#define backq(a) deb_backq(__FILE__,__LINE__,a)
extern queue_t *deb_allocq(const char *, unsigned int);
#define allocq() deb_allocq(__FILE__,__LINE__)
extern void deb_freeq(const char *, unsigned int, queue_t *q);
#define freeq(a) deb_freeq(__FILE__,__LINE__,a)
extern void deb_qreply(const char *, unsigned int, queue_t *p_queue, mblk_t *p_msg);
#define qreply(a,b) deb_qreply(__FILE__,__LINE__,a,b)
extern void deb_qenable(const char *, unsigned int, queue_t *p_queue);
#define qenable(a) deb_qenable(__FILE__,__LINE__,a)
extern void deb_runqueues(const char *, unsigned int);
#define runqueues() deb_runqueues(__FILE__,__LINE__)
extern int deb_qattach (const char *, unsigned int, struct streamtab *qinfo, queue_t * qp, dev_t dev, int flag);
#define qattach(a,b,c,d) deb_qattach(__FILE__,__LINE__,a,b,c,d)
extern void deb_qdetach (const char *, unsigned int, queue_t * qp, int clmode, int flag);
#define qdetach(a,b,c) deb_qdetach(__FILE__,__LINE__,a,b,c)

extern int deb_findmod(const char *, unsigned int, const char *);
#define findmod(a) deb_findmod(__FILE__,__LINE__,a)
extern int deb_qsize(const char *, unsigned int, queue_t *);
#define qsize(a) deb_qsize(__FILE__,__LINE__,a)
extern void deb_setq (const char *, unsigned int, queue_t *, struct qinit *, struct qinit *);
#define setq(a,b,c) deb_setq(__FILE__,__LINE__,a,b,c)
extern void deb_get_post (const char *, unsigned int, queue_t *);
#define get_post(a) deb_get_post(__FILE__,__LINE__,a)
#else /* !DEBUG */
extern mblk_t *rmvb(mblk_t *, mblk_t *);	/* Remove a message block (2nd arg) from the message. */
extern mblk_t *allocb(ushort,ushort);		/* Allocate a message */
extern int testb(ushort,ushort);		/* Check if a message can be allocated */
extern void freeb(mblk_t *);			/* Free a message block */
extern void freemsg(mblk_t *);			/* Free a message list */
extern mblk_t *dupb(mblk_t *);			/* Duplicate a message block */
extern mblk_t *dupmsg(mblk_t *);		/* Duplicate a message */
extern mblk_t *copyb(mblk_t *);			/* Copy a message block */
extern mblk_t *copymsg(mblk_t *);		/* Copy a message */
extern mblk_t *copybufb(mblk_t *);			/* Copy a message block */
extern mblk_t *copybufmsg(mblk_t *);		/* Copy a message */
extern int pullupmsg(mblk_t *, short);		/* Pull up the first N bytes */
extern int adjmsg(mblk_t *, short);		/* Trim first/last(N<0) bytes */
extern int msgdsize(mblk_t *);			/* # data bytes */
extern int xmsgsize(mblk_t *);			/* # bytes of first msg block type */
extern int msgsize(mblk_t *);			/* # bytes of the whole message */
extern mblk_t *getq(queue_t *);			/* Remove first msg from queue */
extern void rmvq(queue_t *, mblk_t *);		/* Remove this msg from queue */
extern void flushq(queue_t *, int);		/* Flush messages */
extern void putq(queue_t *, mblk_t *);		/* Append message to queue */
extern void putbq(queue_t *, mblk_t *);		/* Put message back onto queue */
extern void insq(queue_t *, mblk_t *, mblk_t *);/* Insert message(3) before message(2) or at end */
extern void appq(queue_t *, mblk_t *, mblk_t *);/* Insert message(3) after message(2) or at start */
extern int putctl(queue_t *, uchar_t);		/* Zero-byte ctl msg */
extern int putctlerr(queue_t *, int);		/* Error msg */
extern int putctl1(queue_t *, uchar_t, streamchar);/* One-byte ctl msg */
extern int canput(queue_t *);			/* Room in queue? */

extern queue_t *backq(queue_t *);		/* Get the queue pointing to me */
extern queue_t *allocq(void);			/* Allocate queues */
extern void freeq(queue_t *q);			/* Free a queue pair */
extern void qreply(queue_t *p_queue, mblk_t *p_msg);/* Send a message back */
extern void qenable(queue_t *p_queue);		/* Schedule a service procedure */
extern void runqueues(void);			/* Schedule queues */
extern int qattach (struct streamtab *qinfo, queue_t * qp, dev_t dev, int flag);
extern void qdetach (queue_t * qp, int clmode, int flag);

extern int findmod(const char *);		/* Find module name in fmodsw[] */
extern int qsize(queue_t *);			/* Count messages on a queue */
extern void setq (queue_t *, struct qinit *, struct qinit *);/* set queue procs and params */
extern void get_post (queue_t *);		/* postprocess after dequeuing a packet */
#endif
extern void qretry(queue_t *);			/* reruns a queue */
extern void do_qretry(queue_t *);		/* timeout proc for qretry */
extern int getclass(ushort);			/* Message class of given size. Unimplemented; returns 1. */

extern int register_strdev (unsigned int major, struct streamtab *strtab, int nminor);
extern int unregister_strdev (unsigned int major, struct streamtab *strtab, int nminor);
extern int register_strmod (struct streamtab *strtab);
extern int unregister_strmod (struct streamtab *strtab);

#ifndef FMNAMESZ
#define FMNAMESZ        16
#endif

struct fmodsw {
        char f_name[FMNAMESZ+1];
        struct streamtab *f_str;
};

extern struct fmodsw fmod_sw[MAX_STRMOD];
extern int fmodcnt;

#ifdef __KERNEL__
extern struct file_operations streams_fops;
extern struct streamtab *fstr_sw[MAX_STRDEV];
#endif

extern int strmsgsz;			/* maximum stream message size */
extern int nstrpush;			/* maximum # of pushed modules */
extern volatile queue_t *sched_first, *sched_last;

#ifdef CONFIG_DEBUG_STREAMS
extern void putnext(queue_t *p_queue, mblk_t *p_msg);
extern void linkb(mblk_t *p_msg1, mblk_t *p_msg2);
extern mblk_t *unlinkb(mblk_t *p_msg);
#else
static inline void putnext(queue_t *p_queue, mblk_t *p_msg)
{
	(*p_queue->q_next->q_qinfo->qi_putp)(p_queue->q_next, p_msg);
}

static inline void linkb(mblk_t *p_msg1, mblk_t *p_msg2)
{
    if(p_msg1 == NULL || p_msg2 == NULL)
        return;

    while(p_msg1->b_cont != NULL)
        p_msg1 = p_msg1->b_cont;

    p_msg1->b_cont = p_msg2;
}

static inline mblk_t *unlinkb(mblk_t *p_msg)
{
    mblk_t *p_nextmsg;

    if(p_msg == NULL)
        return NULL;

    p_nextmsg = p_msg->b_cont;
    p_msg->b_cont = NULL;
    return p_nextmsg;
}
#endif /* DEBUG */

#endif /* __sys_stream_h */
