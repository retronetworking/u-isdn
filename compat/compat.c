#define COMPAT_C

/*
 * Compatibility stuff for Streams-ish drivers and other stuff.
 *
 * Copyright 1994: Matthias Urlichs <urlichs@smurf.noris.de>
 */
#include "f_module.h"
#include "kernel.h"
#include <linux/delay.h>
#include <asm/delay.h>

/*
 * compat.c
 *
 * provides some compatibility code for stuff written for non-Linux kernels
 *
 * Version 0.3 by Matthias Urlichs <urlichs@smurf.noris.de>
 */
#include <linux/config.h>
#include <linux/types.h>
#include <linux/timer.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/malloc.h>
#include <asm/system.h>

#include "compat.h"

unsigned long block_mask = 0;

/*
 * Standard Unix-kernel timeout code. Two versions -- the new version
 * returns a pointer to its internal data, the old version doesn't, which
 * means we have to scan for the thing in an internal list when
 * deallocating the timer.
 */
struct timing {
	struct timer_list tim;
	void (*proc)(void *);
	void *arg;
};

struct timing_old {
	struct timing tim;
	struct timing_old *prev;
	struct timing_old *next;
};
static struct timing_old *start = NULL;

static void
dotimer(void *arg)
{
	struct timing *tim = (struct timing *)arg;

	(*tim->proc)(tim->arg);

	sti();
#ifdef CONFIG_MALLOC_NAMES
	deb_kfree(tim,__FILE__,__LINE__);
#else
	kfree(tim);
#endif

#ifdef MODULE
	MOD_DEC_USE_COUNT;
#endif
}

static void
droptimer_old(void (*func)(void *), void *arg, char del)
{
	struct timing_old *tim;
	unsigned long flags;
	save_flags(flags);
	cli();
	for(tim = start; tim != NULL; tim = tim->next) {
		if(tim->tim.proc == func && tim->tim.arg == arg) {
			if(tim->next != NULL)
				tim->next->prev = tim->prev;
			if(tim->prev != NULL)
				tim->prev->next = tim->next;
			else
				start = tim->next;
			restore_flags(flags);
			if(del) {
				del_timer(&tim->tim.tim);
#ifdef CONFIG_MALLOC_NAMES
				deb_kfree(tim,__FILE__,__LINE__);
#else
				kfree(tim);
#endif
			}
#ifdef MODULE
			MOD_DEC_USE_COUNT;
#endif
			return;
		}
	}

	restore_flags(flags);
	printk("Timer %p:%p not found\n", func,arg);
}

static void
dotimer_old(void *arg)
{
	struct timing_old *tim = (struct timing_old *)arg;

	droptimer_old(tim->tim.proc,tim->tim.arg,0);
	(*tim->tim.proc)(tim->tim.arg);

	sti();
#ifdef CONFIG_MALLOC_NAMES
	deb_kfree(tim,__FILE__,__LINE__);
#else
	kfree(tim);
#endif

}

int
#ifdef CONFIG_MALLOC_NAMES
deb_timeout(const char *deb_file, unsigned int deb_line, void (*func)(void *), void *arg, int expire)
#else
timeout(void (*func)(void *), void *arg, int expire)
#endif
{
	typedef void (*fct)(unsigned long);
	struct timing *timer;

#ifdef CONFIG_MALLOC_NAMES
	timer = (struct timing *)deb_kmalloc(sizeof(struct timing), GFP_ATOMIC,deb_file,deb_line);
#else
	timer = (struct timing *)kmalloc(sizeof(struct timing), GFP_ATOMIC);
#endif
	if(timer == NULL) {
		printf(" *!* No Timeout!\n");
		return (int)NULL;
	}
	init_timer(&timer->tim);
	timer->tim.function = (fct)dotimer;
	timer->proc = func;
	timer->arg = arg;
	timer->tim.data = (unsigned long)timer;
	timer->tim.expires = expire + jiffies;
#ifdef MODULE
	MOD_INC_USE_COUNT;
#endif
	add_timer(&timer->tim);

	return (int)timer;
}

void
#ifdef CONFIG_MALLOC_NAMES
deb_timeout_old(const char *deb_file, unsigned int deb_line, void (*func)(void *), void *arg, int expire)
#else
timeout_old(void (*func)(void *), void *arg, int expire)
#endif
{
	typedef void (*fct)(unsigned long);
	struct timing_old *timer;
	int s;

#ifdef CONFIG_MALLOC_NAMES
	timer = (struct timing_old *)deb_kmalloc(sizeof(struct timing_old), GFP_ATOMIC,deb_file,deb_line);
#else
	timer = (struct timing_old *)kmalloc(sizeof(struct timing_old), GFP_ATOMIC);
#endif
	if(timer == NULL) {
		printf(" *!* No Timeout!\n");
		return;
	}
	init_timer(&timer->tim.tim);
	timer->tim.proc = func;
	timer->tim.arg = arg;
	timer->tim.tim.function = (fct)dotimer_old;
	timer->tim.tim.data = (unsigned long)timer;
	timer->tim.tim.expires = expire + jiffies;
	save_flags(s); cli();
	if(start != NULL)
		start->prev = timer;
	timer->next = start;
	timer->prev = NULL;
	start = timer;
	restore_flags(s);
#ifdef MODULE
	MOD_INC_USE_COUNT;
#endif
	add_timer(&timer->tim.tim);

}

