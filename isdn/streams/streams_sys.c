/*
 * Simple-minded Streams support code.
 *
 * This should probably be heavily tuned...
 *
 * V 0.1. Use at your own risk.
 *
 * Matthias Urlichs <urlichs@smurf.sub.org>
 */

#define UAREA

#ifdef MODULE
#include <linux/module.h>
#include <linux/version.h>
#endif

#include <linux/types.h>
#include <linux/stream.h>
#include <linux/errno.h>
#ifdef __KERNEL__
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/kernel.h>
#include <linux/malloc.h>
#include <linux/tqueue.h>
#include <linux/interrupt.h>
#endif

#ifdef linux
#include <linux/interrupt.h>
#include <linux/syscompat.h>
#ifndef deb_kcheck
#define deb_kcheck(a,b,c) deb_kcheck_s((a),(b),(c),0)
#endif
#else

extern inline int min(int a,int b) { return((a<b)?a:b); }
#endif

/* If not kernel, then ... */
#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif
#ifndef GFP_ATOMIC
#define GFP_ATOMIC 0
#endif

/* Stupid constants. */
int strmsgsz = PAGE_SIZE-sizeof(struct datab);
int nstrpush = 16;

/* Scheduler */
volatile struct queue *sched_first = NULL, *sched_last = NULL;

#ifdef __KERNEL__
struct tq_struct q_immed = {NULL, };
struct timer_list q_later = {NULL,};
int q_timeout = 0;
#endif

#ifdef CONFIG_DEBUG_STREAMS
/* We want to pass on the debug params. */
#undef allocb
#undef freeb
#undef dupb
#undef copyb
#undef unlinkb
#undef qenable
#ifdef CONFIG_MALLOC_NAMES
#undef kmalloc
#undef kfree_s
#define kmalloc(a,b) deb_kmalloc(deb_file,deb_line,a,b)
#define kfree_s(a,b) deb_kfree_s(deb_file,deb_line,a,b)
#endif
#define allocb(a,b) deb_allocb(deb_file,deb_line,a,b)
#define freeb(a) deb_freeb(deb_file,deb_line,a)
#define dupb(a) deb_dupb(deb_file,deb_line,a)
#define copyb(a) deb_copyb(deb_file,deb_line,a)
#define unlinkb(a) deb_unlinkb(deb_file,deb_line,a)
#define qenable(a) deb_qenable(deb_file,deb_line,a)
#endif

/*
 * Data message type? 
 */

static inline const int datamsg(uchar_t type)
{
	return (type == M_DATA || type==M_EXDATA);
}

#ifdef __KERNEL__
/**
 * allocb
 *
 * Allocate a data area, data header, and message header.
 * Using kmalloc may or may not be a good idea here... but we use it anyway.
 *
 * The data header and the data area are allocated in one block.
 * The refcounter is set to one, all other fields are zeroed or set to
 * reasonable values.
 */

#ifdef CONFIG_DEBUG_STREAMS
mblk_t * deb_allocb(const char *deb_file, unsigned int deb_line, ushort size, ushort pri)
#else
mblk_t * allocb(ushort size, ushort pri)
#endif
{
	dblk_t *p_data;
	mblk_t *p_msg;

#if defined(CONFIG_MALLOC_NAMES) && defined(CONFIG_DEBUG_STREAMS) && defined(__KERNEL__)
	p_data = (struct datab *) deb_kmalloc(deb_file,deb_line,size + sizeof(struct datab), GFP_ATOMIC);
#else
	p_data = (struct datab *) kmalloc(size + sizeof(struct datab), GFP_ATOMIC);
#endif
	if(p_data == NULL) {
		printf(KERN_WARNING "Couldn't allocate %d bytes (Streams Data)\n",size+sizeof(struct datab));
		return NULL;
	}
#if defined(CONFIG_MALLOC_NAMES) && defined(CONFIG_DEBUG_STREAMS) && defined(__KERNEL__)
	p_msg = (struct msgb *)deb_kmalloc(deb_file,deb_line, sizeof(struct msgb), GFP_ATOMIC);
#else
	p_msg = (struct msgb *)kmalloc(sizeof(struct msgb), GFP_ATOMIC);
#endif
	if(p_msg == NULL) {
		kfree_s(p_data,sizeof(struct msgb));
		printf(KERN_WARNING "Couldn't allocate %d bytes (Streams Msg)\n",sizeof(struct msgb));
		return NULL;
	}
	p_msg->b_next = p_msg->b_prev = p_msg->b_cont = NULL;
	p_msg->b_datap = p_data;
#ifdef CONFIG_DEBUG_STREAMS
	p_msg->deb_magic = DEB_PMAGIC;
	p_msg->deb_queue = NULL;
	p_msg->deb_file = deb_file;
	p_msg->deb_line = deb_line;
	p_data->deb_magic = DEB_DMAGIC;
#endif
	p_data->db_base = p_msg->b_rptr = p_msg->b_wptr = (streamchar *)(p_data+1);
	p_data->db_lim = p_data->db_base + size;
	p_data->db_type = M_DATA;
	p_data->db_ref = 1;

	return p_msg;
}

/**
 * freeb
 *
 * Free a message header, decrement data block refcount, free data block if zero.
 */

#ifdef CONFIG_DEBUG_STREAMS
void deb_freeb(const char *deb_file, unsigned int deb_line, mblk_t *p_msg)
#else
void freeb(mblk_t *p_msg)
#endif
{
	dblk_t *p_data;

	if (p_msg == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		printf(KERN_ERR "Freeing NULL msg at %s:%d\n",deb_file,deb_line);
#endif
		return;
	}

	p_data = p_msg->b_datap;
#ifdef CONFIG_DEBUG_STREAMS
	if(p_msg->deb_magic != DEB_PMAGIC) 
		panic("Bad_fMagic of %p at %s:%d!\n",p_msg,deb_file,deb_line);
	if(p_msg->b_datap == NULL) 
		panic("Bad_fMagicn of %p at %s:%d!\n",p_msg,deb_file,deb_line);
	if(p_msg->b_datap->deb_magic != DEB_DMAGIC) 
		panic("Bad_Magicd of %p at %s:%d!\n",p_msg,deb_file,deb_line);
	(void)deb_msgdsize(deb_file,deb_line, p_msg);
	if(p_msg->deb_queue != NULL) {
		printf("%s:%d freed msg %p:%p from queue %p, put by %s:%d\n",
			deb_file,deb_line,p_msg,p_data,p_msg->deb_queue,p_msg->deb_file,p_msg->deb_line);
		return;
	}
#endif

	if(--p_data->db_ref <= 0) {
#ifdef CONFIG_DEBUG_STREAMS
		p_data->deb_magic = DEB_DMAGIC+0x2468BDED;
#endif

		if((streamchar *)(p_data+1) == p_data->db_base) {
#if defined(CONFIG_MALLOC_NAMES) && defined(CONFIG_DEBUG_STREAMS) && defined(__KERNEL__)
			deb_kfree_s(deb_file,deb_line, p_data,sizeof(struct datab)+(p_data->db_lim-p_data->db_base));
#else
			kfree_s(p_data,sizeof(struct datab)+(p_data->db_lim-p_data->db_base));
#endif
		} else {
#if defined(CONFIG_MALLOC_NAMES) && defined(CONFIG_DEBUG_STREAMS) && defined(__KERNEL__)
			deb_kfree_s(deb_file,deb_line,p_data,sizeof(struct datab));
#else
			kfree_s(p_data,sizeof(struct datab));
#endif
		}
	}

#ifdef CONFIG_DEBUG_STREAMS
	p_msg->deb_magic = DEB_PMAGIC+0x1357ACEF;
#endif

#if defined(CONFIG_MALLOC_NAMES) && defined(CONFIG_DEBUG_STREAMS) && defined(__KERNEL__)
	deb_kfree_s(deb_file,deb_line, p_msg,sizeof(struct msgb));
#else
	kfree_s(p_msg,sizeof(struct msgb));
#endif
}



/**
 * testb
 *
 * Can a block be allocated? We use the very-simple-minded approach,
 * i.e. the block is allocated and then freed. :-/
 *
 * This is rather inefficient, but then testb() is not widely used anyway.
 */
#ifdef CONFIG_DEBUG_STREAMS
int deb_testb(const char *deb_file, unsigned int deb_line, ushort size, ushort pri)
#else
int testb(ushort size, ushort pri)
#endif
{
	mblk_t *p_msg;

	if((p_msg = allocb(size, pri)) != NULL) {
		freeb(p_msg);
		return 1;
	} else {
		return 0;
	}
}



/**
 * getclass
 *
 * Class of the buffer?
 * We're using kmalloc instead of fixed tables, so this gets skipped.
 */
int getclass(ushort size)
{
	return 1;
}

/**
 * allocq
 *
 * Allocate a queue pair.
 * The read side gets a QREADR flag so OTHERQ knows what to do.
 */

#ifdef CONFIG_DEBUG_STREAMS
queue_t *deb_allocq(const char *deb_file, unsigned int deb_line)
#else
queue_t *allocq(void)
#endif
{
	queue_t *p_queue;

/* We're using GFP_ATOMIC because adding a module from below may be possible */
#if defined(CONFIG_MALLOC_NAMES) && defined(CONFIG_DEBUG_STREAMS) && defined(__KERNEL__)
	if((p_queue = (struct queue *)deb_kmalloc(deb_file,deb_line, 2*sizeof(struct queue),GFP_ATOMIC)) == NULL)
#else
	if((p_queue = (struct queue *)kmalloc(2*sizeof(struct queue),GFP_ATOMIC)) == NULL)
#endif
		return NULL;
	
	memset(p_queue,0,2*sizeof(*p_queue));
	p_queue->q_flag = QREADR;
#ifdef CONFIG_DEBUG_STREAMS
	if(p_queue->q_next != NULL || WR(p_queue)->q_next != NULL)
		panic("memset droppings in %s:%d!\n",__FILE__,__LINE__);
#endif
	return p_queue;
}

