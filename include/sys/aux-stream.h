#if !defined(_LINUX_STREAMS_H)
#define _LINUX_STREAMS_H

#include <linux/types.h>
#include <linux/syscompat.h>
#include <linux/fs.h>
#include "stropts.h"
#include <linux/kernel.h>

#define MAX_STRDEV MAX_CHRDEV /* don't change */
#define MAX_STRMOD MAX_CHRDEV /* change if necessary */

#undef STREAMS_TTY	/* Not yet */
#undef PACKETSIZES 	/* Not implemented because stupid */
#define STREAMS_DEBUG

#define OPENFAIL (-1)	/* compatibility */

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


/*
 * module information
 */
struct module_info {
	short	mi_idnum;		/* module id number */
	char 	*mi_idname;		/* module name */
#ifdef PACKETSIZES
	short   mi_minpsz;		/* min packet size accepted -- unused */
	short   mi_maxpsz;		/* max packet size accepted -- unused */
#else
	char	dummy1;
	char	dummy2;
#endif
	short	mi_hiwat;		/* hi-water mark */
	short 	mi_lowat;		/* lo-water mark */
};



/*
 * stream header -- interface to the system. This is Linux-specific.
 */

struct stream_header {
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
#ifdef STREAMS_TTY
	struct tty_struct *tty;		/* terminal data */
	struct termios termio;		/* tty_struct only has a pointer */
#endif
	long	iocid;			/* ioctl id */
	int     error;			/* hangup or error to set u.u_error */
	ushort	push_count;		/* # modules */
	ushort	flag;			/* state/flags */
	ushort	write_offset;		/* write offset for downstream data */
	ushort  ref;			/* reference count */
	unchar  wait;			/* time to timeout */
};

/*
 * flag bits
 */
#define	STRHUP	       01		/* Hangup */
#define RMSGDIS	       02		/* Read messages: discard rest */
#define RMSGNODIS      04		/* Read messages: keep rest */
#define STWOPEN	      010		/* Wait for open to complete */
#define STWCLOSE      020		/* Wait for open to complete */
#define STRPRI	      040		/* Reading a priority message */
#define CTTYFLG	     0100		/* Create a controlling TTY on open? */
#define IOCWAIT	     0200		/* Waiting for ioctl to finish */

/*
 *  Data block descriptor
 */
typedef struct datab {
	streamchar	*db_base;
	streamchar	*db_lim;
	unsigned char	db_ref;
	unsigned char	db_type;
} dblk_t;


/*
 * Message block descriptor
 */
typedef struct	msgb {
	struct	msgb	*b_next;
	struct	msgb	*b_prev;
	struct	msgb	*b_cont;
	streamchar		*b_rptr;
	streamchar		*b_wptr;
	struct datab 	*b_datap;
} mblk_t;

/*
 * stream information
 */

struct streamtab {
	struct qinit *st_rdinit;
	struct qinit *st_wrinit;
	void *dummy1;
	void *dummy2;
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
	caddr_t	q_ptr;			/* private data structure for driver/module */
	short	q_count;		/* number of queued bytes */
	unsigned short	q_flag;		/* queue state */
	short q_hiwat;			/* queue high water mark */
	short q_lowat;			/* queue low water mark */
} queue_t;

/*
 * queue information
 */
struct qinit {
	void	(*qi_putp)(queue_t *, mblk_t *); /* put procedure */
	void	(*qi_srvp)(queue_t *);		/* service procedure */
	int	(*qi_qopen)(queue_t *,dev_t,int,int);	/* called on every open */
	void	(*qi_qclose)(queue_t *,int);	/* called on last close */
	void *dummy1; /* (*qadmin)() */
	struct module_info *qi_minfo;		/* module information */
	void *dummy2;
};

/********************************************************************************/
/* 			Streams message types					*/
/********************************************************************************/


/*
 * Data and protocol messages (regular priority)
 */
#define	M_DATA		00		/* regular data */
#define M_PROTO		01		/* protocol control */
#define M_SPROTO	02		/* strippable protocol control */

/*
 * Control messages (regular priority)
 */
#define	M_BREAK		010		/* line break */
#define	M_SIG		013		/* generate process signal */
#define	M_DELAY		014		/* real-time xmit delay (1 param) */
#define M_CTL		015		/* device-specific control message */
#define	M_IOCTL		016		/* ioctl; set/get params */
#define M_SETOPTS	020		/* set various stream head options */
#define M_ADMIN		030		/* administration (out of stream) */

/*
 * Expedited messages (placed between regular priority and high
 * priority messages on a queue)
 */
#define M_EXPROTO	0100		/* expedited protocol control */
#define M_EXDATA	0101		/* expedited data */
#define M_EXSPROTO	0102		/* strippable expedited protocol control */
#define	M_EXSIG		0103		/* generate process signal */

/*
 * Control messages (high priority; go to head of queue)
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
	ushort	ioc_uid;		/* effective uid of user */
	ushort	ioc_gid;		/* effective gid of user */
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
#ifdef PACKETSIZES
#define SO_MINPSZ	 04	/* set min packet size */
#define SO_MAXPSZ	010	/* set max packet size */
#endif
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
 * Danger -- some modules use this quite extensively.
 * Remember that under Linux, this turns off all interrupts.
 */
#define splstr() spl6()