void
#ifdef CONFIG_MALLOC_NAMES
deb_untimeout(const char *deb_file, unsigned int deb_line, int timer)
#else
untimeout(int timer)
#endif
{
	if(!del_timer(&((struct timing *)timer)->tim)) {
#ifdef CONFIG_MALLOC_NAMES
		printf("del_timer called freed from %s:%d\n",deb_file,deb_line);
#endif
	}
#ifdef CONFIG_MALLOC_NAMES
	deb_kfree((void *)timer,deb_file,deb_line);
#else
	kfree((void *)timer);
#endif

#ifdef MODULE
	MOD_DEC_USE_COUNT;
#endif
}

void
#ifdef CONFIG_MALLOC_NAMES
deb_untimeout_old(const char *deb_file, unsigned int deb_line, void (*func)(void *), void *arg)
#else
untimeout_old(void (*func)(void *), void *arg)
#endif
{
	droptimer_old(func,arg,1);
}



int crash_time = 300; /* Five minutes until reboot */

#define get_seg_long(seg,addr) ({ \
register unsigned long __res; \
__asm__("push %%fs;mov %%ax,%%fs;movl %%fs:%2,%%eax;pop %%fs" \
        :"=a" (__res):"0" (seg),"m" (*(addr))); \
		__res;})
		 

void sysdump(const char *msg, struct pt_regs *regs, unsigned long err)
{
    int i,nlim=9;
    unsigned long flags;
    unsigned long esp;
    unsigned short ss;
    unsigned int dodump=100;

#if 1
    static unsigned long lticks = 0;

    if(jiffies-lticks < HZ) {
		printk("."); return;
	}
#endif
    save_flags(flags); cli();
	if(err != 0xf00fdead) {
	
		if(regs == NULL)
				esp = (unsigned long) &msg;
		else
			esp = (unsigned long) &regs->esp;
		ss = KERNEL_DS;

		printk("%s\n",KERN_DEBUG);
		if(msg != NULL)
			printk("%s%s: %08lx\n", KERN_DEBUG, msg, err);
		else
			nlim += 4;
		if(regs != NULL) {
			printk("%sEIP:    %04x:%08lx     EFLAGS: %08lx\n", KERN_EMERG, 0xffff & regs->cs,regs->eip,regs->eflags);
			printk("%seax: %08lx   ebx: %08lx   ecx: %08lx edx: %08lx\n", KERN_EMERG,
				regs->eax, regs->ebx, regs->ecx, regs->edx);
			printk("%sesi: %08lx   edi: %08lx   ebp: %08lx esp: %08lx\n",KERN_EMERG, 
				regs->esi, regs->edi, regs->ebp, esp);
			printk("%sds: %04x   es: %04x   fs: %04x   gs: %04x   ss: %04x\n",KERN_EMERG,
				regs->ds, regs->es, regs->fs, regs->gs, ss);
			if (STACK_MAGIC != *(unsigned long *)current->kernel_stack_page)
				printk("%sCorrupted stack page\n",KERN_EMERG);
			else
				nlim += 1;
		
		} else
			nlim += 5;
		store_TR(i);
		if(current != NULL) {
			printk("%sProcess %s (pid: %d, process nr: %d, stackpage=%08lx)\n",
				KERN_EMERG,current->comm, current->pid, 0xffff & i, current->kernel_stack_page);
		} else {
			printk("%sNo current process\n",KERN_EMERG);
		}
		{
			int nlines = 0, linepos = 0;
			printk("%s",KERN_EMERG);
			for (i=0; dodump && nlines < nlim; i++) {
				u_long xx = get_seg_long(ss,(i+(unsigned long *)esp));
				if(dodump > 1 && (xx & 0xfff00000) == 0xbff00000)
					dodump = 4;
#define WRAP(n) do { if((linepos+=(n))>=80) { dodump--; nlines++; linepos=(n); printk("%s", KERN_EMERG); } } while(0)
				if     (xx & 0xFF000000UL) { WRAP(9); printk("%08lx ",xx); }
				else if(xx & 0x00FF0000UL) { WRAP(7); printk("%06lx ",xx); }
				else if(xx & 0x0000FF00UL) { WRAP(5); printk("%04lx ",xx); }
				else                       { WRAP(3); printk("%02lx ",xx); }
#undef WRAP
			}
			printk("\n");
		}
	}
	dodump=0;
	if(err == 0xDEADBEEF)
		dodump=0;
	else if(err == 0xF00DDEAD || err == 0xf00fdead)
		dodump=(crash_time ? crash_time : 60);
	else if(jiffies < 60*HZ)
		dodump=9999999;
	else
		dodump=crash_time;
	if(dodump>0) {
		if(dodump < 10000) {
			int j;

			printk("%sCrash & Burn...",KERN_EMERG);
			for(j=dodump;j>0;j--)  {
				int i;
				if((j%30 == 0) || (j%5 == 0 && j < 60) || j < 10) printk("%d...",j);
				for(i=0;i<9990;i++) udelay(100);
			}
			panic("now.");
		} else {
			printk("%sKernel halted.",KERN_EMERG);
			for(;;);
		}
	}
    restore_flags(flags);
}