/**
 * freeq
 *
 * Frees a queue pair.
 */
#ifdef CONFIG_DEBUG_STREAMS
void deb_freeq(const char *deb_file, unsigned int deb_line, queue_t *p_queue)
#else
void freeq(queue_t *p_queue)
#endif
{
	if(p_queue == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		printf(KERN_ERR "Freeing NULL queue at %s:%d\n",deb_file,deb_line);
#endif
		return;
	}
#if defined(CONFIG_DEBUG_STREAMS) && defined(CONFIG_MALLOC_NAMES)
	deb_kfree_s(deb_file,deb_line, RDQ(p_queue),2*sizeof(*p_queue));
#else
	kfree_s(RDQ(p_queue),2*sizeof(*p_queue));
#endif
}


#endif /* __KERNEL__ */


/**
 * freemsg
 * 
 * Free all blocks in a message.
 *
 * TODO: If we implement specials like file descriptor passing,
 * this is the place to add the appropriate dispose code.
 */

#ifdef CONFIG_DEBUG_STREAMS
void deb_freemsg(const char *deb_file, unsigned int deb_line, mblk_t *p_msg)
#else
void freemsg(mblk_t *p_msg)
#endif
{
	mblk_t *p_temp;

	if(p_msg == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		printf(KERN_ERR "Freeing NULL msg at %s:%d\n",deb_file,deb_line);
#endif
		return;
	}
	while (p_msg != NULL) {
		p_temp = p_msg->b_cont;
		freeb(p_msg);
		p_msg = p_temp;
	}
}


/**
 * dupb
 *
 * Duplicate a message block. A new message header is allocated to point to
 * the existing data; also increment the refcount.
 */

#ifdef CONFIG_DEBUG_STREAMS
mblk_t *deb_dupb(const char *deb_file, unsigned int deb_line, mblk_t *p_msg)
#else
mblk_t *dupb(mblk_t *p_msg)
#endif
{
	mblk_t *p_newmsg;

	if(p_msg == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		printf(KERN_ERR "Dup'ing NULL msg at %s:%d\n",deb_file,deb_line);
#endif
		return NULL;
	}

#if defined(CONFIG_MALLOC_NAMES) && defined(CONFIG_DEBUG_STREAMS) && defined(__KERNEL__)
	p_newmsg = (struct msgb *)deb_kmalloc(deb_file,deb_line,sizeof(struct msgb), GFP_ATOMIC);
#else
	p_newmsg = (struct msgb *)kmalloc(sizeof(struct msgb), GFP_ATOMIC);
#endif

	*p_newmsg = *p_msg;
	p_newmsg->b_next = p_newmsg->b_prev = p_newmsg->b_cont = NULL;
	p_newmsg->b_datap->db_ref ++;
#ifdef CONFIG_DEBUG_STREAMS
	p_newmsg->deb_magic = DEB_PMAGIC;
	p_newmsg->deb_queue = NULL;
	p_newmsg->deb_file = deb_file;
	p_newmsg->deb_line = deb_line;
#endif
	return p_newmsg;
}


/**
 * dupmsg
 *
 * Duplicate a message. Walk through it with dupb().
 */

#ifdef CONFIG_DEBUG_STREAMS
mblk_t *deb_dupmsg(const char *deb_file, unsigned int deb_line, mblk_t *p_msg)
#else
mblk_t *dupmsg(mblk_t *p_msg)
#endif
{
	mblk_t *p_head, *p_newmsg;

	if(p_msg == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		printf(KERN_ERR "Dup'ing NULL msg at %s:%d\n",deb_file,deb_line);
#endif
		return NULL;
	}
	if((p_head = p_newmsg = dupb(p_msg)) == NULL)
		return NULL;

	while (p_msg->b_cont != NULL) {
		if ((p_newmsg->b_cont = dupb(p_msg->b_cont)) == NULL) {
			freemsg(p_head);
			return(NULL);
		}
		p_msg = p_msg->b_cont;
		p_newmsg = p_newmsg->b_cont;
	}
	return p_head;
}


/**
 * copyb
 *
 * Copy a message block by allocating new data and moving stuff.
 * Empty messages aren't copied.
 */

#ifdef CONFIG_DEBUG_STREAMS
mblk_t *deb_copyb(const char *deb_file, unsigned int deb_line, mblk_t *p_msg)
#else
mblk_t *copyb(mblk_t *p_msg)
#endif
{
	mblk_t *p_newmsg;
	short msglen;

	if(p_msg == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		printf(KERN_ERR "Copying NULL msg at %s:%d\n",deb_file,deb_line);
#endif
		return NULL;
	}
	if(p_msg->b_wptr <= p_msg->b_rptr)
		return NULL;
	if((msglen = p_msg->b_wptr - p_msg->b_rptr) <= 0)
		return NULL;
	
	if((p_newmsg = allocb(msglen, BPRI_LO)) == NULL)
		return NULL;
	memcpy(p_newmsg->b_wptr,p_msg->b_rptr,msglen);
	p_newmsg->b_wptr += msglen;

	return p_newmsg;
}



/**
 * copymsg
 *
 * Copy a message. Walk through it with dupb().
 */

#ifdef CONFIG_DEBUG_STREAMS
mblk_t *deb_copymsg(const char *deb_file, unsigned int deb_line, mblk_t *p_msg)
#else
mblk_t *copymsg(mblk_t *p_msg)
#endif
{
	mblk_t *p_head, *p_newmsg;

	if(p_msg == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		printf(KERN_ERR "Copying NULL msg at %s:%d\n",deb_file,deb_line);
#endif
		return NULL;
	}
	if((p_head = p_newmsg = copyb(p_msg)) == NULL)
		return NULL;

	while (p_msg->b_cont != NULL) {
		if ((p_newmsg->b_cont = copyb(p_msg->b_cont)) == NULL) {
			freemsg(p_head);
			return(NULL);
		}
		p_msg = p_msg->b_cont;
		p_newmsg = p_newmsg->b_cont;
	}
	return p_head;
}

/**
 * copyb
 *
 * Copy a message block by allocating new data and moving stuff.
 * Dat areas are kept.
 */

#ifdef CONFIG_DEBUG_STREAMS
mblk_t *deb_copybufb(const char *deb_file, unsigned int deb_line, mblk_t *p_msg)
#else
mblk_t *copybufb(mblk_t *p_msg)
#endif
{
	mblk_t *p_newmsg;
	short msglen;

	if(p_msg == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		printf(KERN_ERR "Copying NULL msg at %s:%d\n",deb_file,deb_line);
#endif
		return NULL;
	}
	if((msglen = p_msg->b_datap->db_lim - p_msg->b_datap->db_base) <= 0)
		return NULL;
	
	if((p_newmsg = allocb(msglen, BPRI_LO)) == NULL)
		return NULL;
	p_newmsg->b_wptr = p_newmsg->b_datap->db_base + (p_msg->b_wptr - p_msg->b_datap->db_base);
	p_newmsg->b_rptr = p_newmsg->b_datap->db_base + (p_msg->b_rptr - p_msg->b_datap->db_base);

	if((msglen = p_msg->b_wptr - p_msg->b_rptr) > 0)
		memcpy(p_newmsg->b_rptr,p_msg->b_rptr,msglen);

	return p_newmsg;
}



/**
 * copybufmsg
 *
 * Copy a message with block alignment. Walk through it with copybufb().
 */

#ifdef CONFIG_DEBUG_STREAMS
mblk_t *deb_copybufmsg(const char *deb_file, unsigned int deb_line, mblk_t *p_msg)
#else
mblk_t *copybufmsg(mblk_t *p_msg)
#endif
{
	mblk_t *p_head, *p_newmsg;

	if(p_msg == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		printf(KERN_ERR "Copying NULL msg at %s:%d\n",deb_file,deb_line);
#endif
		return NULL;
	}
	if((p_head = p_newmsg = copybufb(p_msg)) == NULL)
		return NULL;

	while (p_msg->b_cont != NULL) {
		if ((p_newmsg->b_cont = copybufb(p_msg->b_cont)) == NULL) {
			freemsg(p_head);
			return(NULL);
		}
		p_msg = p_msg->b_cont;
		p_newmsg = p_newmsg->b_cont;
	}
	return p_head;
}


/**
 * linkb
 *
 * Link two messages.
 * This procedure is misnamed and should be called linkmsg().
 *
 * Inlined and moved to the header file.
 */

/**
 * unlinkb
 *
 * Unlink a message block from the head of a message.
 *
 * This code is rather useless because there's no advantage to grabbing the
 * b_cont field and then either freeb()ing the head or clearing b_cont...
 * unless it's inlined, that is.
 */

/**
 * rmvb
 *
 * Remove a message block from a message.
 * The block is not freed.
 *
 * I'm dropping the "return -1 if the block isn't in the message" stuff
 * that SysV Streams does. It's not useful. You don't know if the thing is
 * in the message, you deserve to lose.
 */

