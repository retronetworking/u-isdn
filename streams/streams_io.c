#undef CONFIG_DEBUG_STREAMS_1

/**
 * Streams I/O interface code.
 *
 * Hacked for Linux.	1.2	-M.U-
 */

#ifdef MODULE
#define F_NOCODE
#include "f_module.h"
#undef F_NOCODE
#endif

#include "kernel.h"
#include <linux/types.h>
#include "stream.h"
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/stat.h>
#include <linux/sched.h>
#include <linux/fcntl.h>
#include <linux/string.h>
#include <linux/malloc.h>
#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/termios.h>

#include <linux/tqueue.h>

#define WAIT_READ 1
#define WAIT_WRITE 2
#define WAIT_NOINTR 4

#define TIMEOUT_INFINITE (-1)

/*
 * Serialize ioctl messages
 */
static long ioctl_id;

/*
 *  Structs for the Streams head.
 */
static void stream_rput (queue_t *, mblk_t *);
static void stream_wsrv (queue_t *);
static void stream_rsrv (queue_t *);

static struct stream_header *first_stream = NULL;
/**
 * streams_open
 *
 * Open a stream. Lots of special cases...
 * Specifically, if pp_stream isn't NULL, we're opening a tty.
 */

static int 
xstreams_open (struct inode *inode, struct file *file, struct stream_header **pp_stream, int *isdupe)
{
	struct stream_header *p_stream;
	queue_t *p_queue;
	long s;

	static struct module_info stream_rinfo = {0, "strrhead", 0,0, STRHIGH, STRLOW};
	static struct module_info stream_winfo = {0, "strwhead", 0,0, STRHIGH, STRLOW};
	static struct qinit stream_rdata = {stream_rput, stream_rsrv, NULL, NULL, NULL, &stream_rinfo, NULL};
	static struct qinit stream_wdata = {NULL, stream_wsrv, NULL, NULL, NULL, &stream_winfo, NULL};

  try_again:
	/*
	 * First, if the stream is already open via another device file,
	 * i.e. another inode, find it and redirect us there.
     * Unfortunately this means scanning the Stream table.
	 */

	s = splstr();

	p_stream = (struct stream_header *)inode->u.generic_ip;
	if(p_stream == NULL) {

		for (p_stream = first_stream; p_stream != NULL; p_stream = p_stream->next) {
			if (p_stream->inode == inode || p_stream->inode->i_rdev == inode->i_rdev)
				break;
		}
	}
	splx(s);

	if (p_stream != NULL) {
		/*
	 	 * If the stream already exists: If closing, leave. If another
	 	 * open is in progress, wait, then re-call any open functions.
		 * (Why this re-open-ing is done at all is anybody's guess,
		 * but the specs say it's done -- so we do it.)
	 	 */

		if((pp_stream != NULL) != (p_stream->tty != NULL)) /* Doesn't work. */
			return -EBUSY;

		if(p_stream->magic != STREAM_MAGIC) {
			printk("XOpen BadMagic\n");
			return -EIO;
		}

		if (p_stream->flag & SF_WAITCLOSE) {
			printk("XOpen WaitClose\n");
			return -ENXIO;
		}

		if (p_stream->flag & SF_WAITOPEN) {
#if 0 /* def CONFIG_DEBUG_STREAMS */
			printf("already open %p, sleeping.\n",p_stream);
#endif
			interruptible_sleep_on (&p_stream->waiting);

			if (current->signal & ~current->blocked) {
				printk("XOpen SigRestart\n");
				return (-ERESTARTSYS);
			}
			goto try_again;
		}
		if (p_stream->error < 0) {
			printk("XOpen Err %d\n",p_stream->error);
			return p_stream->error;
		}
		else if (p_stream->flag & SF_HANGUP) {
			printk("XOpen HANGUP\n");
			return -ENXIO;
		}

		if(isdupe != NULL)
			*isdupe = 1;

		p_stream->flag |= SF_WAITOPEN;

		/*
		 * Say hello to the driver.
		 */
#if 0
		/*
		 * The standard says we have to re-open the modules too...
		 * doesn't make much sense IMHO.
		 * Interestingly, SVR3 doesn't seem to pass the file's flags 
		 * to modules.
		 */
		if((p_queue = p_stream->write_queue->q_next) == NULL) {
			printk("XOpen NoNextQ\n");
			return -EIO;
		}

		while(1) {
			if(p_queue->q_next == NULL)
				break;
			if(p_queue->q_next->q_flag & QREADR)
				break;
#if 0
			/* Barring fancy games, this should be a module. */
			p_stream->error = (*RD (p_queue)->q_qinfo->qi_qopen) (RD (p_queue), inode->i_rdev, file->f_flags, MODOPEN);

			if (p_stream->error == OPENFAIL)
				p_stream->error = u.u_error;

			if (p_stream->error < 0) 
				break;
#endif
			p_queue = p_queue->q_next;
		}

		/* ... and this should be the driver. */
		if(p_stream->error >= 0)
			p_stream->error = (*RD (p_queue)->q_qinfo->qi_qopen) (RD (p_queue), inode->i_rdev, file->f_flags, DEVOPEN);

#endif
		if(p_stream->error >= 0) {
			p_stream->count ++;
#ifdef CONFIG_DEBUG_STREAMS_1
			printf("Reopen; count %d\n",p_stream->count);
#endif
			if (pp_stream != NULL)
				*pp_stream = p_stream;
		}
		p_stream->flag &= ~SF_WAITOPEN;
		wake_up (&p_stream->waiting);
		if (p_stream->error < 0) {
#ifdef CONFIG_DEBUG_STREAMS
			printk("XOpen RetErr %d\n",p_stream->error);
#endif
			return p_stream->error;
		}
		return 0;
	}

	/*
	 * Not open yet. Find the driver and open it.
	 */
	if(isdupe != NULL)
		*isdupe = 0;

	if (fstr_sw[MAJOR (inode->i_rdev)] == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		printf("Streams Dev %x: Not in the system!\n",inode->i_rdev);
#endif
		return -ENOENT;
	}

	if ((p_queue = allocq ()) == NULL) {
		printk("XOpen RetNoMem\n");
		return -ENOMEM;
	}

	/*
	 * Allocate a Streams header. We just grab one from kernel memory.
	 */
	if ((p_stream = (struct stream_header *) kmalloc (sizeof (struct stream_header), GFP_ATOMIC)) == NULL) {
		printk("XOpen RetNoMem2\n");
		freeq (p_queue);
		return -ENOMEM;
	}

	/*
	 * Initialize the thing.
	 */
	memset (p_stream, 0, sizeof (*p_stream));

	s = splstr();
	if(first_stream != NULL)
		first_stream->prev = p_stream;
	p_stream->next = first_stream;
	first_stream = p_stream;

	p_stream->magic = STREAM_MAGIC;
	p_stream->inode = inode;
	p_stream->write_queue = WR(p_queue);
	setq (p_queue, &stream_rdata, &stream_wdata);
	p_queue->q_ptr = WR (p_queue)->q_ptr = (caddr_t) p_stream;

	p_stream->flag |= SF_WAITOPEN;
	(struct stream_header *)inode->u.generic_ip = p_stream;
	splx(s);

	/*
	 * Open the driver. qattach does the right thing here.
	 */
	p_stream->error = qattach (fstr_sw[MAJOR(inode->i_rdev)], p_queue, inode->i_rdev, file->f_flags);

	/*
	 * Wake up whoever might be waiting for the stream to be created.
	 * Bail out safely if the open returns an error.
	 */
	p_stream->flag &= ~SF_WAITOPEN;
	if (p_stream->error < 0) {
		int err = p_stream->error;
		inode->u.generic_ip = NULL;
		wake_up (&p_stream->waiting);
		freeq (p_queue);
		kfree_s (p_stream, sizeof (*p_stream));

#ifdef CONFIG_DEBUG_STREAMS
		if(0)printk("XOpen RetErrR %d\n",err);
#endif
		if(p_stream->next != NULL)
			p_stream->next->prev = p_stream->prev;
		if(p_stream->prev != NULL)
			p_stream->prev->next = p_stream->next;
		else 
			first_stream = p_stream->next;
  		return err;
	}

	/*
	 * Clone driver support:
	 * Re-set the inode iff the open code reassigned the stream.
	 *
	 * iput() has already been called on the old inode.
	 */
	file->f_inode = p_stream->inode;
	/* file->f_rdev = p_stream->inode->i_rdev; */

	p_stream->count ++;
	if (pp_stream != NULL)
		*pp_stream = p_stream;

	RD(p_stream->write_queue)->q_flag |= QWANTR;
	p_stream->write_queue->q_flag |= QWANTR;
	wake_up (&p_stream->waiting);
	return 0;
}