char *loghdr(char level)
{
	static char sbuf[30];
	static int tdiff = 0;
	sprintf(sbuf,"<%d>%ld:",level,jiffies-tdiff);
	tdiff = jiffies;
	return sbuf;
}


void do_i_sleep_on(struct wait_queue **p)
{
	long s = spl(1);
    interruptible_sleep_on(p);
	splx(s);
}

void do_sleep_on(struct wait_queue **p)
{
	long s = spl(1);
    sleep_on(p);
	splx(s);
}

#ifdef DO_DEBUGGING

struct d_hdr {
	long magic;
	const char *file;
	unsigned int line;
	ssize_t size;
};
#define MAGIC_HEAD 0x12348725
#define MAGIC_TAIL 0x37867489

void *deb_kmalloc(size_t sz, int prio, const char *deb_file, unsigned int deb_line)
{
	static unsigned int mack = 0;

	struct d_hdr *foo = kmalloc(sz+sizeof(long)+sizeof(struct d_hdr), prio);
	if(foo == NULL) {
		printk("%sNoMem alloc %d from %s:%d\n",KERN_WARNING,sz,deb_file,deb_line);
		return NULL;
	}
	if(++mack == 10000)
		mack = 0;
	foo->magic = MAGIC_HEAD;
	foo->file = deb_file;
	foo->line = deb_line;
	foo->size = mack ? sz : -sz;
	foo++;
	if(!mack) printk("%s[M:A %d %p %s:%d]\n",KERN_DEBUG,sz,foo,deb_file,deb_line);
	*(long *)(sz + (char *)(foo)) = MAGIC_TAIL;

	return foo;
}

int deb_kcheck(void *fo, const char *deb_file, unsigned int deb_line)
{
	struct d_hdr *foo = fo;
	foo--;
	if(foo->magic != MAGIC_HEAD) {
		printk("\n%sBad magic at free in %s:%d\n",KERN_EMERG,deb_file,deb_line);
		return 1;
	}
	if(*(long *)(((foo->size > 0) ? foo->size : -foo->size) + (char *)(fo)) != MAGIC_TAIL) {
		printk("%sMem Overwrite between %s:%d and %s:%d\n",KERN_EMERG,foo->file,foo->line, deb_file,deb_line);
		return 1;
	}
	foo->file = deb_file; foo->line = deb_line;

	return 0;
}

void deb_kfree(void *fo, const char *deb_file, unsigned int deb_line)
{
	struct d_hdr *foo = fo;
	foo--;
	if(foo->magic != MAGIC_HEAD) {
		printk("%sBad magic at free in %s:%d\n",KERN_EMERG,deb_file,deb_line);
		return;
	}
	if(*(long *)(((foo->size > 0) ? foo->size : -foo->size) + (char *)(fo)) != MAGIC_TAIL) {
		printk("%sMem Overwrite between %s:%d and %s:%d\n",KERN_EMERG,foo->file,foo->line, deb_file,deb_line);
		return;
	}
	if(foo->size < 0)
		printk("%s[M:F %d %p %s:%d]\n",KERN_DEBUG,-foo->size,fo,foo->file,foo->line);

	foo->magic = 0x77776666;
	kfree(foo);
}

#endif

#ifdef MODULE

static struct symbol_table compat_symbol_table = {
#include <linux/symtab_begin.h>
#ifdef CONFIG_MALLOC_NAMES
	X(deb_timeout),
	X(deb_untimeout),
	X(deb_timeout_old),
	X(deb_untimeout_old),
#else
	X(timeout),
	X(untimeout),
	X(timeout_old),
	X(untimeout_old),
#endif
	X(loghdr),
	X(sysdump),
#ifdef DO_DEBUGGING
	X(deb_kmalloc),
	X(deb_kcheck),
	X(deb_kfree),
#endif
	Xalias(do_i_sleep_on,interruptible_sleep_on),
	Xalias(do_sleep_on,sleep_on),
#include <linux/symtab_end.h>
};


static int do_init_module(void)
{
	/* Oh well, no more rename_module_symbol... */
	register_symtab(&compat_symbol_table);
	return 0;
}

static int do_exit_module(void)
{
	return 0;
}

#endif