#ifdef CONFIG_DEBUG_STREAMS
mblk_t *deb_rmvb(const char *deb_file, unsigned int deb_line, mblk_t *p_msg, mblk_t *p_block)
#else
mblk_t *rmvb(mblk_t *p_msg, mblk_t *p_block)
#endif
{
	mblk_t *p_ret = p_msg;

	if(p_msg == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		printf(KERN_ERR "Removing NULL msga at %s:%d\n",deb_file,deb_line);
#endif
		return NULL;
	}
	if(p_block == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		printf(KERN_ERR "Removing NULL msgb at %s:%d\n",deb_file,deb_line);
#endif
		return NULL;
	}

	if(p_msg == p_block) 
		p_ret = p_msg->b_cont;
	else do {
		if(p_msg == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
			printf(KERN_ERR "Block not in message at %s:%d\n",deb_file,deb_line);
#endif
			return NULL;
		} else if(p_msg->b_cont == p_block) {
			p_msg->b_cont = p_block->b_cont;
			break;
		} else 
			p_msg = p_msg->b_cont;
	} while(1);
	p_block->b_cont = NULL;
	return p_ret;
}


/**
 * pullupmsg
 *
 * Concatenate the first n bytes of a message.
 * This code is rather stupid. See pullupm() for how to do it better
 * (but not compatible).
 */
#ifdef CONFIG_DEBUG_STREAMS
int deb_pullupmsg(const char *deb_file, unsigned int deb_line, mblk_t *p_msg, short length)
#else
int pullupmsg(mblk_t *p_msg, short length)
#endif
{
	mblk_t *p_temp, *p_newmsg;

	if(p_msg == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		printf(KERN_ERR "Pullup NULL msg at %s:%d\n",deb_file,deb_line);
#endif
		return 0;
	}

	if (length <= 0) {
		if (p_msg->b_cont == NULL)
			return 1;
		length = xmsgsize(p_msg);
	} else {
		if (p_msg->b_wptr - p_msg->b_rptr >= length)
			return 1;
		if (xmsgsize(p_msg) < length)
			return 0;
	}

#if defined(CONFIG_MALLOC_NAMES) && defined(CONFIG_DEBUG_STREAMS) && defined(__KERNEL__)
	if ((p_temp = (struct msgb *)deb_kmalloc(deb_file,deb_line, sizeof(struct msgb),GFP_ATOMIC)) == NULL)
#else
	if ((p_temp = (struct msgb *)kmalloc(sizeof(struct msgb),GFP_ATOMIC)) == NULL)
#endif
		return 0;
	if ((p_newmsg = allocb(length, BPRI_MED)) == NULL) {
		kfree_s(p_temp,sizeof(struct msgb));
		return 0;
	}
	
	p_newmsg->b_datap->db_type = p_msg->b_datap->db_type;
	*p_temp = *p_msg;

	/*
	 * Copy as much data as we need.
	 */ 
	while (length > 0) {
		if(p_temp->b_wptr > p_temp->b_rptr) {
			short n = min(p_temp->b_wptr - p_temp->b_rptr, length);
			memcpy(p_newmsg->b_wptr, p_temp->b_rptr, n);
			p_newmsg->b_wptr += n;
			p_temp->b_rptr += n;
			length -= n;
			if (p_temp->b_rptr != p_temp->b_wptr)
				break;
		}
		{	mblk_t *p_cont;
			p_cont = p_temp->b_cont;
			freeb(p_temp);
			p_temp = p_cont;
		}
	}

	/* If the current block has data left over, keep them. */

	if (p_temp->b_rptr < p_temp->b_wptr)
		p_newmsg->b_cont = p_temp;
	else {
		p_newmsg->b_cont = p_temp->b_cont;
		freeb(p_temp);
	}
	*p_msg = *p_newmsg;
	kfree_s(p_newmsg,sizeof(struct msgb));

	return 1;
}

/**
 * adjmsg
 *
 * Chop bytes off the message.
 * If the length is < 0, chop (-length) bytes off the end.
 *
 * Note that pullupm(0) should be called after adjmsg(length>0)
 * because empty blocks are left in the message.
 * Actually, this is almost the same stupidity pullupmsg() engaged in,
 * but I didn't redo adjmsg() because I don't need it...
 */
#ifdef CONFIG_DEBUG_STREAMS
int deb_adjmsg(const char *deb_file, unsigned int deb_line, mblk_t *p_msg, short length)
#else
int adjmsg(mblk_t *p_msg, short length)
#endif
{
	mblk_t *p_temp = NULL; /* shut up GCC */
	int mlen;

	if(p_msg == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		printf(KERN_ERR "Adjust NULL msg at %s:%d\n",deb_file,deb_line);
#endif
		return 0;
	}

	if ((mlen = xmsgsize(p_msg)) < (length > 0 ? length : -length))
		return 0;

	if (length >= 0) {
		p_temp = p_msg;
		while (length > 0) {
			int n;
			if((n = min(p_temp->b_wptr - p_temp->b_rptr, length)) > 0) {
				p_temp->b_rptr += n;
				length -= n;
			}
			p_temp = p_temp->b_cont;
		}
	} else if((length = mlen - length) == 0) { /* Special case */
		p_msg->b_wptr = p_msg->b_rptr;
		if(p_msg->b_cont != NULL) {
			freemsg(p_msg->b_cont);
			p_msg->b_cont = NULL;
		}
	} else {
		mblk_t *p_last;
		do {
			if((mlen = p_temp->b_wptr - p_temp->b_rptr) > length) 
				p_temp->b_wptr = p_temp->b_rptr + length;
			if(mlen > 0)
				length -= mlen;
			p_last = p_msg;
			p_msg = p_msg->b_cont;
		} while(length > 0);
		if(p_msg != NULL) {
			p_last->b_cont = NULL;
			freemsg(p_msg);
		}
	}
	return 1;
}

/**
 * xmsgsize
 *
 * Count sizes of consecutive blocks of the same type as the first message block.
 *
 */

#ifdef CONFIG_DEBUG_STREAMS
int deb_xmsgsize(const char *deb_file, unsigned int deb_line, mblk_t *p_msg)
#else
int xmsgsize(mblk_t *p_msg)
#endif
{
	unsigned char type;
	int bytes = 0;
#ifdef CONFIG_DEBUG_STREAMS
	int segs = 0;
#endif

	if(p_msg == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		printf(KERN_ERR "XMsgSize of NULL msg at %s:%d\n",deb_file,deb_line);
#endif
		return 0;
	}

	type = p_msg->b_datap->db_type;

	do {
		short n;
#ifdef CONFIG_DEBUG_STREAMS
#if defined(CONFIG_MALLOC_NAMES) && defined(__KERNEL__)
		if(deb_kcheck_s(deb_file,deb_line,p_msg,sizeof(*p_msg)))
			return 0;
#endif
		if(p_msg->deb_magic != DEB_PMAGIC) 
			panic("Bad_Magic of %p/%d at %s:%d!\n",p_msg,segs,deb_file,deb_line);
		if(p_msg->b_datap == NULL) 
			panic("Bad_Magicn of %p/%d at %s:%d!\n",p_msg,segs,deb_file,deb_line);
		if(p_msg->b_datap->deb_magic != DEB_DMAGIC) 
			panic("Bad_Magicd of %p/%d at %s:%d!\n",p_msg,segs,deb_file,deb_line);
#endif
		if((n = p_msg->b_wptr - p_msg->b_rptr) > 0)
			bytes += n;
		if ((p_msg = p_msg->b_cont) == NULL)
			break;
	} while (type == p_msg->b_datap->db_type);

	return bytes;
}


/**
 * msgdsize
 *
 * Count size of data blocks.
 */
#ifdef CONFIG_DEBUG_STREAMS
int deb_msgdsize(const char *deb_file, unsigned int deb_line, mblk_t *p_msg)
#else
int msgdsize(mblk_t *p_msg)
#endif
{
	int bytes = 0;
#ifdef CONFIG_DEBUG_STREAMS
	int segs = 0;
#endif

	if(p_msg == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		printf(KERN_ERR "MsgDSize of NULL msg at %s:%d\n",deb_file,deb_line);
#endif
		return -EFAULT;
	}
	while(p_msg != NULL) {
#ifdef CONFIG_DEBUG_STREAMS
#if defined(CONFIG_MALLOC_NAMES) && defined(__KERNEL__)
		{
			int err;
			if((err = deb_kcheck_s(deb_file,deb_line,p_msg,sizeof(*p_msg))))
				return err;
			if((err = deb_kcheck(deb_file,deb_line+20000,p_msg->b_datap)))
				return err;
		}
#endif
		if(p_msg->deb_magic != DEB_PMAGIC) {
			printf("Bad.Magic of %p/%d at %s:%d!\n",p_msg,segs,deb_file,deb_line);
			return -EFAULT;
		}
		if(p_msg->b_datap == NULL) {
			printf("Bad.Magicn of %p/%d at %s:%d!\n",p_msg,segs,deb_file,deb_line);
			return -EFAULT;
		}
		if(p_msg->b_datap->deb_magic != DEB_DMAGIC) {
			printf("Bad.Magicd of %p/%d at %s:%d!\n",p_msg,segs,deb_file,deb_line);
			return -EFAULT;
		}
#endif
		if (p_msg->b_datap->db_type == M_DATA) {
			short n;
			if((n = p_msg->b_wptr - p_msg->b_rptr) > 0)
				bytes += n;
		}
		p_msg = p_msg->b_cont;
	}
	return bytes;
}


/**
 * rmv_post
 *
 * Adjust a queue after removing a message from it.
 *
 * - Adjust the data count.
 * - Mark the queue non-full if the count falls below the hi water mark.
 */