static int streams_open(struct inode *inode, struct file *file)
{
	return xstreams_open (inode, file, NULL, NULL);
}

/**
 * streams_close
 *
 * Release a stream. This can be called more than once because the stream
 * may have been opened from different inodes.
 */

static void 
streams_close (struct inode *inode, struct file *file)
{
	struct stream_header *p_stream;
	queue_t *p_queue;
	long xtimeout, s;

	if ((p_stream = (struct stream_header *)inode->u.generic_ip) == NULL)
		return;
	if(p_stream->magic != STREAM_MAGIC)
		return;

	fasync_helper(inode, file, 0, &p_stream->fasync);
	if (--p_stream->count > 0) {
#ifdef CONFIG_DEBUG_STREAMS_1
		printf("NoClose, count %d\n",p_stream->count);
#endif
		return;
	}

	p_stream->flag |= SF_WAITCLOSE;
	p_stream->flag &= ~SF_IOCTL;

    wake_up (&p_stream->reading);
    wake_up (&p_stream->writing);
    wake_up (&p_stream->ioctling);

	p_queue = p_stream->write_queue;
#ifdef CONFIG_DEBUG_STREAMS_1
	printf("Closing %p q %p\n",p_stream,RD(p_queue));
#endif
	/*
	 * Invent a maximum timeout, and wait for the data to drain.
	 */
	xtimeout = jiffies + HZ * STRTIMOUT;
	while (p_queue->q_next != NULL) {
		if (jiffies < xtimeout && p_stream->error >= 0 && !(file->f_flags & O_NDELAY)) {
			current->timeout = xtimeout;
			if(p_queue->q_next->q_count != 0 &&
					!(current->signal & ~current->blocked))
				interruptible_sleep_on (&p_stream->waiting);
		}
		qdetach (RD (p_queue->q_next), 1, file->f_flags);
	}
	current->timeout = 0;

	/* Now all we have is the stream head. Kill it. */
	s = splstr();

	flushq (p_queue, FLUSHALL);
	p_queue = RD (p_queue);
	flushq (p_queue, FLUSHALL);

	inode->u.generic_ip = NULL;
	if(p_stream->next != NULL)
		p_stream->next->prev = p_stream->prev;
	if(p_stream->prev != NULL)
		p_stream->prev->next = p_stream->next;
	else 
		first_stream = p_stream->next;

	splx(s);

	if (p_queue->q_flag & QENAB || WR (p_queue)->q_flag & QENAB) {
		queue_t **p_scan = (queue_t **) & sched_first;
		queue_t *p_prev = NULL;

		/*
		 * Unschedule any service procedures. Copied from qdetach().
		 */
		while (*p_scan != NULL) {
			if (*p_scan == p_queue || *p_scan == WR (p_queue)) {
				if (sched_last == *p_scan)
					sched_last = p_prev;
				*p_scan = (*p_scan)->q_link;
			} else {
				p_prev = *p_scan;
				p_scan = &p_prev->q_link;
			}
		}
	}
	if (p_stream->iocblk != NULL)
		freemsg (p_stream->iocblk);
	if (p_stream->write_buf != NULL)
		freemsg(p_stream->write_buf);

	kfree_s (p_stream, sizeof (*p_stream));
	freeq (p_queue);
}

/**
 * streams_read
 *
 * Read from a stream. Empty messages in Stream mode say EOF
 * and must be handled carefully. Empty message blocks are
 * not special, regardless of any bugs Sys5 code may have in that area. ;-)
 *
 * Signals are delivered to the reader; once Linux
 * gets better TTY handling this should be changed.
 *
 * M_PROTO messages et al. cannot be read (yet...) and return an error.
 */