/*
 * noenable - prevent queue from running.
 * enableok - Let queue be runnable again.
 * canenable - check if queue is runnable. DO NOT call qenable() unless canenable is true.
 */
#define noenable(q)	(q)->q_flag |= QNOENB
#define enableok(q)	(q)->q_flag &= ~QNOENB
#define canenable(q)	!((q)->q_flag & QNOENB)

/*
 * Getting related queues. RDQ and WRQ are safe versions of RD and WR.
 */
#define	OTHERQ(q)	((q)->q_flag&QREADR? (q)+1: (q)-1)
#define	RDQ(q)		((q)->q_flag&QREADR? (q): (q)-1)
#define	WRQ(q)		((q)->q_flag&QREADR? (q)+1: (q))
#define	WR(q)		(q+1)
#define	RD(q)		(q-1)

/*
 * Message priority.
 */
#define queclass(bp) (bp->b_datap->db_type & (QPCTL | QEXP))


/*
 * Free a queue (pair). WARNING: Call flushq() on both queues before doing this!
 */

/*
 * The usual, except this is ANSI.
 */
extern mblk_t *rmvb(mblk_t *, mblk_t *);	/* Remove a message block (2nd arg) from the message. */
extern mblk_t *allocb(ushort,ushort);		/* Allocate a message */
extern int testb(ushort,ushort);		/* Check if a message can be allocated */
extern int getclass(ushort);			/* Message class of given size. Unimplemented; returns 1. */
extern void freeb(mblk_t *);			/* Free a message block */
extern void freemsg(mblk_t *);			/* Free a message list */
extern void linkb(mblk_t *, mblk_t *);		/* Link two messages */
extern mblk_t *unlinkb(mblk_t *);		/* Unlink first message block */
extern mblk_t *dupb(mblk_t *);			/* Duplicate a message block */
extern mblk_t *dupmsg(mblk_t *);		/* Duplicate a message */
extern mblk_t *copyb(mblk_t *);			/* Copy a message block */
extern mblk_t *copymsg(mblk_t *);		/* Copy a message */
extern int pullupmsg(mblk_t *, short);		/* Pull up the first N bytes */
extern mblk_t *pullupmsg_(mblk_t *, short);	/* Same, but better */
extern int adjmsg(mblk_t *, short);		/* Trim first/last(N<0) bytes */
extern int msgdsize(mblk_t *);			/* # data bytes */
extern int xmsgsize(mblk_t *);			/* # bytes of first msg block type */
extern mblk_t *getq(queue_t *);			/* Remove first msg from queue */
extern void rmvq(queue_t *, mblk_t *);		/* Remove this msg from queue */
extern void flushq(queue_t *, int);		/* Flush messages */
extern void putq(queue_t *, mblk_t *);		/* Append message to queue */
extern void putbq(queue_t *, mblk_t *);		/* Put message back onto queue */
extern void insq(queue_t *, mblk_t *, mblk_t *);/* Insert message(3) before message(2) or at end */
extern void appq(queue_t *, mblk_t *, mblk_t *);/* Insert message(3) after message(2) or at start */
extern int putctl(queue_t *, unchar);		/* Zero-byte ctl msg */
extern int putctl1(queue_t *, unchar, streamchar);/* One-byte ctl msg */
extern int canput(queue_t *);			/* Room in queue? */

extern queue_t *backq(queue_t *);		/* Get the queue pointing to me */
extern queue_t *allocq(void);			/* Allocate queues */
extern void freeq(queue_t *q);			/* Free a queue pair */
extern void qreply(queue_t *p_queue, mblk_t *p_msg);/* Send a message back */
extern void qenable(queue_t *p_queue);		/* Schedule a service procedure */
extern void runqueues(void);			/* Schedule queues */

extern int findmod(const char *);		/* Find module name in fmodsw[] */
extern int qsize(queue_t *queue_t);		/* Count messages on a queue */
extern void setq (queue_t *, struct qinit *, struct qinit *);/* set queue procs and params */
extern int register_strdev(unsigned int major, const char * name, struct streamtab *strtab);
extern int register_strmod(const char *name, struct streamtab *strtab);

#ifndef FMNAMESZ
#define FMNAMESZ        8
#endif

struct fmodsw {
        char f_name[FMNAMESZ+1];
        struct streamtab *f_str;
};

extern struct fmodsw fmodsw[];
extern int fmodcnt;

extern struct file_operations streams_fops;
extern struct streamtab *fstrsw[MAX_STRDEV];

extern int strmsgsz;			/* maximum stream message size */
extern int nstrpush;			/* maximum # of pushed modules */
extern volatile queue_t *sched_first, *sched_last;



#if 0
extern int (*stream_open)();		/* stropen() hook */
extern int (*stream_close)();		/* strclose() hook */
extern int (*stream_read)();		/* strread() hook */
extern int (*stream_write)();		/* strwrite() hook */
extern int (*stream_ioctl)();		/* strioctl() hook */
extern int (*stream_run)();		/* stream scheduler */

#define ALLOC_LOCK(x)	
#define EXTERN_LOCK(x)	
#define INITLOCK(x,y)	
#define SPSEMA(x)	
#define SVSEMA(x)	
#define PSEMA(x,y)	
#define VSEMA(x,y)	
#endif

#define putnext(q, m) (*(q)->q_next->q_qinfo->qi_putp)((q)->q_next, m)

#endif /* __sys_stream_h */