#ifdef CONFIG_DEBUG_STREAMS
#define rmv_post(a,b) deb_rmv_post(deb_file,deb_line,a,b)
static inline void deb_rmv_post(const char *deb_file, unsigned int deb_line, queue_t *p_queue, mblk_t *p_msg)
#else
static inline void rmv_post(queue_t *p_queue, mblk_t *p_msg)
#endif
{
	mblk_t *p_tmp = p_msg;
	unsigned long s = splstr();

#if defined(CONFIG_DEBUG_STREAMS) && defined(CONFIG_MALLOC_NAMES) && defined(__KERNEL__)
	deb_kcheck_s(deb_file,deb_line, RDQ(p_queue),2*sizeof(*p_queue));
	deb_kcheck_s(deb_file,deb_line,p_msg,sizeof(*p_msg));
	deb_kcheck(deb_file,deb_line,p_msg->b_datap);
	if(p_msg->b_cont != NULL) {
		deb_kcheck_s(deb_file,deb_line,p_msg->b_cont,sizeof(*p_msg));
		deb_kcheck(deb_file,deb_line,p_msg->b_cont->b_datap);
	}
#endif

#ifdef CONFIG_DEBUG_STREAMS
	if(p_msg->deb_magic != DEB_PMAGIC) 
		panic("Bad,Magic of %p at %s:%d!\n",
			p_msg,deb_file,deb_line);
	if(p_msg->b_datap == NULL) 
		panic("Bad,Magicn of %p at %s:%d!\n",
			p_msg,deb_file,deb_line);
	if(p_msg->b_datap->deb_magic != DEB_DMAGIC) 
		panic("Bad,Magicd of %p at %s:%d!\n",
			p_msg,deb_file,deb_line);
	if(p_msg->deb_queue != p_queue) 
		panic("rmv_post for msg %p, queue %p, by %s:%d; msg is on %p by %s:%d\n",
			p_msg,p_queue,deb_file,deb_line,p_msg->deb_queue,p_msg->deb_file,p_msg->deb_line);
#endif

	do {
		dblk_t *p_data = p_tmp->b_datap;
		p_queue->q_count -= p_data->db_lim - p_data->db_base;
	} while ((p_tmp = p_tmp->b_cont) != NULL);

	if (p_queue->q_count <= p_queue->q_hiwat)
		p_queue->q_flag &= ~QFULL;

#ifdef CONFIG_DEBUG_STREAMS
	p_msg->deb_queue = NULL;
	p_msg->deb_file = deb_file;
	p_msg->deb_line = deb_line;
#endif
	splx(s);
}

/**
 * put_post
 *
 * Adjust a queue after adding a message to it.
 *
 * - Adjust the data count.
 * - Mark the queue full if count rises above hi water mark.
 */

#ifdef CONFIG_DEBUG_STREAMS
#define put_post(a,b) deb_put_post(deb_file,deb_line,a,b)
static inline void deb_put_post(const char *deb_file, unsigned int deb_line, queue_t *p_queue, mblk_t *p_msg)
#else
static inline void put_post(queue_t *p_queue, mblk_t *p_msg)
#endif
{
	mblk_t *p_tmp = p_msg;
	unsigned long s = splstr();

#if defined(CONFIG_DEBUG_STREAMS) && defined(CONFIG_MALLOC_NAMES) && defined(__KERNEL__)
	deb_kcheck_s(deb_file,deb_line, RDQ(p_queue),2*sizeof(*p_queue));
	deb_kcheck_s(deb_file,deb_line,p_msg,sizeof(*p_msg));
	deb_kcheck(deb_file,deb_line,p_msg->b_datap);
	if(p_msg->b_cont != NULL) {
		deb_kcheck_s(deb_file,deb_line,p_msg->b_cont,sizeof(*p_msg));
		deb_kcheck(deb_file,deb_line,p_msg->b_cont->b_datap);
	}
#endif

#ifdef CONFIG_DEBUG_STREAMS
	if(p_msg->deb_magic != DEB_PMAGIC) 
		panic("Bad:Magic of %p at %s:%d!\n",
			p_msg,deb_file,deb_line);
	if(p_msg->b_datap == NULL) 
		panic("Bad:Magicn of %p at %s:%d!\n",
			p_msg,deb_file,deb_line);
	if(p_msg->b_datap->deb_magic != DEB_DMAGIC) 
		panic("Bad:Magicd of %p at %s:%d!\n",
			p_msg,deb_file,deb_line);
	if(p_msg->deb_queue != NULL) 
		panic("put_post for msg %p, queue %p, by %s:%d; msg is on %p by %s:%d\n",
			p_msg,p_queue,deb_file,deb_line,p_msg->deb_queue,p_msg->deb_file,p_msg->deb_line);
#endif
	do {
		dblk_t *p_data = p_tmp->b_datap;
		p_queue->q_count += p_data->db_lim - p_data->db_base;
	} while ((p_tmp = p_tmp->b_cont) != NULL);

	if (p_queue->q_count > p_queue->q_hiwat) 
		p_queue->q_flag |= QFULL;

#ifdef CONFIG_DEBUG_STREAMS
	p_msg->deb_queue = p_queue;
	p_msg->deb_file = deb_file;
	p_msg->deb_line = deb_line;
#endif
	splx(s);
}

/**
 * get_post
 *
 * Adjust a queue, part 2:
 * - Enable back queue if QWANTW is set, i.e. if a back queue is full.
 */
#ifdef CONFIG_DEBUG_STREAMS
inline void deb_get_post(const char *deb_file, unsigned int deb_line, queue_t *p_queue)
#else
inline void get_post(queue_t *p_queue)
#endif
{
	if (p_queue->q_count<=p_queue->q_lowat && p_queue->q_flag&QWANTW) {
		p_queue->q_flag &= ~QWANTW;
#if 0 /* def CONFIG_DEBUG_STREAMS */
		printf("Queue %s nonfull %s:%d\n",p_queue->q_qinfo->qi_minfo->mi_idname,deb_file,deb_line);
#endif
	
		while((p_queue = backq(p_queue)) != NULL) {
			 if (p_queue->q_qinfo->qi_srvp != NULL) {
			 	qenable(p_queue);
#if 0 /* def CONFIG_DEBUG_STREAMS */
				printf("	restarting %s\n",p_queue->q_qinfo->qi_minfo->mi_idname);
#endif
			 	break;
			}
		}
	}
}

/*
 * Read a message from a queue.
 *
 * - Unqueue the message.
 * - Adjust flow control.
 */

#ifdef CONFIG_DEBUG_STREAMS
mblk_t *deb_getq(const char *deb_file, unsigned int deb_line, queue_t *p_queue)
#else
mblk_t *getq(queue_t *p_queue)
#endif
{
	mblk_t *p_msg;
	unsigned long s;

	if (p_queue == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		printf(KERN_ERR "Get from NULL queue at %s:%d\n",deb_file,deb_line);
#endif
		return NULL;
	}

	s = splstr();
#if defined(CONFIG_DEBUG_STREAMS) && defined(CONFIG_MALLOC_NAMES) && defined(__KERNEL__)
	deb_kcheck_s(deb_file,deb_line, RDQ(p_queue),2*sizeof(*p_queue));
#endif
if(0)printf(KERN_ERR "G %s:%d  ",deb_file,deb_line);
	if ((p_msg = p_queue->q_first) == NULL) 
		p_queue->q_flag |= QWANTR;
	else {
		if ((p_queue->q_first = p_msg->b_next) == NULL)
			p_queue->q_last = NULL;
		else
			p_queue->q_first->b_prev = NULL;

		p_queue->q_flag &= ~QWANTR;
		p_msg->b_prev = p_msg->b_next = NULL;
		rmv_post(p_queue,p_msg);
#ifdef CONFIG_DEBUG_STREAMS
		if (p_msg->b_rptr == NULL) {
			printf(KERN_ERR "GetQ NULL stream/rptr at %s:%d, last at %s:%d\n",deb_file,deb_line,p_msg->deb_file,p_msg->deb_line);
			freemsg(p_msg);
			p_msg = NULL;
		}
		if (p_msg->b_wptr == NULL) {
			printf(KERN_ERR "GetQ NULL stream/wptr at %s:%d, last at %s:%d\n",deb_file,deb_line,p_msg->deb_file,p_msg->deb_line);
			freemsg(p_msg);
			p_msg = NULL;
		}
#endif
	}
	get_post(p_queue);

#if defined(CONFIG_DEBUG_STREAMS) && defined(CONFIG_MALLOC_NAMES) && defined(__KERNEL__)
	deb_kcheck_s(deb_file,deb_line, RDQ(p_queue),2*sizeof(*p_queue));
	{
		mblk_t *xm = p_msg;
		while(xm != NULL) {
			deb_kcheck_s(deb_file,deb_line,p_msg,sizeof(*p_msg));
			deb_kcheck(deb_file,deb_line,p_msg->b_datap);
			xm = xm->b_cont;
		}
	}
#endif
#ifdef CONFIG_DEBUG_STREAMS
	if((p_msg != NULL) && (msgdsize(p_msg) < 0))
		p_msg = NULL;
#endif
	splx(s);
	return p_msg;
}


/**
 * rmvq
 *
 * Like getq, but remove a specific message.  
 * The message must be on the queue.  
 */