static int 
streams_read (struct inode *inode, struct file *file, char *buf, int count)
{
	struct stream_header *p_stream;
	mblk_t *p_msg;
	int bytes = 0;
	unsigned char oldtype = 0;

	if ((p_stream = (struct stream_header *)inode->u.generic_ip) == NULL) 
		return -ENXIO;
	if(p_stream->magic != STREAM_MAGIC)
		return -EIO;
	if (p_stream->error < 0)
		return p_stream->error;
	if(p_stream->flag & SF_HANGUP)
		return 0;
	if(p_stream->flag & SF_WAITCLOSE)
		return -EIO;

	while (bytes < count) {
		unsigned long s = splstr();
		while ((p_msg = getq (RD (p_stream->write_queue))) == NULL) {
#if 0 /* defined(CONFIG_DEBUG_STREAMS) && defined(MODULE) */
	printf("READ No data at %p, bytes %d, flags %p, err %d\n",p_stream,bytes,p_stream->flag,p_stream->error);
#endif
			if(bytes > 0) {
				splx(s);
				return bytes;
			}
			if (p_stream->error < 0) {
				splx(s);
				return p_stream->error;
			}
			if (file->f_mode & O_NDELAY) {
				splx(s);
				return -EAGAIN;
			}

			interruptible_sleep_on (&p_stream->reading);

			if (p_stream->error < 0) {
				splx(s);
				return p_stream->error;
			}
			if(p_stream->flag & SF_HANGUP) {
				splx(s);
				return 0;
			}
			if (current->signal & ~current->blocked)  {
				splx(s);
				return -ERESTARTSYS;
			}
		}
		p_stream->flag &= ~SF_PARTIALREAD;
		splx(s);
		/*
		 * Now that we have pulled a message off, let flow control
		 * do its thing if necessary.
		 */
		runqueues ();

		while(p_msg->b_cont != NULL && p_msg->b_rptr >= p_msg->b_wptr) {
			mblk_t *p_cont = p_msg->b_cont;
			freeb(p_msg);
			p_msg = p_cont;
		}

		switch (DATA_TYPE(p_msg)) {
		case M_DATA:
		case M_EXDATA:
			/*
			 * If reading a message stream, and if the msg type changes,
			 * break off and unconditionally put the message back in front.
			 */
			if(bytes > 0 && DATA_TYPE(p_msg) != oldtype) {
				appq (RD (p_stream->write_queue), NULL, p_msg);
				return bytes;
			}
			oldtype = DATA_TYPE(p_msg);
			/*
			 * Empty message? Simulate one EOF.
			 */
			while(p_msg->b_cont != NULL && p_msg->b_rptr >= p_msg->b_wptr) {
				mblk_t *p_cont = p_msg->b_cont;
				freeb(p_msg);
				p_msg = p_cont;
			}
			if (p_msg->b_wptr <= p_msg->b_rptr) {
				if (bytes > 0) {
					appq (RD (p_stream->write_queue), NULL, p_msg);
					p_stream->flag |= SF_PARTIALREAD;
				} else
					freemsg (p_msg);
				return bytes;
			}
			while (p_msg != NULL && count > 0) {
				int n;

#ifdef CONFIG_DEBUG_STREAMS
				if(p_msg->deb_magic != DEB_PMAGIC) {
					printf("MsgBad at read! %lx %p\n",p_msg->deb_magic,p_msg->b_rptr);
					p_msg = NULL;
				} else
				  if(p_msg->b_datap->deb_magic != DEB_DMAGIC) {
					printf("MsgBad at read! %lx %p\n",p_msg->b_datap->deb_magic,p_msg->b_rptr);
					p_msg = NULL;
				} else
				  if(p_msg->b_rptr == NULL) {
		 			printf("MsgNull from %s:%d\n",p_msg->deb_file,p_msg->deb_line);
					p_msg->b_wptr = NULL;
				} else
#endif
				if ((n = min (count, p_msg->b_wptr - p_msg->b_rptr)) > 0) {
					memcpy_tofs (buf, p_msg->b_rptr, n);
					buf += n;
					bytes += n;
					count -= n;
					p_msg->b_rptr += n;
				}
				while(p_msg != NULL && p_msg->b_rptr >= p_msg->b_wptr) {
					mblk_t *p_cont = p_msg->b_cont;
					freeb(p_msg);
					p_msg = p_cont;
				}
			}

			/*
			 * Out of space?
			 */
			if (p_msg != NULL) {
				if (p_stream->flag & SF_MSGDISCARD)
					freemsg (p_msg);
				else {
					appq (RD (p_stream->write_queue), NULL, p_msg);
					p_stream->flag |= SF_PARTIALREAD;
				}

			}

			if ((count == 0) || (p_stream->flag & (SF_MSGKEEP | SF_MSGDISCARD)))
				return bytes;
			continue;

		case M_SIG:
		case M_EXSIG:
		case M_PCSIG:	/* shouldn't happen, but... */
			psignal (*p_msg->b_rptr, current);
			freemsg (p_msg);

			if(current->signal & ~current->blocked)
				return (bytes > 0 ? bytes : -ERESTARTSYS);
			break;

		case M_PROTO:
		case M_EXPROTO:
		case M_PCPROTO:
			/*
			 * Non-data messages -- put back.
			 * Since we don't have a arbitrary-message-reading system call
			 * yet(?), EBADMSG is returned (once) and the message is discarded.
			 */
#if 1
			if(bytes == 0)
				freemsg(p_msg);
			else
#endif
				appq (RD (p_stream->write_queue), NULL, p_msg);
			return (bytes ? bytes : -EBADMSG);

		default:
			/*
			 * Anything else shouldn't be here -- delete it.
			 */
			freemsg (p_msg);
			if(bytes) return bytes;
		}
	}
	return bytes;
}


static int
streams_fasync(struct inode *inode, struct file *filp, int on)
{
	int retval;
	struct stream_header *p_stream;

	if ((p_stream = (struct stream_header *)inode->u.generic_ip) == NULL) 
		return -ENXIO;
	if(p_stream->magic != STREAM_MAGIC)
		return -EIO;
	if (p_stream->error < 0)
		return p_stream->error;
	if(p_stream->flag & SF_HANGUP)
		return 0;

    retval = fasync_helper(inode, filp, on, &p_stream->fasync);
    if (retval < 0)
        return retval;
    return 0;
}


static void isig(int sig, struct tty_struct *tty)
{
	if (tty->pgrp > 0)
		kill_pg(tty->pgrp, sig, 1);
}


static void
stream_rsrv(queue_t *p_queue)
{
	struct stream_header *p_stream = (struct stream_header *) p_queue->q_ptr;
	struct tty_struct *tty;
	mblk_t *p_msg = NULL;

	if(p_stream == NULL) {
		printk("No Stream for %p\n",p_queue);
		return;
	}
	if((tty = p_stream->tty) == NULL) { /* the read routine will pick up the bits */
    	if (p_stream->fasync)
        	kill_fasync(p_stream->fasync, SIGIO);
		return;
	}
	while ((p_msg = getq(p_queue)) != NULL) {

		switch(DATA_TYPE(p_msg)) {
		case M_EXDATA:
		case M_DATA:
		  cont:
			while (p_msg->b_rptr >= p_msg->b_wptr) {
				mblk_t *p_cont = p_msg->b_cont;
				freeb(p_msg);
				if (p_cont == NULL)
					goto cont2;
				p_msg = p_cont;
			}
			{
				int i = min(tty->ldisc.receive_room(tty), p_msg->b_wptr-p_msg->b_rptr);
				if(i <= 0) {
					putbq (p_queue,p_msg);
					goto out;
				}
				tty->ldisc.receive_buf(tty,p_msg->b_rptr,NULL,i);
				p_msg->b_rptr += i;
			}
			goto cont;
		case M_PCSIG:
		case M_EXSIG:
		case M_SIG:
			{
				isig(*p_msg->b_rptr, tty);
				
				freemsg(p_msg);
				p_msg = NULL;
				break;
			}
		default:
			freemsg(p_msg);
			p_msg = NULL;
			break;
		}
cont2:;
	}
out:;
}


/**
 * stream_rput
 *
 * Some special processing.
 */

