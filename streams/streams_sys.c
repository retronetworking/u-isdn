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
#include "f_module.h"
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
#else
unsigned long bh_mask;
#endif
#include "kernel.h"

#ifdef linux
#ifdef __KERNEL__
#include <linux/interrupt.h>
#endif
#include <linux/syscompat.h>
#ifdef SK_STREAM
#include <linux/skbuff.h>
#endif
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
#ifdef SK_STREAM
int strmsgsz = PAGE_SIZE-sizeof(struct sk_buff);
#else
int strmsgsz = PAGE_SIZE-sizeof(struct datab);
#endif
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
#define qenable(a) deb_qenable(deb_file,deb_line,a)

void traceback(char str) {
#if 0 /* def KERNEL */
#define VMALLOC_OFFSET (8*1024*1024)
#define MODULE_RANGE (8*1024*1024)
#define get_seg_long(seg,addr) ({ \
register unsigned long __res; \
__asm__("push %%fs;mov %%ax,%%fs;movl %%fs:%2,%%eax;pop %%fs" \
	:"=a" (__res):"0" (seg),"m" (*(addr))); \
__res;})

    long *stack = (long *)&str;
    int i = 1;
    long module_start, module_end;
    extern char start_kernel, etext;

    printk("%sStackback of %d in %s! Call Trace: ",KERN_EMERG,str, current->comm);
    module_start = ((high_memory + VMALLOC_OFFSET) & ~(VMALLOC_OFFSET-1));
    module_end = module_start + MODULE_RANGE;
    while (((long) stack & 4095) != 0) {
	long addr = get_seg_long(KERNEL_DS, stack++);
	/*
	 * Stolen from kernel/traps.c.
	 */
	if (((addr >= (unsigned long) &start_kernel) &&
	        (addr <= (unsigned long) &etext)) ||
	        ((addr >= module_start) && (addr <= module_end))) {
	    printk("%lx ", addr);
	    i++;
	}
    }
    printk("\n");
#endif
}

#endif

/*
 * Data message type? 
 */

static inline const int
datamsg(uchar_t type)
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

mblk_t * 
#ifdef CONFIG_DEBUG_STREAMS
deb_allocb(const char *deb_file, unsigned int deb_line, ushort size, ushort pri)
#else
allocb(ushort size, ushort pri)
#endif
{
	mblk_t *p_msg;
#ifdef SK_STREAM
	struct sk_buff *skb;
#else
	dblk_t *p_data;
#endif

#ifdef SK_STREAM
	skb = alloc_skb(size,GFP_ATOMIC);
	if(skb == NULL) {
		printf("%sCouldn't allocate %d bytes (Streams SKB)\n",KERN_WARNING,size);
		return NULL;
	}
#else
#if defined(CONFIG_MALLOC_NAMES) && defined(CONFIG_DEBUG_STREAMS) && defined(__KERNEL__)
	p_data = (struct datab *) deb_kmalloc(deb_file,deb_line,size + sizeof(struct datab), GFP_ATOMIC);
#else
	p_data = (struct datab *) kmalloc(size + sizeof(struct datab), GFP_ATOMIC);
#endif
	if(p_data == NULL) {
		printf("%sCouldn't allocate %d bytes (Streams Data)\n",KERN_WARNING,size+sizeof(struct datab));
		return NULL;
	}
#endif
#if defined(CONFIG_MALLOC_NAMES) && defined(CONFIG_DEBUG_STREAMS) && defined(__KERNEL__)
	p_msg = (struct msgb *)deb_kmalloc(deb_file,deb_line, sizeof(struct msgb), GFP_ATOMIC);
#else
	p_msg = (struct msgb *)kmalloc(sizeof(struct msgb), GFP_ATOMIC);
#endif
	if(p_msg == NULL) {
#ifdef SK_STREAM
		skb->free=1;
		kfree_skb(skb,0);
#else
		kfree_s(p_data,sizeof(struct msgb));
#endif
		printf("%sCouldn't allocate %d bytes (Streams Msg)\n",KERN_WARNING,sizeof(struct msgb));
		return NULL;
	}
	p_msg->b_next = p_msg->b_prev = p_msg->b_cont = NULL;
#ifdef CONFIG_DEBUG_STREAMS
	p_msg->deb_magic = DEB_PMAGIC;
	p_msg->deb_queue = NULL;
	p_msg->deb_file = deb_file;
	p_msg->deb_line = deb_line;
#ifndef SK_STREAM
	p_data->deb_magic = DEB_DMAGIC;
#endif
#endif
#ifdef SK_STREAM
	p_msg->b_skb = skb;
	p_msg->b_rptr = p_msg->b_wptr = skb->data;
	skb->users = 1;
#else
	p_msg->b_datap = p_data;
	p_data->db_base = p_msg->b_rptr = p_msg->b_wptr = (streamchar *)(p_data+1);
	p_data->db_lim = p_data->db_base + size;
	p_data->db_type = M_DATA;
	p_data->db_ref = 1;
#endif

	return p_msg;
}

/**
 * freeb
 *
 * Free a message header, decrement data block refcount, free data block if zero.
 */