#ifdef CONFIG_DEBUG_STREAMS
void deb_rmvq(const char *deb_file, unsigned int deb_line, queue_t *p_queue, mblk_t *p_msg)
#else
void rmvq(queue_t *p_queue, mblk_t *p_msg)
#endif
{ 
	unsigned long s;

	if (p_msg == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		printf(KERN_ERR "Rmv NULL msg from queue at %s:%d\n",deb_file,deb_line);
#endif
		return ;
	}
	if (p_queue == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		printf(KERN_ERR "Rmv msg from NULL queue at %s:%d\n",deb_file,deb_line);
#endif
		return ;
	}

#ifdef CONFIG_DEBUG_STREAMS
	if(p_msg->deb_magic != DEB_PMAGIC) 
		panic("Bad'Magic of %p at %s:%d!\n",
			p_msg,deb_file,deb_line);
	if(p_msg->b_datap == NULL) 
		panic("Bad'Magicn of %p at %s:%d!\n",
			p_msg,deb_file,deb_line);
	if(p_msg->b_datap->deb_magic != DEB_DMAGIC) 
		panic("Bad'Magicd of %p at %s:%d!\n",
			p_msg,deb_file,deb_line);

	if(p_msg->deb_queue != p_queue) 
		panic("rmvq for msg %p, queue %p, by %s:%d; msg is on %p by %s:%d\n",
			p_msg,p_queue,deb_file,deb_line,p_msg->deb_queue,p_msg->deb_file,p_msg->deb_line);
#endif
#if defined(CONFIG_DEBUG_STREAMS) && defined(CONFIG_MALLOC_NAMES) && defined(__KERNEL__)
	deb_kcheck_s(deb_file,deb_line, RDQ(p_queue),2*sizeof(*p_queue));
	deb_kcheck_s(deb_file,deb_line,p_msg,sizeof(*p_msg));
	deb_kcheck(deb_file,deb_line,p_msg->b_datap);
#endif
	s = splstr();

	if (p_msg->b_prev != NULL)
		p_msg->b_prev->b_next = p_msg->b_next;
	else
		p_queue->q_first = p_msg->b_next;
	if (p_msg->b_next)
		p_msg->b_next->b_prev = p_msg->b_prev;
	else
		p_queue->q_last = p_msg->b_prev;

	p_msg->b_prev = p_msg->b_next = NULL;
	rmv_post(p_queue,p_msg);
	get_post(p_queue);

	splx(s);
}

/**
 * flushq
 *
 * Empty a queue.  
 * If the flag is set, remove all messages.  Otherwise, remove 
 * only non-control messages. 
 * Restore flow control.
 */

#ifdef CONFIG_DEBUG_STREAMS
void deb_flushq(const char *deb_file, unsigned int deb_line, queue_t *p_queue, int flag)
#else
void flushq(queue_t *p_queue, int flag)
#endif
{
	mblk_t *p_msg, *p_next;
	unsigned long s;
	int q_flag;

	if (p_queue == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		printf(KERN_ERR "Flush NULL queue at %s:%d\n",deb_file,deb_line);
#endif
		return ;
	}

	s = splstr();

	q_flag = p_queue->q_flag;

	p_msg = p_queue->q_first;
	p_queue->q_first = NULL;
	p_queue->q_last = NULL;
	p_queue->q_count = 0;
	p_queue->q_flag &= ~(QFULL|QWANTW);
#if 0 /* def CONFIG_DEBUG_STREAMS */
	printf("Flush %p %s %s:%d\n",p_queue, p_queue->q_qinfo->qi_minfo->mi_idname, deb_file,deb_line);
#endif

	while (p_msg != NULL) {
		p_next = p_msg->b_next; 
#if defined(CONFIG_DEBUG_STREAMS) && defined(CONFIG_MALLOC_NAMES) && defined(__KERNEL__)
		deb_kcheck_s(deb_file,deb_line, RDQ(p_queue),2*sizeof(*p_queue));
		deb_kcheck_s(deb_file,deb_line,p_msg,sizeof(*p_msg));
		deb_kcheck(deb_file,deb_line,p_msg->b_datap);
#endif
#ifdef CONFIG_DEBUG_STREAMS
		if(p_msg->deb_queue == p_queue)
			p_msg->deb_queue = NULL;
#endif
		if (!flag && !datamsg(p_msg->b_datap->db_type))
			putq(p_queue, p_msg);
		else
			freemsg(p_msg);
		p_msg = p_next;
	}

	p_queue->q_flag |= q_flag & QWANTW;
	get_post(p_queue);

	splx(s);
}


/**
 * canput
 *
 * Reports if the queue is full and kicks the next available writer if it is.
 */

#ifdef CONFIG_DEBUG_STREAMS
int deb_canput(const char *deb_file, unsigned int deb_line, queue_t *p_queue)
#else
int canput(queue_t *p_queue)
#endif
{
	if (p_queue == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		printf(KERN_ERR "Queue NULL in canput at %s:%d\n",deb_file,deb_line);
#endif
		return 0;
	}
if(0)printf(KERN_ERR "P %s:%d  ",deb_file,deb_line);
	while (p_queue->q_next != NULL && p_queue->q_qinfo->qi_srvp == NULL)
		p_queue = p_queue->q_next;
	if (p_queue->q_flag & QFULL) {
		p_queue->q_flag |= QWANTW;
#if 0 /* def CONFIG_DEBUG_STREAMS */
		printf("Queue %s full in canput %s:%d\n",p_queue->q_qinfo->qi_minfo->mi_idname,deb_file,deb_line);
#endif
		return 0;
	}
	return 1;
}


/**
 * putq
 *
 * Put a message on a queue, after all other messages with the same priority.  
 * If queue hits its high water mark then set the QFULL flag.
 *
 */

#ifdef CONFIG_DEBUG_STREAMS
void deb_putq(const char *deb_file, unsigned int deb_line, queue_t *p_queue, mblk_t *p_msg)
#else
void putq(queue_t *p_queue, mblk_t *p_msg)
#endif
{
	unsigned long s;
	uchar_t msg_class = queclass(p_msg);

	if(p_msg == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		printf(KERN_ERR "Putting NULL msg at %s:%d\n",deb_file,deb_line);
#endif
		return;
	}
	if(p_queue == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		printf(KERN_ERR "Putting on NULL stream at %s:%d\n",deb_file,deb_line);
#endif
		freemsg(p_msg);
		return;
	}
#ifdef CONFIG_DEBUG_STREAMS
	if (p_msg->b_rptr == NULL) {
		printf(KERN_ERR "PutQ NULL stream/rptr at %s:%d, last at %s:%d\n",deb_file,deb_line,p_msg->deb_file,p_msg->deb_line);
		freemsg(p_msg);
		return;
	}
	if (p_msg->b_wptr == NULL) {
		printf(KERN_ERR "PutQ NULL stream/wptr at %s:%d, last at %s:%d\n",deb_file,deb_line,p_msg->deb_file,p_msg->deb_line);
		freemsg(p_msg);
		return;
	}
#endif

#if defined(CONFIG_DEBUG_STREAMS) && defined(CONFIG_MALLOC_NAMES) && defined(__KERNEL__)
	deb_kcheck_s(deb_file,deb_line, RDQ(p_queue),2*sizeof(*p_queue));
	deb_kcheck_s(deb_file,deb_line,p_msg,sizeof(*p_msg));
	deb_kcheck(deb_file,deb_line,p_msg->b_datap);
#endif
if(0)printf(KERN_ERR "P %s:%d  ",deb_file,deb_line);
	s = splstr();

	if (p_queue->q_first == NULL) {
		p_msg->b_prev = p_msg->b_next = NULL;
		p_queue->q_first = p_queue->q_last = p_msg;
	} else if (msg_class <= queclass(p_queue->q_last)) {
		p_queue->q_last->b_next = p_msg;
		p_msg->b_prev = p_queue->q_last;

		p_queue->q_last = p_msg;
		p_msg->b_next = NULL;
	} else {
		mblk_t *p_cont = p_queue->q_first;

		while (queclass(p_cont) >= msg_class)
			p_cont = p_cont->b_next;

		p_msg->b_next = p_cont;
		p_msg->b_prev = p_cont->b_prev;
		if (p_cont->b_prev != NULL)
			p_cont->b_prev->b_next = p_msg;
		else
			p_queue->q_first = p_msg;
		p_cont->b_prev = p_msg;
	}

	put_post(p_queue,p_msg);

	if ((msg_class > QNORM)
			|| ((p_queue->q_flag & QWANTR) && canenable(p_queue)))
		qenable(p_queue);

	splx(s);
}


/**
 * putbq
 *
 * Put a message back onto its queue.
 */

#ifdef CONFIG_DEBUG_STREAMS
void deb_putbq(const char *deb_file, unsigned int deb_line, queue_t *p_queue, mblk_t *p_msg)
#else
void putbq(queue_t *p_queue, mblk_t *p_msg)
#endif
{
	unsigned long s;
	uchar_t msg_class = queclass(p_msg);

	if (p_msg == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		printf(KERN_ERR "PutBack msg to NULL queue at %s:%d\n",deb_file,deb_line);
#endif
		return ;
	}
	if (p_queue == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		printf(KERN_ERR "PutBack NULL msg to queue at %s:%d\n",deb_file,deb_line);
#endif
		freemsg(p_msg);
		return ;
	}
#ifdef CONFIG_DEBUG_STREAMS
	if (p_msg->b_rptr == NULL) {
		printf(KERN_ERR "PutBQ NULL stream/rptr at %s:%d, last at %s:%d\n",deb_file,deb_line,p_msg->deb_file,p_msg->deb_line);
		freemsg(p_msg);
		return;
	}
	if (p_msg->b_wptr == NULL) {
		printf(KERN_ERR "PutBQ NULL stream/wptr at %s:%d, last at %s:%d\n",deb_file,deb_line,p_msg->deb_file,p_msg->deb_line);
		freemsg(p_msg);
		return;
	}
#endif

#if defined(CONFIG_DEBUG_STREAMS) && defined(CONFIG_MALLOC_NAMES) && defined(__KERNEL__)
	deb_kcheck_s(deb_file,deb_line, RDQ(p_queue),2*sizeof(*p_queue));
	deb_kcheck_s(deb_file,deb_line,p_msg,sizeof(*p_msg));
	deb_kcheck(deb_file,deb_line,p_msg->b_datap);
#endif
	s = splstr();

	if (p_queue->q_first == NULL) {
		p_msg->b_prev = p_msg->b_next = NULL;
		p_queue->q_first = p_queue->q_last = p_msg;
	} else if (msg_class >= queclass(p_queue->q_first)) {
		p_msg->b_next = p_queue->q_first;
		p_msg->b_prev = NULL;
		p_queue->q_first->b_prev = p_msg;
		p_queue->q_first = p_msg;
	} else {
		mblk_t *p_cont = p_queue->q_first;

		while ((p_cont->b_next != NULL) && (queclass(p_cont->b_next) > msg_class))
			p_cont = p_cont->b_next;

		if ((p_msg->b_next = p_cont->b_next) != NULL)
			p_cont->b_next->b_prev = p_msg;
		else
			p_queue->q_last = p_msg;
		p_cont->b_next = p_msg;
		p_msg->b_prev = p_cont;
	}

	put_post(p_queue,p_msg);

	if ((msg_class > QNORM) || (canenable(p_queue) && p_queue->q_flag & QWANTR))
		qenable(p_queue);

	splx(s);
}