static void 
stream_rput (queue_t * p_queue, mblk_t * p_msg)
{
	struct stream_header *p_stream;
	struct iocblk *ioctl_pb;
	struct stroptions *sop;

	p_stream = (struct stream_header *) p_queue->q_ptr;

	switch (DATA_TYPE(p_msg)) {

	case M_PCPROTO:
		/*
		 * The standard says that if there's more than one priority message,
		 * the second is deleted. Hmm. You implement it if you need it.
		 */
		/* FALL THRU */
	case M_EXPROTO:
	case M_PROTO:
	case M_EXDATA:
	case M_DATA:
		if((p_stream->flag & SF_PARTIALREAD)
				&& p_queue->q_first != NULL
				&& queclass(p_queue->q_first) < queclass(p_msg)) 
			appq(p_queue,p_queue->q_first, p_msg);
		else
			putq (p_queue, p_msg);
		p_stream->flag &=~ SF_PARTIALREAD;
		wake_up (&p_stream->reading);
#if 0 /* defined(CONFIG_DEBUG_STREAMS) && defined(MODULE) */
		printk("Wakeup Read %p:%p\n",p_stream,p_queue);
#endif
		return;

	case M_ERROR:
		if (*p_msg->b_rptr != 0) {
			struct tty_struct *tty;
		
			p_stream->error = -(unsigned char) *p_msg->b_rptr;
			wake_up (&p_stream->waiting);
			wake_up (&p_stream->reading);
			wake_up (&p_stream->writing);
			wake_up (&p_stream->ioctling);

			/*
			 * Flush the queues.
			 * Reading and writing is no longer possible anyway.
			 */
			DATA_TYPE(p_msg) = M_FLUSH;
			*p_msg->b_rptr = FLUSHRW;
			qreply (p_queue, p_msg);

			if(p_stream == NULL)
				return;
			if((tty = p_stream->tty) != NULL)
				tty_hangup(tty);
			return;
		}
		freemsg (p_msg);
		return;

	case M_HANGUP:
		{
			struct tty_struct *tty;
			freemsg (p_msg);
			p_stream->flag |= SF_HANGUP;

			wake_up (&p_stream->waiting);
			wake_up (&p_stream->reading);
			wake_up (&p_stream->writing);
			wake_up (&p_stream->ioctling);

			if((tty = p_stream->tty) != NULL)
				tty_hangup(tty);
			return;
		}
	case M_PCSIG:
		/*
		 * Immediately post the signal.
		 */
		{
			struct tty_struct *tty;

			if((tty = p_stream->tty) != NULL) {
				isig(*p_msg->b_rptr, tty);
				return;
			}
		}
		/* FALL THRU */
	case M_EXSIG:
	case M_SIG:
		{
			putq (p_queue, p_msg);
			wake_up (&p_stream->reading);
			return;
		}

	case M_FLUSH:
		/*
		 * Flush queues. Standard processing; see any sample driver code
		 * except that read and write queues are reversed.
		 */
		if (*p_msg->b_rptr & FLUSHR)
			flushq (p_queue, FLUSHALL);
		if (*p_msg->b_rptr & FLUSHW) {
			struct tty_struct *tty;

			flushq (WR(p_queue), FLUSHALL);
			*p_msg->b_rptr &= ~FLUSHR;
			qreply (p_queue, p_msg);

			if((tty = p_stream->tty) != NULL && !L_NOFLSH(tty)) {
				if (tty->ldisc.flush_buffer)
					tty->ldisc.flush_buffer(tty);
				if (tty->driver.flush_buffer)
					tty->driver.flush_buffer(tty);
			}
		} else
			freemsg (p_msg);
		return;

	case M_IOCACK:
	case M_IOCNAK:
		ioctl_pb = (struct iocblk *) p_msg->b_rptr;
		/*
		 * Drop if superfluous, incorrect seqnum, or duplicate.
		 * Actually, I'd panic if it's duplicate...
		 */
		if (!(p_stream->flag & SF_IOCTL) || p_stream->iocblk != NULL || (p_stream->ioctl_id != ioctl_pb->ioc_id)) {
			freemsg (p_msg);
			return;
		}
		/*
		 * Wake up the user.
		 */
		p_stream->iocblk = p_msg;
		wake_up (&p_stream->ioctling);
		return;

	case M_SETOPTS:
		if (p_msg->b_wptr - p_msg->b_rptr < sizeof (struct stroptions)) {
			freemsg (p_msg);
			return;
		}
		sop = (struct stroptions *) p_msg->b_rptr;
		if (sop->so_flags & SO_READOPT) {
			switch (sop->so_readopt) {
			case RNORM:
				p_stream->flag &= ~(SF_MSGDISCARD | SF_MSGKEEP);
				break;
			case RMSGD:
				p_stream->flag = (p_stream->flag & ~SF_MSGKEEP) | SF_MSGDISCARD;
				break;
			case RMSGN:
				p_stream->flag = (p_stream->flag & ~SF_MSGDISCARD) | SF_MSGKEEP;
				break;
			}
		}
		if (sop->so_flags & SO_WROFF)
			p_stream->write_offset = sop->so_wroff;
		if (sop->so_flags & SO_HIWAT)
			p_queue->q_hiwat = sop->so_hiwat;

		if (sop->so_flags & SO_LOWAT) {
			p_queue->q_lowat = sop->so_lowat;

			/* Standard flow control stuff... */
			get_post(p_queue);
		}
		freemsg (p_msg);
		return;

	case M_IOCTL:
		ioctl_pb = (struct iocblk *) p_msg->b_rptr;
		/*
		 * Always send a NAK back; two Streams heads might be connected.
		 * The driver responsible for this crossover should have caught
		 * this...
		 */
		DATA_TYPE(p_msg) = M_IOCNAK;
		ioctl_pb->ioc_error = EINVAL;
		qreply (p_queue, p_msg);
		return;

	default:
		freemsg (p_msg);
		return;
	}
}


/*
 * Write...
 */

static int xstream_write(struct stream_header *p_stream, int fromuser, const char *buf, int count)
{
	mblk_t *p_msg = NULL;
	int bytes = 0;

	do {
		int len = min (count, strmsgsz);
		mblk_t *p_cont;
		int try;

		for (try = 0; try < 4; try++) {
			if ((p_cont = allocb (len, BPRI_LO)) != NULL)
				break;
			else {
				/* Can't get a lot of memory? Try a bit less than that. */
				schedule ();
				if(len > 200)
					len -= (len >> 2);
			}
		}
		if (p_cont == NULL) {
			/*
			 * Ugh. No memory. If possible, send the stuff we now have,
			 * else punt.
			 */
			if (p_msg != NULL) {
				if (p_stream->flag & (SF_MSGDISCARD | SF_MSGKEEP)) {
					freemsg (p_msg);
					printk("%sExWrite 8\n",KERN_DEBUG );
					return -ENOSR;
				} else {
					putq (p_stream->write_queue, p_msg);
					printk("%sExWriteS %d\n",KERN_DEBUG,bytes);
					return bytes;
				}
			} else {
				printk("%sExWrite 9\n",KERN_DEBUG );
				return -ENOSR;
			}
		}
		if(fromuser)
			memcpy_fromfs (p_cont->b_wptr, buf, len);
		else
			memcpy (p_cont->b_wptr, buf, len);
		p_cont->b_wptr += len;
		bytes += len;
		count -= len;

		if (p_msg == NULL)
			p_msg = p_cont;
		else 
			linkb (p_msg, p_cont);

		runqueues();
	} while(count > 0);

	if (p_msg != NULL) {
		putq (p_stream->write_queue, p_msg);
		runqueues();
	}

	if(bytes <= 0) 	
		printk("%sExWriteT %d\n",KERN_DEBUG,bytes);
	return bytes;
}

static int 
streams_write (struct inode *inode, struct file *file, const char *buf, int count)
{
	struct stream_header *p_stream;
	unsigned long s;

	if ((p_stream = (struct stream_header *)inode->u.generic_ip) == NULL) {
		printk("%sExWrite 7\n",KERN_DEBUG );
		return -ENXIO;
	}
	if(p_stream->magic != STREAM_MAGIC) {
		printk("%sExWrite 8\n",KERN_DEBUG );
		return -EIO;
	}

	if (p_stream->error < 0) {
		printk("%sExWrite 5\n",KERN_DEBUG );
		return p_stream->error;
	} else if (p_stream->flag & SF_HANGUP)  {
		printk("%sExWrite 6\n",KERN_DEBUG );
		return -ENXIO;
	}
	/*
	 * do until count satisfied or error
	 */

	s = splstr();

	while (!canput (p_stream->write_queue)) {
		if (file->f_mode & O_NDELAY) {
			splx(s);
			printk("%sExWrite 1\n",KERN_DEBUG );
			return -EAGAIN;
		}

		interruptible_sleep_on (&p_stream->writing);

		if (p_stream->error < 0) {
			splx(s);
			printk("%sExWrite 3\n",KERN_DEBUG );
			return p_stream->error;
		}
		if (p_stream->flag & SF_HANGUP) {
			splx(s);
			printk("%sExWrite 4\n",KERN_DEBUG );
			return -ENXIO;
		}
		if (current->signal & ~current->blocked) {
			splx(s);
			printk("%sExWrite 2\n",KERN_DEBUG );
			return -ERESTARTSYS;
		}
	}
	splx(s);

	return xstream_write(p_stream, 1, buf, count);
}


