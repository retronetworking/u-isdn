#define COMPAT_C

/*
 * Compatibility stuff for Streams-ish drivers and other stuff.
 *
 * Copyright 1994: Matthias Urlichs <urlichs@smurf.noris.de>
 */
#ifdef MODULE
#include <linux/config.h>
#include <linux/module.h>
#include <linux/version.h>
#endif

#include <linux/kernel.h>
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
const char *xdeb_file; unsigned int xdeb_line;

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

static void dotimer(void *arg)
{
	struct timing *tim = (struct timing *)arg;

	(*tim->proc)(tim->arg);
	sti();
	kfree_s(tim,sizeof(struct timing));
#ifdef MODULE
	MOD_DEC_USE_COUNT;
#endif
}

static void droptimer_old(void (*func)(void *), void *arg, char del)
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
				kfree_s(tim,sizeof(struct timing_old));
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

static void dotimer_old(void *arg)
{
	struct timing_old *tim = (struct timing_old *)arg;

	droptimer_old(tim->tim.proc,tim->tim.arg,0);
	(*tim->tim.proc)(tim->tim.arg);

	sti();
	kfree_s(tim,sizeof(*tim));
}

#ifdef CONFIG_MALLOC_NAMES
int deb_timeout(const char *deb_file, unsigned int deb_line, void (*func)(void *), void *arg, int expire)
#else
int timeout(void (*func)(void *), void *arg, int expire)
#endif
{
	typedef void (*fct)(unsigned long);
	struct timing *timer;

#ifdef CONFIG_MALLOC_NAMES
	timer = (struct timing *)deb_kmalloc(deb_file,deb_line, sizeof(struct timing), GFP_ATOMIC);
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
	timer->tim.expires = expire
#if LINUX_VERSION_CODE >= 66304 /* 1.3.0 -- is that right ?? */
		+ jiffies;
#endif
		;
#ifdef MODULE
	MOD_INC_USE_COUNT;
#endif
	add_timer(&timer->tim);
	return (int)timer;
}

#ifdef CONFIG_MALLOC_NAMES
void deb_timeout_old(const char *deb_file, unsigned int deb_line, void (*func)(void *), void *arg, int expire)
#else
void timeout_old(void (*func)(void *), void *arg, int expire)
#endif
{
	typedef void (*fct)(unsigned long);
	struct timing_old *timer;
	int s;

#ifdef CONFIG_MALLOC_NAMES
	timer = (struct timing_old *)deb_kmalloc(deb_file,deb_line, sizeof(struct timing_old), GFP_ATOMIC);
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
	timer->tim.tim.expires = expire
#if LINUX_VERSION_CODE >= 66304 /* 1.3.0 -- is that right ?? */
		+ jiffies;
#endif
		;
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

#ifdef CONFIG_MALLOC_NAMES
void deb_untimeout(const char *deb_file, unsigned int deb_line, int timer)
#else
void untimeout(int timer)
#endif
{
	del_timer(&((struct timing *)timer)->tim);
#ifdef CONFIG_MALLOC_NAMES
	deb_kfree_s(deb_file,deb_line, (void *)timer,sizeof(struct timing));
#else
	kfree_s((void *)timer,sizeof(struct timing));
#endif
#ifdef MODULE
	MOD_DEC_USE_COUNT;
#endif
}

#ifdef CONFIG_MALLOC_NAMES
void deb_untimeout_old(const char *deb_file, unsigned int deb_line, void (*func)(void *), void *arg)
#else
void untimeout_old(void (*func)(void *), void *arg)
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

		printk(KERN_DEBUG "\n");
		if(msg != NULL)
			printk(KERN_EMERG "%s: %08lx\n", msg, err);
		else
			nlim += 4;
		if(regs != NULL) {
			printk(KERN_EMERG "EIP:    %04x:%08lx     EFLAGS: %08lx\n", 0xffff & regs->cs,regs->eip,regs->eflags);
			printk(KERN_EMERG "eax: %08lx   ebx: %08lx   ecx: %08lx edx: %08lx\n",
				regs->eax, regs->ebx, regs->ecx, regs->edx);
			printk(KERN_EMERG "esi: %08lx   edi: %08lx   ebp: %08lx esp: %08lx\n",
				regs->esi, regs->edi, regs->ebp, esp);
			printk(KERN_EMERG "ds: %04x   es: %04x   fs: %04x   gs: %04x   ss: %04x\n",
				regs->ds, regs->es, regs->fs, regs->gs, ss);
			if (STACK_MAGIC != *(unsigned long *)current->kernel_stack_page)
				printk(KERN_EMERG "Corrupted stack page\n");            
			else
				nlim += 1;
		
		} else
			nlim += 5;
		store_TR(i);
		if(current != NULL) {
			printk(KERN_EMERG "Process %s (pid: %d, process nr: %d, stackpage=%08lx)\n",
				current->comm, current->pid, 0xffff & i, current->kernel_stack_page);
		} else {
			printk(KERN_EMERG "No current process\n");
		}
		{
			int nlines = 0, linepos = 0;
			printk(KERN_EMERG "");
			for (i=0; dodump && nlines < nlim; i++) {
				u_long xx = get_seg_long(ss,(i+(unsigned long *)esp));
				if(dodump > 1 && (xx & 0xfff00000) == 0xbff00000)
					dodump = 4;
	#define WRAP(n) do { if((linepos+=(n))>=80) { dodump--; nlines++; linepos=(n); printk("\n" KERN_EMERG); } } while(0)
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

			printk(KERN_EMERG "Crash & Burn...");
			for(j=dodump;j>0;j--)  {
				int i;
				if((j%30 == 0) || (j%5 == 0 && j < 60) || j < 10) printk("%d...",j);
				for(i=0;i<9990;i++) udelay(100);
			}
			panic("now.");
		} else {
			printk(KERN_EMERG "Kernel halted.");
			for(;;);
		}
	}
    restore_flags(flags);
}


void do_i_sleep_on(struct wait_queue **p)
{
	long s = splx(~0);
    interruptible_sleep_on(p);
	splx(s);
}

void do_sleep_on(struct wait_queue **p)
{
	long s = splx(~0);
    sleep_on(p);
	splx(s);
}

#ifdef MODULE

char kernel_version[] = UTS_RELEASE;

int init_module(void)
{
#if defined(__ELF__) || LINUX_VERSION_CODE >= 66304 /* 1.3.0. Is that right??? */
#define U
#else
#define U "_"
#endif
	rename_module_symbol(U "do_i_sleep_on",U "interruptible_sleep_on");
	rename_module_symbol(U "do_sleep_on",U "sleep_on");
	return 0;
}

int cleanup_module(void)
{
	return 0;
}

#endif