/**
 * insq
 *
 * Insert a message before an existing message in a queue.
 * If NULL, insert at the end.
 */

#ifdef CONFIG_DEBUG_STREAMS
void deb_insq(const char *deb_file, unsigned int deb_line, queue_t *p_queue, mblk_t *p_oldmsg, mblk_t *p_msg)
#else
void insq(queue_t *p_queue, mblk_t *p_oldmsg, mblk_t *p_msg)
#endif
{
	unsigned long s;

	if (p_msg == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		printf(KERN_ERR "InsQ msg to NULL queue at %s:%d\n",deb_file,deb_line);
#endif
		return ;
	}
	if (p_queue == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		printf(KERN_ERR "InsQ NULL msg to queue at %s:%d\n",deb_file,deb_line);
#endif
		freemsg(p_msg);
		return ;
	}
#if defined(CONFIG_DEBUG_STREAMS) && defined(CONFIG_MALLOC_NAMES) && defined(__KERNEL__)
	deb_kcheck_s(deb_file,deb_line, RDQ(p_queue),2*sizeof(*p_queue));
	deb_kcheck_s(deb_file,deb_line,p_msg,sizeof(*p_msg));
	deb_kcheck(deb_file,deb_line,p_msg->b_datap);
	if(p_oldmsg != NULL) {
		deb_kcheck_s(deb_file,deb_line,p_oldmsg,sizeof(*p_oldmsg));
		deb_kcheck(deb_file,deb_line,p_oldmsg->b_datap);
	}
#endif
	s = splstr();

	if ((p_msg->b_next = p_oldmsg) != NULL) {
		if ((p_msg->b_prev = p_oldmsg->b_prev) != NULL)
			p_oldmsg->b_prev->b_next = p_msg;
		else
			p_queue->q_first = p_msg;
		p_oldmsg->b_prev = p_msg;
	} else {
		if ((p_msg->b_prev = p_queue->q_last) != NULL)
			p_queue->q_last->b_next = p_msg;
		else 
			p_queue->q_first = p_msg;
		p_queue->q_last = p_msg;
	}

	put_post(p_queue,p_msg);
	
	if (canenable(p_queue) && (p_queue->q_flag & QWANTR))
		qenable(p_queue);

	splx(s);
}



/**
 * appq
 *
 * Insert a message after an existing message in a queue.
 * if NULL, insert at the beginning.
 */

#ifdef CONFIG_DEBUG_STREAMS
void deb_appq(const char *deb_file, unsigned int deb_line, queue_t *p_queue, mblk_t *p_oldmsg, mblk_t *p_msg)
#else
void appq(queue_t *p_queue, mblk_t *p_oldmsg, mblk_t *p_msg)
#endif
{
	unsigned long s;

	if (p_msg == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		printf(KERN_ERR "AppQ msg to NULL queue at %s:%d\n",deb_file,deb_line);
#endif
		return ;
	}
	if (p_queue == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		printf(KERN_ERR "AppQ NULL msg to queue at %s:%d\n",deb_file,deb_line);
#endif
		freemsg(p_msg);
		return ;
	}
#ifdef CONFIG_DEBUG_STREAMS
	if (p_msg->b_rptr == NULL) {
		printf(KERN_ERR "AppQ NULL stream/rptr at %s:%d, last at %s:%d\n",deb_file,deb_line,p_msg->deb_file,p_msg->deb_line);
		freemsg(p_msg);
		return;
	}
	if (p_msg->b_wptr == NULL) {
		printf(KERN_ERR "AppQ NULL stream/wptr at %s:%d, last at %s:%d\n",deb_file,deb_line,p_msg->deb_file,p_msg->deb_line);
		freemsg(p_msg);
		return;
	}
#endif
#if defined(CONFIG_DEBUG_STREAMS) && defined(CONFIG_MALLOC_NAMES) && defined(__KERNEL__)
	deb_kcheck_s(deb_file,deb_line, RDQ(p_queue),2*sizeof(*p_queue));
	deb_kcheck_s(deb_file,deb_line,p_msg,sizeof(*p_msg));
	deb_kcheck(deb_file,deb_line,p_msg->b_datap);
	if(p_oldmsg != NULL) {
		deb_kcheck_s(deb_file,deb_line,p_oldmsg,sizeof(*p_oldmsg));
		deb_kcheck(deb_file,deb_line,p_oldmsg->b_datap);
	}
#endif
	s = splstr();

	if ((p_msg->b_prev = p_oldmsg) != NULL) {
		if ((p_msg->b_next = p_oldmsg->b_next) != NULL)
			p_oldmsg->b_next->b_prev = p_msg;
		else
			p_queue->q_last = p_msg;
		p_oldmsg->b_next = p_msg;
	} else {
		if ((p_msg->b_next = p_queue->q_first) != NULL)
			p_queue->q_first->b_prev = p_msg;
		else 
			p_queue->q_last = p_msg;
		p_queue->q_first = p_msg;
	}

	put_post(p_queue,p_msg);
	
	if (canenable(p_queue) && (p_queue->q_flag & QWANTR))
		qenable(p_queue);

	splx(s);
}


/**
 * putctl
 *
 * Create a zero-byte control message and put it onto a queue.
 */
#ifdef CONFIG_DEBUG_STREAMS
int deb_putctl(const char *deb_file, unsigned int deb_line, queue_t *p_queue, uchar_t type)
#else
int putctl(queue_t *p_queue, uchar_t type)
#endif
{
	mblk_t *p_msg;

	if ((p_msg = allocb(0, BPRI_HI)) == NULL)
		return 0;
	p_msg->b_datap->db_type = type;
	(*p_queue->q_qinfo->qi_putp)(p_queue, p_msg);
	return 1;
}



/**
 * putctl1
 *
 * Create a one-byte control message and put it onto a queue.
 */
#ifdef CONFIG_DEBUG_STREAMS
int deb_putctl1(const char *deb_file, unsigned int deb_line, queue_t *p_queue, uchar_t type, streamchar param)
#else
int putctl1(queue_t *p_queue, uchar_t type, streamchar param)
#endif
{
	mblk_t *p_msg;

	if ((p_msg = allocb(1, BPRI_HI)) == NULL)
		return 0;
	p_msg->b_datap->db_type = type;
	*p_msg->b_wptr++ = param;
	(*p_queue->q_qinfo->qi_putp)(p_queue, p_msg);
	return 1;
}



/**
 * backq
 * 
 * Return the queue which feeds this one.
 */

#ifdef CONFIG_DEBUG_STREAMS
queue_t *deb_backq(const char *deb_file, unsigned int deb_line, queue_t *p_queue)
#else
queue_t *backq(queue_t *p_queue)
#endif
{
	if(p_queue == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		printf(KERN_ERR "BackQ from NULL queue at %s:%d\n",deb_file,deb_line);
#endif
		return NULL;
	}

	if((p_queue = OTHERQ(p_queue)) == NULL)
		return NULL;
	if((p_queue = p_queue->q_next) == NULL)
		return NULL;
	return OTHERQ(p_queue);
}


/**
 * qreply
 *
 * Send something in the "other" direction.
 */

#ifdef CONFIG_DEBUG_STREAMS
void deb_qreply(const char *deb_file, unsigned int deb_line, queue_t *p_queue, mblk_t *p_msg)
#else
void qreply(queue_t *p_queue, mblk_t *p_msg)
#endif
{
	if(p_msg == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		printf(KERN_ERR "Replying with NULL msg at %s:%d\n",deb_file,deb_line);
#endif
		return;
	}
	if(p_queue == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		printf(KERN_ERR "Replying on NULL stream at %s:%d\n",deb_file,deb_line);
#endif
		freemsg(p_msg);
		return;
	}
#ifdef CONFIG_DEBUG_STREAMS
	if (p_msg->b_rptr == NULL) {
		printf(KERN_ERR "QReply NULL stream/rptr at %s:%d, last at %s:%d\n",deb_file,deb_line,p_msg->deb_file,p_msg->deb_line);
		freemsg(p_msg);
		return;
	}
	if (p_msg->b_wptr == NULL) {
		printf(KERN_ERR "QReply NULL stream/wptr at %s:%d, last at %s:%d\n",deb_file,deb_line,p_msg->deb_file,p_msg->deb_line);
		freemsg(p_msg);
		return;
	}
#endif

#if defined(CONFIG_DEBUG_STREAMS) && defined(CONFIG_MALLOC_NAMES) && defined(__KERNEL__)
	deb_kcheck_s(deb_file,deb_line, RDQ(p_queue),2*sizeof(*p_queue));
	deb_kcheck_s(deb_file,deb_line,p_msg,sizeof(*p_msg));
	deb_kcheck(deb_file,deb_line,p_msg->b_datap);
#endif
	p_queue = OTHERQ(p_queue);
	if(p_queue->q_next == NULL) {
		freemsg(p_msg);
		return;
	}
	(*p_queue->q_next->q_qinfo->qi_putp)(p_queue->q_next, p_msg);
}