/*
 * Write service. The local queue, unfortunately, is necessary.
 */

static void 
stream_wsrv (queue_t * p_queue)
{
	struct stream_header *p_stream = (struct stream_header *) p_queue->q_ptr;
	struct tty_struct *tty = p_stream->tty;
	mblk_t *p_msg;

	if(p_queue->q_next != NULL) {
		while(canput(p_queue->q_next) && (p_msg = getq(p_queue)) != NULL) {
			putnext(p_queue,p_msg);
		}
	}
	if(canput(p_queue)) {
		if(tty != NULL) {
			if ((tty->flags & (1 << TTY_DO_WRITE_WAKEUP)) &&
				tty->ldisc.write_wakeup)
				(tty->ldisc.write_wakeup)(tty);
			wake_up_interruptible(&tty->write_wait);
		} else {
		    if (p_stream->fasync)
	        	kill_fasync(p_stream->fasync, SIGIO);
		}
		wake_up (&p_stream->writing);
		if (p_stream->flag & SF_WAITCLOSE)
			wake_up (&p_stream->waiting);
	}
}


/*
 * Send an ioctl message downstream and wait for acknowledgement
 */

#ifdef CONFIG_DEBUG_STREAMS
#define do_ioctl(a,b,c) deb_do_ioctl(__FILE__,__LINE__,(a),(b),(c))
int deb_do_ioctl (const char *deb_file, unsigned int deb_line, struct stream_header *p_stream, struct strioctl *strioctl_pb, unsigned long cmd)
#else
int do_ioctl (struct stream_header *p_stream, struct strioctl *strioctl_pb, unsigned long cmd)
#endif
{
	mblk_t *p_msg;
	struct iocblk *ioctl_pb;
	int error;
	unsigned long s;

	if ((strioctl_pb->ic_len < 0) || (strioctl_pb->ic_len > strmsgsz))
		return -EINVAL;

	if ((p_msg = allocb (sizeof (struct iocblk), BPRI_HI)) == NULL)
		 return -ENOSR;

	ioctl_pb = ((struct iocblk *)p_msg->b_wptr)++;
	DATA_TYPE(p_msg) = M_IOCTL;
	memset(ioctl_pb,0,sizeof(*ioctl_pb));

	ioctl_pb->ioc_count = strioctl_pb->ic_len;
	ioctl_pb->ioc_cmd = strioctl_pb->ic_cmd;
	ioctl_pb->ioc_uid = current->fsuid;
	ioctl_pb->ioc_gid = current->fsgid;

	/*
	 * Send down data, if any.
	 */
	if (ioctl_pb->ioc_count && (cmd & IOC_IN)) {
		mblk_t *p_cont = allocb (ioctl_pb->ioc_count, BPRI_HI);

		if (p_cont == NULL) {
			freemsg (p_msg);
			return -ENOSR;
		}
		error = verify_area(VERIFY_READ,strioctl_pb->ic_dp,ioctl_pb->ioc_count);
		if(error) {
			freemsg(p_msg);
			return error;
		}
		memcpy_fromfs (p_cont->b_wptr, strioctl_pb->ic_dp, ioctl_pb->ioc_count);
		p_cont->b_wptr += ioctl_pb->ioc_count;
		linkb (p_msg, p_cont);
	}

	/*
	 * If there's already an ioctl outstanding, wait.
	 */
	while (p_stream->flag & SF_IOCTL) {
		current->timeout = jiffies + HZ * STRTIMOUT;

		interruptible_sleep_on (&p_stream->ioctling);
		if (current->timeout == 0 || (current->signal & ~current->blocked) ||
			p_stream->error < 0 || (p_stream->flag & SF_HANGUP)) {
			freemsg (p_msg);
			current->timeout = 0;
			if(p_stream->error < 0)
				return p_stream->error;
			if(p_stream->flag & SF_HANGUP)
				return -EIO;
			if(current->signal & ~current->blocked)
				return -ERESTARTSYS;
			return -ETIME;
		}
	}
	current->timeout = 0;
	p_stream->flag |= SF_IOCTL;

	if (p_stream->iocblk) {
		freemsg (p_stream->iocblk);
		p_stream->iocblk = NULL;
	}

	ioctl_pb->ioc_id = p_stream->ioctl_id = ++ioctl_id;

	putq (p_stream->write_queue, p_msg);
	runqueues ();

	if (strioctl_pb->ic_timout >= 0)
		current->timeout = jiffies + HZ * (strioctl_pb->ic_timout ? strioctl_pb->ic_timout : STRTIMOUT);

	/*
	 * If the reply has already arrived, don't sleep.  If awakened from
	 * the sleep, fail only if the reply has not arrived by then.
	 * Otherwise, process the reply.
	 */
	s = splstr();
	while (p_stream->iocblk == NULL) {
		if (p_stream->error < 0) {
			p_stream->flag &= ~SF_IOCTL;
			splx(s);
			wake_up (&p_stream->ioctling);
			return p_stream->error;
		}
		
		interruptible_sleep_on (&p_stream->ioctling);
	
		if ((current->timeout == 0 && strioctl_pb->ic_timout >= 0)
				|| (current->signal & ~current->blocked)
				|| (p_stream->flag & SF_HANGUP)
				|| (p_stream->error < 0)) {
			p_stream->flag &= ~SF_IOCTL;
			if (p_stream->iocblk != NULL) {
				freemsg (p_stream->iocblk);
				p_stream->iocblk = NULL;
			}
			current->timeout = 0;
			splx(s);
			wake_up (&p_stream->ioctling);

			if(p_stream->error < 0)
				return p_stream->error;
			if(p_stream->flag & SF_HANGUP)
				return -EIO;
			if(current->signal & ~current->blocked)
				return -ERESTARTSYS;
			return -ETIME;
		}
	}
	splx(s);
	/* Done. */
	p_msg = p_stream->iocblk;
	p_stream->iocblk = NULL;
	p_stream->flag &= ~SF_IOCTL;
	current->timeout = 0;
	wake_up (&p_stream->ioctling);

	ioctl_pb = (struct iocblk *) p_msg->b_rptr;
	switch (DATA_TYPE(p_msg)) {
	case M_IOCACK:
		if (ioctl_pb->ioc_error) {
			error = -ioctl_pb->ioc_error;
			break;
		}
		/*
		 * set return value
		 */
		error = ioctl_pb->ioc_rval;

		/*
		 * Data returned?
		 */
		if ((cmd & IOC_OUT) && ioctl_pb->ioc_count) {
			int count;
			char *arg = strioctl_pb->ic_dp;

			count = ((struct iocblk *) p_msg->b_rptr)->ioc_count;
			if (count <= 0) 
				break;
			if((error = verify_area(VERIFY_WRITE,arg,count)))
				return error;

			while ((p_msg = p_msg->b_cont) != NULL && count > 0) {
				int n = min (count, p_msg->b_wptr - p_msg->b_rptr);

				if (n > 0) {
					memcpy_tofs (arg, p_msg->b_rptr, n);
					arg += n;
					count -= n;
				}
			}
			if (count > 0)
				error = -EIO;

		}
		strioctl_pb->ic_len = ioctl_pb->ioc_count;
		break;

	case M_IOCNAK:
		error = (ioctl_pb->ioc_error ? -ioctl_pb->ioc_error : -EINVAL);
		break;

	default:
		error = -EIO;
		break;
	}

	if(p_msg != NULL)
		freemsg (p_msg);
	return error;
}