void
#ifdef CONFIG_DEBUG_STREAMS
deb_freeb(const char *deb_file, unsigned int deb_line, mblk_t *p_msg)
#else
freeb(mblk_t *p_msg)
#endif
{
#ifdef SK_STREAM
	struct sk_buff *skb;
#else
	dblk_t *p_data;
#endif

	if (p_msg == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		printf("%sFreeing NULL msg at %s:%d\n",KERN_ERR,deb_file,deb_line);
#endif
		return;
	}

#ifdef SK_STREAM
	skb = p_msg->b_skb;
#else
	p_data = p_msg->b_datap;
#endif
#ifdef CONFIG_DEBUG_STREAMS
	if(p_msg->deb_magic != DEB_PMAGIC) 
		panic("Bad_fMagic of %p at %s:%d!\n",p_msg,deb_file,deb_line);
	if(DATA_BLOCK(p_msg) == NULL) 
		panic("Bad_fMagicn of %p at %s:%d!\n",p_msg,deb_file,deb_line);
#ifndef SK_STREAM
	if(p_msg->b_datap->deb_magic != DEB_DMAGIC) 
		panic("Bad_Magicd of %p at %s:%d!\n",p_msg,deb_file,deb_line);
#endif
	(void)deb_msgdsize(deb_file,deb_line, p_msg);
	if(p_msg->deb_queue != NULL) {
		printf("%s:%d freed msg %p:%p from queue %p, put by %s:%d\n",
			deb_file,deb_line,p_msg,
#ifdef SK_STREAM
			skb,
#else
			p_data,
#endif
			p_msg->deb_queue,p_msg->deb_file,p_msg->deb_line);
		return;
	}
#endif

	if(
#ifdef SK_STREAM
			--skb->users <= 0
#else
			--p_data->db_ref <= 0
#endif
		) {
#ifdef SK_STREAM
		skb->free=1;
		kfree_skb(skb,0);
#else
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
#endif /* SK_STREAM */
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
 * Can a block be allocated? We use a very-simple-minded and totally
 * misguided approach, i.e. the block is allocated and then freed. :-/
 *
 * This is rather inefficient and unsafe, but testb() is not widely used anyway.
 */
int
#ifdef CONFIG_DEBUG_STREAMS
deb_testb(const char *deb_file, unsigned int deb_line, ushort size, ushort pri)
#else
testb(ushort size, ushort pri)
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
int
getclass(ushort size)
{
	return 1;
}

/**
 * allocq
 *
 * Allocate a queue pair.
 * The read side gets a QREADR flag so OTHERQ knows what to do.
 */

queue_t *
#ifdef CONFIG_DEBUG_STREAMS
deb_allocq(const char *deb_file, unsigned int deb_line)
#else
allocq(void)
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
void
#ifdef CONFIG_DEBUG_STREAMS
deb_freeq(const char *deb_file, unsigned int deb_line, queue_t *p_queue)
#else
freeq(queue_t *p_queue)
#endif
{
	if(p_queue == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		printf("%sFreeing NULL queue at %s:%d\n",KERN_ERR ,deb_file,deb_line);
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

void
#ifdef CONFIG_DEBUG_STREAMS
deb_freemsg(const char *deb_file, unsigned int deb_line, mblk_t *p_msg)
#else
freemsg(mblk_t *p_msg)
#endif
{
	mblk_t *p_temp;

	if(p_msg == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		printf("%sFreeing NULL msg at %s:%d\n",KERN_ERR ,deb_file,deb_line);
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

mblk_t *
#ifdef CONFIG_DEBUG_STREAMS
deb_dupb(const char *deb_file, unsigned int deb_line, mblk_t *p_msg)
#else
dupb(mblk_t *p_msg)
#endif
{
	mblk_t *p_newmsg;

	if(p_msg == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		printf("%sDup'ing NULL msg at %s:%d\n",KERN_ERR ,deb_file,deb_line);
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
	DATA_REFS(p_newmsg)++;
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

mblk_t *
#ifdef CONFIG_DEBUG_STREAMS
deb_dupmsg(const char *deb_file, unsigned int deb_line, mblk_t *p_msg)
#else
dupmsg(mblk_t *p_msg)
#endif
{
	mblk_t *p_head, *p_newmsg;

	if(p_msg == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		printf("%sDup'ing NULL msg at %s:%d\n",KERN_ERR ,deb_file,deb_line);
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
 * Empty messages aren't copied. Free space at the beginning or end
 * of the block is _not_ preserved.
 */

mblk_t *
#ifdef CONFIG_DEBUG_STREAMS
deb_copyb(const char *deb_file, unsigned int deb_line, mblk_t *p_msg)
#else
copyb(mblk_t *p_msg)
#endif
{
	mblk_t *p_newmsg;
	short msglen;

	if(p_msg == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		printf("%sCopying NULL msg at %s:%d\n",KERN_ERR ,deb_file,deb_line);
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

mblk_t *
#ifdef CONFIG_DEBUG_STREAMS
deb_copymsg(const char *deb_file, unsigned int deb_line, mblk_t *p_msg)
#else
copymsg(mblk_t *p_msg)
#endif
{
	mblk_t *p_head, *p_newmsg;

	if(p_msg == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		printf("%sCopying NULL msg at %s:%d\n",KERN_ERR ,deb_file,deb_line);
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
 * copybufb
 *
 * Copy a message block by allocating new data and moving stuff.
 * Free space is preserved.
 */

mblk_t *
#ifdef CONFIG_DEBUG_STREAMS
deb_copybufb(const char *deb_file, unsigned int deb_line, mblk_t *p_msg)
#else
copybufb(mblk_t *p_msg)
#endif
{
	mblk_t *p_newmsg;
	short msglen;

	if(p_msg == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		printf("%sCopying NULL msg at %s:%d\n",KERN_ERR ,deb_file,deb_line);
#endif
		return NULL;
	}
	if((msglen = DATA_END(p_msg) - DATA_START(p_msg)) <= 0)
		return NULL;
	
	if((p_newmsg = allocb(msglen, BPRI_LO)) == NULL)
		return NULL;
	p_newmsg->b_wptr = DATA_START(p_newmsg) + (p_msg->b_wptr - DATA_START(p_msg));
	p_newmsg->b_rptr = DATA_START(p_newmsg) + (p_msg->b_rptr - DATA_START(p_msg));

	if((msglen = p_msg->b_wptr - p_msg->b_rptr) > 0)
		memcpy(p_newmsg->b_rptr,p_msg->b_rptr,msglen);

	return p_newmsg;
}



/**
 * copybufmsg
 *
 * Copy a message with block alignment. Walk through it with copybufb().
 */

mblk_t *
#ifdef CONFIG_DEBUG_STREAMS
deb_copybufmsg(const char *deb_file, unsigned int deb_line, mblk_t *p_msg)
#else
copybufmsg(mblk_t *p_msg)
#endif
{
	mblk_t *p_head, *p_newmsg;

	if(p_msg == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		printf("%sCopying NULL msg at %s:%d\n",KERN_ERR ,deb_file,deb_line);
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
 * rmvb
 *
 * Remove a message block from a message.
 * The block is not freed.
 *
 * I'm dropping the "return -1 if the block isn't in the message" stuff
 * that SysV Streams does. It's not useful. If you don't know whether the
 * block is in the message, you deserve to lose.
 */

mblk_t *
#ifdef CONFIG_DEBUG_STREAMS
deb_rmvb(const char *deb_file, unsigned int deb_line, mblk_t *p_msg, mblk_t *p_block)
#else
rmvb(mblk_t *p_msg, mblk_t *p_block)
#endif
{
	mblk_t *p_ret = p_msg;

	if(p_msg == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		printf("%sRemoving NULL msga at %s:%d\n",KERN_ERR ,deb_file,deb_line);
#endif
		return NULL;
	}
	if(p_block == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		printf("%sRemoving NULL msgb at %s:%d\n",KERN_ERR ,deb_file,deb_line);
#endif
		return NULL;
	}

	if(p_msg == p_block) 
		p_ret = p_msg->b_cont;
	else do {
		if(p_msg == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
			printf("%sBlock not in message at %s:%d\n",KERN_ERR ,deb_file,deb_line);
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
int
#ifdef CONFIG_DEBUG_STREAMS
deb_pullupmsg(const char *deb_file, unsigned int deb_line, mblk_t *p_msg, short length)
#else
pullupmsg(mblk_t *p_msg, short length)
#endif
{
	mblk_t *p_temp, *p_newmsg;

	if(p_msg == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		printf("%sPullup NULL msg at %s:%d\n",KERN_ERR ,deb_file,deb_line);
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
	
	DATA_TYPE(p_newmsg) = DATA_TYPE(p_msg);
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
 * Actually, this is the same stupidity pullupmsg() engaged in,
 * but I didn't redo adjmsg() because I don't need it...
 */
int
#ifdef CONFIG_DEBUG_STREAMS
deb_adjmsg(const char *deb_file, unsigned int deb_line, mblk_t *p_msg, short length)
#else
adjmsg(mblk_t *p_msg, short length)
#endif
{
	mblk_t *p_temp = NULL; /* shut up GCC */
	int mlen;

	if(p_msg == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		printf("%sAdjust NULL msg at %s:%d\n",KERN_ERR ,deb_file,deb_line);
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

int
#ifdef CONFIG_DEBUG_STREAMS
deb_xmsgsize(const char *deb_file, unsigned int deb_line, mblk_t *p_msg)
#else
xmsgsize(mblk_t *p_msg)
#endif
{
	unsigned char type;
	int bytes = 0;
#ifdef CONFIG_DEBUG_STREAMS
	int segs = 0;
#endif

	if(p_msg == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		printf("%sXMsgSize of NULL msg at %s:%d\n",KERN_ERR ,deb_file,deb_line);
#endif
		return 0;
	}

	type = DATA_TYPE(p_msg);

	do {
		short n;
#ifdef CONFIG_DEBUG_STREAMS
#if defined(CONFIG_MALLOC_NAMES) && defined(__KERNEL__)
		if(deb_kcheck_s(deb_file,deb_line,p_msg,sizeof(*p_msg)))
			return 0;
#endif
		if(p_msg->deb_magic != DEB_PMAGIC) 
			panic("Bad_Magic of %p/%d at %s:%d!\n",p_msg,segs,deb_file,deb_line);
		if(DATA_BLOCK(p_msg) == NULL) 
			panic("Bad_Magicn of %p/%d at %s:%d!\n",p_msg,segs,deb_file,deb_line);
#ifndef SK_STREAM
		if(p_msg->b_datap->deb_magic != DEB_DMAGIC) 
			panic("Bad_Magicd of %p/%d at %s:%d!\n",p_msg,segs,deb_file,deb_line);
#endif
#endif
		if((n = p_msg->b_wptr - p_msg->b_rptr) > 0)
			bytes += n;
		segs++;
		if ((p_msg = p_msg->b_cont) == NULL)
			break;
	} while (type == DATA_TYPE(p_msg));

	return bytes;
}


/**
 * msgdsize
 *
 * Count size of data blocks.
 */
int
#ifdef CONFIG_DEBUG_STREAMS
deb_msgdsize(const char *deb_file, unsigned int deb_line, mblk_t *p_msg)
#else
msgdsize(mblk_t *p_msg)
#endif
{
	int bytes = 0;
#ifdef CONFIG_DEBUG_STREAMS
	int segs = 0;
#endif

	if(p_msg == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		printf("%sMsgDSize of NULL msg at %s:%d\n",KERN_ERR ,deb_file,deb_line);
#endif
		traceback(0);
		return -EFAULT;
	}
	while(p_msg != NULL) {
#ifdef CONFIG_DEBUG_STREAMS
#if defined(CONFIG_MALLOC_NAMES) && defined(__KERNEL__)
		{
			int err;
			if((err = deb_kcheck_s(deb_file,deb_line,p_msg,sizeof(*p_msg))))
				return err;
#ifndef SK_STREAM
			if((err = deb_kcheck(deb_file,deb_line+20000,p_msg->b_datap)))
				return err;
#endif
		}
#endif
		if(p_msg->deb_magic != DEB_PMAGIC) {
			printf("Bad.Magic of %p/%d at %s:%d!\n",p_msg,segs,deb_file,deb_line);
			traceback(1);
			return -EFAULT;
		}
		if(DATA_BLOCK(p_msg) == NULL) {
			printf("Bad.Magicn of %p/%d at %s:%d!\n",p_msg,segs,deb_file,deb_line);
			traceback(2);
			return -EFAULT;
		}
#ifndef SK_STREAM
		if(p_msg->b_datap->deb_magic != DEB_DMAGIC) {
			printf("Bad.Magicd of %p/%d at %s:%d!\n",p_msg,segs,deb_file,deb_line);
			traceback(3);
			return -EFAULT;
		}
#endif
#endif
		if (DATA_TYPE(p_msg) == M_DATA) {
			short n;
			if((n = p_msg->b_wptr - p_msg->b_rptr) > 0)
				bytes += n;
		}
		p_msg = p_msg->b_cont;
		segs++;
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
static void
deb_rmv_post(const char *deb_file, unsigned int deb_line, queue_t *p_queue, mblk_t *p_msg)
#else
static inline void
rmv_post(queue_t *p_queue, mblk_t *p_msg)
#endif
{
	mblk_t *p_tmp = p_msg;
	unsigned long s = splstr();

#if defined(CONFIG_DEBUG_STREAMS) && defined(CONFIG_MALLOC_NAMES) && defined(__KERNEL__)
	deb_kcheck_s(deb_file,deb_line, RDQ(p_queue),2*sizeof(*p_queue));
	deb_kcheck_s(deb_file,deb_line,p_msg,sizeof(*p_msg));
#ifndef SK_STREAM
	deb_kcheck(deb_file,deb_line,p_msg->b_datap);
#endif
	if(p_msg->b_cont != NULL) {
		deb_kcheck_s(deb_file,deb_line,p_msg->b_cont,sizeof(*p_msg));
#ifndef SK_STREAM
		deb_kcheck(deb_file,deb_line,p_msg->b_cont->b_datap);
#endif
	}
#endif

#ifdef CONFIG_DEBUG_STREAMS
	if(p_msg->deb_magic != DEB_PMAGIC) 
		panic("Bad,Magic of %p at %s:%d!\n",
			p_msg,deb_file,deb_line);
	if(DATA_BLOCK(p_msg) == NULL) 
		panic("Bad,Magicn of %p at %s:%d!\n",
			p_msg,deb_file,deb_line);
#ifndef SK_STREAM
	if(p_msg->b_datap->deb_magic != DEB_DMAGIC) 
		panic("Bad,Magicd of %p at %s:%d!\n",
			p_msg,deb_file,deb_line);
#endif
	if(p_msg->deb_queue != p_queue) 
		panic("rmv_post for msg %p, queue %p, by %s:%d; msg is on %p by %s:%d\n",
			p_msg,p_queue,deb_file,deb_line,p_msg->deb_queue,p_msg->deb_file,p_msg->deb_line);
#endif

	do {
#ifdef SK_STREAM
		struct sk_buff *skb = p_tmp->b_skb;
		p_queue->q_count -= skb->tail - skb->head;
#else
		dblk_t *p_data = p_tmp->b_datap;
		p_queue->q_count -= p_data->db_lim - p_data->db_base;
#endif
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
static void
deb_put_post(const char *deb_file, unsigned int deb_line, queue_t *p_queue, mblk_t *p_msg)
#else
static inline void
put_post(queue_t *p_queue, mblk_t *p_msg)
#endif
{
	mblk_t *p_tmp = p_msg;
	unsigned long s = splstr();

#if defined(CONFIG_DEBUG_STREAMS) && defined(CONFIG_MALLOC_NAMES) && defined(__KERNEL__)
	deb_kcheck_s(deb_file,deb_line, RDQ(p_queue),2*sizeof(*p_queue));
	deb_kcheck_s(deb_file,deb_line,p_msg,sizeof(*p_msg));
#ifndef SK_STREAM
	deb_kcheck(deb_file,deb_line,p_msg->b_datap);
#endif
	if(p_msg->b_cont != NULL) {
		deb_kcheck_s(deb_file,deb_line,p_msg->b_cont,sizeof(*p_msg));
#ifndef SK_STREAM
		deb_kcheck(deb_file,deb_line,p_msg->b_cont->b_datap);
#endif
	}
#endif

#ifdef CONFIG_DEBUG_STREAMS
	if(p_msg->deb_magic != DEB_PMAGIC) 
		panic("Bad:Magic of %p at %s:%d!\n",
			p_msg,deb_file,deb_line);
	if(DATA_BLOCK(p_msg) == NULL) 
		panic("Bad:Magicn of %p at %s:%d!\n",
			p_msg,deb_file,deb_line);
#ifndef SK_STREAM
	if(p_msg->b_datap->deb_magic != DEB_DMAGIC) 
		panic("Bad:Magicd of %p at %s:%d!\n",
			p_msg,deb_file,deb_line);
#endif
	if(p_msg->deb_queue != NULL) 
		panic("put_post for msg %p, queue %p, by %s:%d; msg is on %p by %s:%d\n",
			p_msg,p_queue,deb_file,deb_line,p_msg->deb_queue,p_msg->deb_file,p_msg->deb_line);
#endif
	do {
#ifdef SK_STREAM
		struct sk_buff *skb = p_tmp->b_skb;
		p_queue->q_count += skb->tail - skb->head;
#else
		dblk_t *p_data = p_tmp->b_datap;
		p_queue->q_count += p_data->db_lim - p_data->db_base;
#endif
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
void
deb_get_post(const char *deb_file, unsigned int deb_line, queue_t *p_queue)
#else
inline void
get_post(queue_t *p_queue)
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

mblk_t *
#ifdef CONFIG_DEBUG_STREAMS
deb_getq(const char *deb_file, unsigned int deb_line, queue_t *p_queue)
#else
getq(queue_t *p_queue)
#endif
{
	mblk_t *p_msg;
	unsigned long s;

	if (p_queue == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		printf("%sGet from NULL queue at %s:%d\n",KERN_ERR ,deb_file,deb_line);
#endif
		return NULL;
	}

	s = splstr();
#if defined(CONFIG_DEBUG_STREAMS) && defined(CONFIG_MALLOC_NAMES) && defined(__KERNEL__)
	deb_kcheck_s(deb_file,deb_line, RDQ(p_queue),2*sizeof(*p_queue));
#endif
if(0)printf("%sG %s:%d  ",KERN_ERR ,deb_file,deb_line);
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
			printf("%sGetQ NULL stream/rptr at %s:%d, last at %s:%d\n",KERN_ERR ,deb_file,deb_line,p_msg->deb_file,p_msg->deb_line);
			freemsg(p_msg);
			p_msg = NULL;
		}
		if (p_msg->b_wptr == NULL) {
			printf("%sGetQ NULL stream/wptr at %s:%d, last at %s:%d\n",KERN_ERR ,deb_file,deb_line,p_msg->deb_file,p_msg->deb_line);
			freemsg(p_msg);
			p_msg = NULL;
		}
		if (p_msg->b_datap == NULL) {
			printf("%sGetQ NULL stream/datap at %s:%d, last at %s:%d\n",KERN_ERR ,deb_file,deb_line,p_msg->deb_file,p_msg->deb_line);
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
#ifndef SK_STREAM
			deb_kcheck(deb_file,deb_line,p_msg->b_datap);
#endif
			xm = xm->b_cont;
		}
	}
#endif
#ifdef CONFIG_DEBUG_STREAMS
	if((p_msg != NULL) && (deb_msgdsize(deb_file,deb_line,p_msg) < 0))
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

void
#ifdef CONFIG_DEBUG_STREAMS
deb_rmvq(const char *deb_file, unsigned int deb_line, queue_t *p_queue, mblk_t *p_msg)
#else
rmvq(queue_t *p_queue, mblk_t *p_msg)
#endif
{ 
	unsigned long s;

	if (p_msg == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		printf("%sRmv NULL msg from queue at %s:%d\n",KERN_ERR ,deb_file,deb_line);
#endif
		return ;
	}
	if (p_queue == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		printf("%sRmv msg from NULL queue at %s:%d\n",KERN_ERR ,deb_file,deb_line);
#endif
		return ;
	}

#ifdef CONFIG_DEBUG_STREAMS
	if(p_msg->deb_magic != DEB_PMAGIC) 
		panic("Bad'Magic of %p at %s:%d!\n",
			p_msg,deb_file,deb_line);
	if(DATA_BLOCK(p_msg) == NULL) 
		panic("Bad'Magicn of %p at %s:%d!\n",
			p_msg,deb_file,deb_line);
#ifndef SK_STREAM
	if(p_msg->b_datap->deb_magic != DEB_DMAGIC) 
		panic("Bad'Magicd of %p at %s:%d!\n",
			p_msg,deb_file,deb_line);
#endif

	if(p_msg->deb_queue != p_queue) 
		panic("rmvq for msg %p, queue %p, by %s:%d; msg is on %p by %s:%d\n",
			p_msg,p_queue,deb_file,deb_line,p_msg->deb_queue,p_msg->deb_file,p_msg->deb_line);
#endif
#if defined(CONFIG_DEBUG_STREAMS) && defined(CONFIG_MALLOC_NAMES) && defined(__KERNEL__)
	deb_kcheck_s(deb_file,deb_line, RDQ(p_queue),2*sizeof(*p_queue));
	deb_kcheck_s(deb_file,deb_line,p_msg,sizeof(*p_msg));
#ifndef SK_STREAM
	deb_kcheck(deb_file,deb_line,p_msg->b_datap);
#endif
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

void
#ifdef CONFIG_DEBUG_STREAMS
deb_flushq(const char *deb_file, unsigned int deb_line, queue_t *p_queue, int flag)
#else
flushq(queue_t *p_queue, int flag)
#endif
{
	mblk_t *p_msg, *p_next;
	unsigned long s;
	int q_flag;

	if (p_queue == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		printf("%sFlush NULL queue at %s:%d\n",KERN_ERR ,deb_file,deb_line);
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
#ifndef SK_STREAM
		deb_kcheck(deb_file,deb_line,p_msg->b_datap);
#endif
#endif
#ifdef CONFIG_DEBUG_STREAMS
		if(p_msg->deb_queue == p_queue)
			p_msg->deb_queue = NULL;
#endif
		if (!flag && !datamsg(DATA_TYPE(p_msg)))
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

int
#ifdef CONFIG_DEBUG_STREAMS
deb_canput(const char *deb_file, unsigned int deb_line, queue_t *p_queue)
#else
canput(queue_t *p_queue)
#endif
{
	if (p_queue == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		printf("%sQueue NULL in canput at %s:%d\n",KERN_ERR ,deb_file,deb_line);
#endif
		return 0;
	}
if(0)printf("%sP %s:%d  ",KERN_ERR ,deb_file,deb_line);
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

void
#ifdef CONFIG_DEBUG_STREAMS
deb_putq(const char *deb_file, unsigned int deb_line, queue_t *p_queue, mblk_t *p_msg)
#else
putq(queue_t *p_queue, mblk_t *p_msg)
#endif
{
	unsigned long s;
	uchar_t msg_class = queclass(p_msg);

	if(p_msg == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		printf("%sPutting NULL msg at %s:%d\n",KERN_ERR ,deb_file,deb_line);
#endif
		return;
	}
	if(p_queue == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		printf("%sPutting on NULL stream at %s:%d\n",KERN_ERR ,deb_file,deb_line);
#endif
		freemsg(p_msg);
		return;
	}
#ifdef CONFIG_DEBUG_STREAMS
	if (p_msg->b_rptr == NULL) {
		printf("%sPutQ NULL stream/rptr at %s:%d, last at %s:%d\n",KERN_ERR ,deb_file,deb_line,p_msg->deb_file,p_msg->deb_line);
		freemsg(p_msg);
		return;
	}
	if (p_msg->b_wptr == NULL) {
		printf("%sPutQ NULL stream/wptr at %s:%d, last at %s:%d\n",KERN_ERR ,deb_file,deb_line,p_msg->deb_file,p_msg->deb_line);
		freemsg(p_msg);
		return;
	}
	if (p_msg->b_datap == NULL) {
		printf("%sPutQ NULL stream/datap at %s:%d, last at %s:%d\n",KERN_ERR ,deb_file,deb_line,p_msg->deb_file,p_msg->deb_line);
		freemsg(p_msg);
		return;
	}
#endif

#if defined(CONFIG_DEBUG_STREAMS) && defined(CONFIG_MALLOC_NAMES) && defined(__KERNEL__)
	deb_kcheck_s(deb_file,deb_line, RDQ(p_queue),2*sizeof(*p_queue));
	deb_kcheck_s(deb_file,deb_line,p_msg,sizeof(*p_msg));
#ifndef SK_STREAM
	deb_kcheck(deb_file,deb_line,p_msg->b_datap);
#endif
#endif
if(0)printf("%sP %s:%d  ",KERN_ERR ,deb_file,deb_line);
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

void
#ifdef CONFIG_DEBUG_STREAMS
deb_putbq(const char *deb_file, unsigned int deb_line, queue_t *p_queue, mblk_t *p_msg)
#else
putbq(queue_t *p_queue, mblk_t *p_msg)
#endif
{
	unsigned long s;
	uchar_t msg_class = queclass(p_msg);

	if (p_msg == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		printf("%sPutBack msg to NULL queue at %s:%d\n",KERN_ERR ,deb_file,deb_line);
#endif
		return ;
	}
	if (p_queue == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		printf("%sPutBack NULL msg to queue at %s:%d\n",KERN_ERR ,deb_file,deb_line);
#endif
		freemsg(p_msg);
		return ;
	}
#ifdef CONFIG_DEBUG_STREAMS
	if (p_msg->b_rptr == NULL) {
		printf("%sPutBQ NULL stream/rptr at %s:%d, last at %s:%d\n",KERN_ERR ,deb_file,deb_line,p_msg->deb_file,p_msg->deb_line);
		freemsg(p_msg);
		return;
	}
	if (p_msg->b_wptr == NULL) {
		printf("%sPutBQ NULL stream/wptr at %s:%d, last at %s:%d\n",KERN_ERR ,deb_file,deb_line,p_msg->deb_file,p_msg->deb_line);
		freemsg(p_msg);
		return;
	}
	if (p_msg->b_datap == NULL) {
		printf("%sPutBQ NULL stream/datap at %s:%d, last at %s:%d\n",KERN_ERR ,deb_file,deb_line,p_msg->deb_file,p_msg->deb_line);
		freemsg(p_msg);
		return;
	}
#endif

#if defined(CONFIG_DEBUG_STREAMS) && defined(CONFIG_MALLOC_NAMES) && defined(__KERNEL__)
	deb_kcheck_s(deb_file,deb_line, RDQ(p_queue),2*sizeof(*p_queue));
	deb_kcheck_s(deb_file,deb_line,p_msg,sizeof(*p_msg));
#ifndef SK_STREAM
	deb_kcheck(deb_file,deb_line,p_msg->b_datap);
#endif
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

void
#ifdef CONFIG_DEBUG_STREAMS
deb_insq(const char *deb_file, unsigned int deb_line, queue_t *p_queue, mblk_t *p_oldmsg, mblk_t *p_msg)
#else
insq(queue_t *p_queue, mblk_t *p_oldmsg, mblk_t *p_msg)
#endif
{
	unsigned long s;

	if (p_msg == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		printf("%sInsQ msg to NULL queue at %s:%d\n",KERN_ERR ,deb_file,deb_line);
#endif
		return ;
	}
	if (p_queue == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		printf("%sInsQ NULL msg to queue at %s:%d\n",KERN_ERR ,deb_file,deb_line);
#endif
		freemsg(p_msg);
		return ;
	}
#if defined(CONFIG_DEBUG_STREAMS) && defined(CONFIG_MALLOC_NAMES) && defined(__KERNEL__)
	deb_kcheck_s(deb_file,deb_line, RDQ(p_queue),2*sizeof(*p_queue));
	deb_kcheck_s(deb_file,deb_line,p_msg,sizeof(*p_msg));
#ifndef SK_STREAM
	deb_kcheck(deb_file,deb_line,p_msg->b_datap);
#endif
	if(p_oldmsg != NULL) {
		deb_kcheck_s(deb_file,deb_line,p_oldmsg,sizeof(*p_oldmsg));
#ifndef SK_STREAM
		deb_kcheck(deb_file,deb_line,p_oldmsg->b_datap);
#endif
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

void
#ifdef CONFIG_DEBUG_STREAMS
deb_appq(const char *deb_file, unsigned int deb_line, queue_t *p_queue, mblk_t *p_oldmsg, mblk_t *p_msg)
#else
appq(queue_t *p_queue, mblk_t *p_oldmsg, mblk_t *p_msg)
#endif
{
	unsigned long s;

	if (p_msg == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		printf("%sAppQ msg to NULL queue at %s:%d\n",KERN_ERR ,deb_file,deb_line);
#endif
		return ;
	}
	if (p_queue == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		printf("%sAppQ NULL msg to queue at %s:%d\n",KERN_ERR ,deb_file,deb_line);
#endif
		freemsg(p_msg);
		return ;
	}
#ifdef CONFIG_DEBUG_STREAMS
	if (p_msg->b_rptr == NULL) {
		printf("%sAppQ NULL stream/rptr at %s:%d, last at %s:%d\n",KERN_ERR ,deb_file,deb_line,p_msg->deb_file,p_msg->deb_line);
		freemsg(p_msg);
		return;
	}
	if (p_msg->b_wptr == NULL) {
		printf("%sAppQ NULL stream/wptr at %s:%d, last at %s:%d\n",KERN_ERR ,deb_file,deb_line,p_msg->deb_file,p_msg->deb_line);
		freemsg(p_msg);
		return;
	}
	if (p_msg->b_datap == NULL) {
		printf("%sAppQ NULL stream/datap at %s:%d, last at %s:%d\n",KERN_ERR ,deb_file,deb_line,p_msg->deb_file,p_msg->deb_line);
		freemsg(p_msg);
		return;
	}
#endif
#if defined(CONFIG_DEBUG_STREAMS) && defined(CONFIG_MALLOC_NAMES) && defined(__KERNEL__)
	deb_kcheck_s(deb_file,deb_line, RDQ(p_queue),2*sizeof(*p_queue));
	deb_kcheck_s(deb_file,deb_line,p_msg,sizeof(*p_msg));
#ifndef SK_STREAM
	deb_kcheck(deb_file,deb_line,p_msg->b_datap);
#endif
	if(p_oldmsg != NULL) {
		deb_kcheck_s(deb_file,deb_line,p_oldmsg,sizeof(*p_oldmsg));
#ifndef SK_STREAM
		deb_kcheck(deb_file,deb_line,p_oldmsg->b_datap);
#endif
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
int
#ifdef CONFIG_DEBUG_STREAMS
deb_putctl(const char *deb_file, unsigned int deb_line, queue_t *p_queue, uchar_t type)
#else
putctl(queue_t *p_queue, uchar_t type)
#endif
{
	mblk_t *p_msg;

	if ((p_msg = allocb(0, BPRI_HI)) == NULL)
		return 0;
	DATA_TYPE(p_msg) = type;
	(*p_queue->q_qinfo->qi_putp)(p_queue, p_msg);
	return 1;
}



/**
 * putctl1
 *
 * Create a one-byte control message and put it onto a queue.
 */
int
#ifdef CONFIG_DEBUG_STREAMS
deb_putctl1(const char *deb_file, unsigned int deb_line, queue_t *p_queue, uchar_t type, streamchar param)
#else
putctl1(queue_t *p_queue, uchar_t type, streamchar param)
#endif
{
	mblk_t *p_msg;

	if ((p_msg = allocb(1, BPRI_HI)) == NULL)
		return 0;
	DATA_TYPE(p_msg) = type;
	*p_msg->b_wptr++ = param;
	(*p_queue->q_qinfo->qi_putp)(p_queue, p_msg);
	return 1;
}



/**
 * backq
 * 
 * Return the queue which feeds this one.
 */

queue_t
#ifdef CONFIG_DEBUG_STREAMS
*deb_backq(const char *deb_file, unsigned int deb_line, queue_t *p_queue)
#else
*backq(queue_t *p_queue)
#endif
{
	if(p_queue == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		printf("%sBackQ from NULL queue at %s:%d\n",KERN_ERR ,deb_file,deb_line);
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

void
#ifdef CONFIG_DEBUG_STREAMS
deb_qreply(const char *deb_file, unsigned int deb_line, queue_t *p_queue, mblk_t *p_msg)
#else
qreply(queue_t *p_queue, mblk_t *p_msg)
#endif
{
	if(p_msg == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		printf("%sReplying with NULL msg at %s:%d\n",KERN_ERR ,deb_file,deb_line);
#endif
		return;
	}
	if(p_queue == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		printf("%sReplying on NULL stream at %s:%d\n",KERN_ERR ,deb_file,deb_line);
#endif
		freemsg(p_msg);
		return;
	}
#ifdef CONFIG_DEBUG_STREAMS
	if (p_msg->b_rptr == NULL) {
		printf("%sQReply NULL stream/rptr at %s:%d, last at %s:%d\n",KERN_ERR ,deb_file,deb_line,p_msg->deb_file,p_msg->deb_line);
		freemsg(p_msg);
		return;
	}
	if (p_msg->b_wptr == NULL) {
		printf("%sQReply NULL stream/wptr at %s:%d, last at %s:%d\n",KERN_ERR ,deb_file,deb_line,p_msg->deb_file,p_msg->deb_line);
		freemsg(p_msg);
		return;
	}
	if (p_msg->b_datap == NULL) {
		printf("%sQReply NULL stream/datap at %s:%d, last at %s:%d\n",KERN_ERR ,deb_file,deb_line,p_msg->deb_file,p_msg->deb_line);
		freemsg(p_msg);
		return;
	}
#endif

#if defined(CONFIG_DEBUG_STREAMS) && defined(CONFIG_MALLOC_NAMES) && defined(__KERNEL__)
	deb_kcheck_s(deb_file,deb_line, RDQ(p_queue),2*sizeof(*p_queue));
	deb_kcheck_s(deb_file,deb_line,p_msg,sizeof(*p_msg));
#ifndef SK_STREAM
	deb_kcheck(deb_file,deb_line,p_msg->b_datap);
#endif
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
int
#ifdef CONFIG_DEBUG_STREAMS
deb_qsize(const char *deb_file, unsigned int deb_line, queue_t *p_queue)
#else
qsize(queue_t *p_queue)
#endif
{
	int msgs = 0;
	mblk_t *p_msg;

	if(p_queue == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		printf("%sSizeQ on NULL queue at %s:%d\n",KERN_ERR ,deb_file,deb_line);
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
void
#ifdef CONFIG_DEBUG_STREAMS
deb_setq (const char *deb_file, unsigned int deb_line, queue_t * p_queue, struct qinit *read_init, struct qinit *write_init)
#else
setq (queue_t * p_queue, struct qinit *read_init, struct qinit *write_init)
#endif
{
	if(p_queue == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		printf("%sSetQ on NULL queue at %s:%d\n",KERN_ERR ,deb_file,deb_line);
#endif
		return;
	}
#if 0 /* def CONFIG_DEBUG_STREAMS */
	printf("%ssetq: Queue %p, read_init %p, write_init %p",KERN_DEBUG ,p_queue,read_init,write_init);
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

void
#ifdef CONFIG_DEBUG_STREAMS
deb_qdetach (const char *deb_file, unsigned int deb_line, queue_t * p_queue, int do_close, int flag)
#else
qdetach (queue_t * p_queue, int do_close, int flag)
#endif
{
	unsigned long s;

	if(p_queue == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		printf("%sDetach on NULL queue at %s:%d\n",KERN_ERR ,deb_file,deb_line);
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
		printf("%sClosing %s\n",KERN_DEBUG , p_queue->q_qinfo->qi_minfo->mi_idname);
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

int
#ifdef CONFIG_DEBUG_STREAMS
deb_qattach (const char *deb_file, unsigned int deb_line, struct streamtab *qinfo, queue_t * p_queue, dev_t dev, int flag)
#else
qattach (struct streamtab *qinfo, queue_t * p_queue, dev_t dev, int flag)
#endif
{
	queue_t *p_newqueue;
	unsigned long s;
	int open_mode;
	int err;

	if(p_queue == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		printf("%sAttach on NULL queue at %s:%d\n",KERN_ERR ,deb_file,deb_line);
#endif
		return -EIO;
	}
	if ((p_newqueue = allocq ()) == NULL)
		return -ENOMEM;
#if 0 /* def CONFIG_DEBUG_STREAMS */
	printf("%sqattach: info %p, oldq %p, newq %p\n",KERN_DEBUG ,qinfo,p_queue,p_newqueue);
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
	printf("%sCallOpen %p %p %p\n",KERN_DEBUG ,p_newqueue,p_newqueue->q_qinfo,p_newqueue->q_qinfo->qi_qopen);
#endif
	if ((err = (*p_newqueue->q_qinfo->qi_qopen) (p_newqueue, dev, flag, open_mode)) < 0) {
printf("%sCallOpen %s got %d\n",KERN_DEBUG ,p_newqueue->q_qinfo->qi_minfo->mi_idname,err);
		qdetach (p_newqueue, 0, 0);
		splx (s);
		return err;
	}
printf("%sDriver %s opened\n",KERN_DEBUG ,p_newqueue->q_qinfo->qi_minfo->mi_idname);
	splx (s);
	return 0;
}
/**
 * qenable
 *
 * Schedule a queue.
 */

void 
#ifdef CONFIG_DEBUG_STREAMS
deb_qenable(const char *deb_file, unsigned int deb_line, queue_t *p_queue)
#else
qenable(queue_t *p_queue)
#endif
{
	unsigned long s;

	if(p_queue == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		printf("%sQEnable on NULL queue at %s:%d\n",KERN_ERR ,deb_file,deb_line);
#endif
		return ;
	}
	if (p_queue->q_qinfo->qi_srvp == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		if(p_queue->q_next || !(p_queue->q_flag & QREADR)) /* not Stream head? */
			printf("%sQEnable on queue for %s with NULL service proc at %s:%d\n",KERN_ERR ,p_queue->q_qinfo->qi_minfo->mi_idname, deb_file,deb_line);
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
		printf("%sQErr Queue not in list, from %s:%d\n",KERN_EMERG ,deb_file,deb_line);
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
		printf("%sQErr Sched_First != NULL; _Last == NULL ; at %s:%d\n",KERN_EMERG ,deb_file,deb_line);
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

void
qretry(queue_t *p_queue)
{
	long s = splstr();
	if(!(p_queue->q_flag & QRETRY)) {
		p_queue->q_flag |= QRETRY;
#ifdef NEW_TIMEOUT
		p_queue->q_retry =
#endif
		timeout((void *)do_qretry,p_queue,HZ/3);
	}
	splx(s);
}


/**
 * runqueues
 *
 * Service the queues; now if possible, soon if not.
 *
 */

void
#ifdef CONFIG_DEBUG_STREAMS
deb_runqueues(const char *deb_file, unsigned int deb_line)
#else
runqueues(void)
#endif
{
	if(sched_first != NULL) {
#ifdef __KERNEL__
		if (q_timeout == 0) {
			queue_task(&q_immed, &tq_immediate);
			mark_bh(IMMEDIATE_BH);	/* Later */
		}
#else /* !KERNEL */
		static void do_runqueues(void*);
		static int isrunning = 0;

		if(isrunning++) {
			--isrunning;
			return;
		}
		do_runqueues(NULL);
		--isrunning;
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

static void
do_runqueues(void *dummy)
{
	queue_t *p_queue;
	unsigned long s;
	int cnt = 100;
	static int looping = 0;

	s = splstr();
	while ((p_queue = (queue_t *)sched_first) != NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		if((p_queue->q_link == NULL) != (p_queue == sched_last)) {
			printf("%sEnd of Queue bad; link %p, last %p\n",KERN_ERR ,
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
			printf("%sQueue on list but QENAB not set for %s\n",KERN_ERR ,p_queue->q_qinfo->qi_minfo->mi_idname);
#if defined(__KERNEL__) && defined(linux)
			sysdump(NULL,NULL,0);
#endif
		}
#endif
		p_queue->q_flag &= ~QENAB;
		splx(s);
#if 0 /* def CONFIG_DEBUG_STREAMS */
		printf("%c:%s ",(p_queue->q_flag & QREADR ? 'R':'W'), p_queue->q_qinfo->qi_minfo->mi_idname);
#endif
		if (p_queue->q_qinfo->qi_srvp) 
			(*p_queue->q_qinfo->qi_srvp)(p_queue);
		(void)splstr();
		if(!--cnt) {
			if(!looping)
				printf("%sStreams loop?\n",KERN_WARNING);
			looping++; looping++;
			break;
		}
	}
	if(looping) looping--;
#if 0 /* def CONFIG_DEBUG_STREAMS */
	if(cnt < 100)
		printf("\n");
#endif
	splx(s);
}

/**
 * splstr 
 *
 */
#ifdef linux
#ifdef CONFIG_DEBUG_STREAMS

const char *str__file; unsigned long str__line;

int
deb_splstr(const char * deb_file, unsigned int deb_line)
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

static void
streams_init(void)
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
static void
q_run(void *dummy)
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

#ifdef CONFIG_DEBUG_STREAMS

void
putnext(queue_t *p_queue, mblk_t *p_msg)
{
	(*p_queue->q_next->q_qinfo->qi_putp)(p_queue->q_next, p_msg);
}

/**
 * linkb
 *
 * Link two messages.
 * This procedure is misnamed and should be called linkmsg().
 */

void
linkb(mblk_t *p_msg1, mblk_t *p_msg2)
{
    if(p_msg1 == NULL || p_msg2 == NULL)
        return;

    while(p_msg1->b_cont != NULL)
        p_msg1 = p_msg1->b_cont;

    p_msg1->b_cont = p_msg2;
}

/**
 * unlinkb
 *
 * Unlink a message block from the head of a message.
 *
 * Some people think this code is actually useful...
 */

mblk_t *
unlinkb(mblk_t *p_msg)
{
    mblk_t *p_nextmsg;

    if(p_msg == NULL)
        return NULL;

    p_nextmsg = p_msg->b_cont;
    p_msg->b_cont = NULL;
    return p_nextmsg;
}

#endif /* CONFIG_DEBUG_STREAMS */


/**
 * findmod
 *
 * Find a Streams module by name.
 */
int
#ifdef CONFIG_DEBUG_STREAMS
deb_findmod(const char *deb_file, unsigned int deb_line, const char *name)
#else
findmod(const char *name)
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

int
register_strdev(unsigned int major, struct streamtab *strtab, int nminor)
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
	MORE_USE;
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
		LESS_USE;
	} else
		printf("%sUnregister: Driver %s not deleted: %d\n",KERN_WARNING ,name,err);
#endif
	return err;
}

#endif

/**
 * register_strmod
 *
 * Register a Streams module.
 */
struct fmodsw fmod_sw[MAX_STRMOD] = {{{0}}};
int fmodcnt = MAX_STRMOD;

int
register_strmod(struct streamtab *strtab)
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
			MORE_USE;
#else
			streams_init();
#endif
			return 0;
		}
	}
	return -EBUSY;
}

int
unregister_strmod(struct streamtab *strtab)
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
		LESS_USE;
#endif
		return 0;
	} else
		printf("Unregister: Module %s not found!\n",name);
	return -ENOENT;
}


#ifdef MODULE

static int do_init_module(void) {
	streams_init();
	enable_bh (IMMEDIATE_BH);
	enable_bh (STREAMS_BH);
	return 0;
}

static int do_exit_module( void) {
	if(streams_inited) {
		if (q_timeout > 0)
			del_timer(&q_later);
	}
	return 0;
}
#endif