/**
 * qsize
 * 
 * return number of messages on queue
 */
#ifdef CONFIG_DEBUG_STREAMS
int deb_qsize(const char *deb_file, unsigned int deb_line, queue_t *p_queue)
#else
int qsize(queue_t *p_queue)
#endif
{
	int msgs = 0;
	mblk_t *p_msg;

	if(p_queue == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		printf(KERN_ERR "SizeQ on NULL queue at %s:%d\n",deb_file,deb_line);
#endif
		return 0;
	}

	for (p_msg = p_queue->q_first; p_msg != NULL; p_msg = p_msg->b_next)
		msgs++;

	return msgs;
}

/**
 * setq
 *
 * Set queue variables
 */
#ifdef CONFIG_DEBUG_STREAMS
void deb_setq (const char *deb_file, unsigned int deb_line, queue_t * p_queue, struct qinit *read_init, struct qinit *write_init)
#else
void setq (queue_t * p_queue, struct qinit *read_init, struct qinit *write_init)
#endif
{
	if(p_queue == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		printf(KERN_ERR "SetQ on NULL queue at %s:%d\n",deb_file,deb_line);
#endif
		return;
	}
#if 0 /* def CONFIG_DEBUG_STREAMS */
	printf(KERN_DEBUG "setq: Queue %p, read_init %p, write_init %p",p_queue,read_init,write_init);
	if(read_init) printf(", read_init.minfo %p",read_init->qi_minfo);
	if(read_init->qi_minfo) printf(" %s %d %d",read_init->qi_minfo->mi_idname,read_init->qi_minfo->mi_lowat,read_init->qi_minfo->mi_hiwat);
	if(write_init) printf(", write_init.minfo %p",write_init->qi_minfo);
	if(write_init->qi_minfo) printf(" %s %d %d",write_init->qi_minfo->mi_idname,write_init->qi_minfo->mi_lowat,write_init->qi_minfo->mi_hiwat);
	printf("\n");
#endif
	p_queue->q_qinfo = read_init;
	p_queue->q_lowat = read_init->qi_minfo->mi_lowat;
	p_queue->q_hiwat = read_init->qi_minfo->mi_hiwat;
	p_queue = WR (p_queue);
	p_queue->q_qinfo = write_init;
	p_queue->q_lowat = write_init->qi_minfo->mi_lowat;
	p_queue->q_hiwat = write_init->qi_minfo->mi_hiwat;
}

/**
 * qdetach
 *
 * Detach a stream module / device.
 * do_close is true if the module was opened successfully, which means that
 * the close routine should be called.
 *
 * Before calling close(), run the queues and turn off Streams interrupts.
 * Afterwards, unschedule the service procedures if necessary.
 */