/*
 * ioctl. Assume reasonable flag &c bytes in the command.
 */

static int 
xstreams_ioctl (struct stream_header *p_stream, unsigned int cmd, unsigned long arg)
{
	struct strioctl strioc;
	int error;

	if (p_stream->error < 0)
		return p_stream->error;

	/* In case we need it ... */
	strioc.ic_cmd = cmd;
	strioc.ic_timout = TIMEOUT_INFINITE;
	strioc.ic_dp = (caddr_t) arg;

	switch (cmd) {
#ifdef TCSETS
	case TCSETS:
	case TCSETSW:
	case TCSETSF:
		strioc.ic_len = sizeof(struct termios);
		cmd |= IOC_IN;
		goto do_strioctl;
#endif
#ifdef TCSETA
	case TCSETA:
	case TCSETAW:
	case TCSETAF:
		strioc.ic_len = sizeof(struct termio);
		cmd |= IOC_IN;
		goto do_strioctl;
#endif
#ifdef TCGETS
	case TCGETS:
		strioc.ic_len = sizeof(struct termios);
		cmd |= IOC_OUT;
		goto do_strioctl;
#endif
#ifdef TCGETA
	case TCGETA:
		strioc.ic_len = sizeof(struct termio);
		cmd |= IOC_OUT;
		goto do_strioctl;
#endif
#ifdef UIOCTTSTAT
	case UIOCTTSTAT:
		strioc.ic_len = 3;
		cmd |= IOC_OUT;
		goto do_strioctl;
#endif
	default:
		strioc.ic_len = (cmd & IOCSIZE_MASK) >> IOCSIZE_SHIFT;
		goto do_strioctl;

	case I_STR:
        error = verify_area(VERIFY_WRITE,(void *)arg,sizeof(struct strioctl));
        if(error)
            return error;
		memcpy_fromfs (&strioc, (caddr_t) arg, sizeof (struct strioctl));

		if (strioc.ic_len < 0 || strioc.ic_timout < TIMEOUT_INFINITE)
			return -EINVAL;

		cmd = strioc.ic_cmd;

	  do_strioctl:
		if (p_stream->flag & SF_HANGUP)
			return -ENXIO;

		error = do_ioctl (p_stream, &strioc, cmd);

		if(cmd == I_STR) 
			memcpy_tofs ((caddr_t) arg, &strioc, sizeof (struct strioctl));
		return error;


	case I_NREAD:
		{
			int size = 0;
			mblk_t *p_msg;

			error = verify_area(VERIFY_WRITE,(void *)arg,sizeof(size));
			if (error)
				return error;
			if ((p_msg = RD (p_stream->write_queue)->q_first) != NULL)
				size = msgdsize (p_msg);
			memcpy_tofs ((caddr_t)arg, &size, sizeof (size));
			return qsize (RD (p_stream->write_queue));
		}

	case I_FIND:
		{
			char mname[FMNAMESZ + 1];
			queue_t *p_queue;
			int i;

			error = verify_area(VERIFY_READ,(void *)arg,FMNAMESZ+1);
			if (error)
				return error;
			memcpy_fromfs (mname, (caddr_t) arg, FMNAMESZ + 1);

			if ((i = findmod (mname)) < 0)
				return -EINVAL;

			for (p_queue = p_stream->write_queue->q_next;
					p_queue != NULL; p_queue = p_queue->q_next) 
				if(fmod_sw[i].f_str->st_wrinit == p_queue->q_qinfo)
					return 1;

			return 0;
		}
	case I_PUSH:
		{
			char mname[FMNAMESZ + 1];
			int i;
			queue_t * p_queue;

			if (p_stream->flag & SF_HANGUP)
				return -ENXIO;
			else if(p_stream->error < 0)
				return p_stream->error;

			if (p_stream->push_count > nstrpush)
				return -EINVAL;

			error = verify_area(VERIFY_READ,(void *)arg,FMNAMESZ+1);
			if (error)
				return error;
			memcpy_fromfs (mname, (caddr_t)arg, FMNAMESZ + 1);
			if ((i = findmod (mname)) < 0)
				return -EINVAL;

			while (p_stream->flag & SF_WAITOPEN) {
				interruptible_sleep_on (&p_stream->waiting);

				if (p_stream->error < 0)
					return p_stream->error;
				if (p_stream->flag & SF_HANGUP)
					return -ENXIO;
				if (current->signal & ~current->blocked)
					return -ERESTARTSYS;
			}
			p_stream->flag |= SF_WAITOPEN;

			/*
			 * push new module and call its open routine via qattach
			 */
			if ((error = qattach (fmod_sw[i].f_str, RD (p_stream->write_queue), p_stream->inode->i_rdev, 0)) >= 0) 
				p_stream->push_count++;
			/*
			 * Reestablish flow control.
			 */
			p_queue = RD (p_stream->write_queue);
			if (p_queue->q_flag & QWANTW) {
				p_queue = p_queue->q_next;
				while ((p_queue = backq (p_queue)) != NULL) {
					if (p_queue->q_qinfo->qi_srvp != NULL) {
						qenable (p_queue);
						break;
					}
				}
			}
			p_stream->flag &= ~SF_WAITOPEN;
			wake_up (&p_stream->waiting);
			return error;
		}


	case I_POP:
		if (p_stream->flag & SF_HANGUP)
			return -ENXIO;
		if(p_stream->error < 0)
			return p_stream->error;

		if (p_stream->write_queue->q_next->q_next != NULL &&
				!(p_stream->write_queue->q_next->q_next->q_flag & QREADR)) {
			qdetach (RD (p_stream->write_queue->q_next), 1, 0);
			p_stream->push_count--;
			return 0;
		}
		return -EINVAL;


	case I_LOOK:
		{
			int i;

			for (i = 0; i < fmodcnt; i++)
				if (fmod_sw[i].f_str->st_wrinit == p_stream->write_queue->q_next->q_qinfo) {
					error = verify_area(VERIFY_WRITE,(void *)arg,FMNAMESZ+1);
					if (error)
						return error;
					memcpy_tofs ((caddr_t)arg, fmod_sw[i].f_name, FMNAMESZ + 1);
					return 0;
				}
			return -EINVAL;
		}

	case I_FLUSH:
		if (arg & ~FLUSHRW)
			return -EINVAL;
		while (!putctl1 (p_stream->write_queue->q_next, M_FLUSH, arg)) 
			schedule ();
		runqueues ();
		return 0;

	case I_SRDOPT:
		switch (arg) {
		case RNORM:
			p_stream->flag &= ~(SF_MSGDISCARD | SF_MSGKEEP);
			return 0;
		case RMSGD:
			p_stream->flag = (p_stream->flag & ~SF_MSGKEEP) | SF_MSGDISCARD;
			return 0;
		case RMSGN:
			p_stream->flag = (p_stream->flag & ~SF_MSGDISCARD) | SF_MSGKEEP;
			return 0;
		default:
			return -EINVAL;
		}

	case I_GRDOPT:
		{
			int rdopt;

			rdopt = ((p_stream->flag & SF_MSGDISCARD ? RMSGD :
							(p_stream->flag & SF_MSGKEEP ? RMSGN :
									RNORM)));

			error = verify_area(VERIFY_WRITE,(void *)arg,sizeof(rdopt));
			if (error)
				return error;
			memcpy_tofs ((caddr_t)arg, &rdopt, sizeof (rdopt));
			return 0;
		}

	}
}

static int streams_ioctl (struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	struct stream_header *p_stream;

	if ((p_stream = (struct stream_header *)inode->u.generic_ip) == NULL)
		return -ENXIO;
	if(p_stream->magic != STREAM_MAGIC)
		return -EIO;

	return xstreams_ioctl(p_stream, cmd, arg);
}

static int streams_seek(struct inode * inode, struct file * file, off_t offset, int orig)
{
    return -ESPIPE;
}

static int streams_select(struct inode *inode, struct file *file, int sel_type, select_table *wait)
{
	struct stream_header *p_stream;

	if (inode == NULL || (p_stream = (struct stream_header *)inode->u.generic_ip) == NULL 
			|| p_stream->magic != STREAM_MAGIC) {
#if 0 /* def CONFIG_DEBUG_STREAMS */
		printf("%sSTREAMS SELECT inode %p, file %p, selTable %p!\n",KERN_EMERG,inode,file,wait);
		sysdump(NULL,NULL,0xdeadbeef);
#endif
		return 1;
	}
 
	if((p_stream->flag & (SF_HANGUP|SF_WAITCLOSE)) || p_stream->error < 0)
		return 1;

	switch (sel_type) {
		case SEL_IN:
			if (RD(p_stream->write_queue)->q_first != NULL)
				return 1;

			select_wait(&p_stream->reading, wait);
			return 0;
		case SEL_OUT:
			if (canput(p_stream->write_queue))
				return 1;
			select_wait(&p_stream->writing, wait);
			return 0;
		case SEL_EX:
			select_wait(&p_stream->waiting, wait);
			return 0;
	}
	return 0;
}

struct file_operations streams_fops =
{
	streams_seek,
	streams_read,
	streams_write,
	NULL,					  /* streams_readdir */
	streams_select,
	streams_ioctl,
	NULL,					  /* streams_mmap */
	streams_open,
	streams_close,
	NULL,
	streams_fasync,
};



static int tstreams_open(struct tty_struct * tty, struct file * file)
{
	struct stream_header *p_stream;
	int err;

	p_stream = (struct stream_header *)tty->driver_data;

	if(p_stream != NULL) {
		if(file->f_inode->u.generic_ip != NULL && file->f_inode->u.generic_ip != tty->driver_data) {
			return -EIO;
		}
		file->f_inode->u.generic_ip = (void *)p_stream;
		if (p_stream->tty == NULL)
			p_stream->tty = tty;
#ifdef CONFIG_DEBUG_STREAMS
		else if (p_stream->tty != tty)
			printf("BadTTY %p: ",p_stream->tty);
		printf("DidOpen %p\n",p_stream);
#endif
		return 0;
	} else if(file->f_inode->u.generic_ip != NULL) {
		p_stream = (struct stream_header *)file->f_inode->u.generic_ip;
		tty->driver_data = p_stream;
		p_stream->count++;
		if (p_stream->tty == NULL)
			p_stream->tty = tty;
#ifdef CONFIG_DEBUG_STREAMS_1
		else if (p_stream->tty != tty)
			printf("BadTTY %p: ",p_stream->tty);
		printf("ReOpen: Stream count is %d, tty count is %d\n", p_stream->count, tty->count);
#endif
		return 0;
	}

	err = xstreams_open(file->f_inode, file, &p_stream, NULL);
	tty->driver_data = p_stream;
	if(p_stream != NULL)
		p_stream->tty = tty;
	return err;
}

/*
 * Once upon a time, the clone driver actually worked with the
 * Linux tty code -- that is, unfortunately, with a patched TTY code ... 
 */
#if 0
static int tstreams_preopen(struct inode *inode, struct file * file)
{
	int error,isdup;
	struct stream_header *p_stream = NULL;

	if(inode->u.generic_ip != NULL) {
		return -ERESTARTNOHAND;
	} else if(MINOR(inode->i_rdev) == 0)  {
		file->f_op = &streams_fops;

		error = xstreams_open(inode,file,NULL,&isdup);
	} else {
		error = xstreams_open(inode,file,&p_stream,&isdup);
		if (error == 0) {
			error = -ERESTART;
		}
	}

	return error;
}
#endif

static void tstreams_close(struct tty_struct * tty, struct file * file)
{
	struct stream_header *p_stream = (struct stream_header *)tty->driver_data;

	if (p_stream == NULL)
		return;

	if(tty->count == 1) {
		p_stream->tty = NULL;
#ifdef CONFIG_DEBUG_STREAMS_1
		if(p_stream->count != 1)
			printf("Close: Stream count is %d, tty count is %d\n", p_stream->count, tty->count);
#endif
		p_stream->count = 1;
		tty->driver_data = NULL;
		streams_close(file->f_inode, file);
	} else if(p_stream->count > 1)
		-- p_stream->count;
#ifdef CONFIG_DEBUG_STREAMS_1
	else
		printf("Close: Stream count is %d, tty count is %d\n", p_stream->count, tty->count);
#endif
}

static int tstreams_write(struct tty_struct * tty, int fromuser, const unsigned char *buf, int count)
{
	struct stream_header *p_stream = (struct stream_header *)tty->driver_data;

	if(p_stream == NULL)
		return 0;

	if(!canput(p_stream->write_queue))
		return 0;

	return xstream_write((struct stream_header *)tty->driver_data, fromuser, buf, count);
}

static int tstreams_write_room(struct tty_struct *tty)
{
	struct stream_header *p_stream = (struct stream_header *)tty->driver_data;
	long s;
	int len;

	if(p_stream == NULL)
		return 0;

	s = splstr();
	if (p_stream->write_buf == NULL && canput(p_stream->write_queue))
		p_stream->write_buf = allocb(256, BPRI_MED);
	if (p_stream->write_buf == NULL) {
		splx(s);
		return 0;
	}
	len = DATA_END(p_stream->write_buf) - p_stream->write_buf->b_wptr;
	splx(s);
	return len;
}

static void tstreams_put_char(struct tty_struct *tty, unsigned char ch)
{
	struct stream_header *p_stream = (struct stream_header *)tty->driver_data;
	mblk_t *p_msg;
	long s;

	if(p_stream == NULL)
		return;

	s = splstr();
	p_msg = p_stream->write_buf;
#if 1
	if(p_msg == NULL) {
		(void)tstreams_write_room(tty);
		p_msg = p_stream->write_buf;
	}
#endif
	if (p_msg != NULL && p_msg->b_wptr < DATA_END(p_msg))
		*p_msg->b_wptr++ = ch;
	splx(s);
}