#ifdef CONFIG_DEBUG_STREAMS
void deb_qdetach (const char *deb_file, unsigned int deb_line, queue_t * p_queue, int do_close, int flag)
#else
void qdetach (queue_t * p_queue, int do_close, int flag)
#endif
{
	unsigned long s;

	if(p_queue == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		printf(KERN_ERR "Detach on NULL queue at %s:%d\n",deb_file,deb_line);
#endif
		return ;
	}
	runqueues ();

	s = splstr ();
	/*
 	 * Remove retry timers.
	 */
	if(p_queue->q_flag & QRETRY) {
		p_queue->q_flag &=~ QRETRY;
#ifdef NEW_TIMEOUT
		untimeout(p_queue->q_retry);
#else
		untimeout(do_qretry,p_queue);
#endif
	}
	if(WR(p_queue)->q_flag & QRETRY) {
		WR(p_queue)->q_flag &=~ QRETRY;
#ifdef NEW_TIMEOUT
		untimeout(WR(p_queue)->q_retry);
#else
		untimeout(do_qretry,WR(p_queue));
#endif
	}
	if (do_close) {
		printf(KERN_DEBUG "Closing %s\n", p_queue->q_qinfo->qi_minfo->mi_idname);
		(*p_queue->q_qinfo->qi_qclose) (p_queue, (p_queue->q_next ? 0 : flag));
	}

	if (p_queue->q_flag & QENAB || WR (p_queue)->q_flag & QENAB) {  /* also in stream_close(). */
		queue_t **p_scan = (queue_t **) & sched_first;
		queue_t *p_prev = NULL;

		/*
		 * Unschedule the service procedures.
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

	flushq (p_queue, FLUSHALL);
	flushq (WR (p_queue), FLUSHALL);

	if (WR (p_queue)->q_next != NULL)
		backq (p_queue)->q_next = p_queue->q_next;
	if (p_queue->q_next != NULL)
		backq (WR (p_queue))->q_next = WR (p_queue)->q_next;

	freeq (p_queue);
	splx (s);
}

/**
 * qattach
 *
 * Attach a stream device or module.
 * Pass in the read queue of the module/streams head above the to-be-attached
 * driver/module.
 */

#ifdef CONFIG_DEBUG_STREAMS
int deb_qattach (const char *deb_file, unsigned int deb_line, struct streamtab *qinfo, queue_t * p_queue, dev_t dev, int flag)
#else
int qattach (struct streamtab *qinfo, queue_t * p_queue, dev_t dev, int flag)
#endif
{
	queue_t *p_newqueue;
	unsigned long s;
	int open_mode;
	int err;

	if(p_queue == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		printf(KERN_ERR "Attach on NULL queue at %s:%d\n",deb_file,deb_line);
#endif
		return -EIO;
	}
	if ((p_newqueue = allocq ()) == NULL)
		return ENOMEM;
#if 0 /* def CONFIG_DEBUG_STREAMS */
	printf(KERN_DEBUG "qattach: info %p, oldq %p, newq %p\n",qinfo,p_queue,p_newqueue);
#endif
	s = splstr ();

	/* Insert the queue */
	p_newqueue->q_next = p_queue;
	p_queue = WR(p_queue);
	if ((WR(p_newqueue)->q_next = p_queue->q_next) == NULL)
		open_mode = DEVOPEN;
	else {
		OTHERQ (p_queue->q_next)->q_next = p_newqueue;
		open_mode = MODOPEN;
	}
	p_queue->q_next = WR(p_newqueue);

	/* Set up the queue interface stuff */
	setq (p_newqueue, qinfo->st_rdinit, qinfo->st_wrinit);
	p_newqueue->q_flag |= QWANTR;
	WR(p_newqueue)->q_flag |= QWANTR;

	/*
	 * Now call the new module/driver's open routine.
	 */
#if 0 /* def CONFIG_DEBUG_STREAM */
	printf(KERN_DEBUG "CallOpen %p %p %p\n",p_newqueue,p_newqueue->q_qinfo,p_newqueue->q_qinfo->qi_qopen);
#endif
	if ((err = (*p_newqueue->q_qinfo->qi_qopen) (p_newqueue, dev, flag, open_mode)) < 0) {
		if(err == OPENFAIL)
			err = u.u_error;
printf(KERN_DEBUG "CallOpen %s got %d\n",p_newqueue->q_qinfo->qi_minfo->mi_idname,err);
		qdetach (p_newqueue, 0, 0);
		splx (s);
		return err;
	}
printf(KERN_DEBUG "Driver %s opened\n",p_newqueue->q_qinfo->qi_minfo->mi_idname);
	splx (s);
	return 0;
}
/**
 * qenable
 *
 * Schedule a queue.
 */

#ifdef CONFIG_DEBUG_STREAMS
void deb_qenable(const char *deb_file, unsigned int deb_line, queue_t *p_queue)
#else
void qenable(queue_t *p_queue)
#endif
{
	unsigned long s;

	if(p_queue == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		printf(KERN_ERR "QEnable on NULL queue at %s:%d\n",deb_file,deb_line);
#endif
		return ;
	}
	if (p_queue->q_qinfo->qi_srvp == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		if(p_queue->q_next || !(p_queue->q_flag & QREADR)) /* not Stream head? */
			printf(KERN_ERR "QEnable on queue for %s with NULL service proc at %s:%d\n",p_queue->q_qinfo->qi_minfo->mi_idname, deb_file,deb_line);
#endif
		return ;
	}

	s = splstr();
#if defined (CONFIG_DEBUG_STREAMS) && defined(CONFIG_MALLOC_NAMES) && defined(__KERNEL__)
	deb_kcheck_s(deb_file,deb_line, RDQ(p_queue),2*sizeof(*p_queue));
#endif

	if (p_queue->q_flag & QENAB) {
#ifdef CONFIG_DEBUG_STREAMS
		queue_t *p_next;
		for(p_next = (queue_t *)sched_first; p_next != NULL; p_next = p_next->q_link) {
			if(p_next == sched_last) {
				splx(s);
				return;
			}
		}
		printf(KERN_EMERG "QErr Queue not in list, from %s:%d\n",deb_file,deb_line);
#else
		splx(s);
		return;
#endif
	}

	/*
	 * mark queue enabled and place on run list
	 */
	p_queue->q_flag |= QENAB;
	p_queue->q_link = NULL;

	if (sched_first == NULL)
		sched_first = p_queue;
#ifdef CONFIG_DEBUG_STREAMS
	else if(sched_last == NULL) {
		sched_first = sched_last = p_queue;
		printf(KERN_EMERG "QErr Sched_First != NULL; _Last == NULL ; at %s:%d\n",deb_file,deb_line);
	}
#endif
	else
		sched_last->q_link = p_queue;
		
	sched_last = p_queue;

	splx(s);
	runqueues();
}

/**
 * qretry
 *
 * Reschedule a queue, eg. after a (hopefully temporary) low-memory situation.
 */

void
qretry(queue_t *p_queue)
{
	long s = splstr();
	if(!(p_queue->q_flag & QRETRY)) {
		p_queue->q_flag |= QRETRY;
#ifdef NEW_TIMEOUT
		p_queue->q_retry =
#endif
		timeout((void *)qretry,p_queue,HZ/10);
	}
	splx(s);
}

void
do_qretry(queue_t *p_queue)
{
	long s = splstr();
#ifdef CONFIG_DEBUG_STREAMS
	const char deb_file[] = __FILE__;
	const int deb_line = __LINE__+4;
#endif
	if(p_queue->q_flag & QRETRY) {
		p_queue->q_flag &=~ QRETRY;
		qenable(p_queue);
	}
	splx(s);
}


/**
 * runqueues
 *
 * Service the queues; now if possible, soon if not.
 *
 */

#ifdef CONFIG_DEBUG_STREAMS
void deb_runqueues(const char *deb_file, unsigned int deb_line)
#else
void runqueues(void)
#endif
{
	if(sched_first != NULL) {
		static void do_runqueues(void*);
#ifdef __KERNEL__
#if 0
		unsigned long s;
		extern long intr_count;

		save_flags_fast(s);
		if((intr_count == 0) && (s & 0x200) && (bh_mask & (1<<STREAMS_BH))) {
			bh_mask &=~ (1<<STREAMS_BH);
			do_runqueues(NULL);
			bh_mask |= 1<<STREAMS_BH;
		} else
#endif
		if (q_timeout == 0) {
			queue_task(&q_immed, &tq_immediate);
			mark_bh(IMMEDIATE_BH);	/* Later */
		}
#else /* !KERNEL */
		static int isrunning = 0;
		unsigned long s = splstr();
		if(isrunning) {
			splx(s);
			return;
		}
		isrunning ++;
		splx(s);
		do_runqueues(NULL);
		isrunning --;
#endif
	}
}

/**
 * do_runqueues
 *
 * For each enabled queue, call its service procedure.
 *
 * Must not be reentered. (Guaranteed by Linux and by the above code.)
 */

static void do_runqueues(void *dummy)
{
	queue_t *p_queue;
	unsigned long s;
	int cnt = 100;
	static int looping = 0;

	s = splstr();
	while ((p_queue = (queue_t *)sched_first) != NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		if((p_queue->q_link == NULL) != (p_queue == sched_last)) {
			printf(KERN_ERR "End of Queue bad; link %p, last %p\n",
				p_queue->q_link, sched_last);
#if defined(__KERNEL__) && defined(linux)
			sysdump(NULL,NULL,0);
#endif
		sched_first = sched_last = NULL;
		break;
		}
#endif
		if ((sched_first = p_queue->q_link) == NULL)
			sched_last = NULL;
#ifdef CONFIG_DEBUG_STREAMS
		if(!(p_queue->q_flag & QENAB)) {
			printf(KERN_ERR "Queue on list but QENAB not set for %s\n",p_queue->q_qinfo->qi_minfo->mi_idname);
#if defined(__KERNEL__) && defined(linux)
			sysdump(NULL,NULL,0);
#endif
		}
#endif
		p_queue->q_flag &= ~QENAB;
		splx(s);
		if (p_queue->q_qinfo->qi_srvp) 
			(*p_queue->q_qinfo->qi_srvp)(p_queue);
		(void)splstr();
		if(!--cnt) {
			if(!looping)
				printf(KERN_WARNING "Streams loop?\n");
			looping++; looping++;
			break;
		}
	}
	if(looping) looping--;
	splx(s);
}

/**
 * splstr 
 *
 */
#ifdef linux
#ifdef CONFIG_DEBUG_STREAMS

const char *str__file; unsigned long str__line;

int deb_splstr(const char * deb_file, unsigned int deb_line)
{
	if(bh_mask & (1<<STREAMS_BH)) {
		long x;
#ifdef CONFIG_DEBUG_LATENCY
		x = deb_spl(deb_file,deb_line,(1<<STREAMS_BH)|(1<<IRQ_BH));
#else
		x = spl((1<<STREAMS_BH)|(1<<IRQ_BH));
#endif
		str__file = deb_file; str__line = deb_line;
		return x;
	} else
		return spl((1<<STREAMS_BH)|(1<<IRQ_BH));
}
#endif
#endif

/**
 * streams_init
 *
 * Initialization code.
 */

char streams_inited = 0;

static void streams_init(void)
{
#ifdef __KERNEL__
	static void q_run(void *);
#endif

	if(streams_inited) return;
	streams_inited = 1;
#ifdef __KERNEL__
	q_immed.routine = q_run;
	init_timer(&q_later);
	q_later.function = (void *)&q_run;
#endif

	return;
}

#ifdef __KERNEL__
static void q_run(void *dummy)
{
	if(bh_mask & (1<<STREAMS_BH)) {
		do_runqueues(dummy);
		q_timeout = 0;
	} else {
		q_later.expires = ++q_timeout;
		add_timer(&q_later);
	}
}
#endif

/**
 * findmod
 *
 * Find a Streams module by name.
 */
#ifdef CONFIG_DEBUG_STREAMS
int deb_findmod(const char *deb_file, unsigned int deb_line, const char *name)
#else
int findmod(const char *name)
#endif
{
	int i, j;

	for (i = 0; i < fmodcnt; i++)
		for (j = 0; j < FMNAMESZ + 1; j++) {
			if (fmod_sw[i].f_name[j] != name[j]) 
				break;
			if (name[j] == '\0')
				return i;
		}
	return -ENOENT;
}

/**
 * register_strdev
 *
 * Register a Streams device driver.
 */
#ifdef __KERNEL__
struct streamtab *fstr_sw[MAX_STRDEV] = {NULL,};
int register_strdev(unsigned int major, struct streamtab *strtab, int nminor)
{
	int err;
	int register_term_strdev(unsigned int major, const char *name, int nminor);
	const char *name = strtab->st_rdinit->qi_minfo->mi_idname;

#ifndef MODULE
	streams_init();
#endif

	if(major >= MAX_STRDEV)
		return -ENXIO;
	if (nminor > 0) 
		err = register_term_strdev(major,name,nminor);
	else 
		err = register_chrdev(major, name, &streams_fops);
	if(err < 0)
		return err;
#ifdef MODULE
	MOD_INC_USE_COUNT;
#endif
	if (err == 0)
		err = major;
	fstr_sw[err] = strtab;

	return err;
}

int unregister_strdev (unsigned int major, struct streamtab *strtab, int nminor) {
	int err;
	int unregister_term_strdev(unsigned int major, const char *name, int nminor);
	const char *name = strtab->st_rdinit->qi_minfo->mi_idname;

	if (nminor > 0)
		err = unregister_term_strdev(major,name,nminor);
	else
		err = unregister_chrdev(major, name);
#ifdef MODULE
	if (err >= 0) {
		MOD_DEC_USE_COUNT;
	} else
		printf("Unregister: Driver %s not deleted: %d\n",name,err);
#endif
	return err;
}

#endif

/**
 * register_strmod
 *
 * Register a Streams module.
 */
struct fmodsw fmod_sw[MAX_STRMOD] = {{0,}};
int fmodcnt = MAX_STRMOD;

int register_strmod(struct streamtab *strtab)
{
	int fm;
	struct fmodsw *f;
	const char *name = strtab->st_rdinit->qi_minfo->mi_idname;

	if(findmod(name) >= 0)
		return -EBUSY;

	for(fm=0,f = fmod_sw;fm < fmodcnt;fm++,f++) {
		if(f->f_str == NULL) {
			f->f_str = strtab;
			memcpy(f->f_name,name,FMNAMESZ+1);
#ifdef MODULE
			MOD_INC_USE_COUNT;
#else
			streams_init();
#endif
			return 0;
		}
	}
	return -EBUSY;
}

int unregister_strmod(struct streamtab *strtab)
{
	int fm;
	struct fmodsw *f;
	const char *name = strtab->st_rdinit->qi_minfo->mi_idname;

	if((fm = findmod(name)) < 0)
		return -ENOENT;

	f = fmod_sw+fm;
	if(f->f_str == strtab) {
		f->f_str = NULL;
		memset(f->f_name,0,FMNAMESZ+1);
#ifdef MODULE
		MOD_DEC_USE_COUNT;
#endif
		return 0;
	} else
		printf("Unregister: Module %s not found!\n",name);
	return -ENOENT;
}


#ifdef MODULE
char kernel_version[] = UTS_RELEASE;

int init_module(void) {
	streams_init();
	enable_bh (IMMEDIATE_BH);
	enable_bh (STREAMS_BH);
	return 0;
}

void cleanup_module( void) {
	if (MOD_IN_USE)
		printf(KERN_INFO "Streams: module is in use, remove delayed\n");

	if(streams_inited) {
		if (q_timeout > 1)
			del_timer(&q_later);
	}
}
#endif