static void tstreams_flush_chars(struct tty_struct *tty)
{
	struct stream_header *p_stream = (struct stream_header *)tty->driver_data;
	long s;
	mblk_t *p_msg;

	if(p_stream == NULL)
		return;

	s = splstr();
	if ((p_msg = p_stream->write_buf) != NULL && p_msg->b_rptr < p_msg->b_wptr) {
		putq (p_stream->write_queue, p_msg);
		p_stream->write_buf = NULL;
	}
#if 1
	(void)tstreams_write_room(tty);
#endif
	splx(s);
}

static int tstreams_ioctl(struct tty_struct *tty, struct file * file,
	    unsigned int cmd, unsigned long arg)
{
	int err;
	struct stream_header *p_stream = (struct stream_header *)tty->driver_data;

	if(p_stream == NULL) 
		return 0;

	err = xstreams_ioctl(p_stream,cmd,arg);
	return err;
}

/*
 * void (*set_termios)(struct tty_struct *tty, struct termios * old);
 *
 * 	This routine allows the tty driver to be notified when
 * 	device's termios settings have changed.  Note that a
 * 	well-designed tty driver should be prepared to accept the case
 * 	where old == NULL, and try to do something rational.
 *
 * void (*set_ldisc)(struct tty_struct *tty);
 *
 * 	This routine allows the tty driver to be notified when the
 * 	device's line discipline has changed.
 */

static void tstreams_throttle(struct tty_struct * tty)
{
	struct stream_header *p_stream = (struct stream_header *)tty->driver_data;

	if(p_stream == NULL)
		return;

	noenable(RD(p_stream->write_queue));
}

static void tstreams_unthrottle(struct tty_struct * tty)
{
	struct stream_header *p_stream = (struct stream_header *)tty->driver_data;

	if(p_stream == NULL)
		return;

	enableok(RD(p_stream->write_queue));
	qenable(RD(p_stream->write_queue));
}

/*
 * void (*stop)(struct tty_struct *tty);
 *
 * 	This routine notifies the tty driver that it should stop
 * 	outputting characters to the tty device.  
 * 
 * void (*start)(struct tty_struct *tty);
 *
 * 	This routine notifies the tty driver that it resume sending
 *	characters to the tty device.
 */

static void tstreams_hangup(struct tty_struct *tty)
{
	struct stream_header *p_stream = (struct stream_header *)tty->driver_data;

	if(p_stream == NULL)
		return;

	if(!(p_stream->error < 0) && !(p_stream->flag & SF_HANGUP)) {
		p_stream->flag |= SF_HANGUP;
		putctl(p_stream->write_queue->q_next,M_HANGUP);

		wake_up (&p_stream->waiting);
		wake_up (&p_stream->reading);
		wake_up (&p_stream->writing);
		wake_up (&p_stream->ioctling);
	}
}

static struct tty_driver *tstreams[MAX_STRDEV] = {NULL,};

int register_term_strdev(unsigned int major, const char *name, int nminor) {
	int err = -ENOMEM;
	struct tty_driver *streams_driver = NULL;
	int *stream_refcount;

	if(major > MAX_STRDEV)
		return -ENXIO;
	stream_refcount = kmalloc(sizeof(struct tty_driver)+sizeof(int), GFP_KERNEL);
	if(stream_refcount == NULL)
		goto bad;
	*stream_refcount = 0;
	streams_driver = (void *)(stream_refcount+1);

	memset(streams_driver, 0, sizeof(struct tty_driver));
	streams_driver->magic = TTY_DRIVER_MAGIC;
	streams_driver->name = (char *)name;
	streams_driver->major = major;
	streams_driver->minor_start = 0;
	streams_driver->num = nminor;
	streams_driver->type = TTY_DRIVER_TYPE_SERIAL;
	streams_driver->subtype = 0;
	streams_driver->init_termios = tty_std_termios;
	streams_driver->init_termios.c_cflag =
		B38400 | CS8 | CREAD | HUPCL | CLOCAL;
	streams_driver->init_termios.c_iflag = 0;
	streams_driver->init_termios.c_oflag = 0;
	streams_driver->init_termios.c_lflag = 0;
	streams_driver->flags = 0;
	streams_driver->refcount = stream_refcount;

	streams_driver->table = kmalloc((sizeof(struct tty_struct *) * nminor), GFP_KERNEL);
	if (streams_driver->table == NULL)
		goto bad;
	memset(streams_driver->table,0, sizeof(struct tty_struct *) * nminor);

	streams_driver->termios = kmalloc((sizeof(struct termios *) * nminor), GFP_KERNEL);
	if (streams_driver->termios == NULL)
		goto bad;
	memset(streams_driver->termios,0, sizeof(struct termios *) * nminor);

	streams_driver->termios_locked = kmalloc((sizeof(struct termios *) * nminor), GFP_KERNEL);
	if (streams_driver->termios_locked == NULL)
		goto bad;
	memset(streams_driver->termios_locked,0, sizeof(struct termios *) * nminor);

#if 0 /* old */
	streams_driver->preopen = tstreams_preopen;
#endif
	streams_driver->open = tstreams_open;
	streams_driver->close = tstreams_close;
	streams_driver->write = tstreams_write;
	streams_driver->put_char = tstreams_put_char;
	streams_driver->flush_chars = tstreams_flush_chars;
	streams_driver->write_room = tstreams_write_room;
	/* streams_driver->chars_in_buffer = tstreams_chars_in_buffer; */
	/* streams_driver->flush_buffer = tstreams_flush_buffer; */
	streams_driver->ioctl = tstreams_ioctl;
	streams_driver->throttle = tstreams_throttle;
	streams_driver->unthrottle = tstreams_unthrottle;
	/* streams_driver->set_termios = tstreams_set_termios; */
	/* streams_driver->stop = tstreams_stop; */
	/* streams_driver->start = tstreams_start; */
	streams_driver->hangup = tstreams_hangup;

	err = tty_register_driver(streams_driver);
	if (err >= 0) {
		tstreams[streams_driver->major] = streams_driver;
		return err;
	}
bad:
	if(streams_driver != NULL) {
		if (streams_driver->table != NULL)
			kfree (streams_driver->table);
		if (streams_driver->termios != NULL)
			kfree (streams_driver->termios);
		if (streams_driver->termios_locked != NULL)
			kfree (streams_driver->termios_locked);
		kfree(((int *)streams_driver)-1);
	}
	return err;
}

int unregister_term_strdev(unsigned int major, const char *name, int nminor)
{
	struct tty_driver *streams_driver = NULL;
	int err;

	if(major == 0 || major >= MAX_STRDEV)
		return -EIO;
	if((streams_driver = tstreams[major]) == NULL)
		return -ENOENT;

	err = tty_unregister_driver(streams_driver);
	if (err < 0)
		return err;
	
	tstreams[major] = 0;
	if (streams_driver->table != NULL)
		kfree (streams_driver->table);
	if (streams_driver->termios != NULL)
		kfree (streams_driver->termios);
	if (streams_driver->termios_locked != NULL)
		kfree (streams_driver->termios_locked);
	kfree(((int *)streams_driver)-1);
	return 0;
}
