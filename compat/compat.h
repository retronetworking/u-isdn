#include <linux/config.h>
#ifndef CONFIG_DEBUG_STREAMS
#define CONFIG_DEBUG_STREAMS
#endif
#ifndef CONFIG_DEBUG_ISDN
#define CONFIG_DEBUG_ISDN
#endif

#ifndef _LINUX_SYSCOMPAT_H_
#define _LINUX_SYSCOMPAT_H_

#include <linux/types.h>
#ifdef __KERNEL__
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/interrupt.h>
#include <asm/segment.h>

#ifndef COMPAT_C
#ifdef interruptible_sleep_on
#undef interruptible_sleep_on
extern void interruptible_sleep_on(struct wait_queue ** p);
#endif
#ifdef sleep_on
#undef sleep_on
extern void sleep_on(struct wait_queue ** p);
#endif
#endif

#else
#include <string.h>
#endif
#include <linux/wait.h>
#include <linux/limits.h>
#include <linux/timer.h>
#include <sys/cdefs.h>

#ifndef MAX_PATH
#define MAX_PATH PATH_MAX
#endif

#define IRQ_BH 31
#define STREAMS_BH 30

#if 1
typedef unsigned char uchar_t;
typedef unsigned short ushort_t;
typedef unsigned int uint_t;
typedef unsigned long ulong_t;
#endif

#ifdef __KERNEL__
/*
 * splx(XXX) sets the interrupt mask to XXX.
 *
 * spl(XXX) adds XXX to the current interrupt mask.
 * Note that all mask bits mean "enabled"!
 */

#ifdef CONFIG_DEBUG_LATENCY
#include <asm/system.h>
struct offtime setter_off[32];
#endif

extern inline unsigned long get_bh_mask(void)
{
	unsigned long flags;
	save_flags(flags);
	if(flags & 0x200)
		return bh_mask | (1<<IRQ_BH);
	else
		return bh_mask &~(1<<IRQ_BH);
}

#define splx(i) (								\
{												\
	unsigned long oldmask;						\
												\
	oldmask = get_bh_mask();					\
												\
	if(i & (1<<IRQ_BH))							\
		sti();									\
	else										\
		cli();									\
	bh_mask = i;								\
	oldmask;									\
}												\
)

#define spl(x) splx(get_bh_mask()&~(x))
#define deb_spl(a,b,x) splx(get_bh_mask()&~(x))
#define splxcheck() do { } while(0)
#define spl0() spl(0)
#define spl1() spl(1<<IRQ_BH)
#define spl2() spl(1<<IRQ_BH)
#define spl3() spl(1<<IRQ_BH)
#define spl4() spl(1<<IRQ_BH)
#define spl5() spl(1<<IRQ_BH)
#define spl6() splx(0)
#define spl7() splx(0)
#define splnet() spl((1<<NET_BH)|(1<<TIMER_BH))
#define splimp() spl((1<<NET_BH)|(1<<TIMER_BH)|(1<<SERIAL_BH)|(1<<IRQ_BH))
#define splhigh() spl((1<<NET_BH)|(1<<TIMER_BH)|(1<<SERIAL_BH)|(1<<IRQ_BH))
#define deb_splimp(a,b) deb_spl(a,b,(1<<NET_BH)|(1<<TIMER_BH)|(1<<IRQ_BH))

#define IRQ1 1
#define IRQ2 2
#define IRQ3 3
#define IRQ4 4
#define IRQ5 5
#define IRQ6 6
#define IRQ7 7
#define IRQ9 9
#define IRQ10 10
#define IRQ11 11
#define IRQ12 12
#define IRQ13 13
#define IRQ14 14
#define IRQ15 15


#define PCATCH 0x80 /* catch the signal instead of returning to the user --
		       ignored and always assumed set, for now */

#endif /* KERNEL */

#ifdef __KERNEL__
__BEGIN_DECLS

/* sleep and wakeup.
   Untested; it's much better to use sleep_on and wake_up instead. */

#if 0
int sleep(caddr_t event, int prio);
void wakeup(caddr_t event);
#endif

/* SysV timeout code. At least they could have returned a void*. */
#ifdef CONFIG_MALLOC_NAMES
int deb_timeout(const char *deb_file, unsigned int deb_line, void (*)(void *), void *, int);
void deb_untimeout(const char *deb_file, unsigned int deb_line, int);

/* BSD timeout code. Walking down the queue ... */
void deb_timeout_old(const char *deb_file, unsigned int deb_line, void (*)(void *), void *, int);
void deb_untimeout_old(const char *deb_file, unsigned int deb_line, void (*)(void *),void *);

#define timeout(a,b,c) deb_timeout(__FILE__,__LINE__,(a),(b),(c))
#define untimeout(a) deb_untimeout(__FILE__,__LINE__,(a))

#define timeout_old(a,b,c) deb_timeout_old(__FILE__,__LINE__,(a),(b),(c))
#define untimeout_old(a,b) deb_untimeout_old(__FILE__,__LINE__,(a),(b))

#else
int timeout(void (*)(void *), void *, int);
void untimeout(int);

/* BSD timeout code. Walking down the queue ... */
void timeout_old(void (*)(void *), void *, int);
void untimeout_old(void (*)(void *),void *);
#endif

#if defined(BSD) || defined(OLD_TIMEOUT)
#ifdef CONFIG_MALLOC_NAMES
#define deb_timeout(a,b,c,d,e) deb_timeout_old((a),(b),(c),(d),(e))
#define deb_untimeout(a,b,c,d) deb_untimeout_old((a),(b),(c),(d))
#else
#define timeout(a,b,c) timeout_old(a,b,c)
#define untimeout(a,b) untimeout_old(a,b)
#endif

#ifndef OLD_TIMEOUT
#define OLD_TIMEOUT
#endif
#undef  NEW_TIMEOUT

#else /* new timeout code */

#ifndef NEW_TIMEOUT
#define NEW_TIMEOUT
#endif
#undef  OLD_TIMEOUT

#endif

__END_DECLS

/* Data transfer. fui* are from i-space (PDP-11 relics). */
#define fubyte(adr) get_fs_byte((adr))
#define fuibyte(adr) get_fs_byte((adr))
#define fuword(adr) get_fs_word((adr))
#define fuiword(adr) get_fs_word((adr))
#define fulong(adr) get_fs_long((adr))
#define fuilong(adr) get_fs_long((adr))
extern inline int subyte(void *adr, int c) {
	int error = verify_area(VERIFY_READ,adr,1);
	if(error) return -1;
	put_fs_byte(c,(char *)adr);
	return 0;
}
#define suibyte(a,b) subyte((a),(b))

extern inline int suword(void *adr, int c) {
	int error = verify_area(VERIFY_READ,adr,2);
	if(error) return -1;
	put_fs_word(c,(unsigned short *)adr);
	return 0;
}
#define suiword(a,b) suword((a),(b))

extern inline int sulong(void *adr, int c) {
	int error = verify_area(VERIFY_READ,adr,4);
	if(error) return -1;
	put_fs_long(c,(int *)adr);
	return 0;
}
#define suilong(a,b) sulong((a),(b))
	
/* Errors are negated on purpose. */
extern inline int copyin(void *from, void *to, int n)
{
	int error = verify_area(VERIFY_READ,from,n);
	if(error) return -error;
	memcpy_fromfs(to,from,n);
	return 0;
}
extern inline int copyout(void *from, void *to, int n)
{
	int error = verify_area(VERIFY_WRITE,to,n);
	if(error) return -error;
	memcpy_tofs(to,from,n);
	return 0;
}
/* Should be memmove() or something... */
static inline void bcopy(void *a, void *b, int c) { memcpy(b,a,c); }
static inline void bzero(void *a, int b) { memset(a,0,b); }
static inline int bcmp(void *a, void *b, int c) { return memcmp(a,b,c); }
#define ovbcopy bcopy

extern inline int imin(int a, int b)
{ return (a < b ? a : b); }

extern inline int imax(int a, int b)
{ return (a > b ? a : b); }

extern inline unsigned int min(unsigned int a, unsigned int b)
{ return (a < b ? a : b); }

extern inline unsigned int max(unsigned int a, unsigned int b)
{ return (a > b ? a : b); }

extern inline long lmin(long a, long b)
{ return (a < b ? a : b); }

extern inline long lmax(long a, long b)
{ return (a > b ? a : b); }

extern inline unsigned long ulmin(unsigned long a, unsigned long b)
{ return (a < b ? a : b); }

extern inline unsigned long ulmax(unsigned long a, unsigned long b)
{ return (a > b ? a : b); }

/* U-area defines, for starters. If you need more, add them */
#ifdef UAREA
#define u (*current)
#endif
#define u_uid fsuid
#define u_gid fsgid
#define u_error errno

#define printf printk
#define log(a,x...) printk(x)

#define curproc current
#define proc task_struct	/* ugliness... */
#define p_pid pid		/* add as necessary */

extern inline void psignal(int sig, struct proc *p) {
	send_sig(sig,p,1); }
extern inline void gsignal(int sig, int pg) {
	kill_pg(pg, sig, 1); }

extern inline void microtime(struct timeval *tv) {
	void do_gettimeofday(struct timeval *tv);
 	do_gettimeofday(tv);
}

extern inline long kvtop(void *addr) { return (long)addr; }

#endif /* __KERNEL__ */

/* The following code is lifted from NetBSD. */

/*
 * Copyright (c) 1993 Christopher G. Demetriou
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * select.h,v 1.1 1993/05/18 18:20:36 cgd Exp
 *
 *	Hacked for Linux.	1.0	-M.U-
 */

/* definiton of info needed to do select on behalf of a device */

struct selinfo {
	struct wait_queue *queue;
};
#ifdef __KERNEL__
#if 0
extern inline void selrecord(struct proc *p, struct selinfo *si) {
	select_wait(&si->queue,current->selwait);
}
#else
#define selrecord(a,b,c,d,e,f,g) WrongNumberOfArgumentsBreak
#endif
extern inline void selwakeup(struct selinfo *si) {
	wake_up_interruptible(&si->queue);
}


struct	prochd {
	struct	prochd *ph_link;	/* linked list of running processes */
	struct	prochd *ph_rlink;
};

extern inline void _insque(struct prochd *element, struct prochd *head)
{
	unsigned long s = spl6();
	element->ph_link = head->ph_link;
	head->ph_link = element;
	element->ph_rlink = head;
	element->ph_link->ph_rlink = element;
	splx(s);
}
#define insque(a,b) _insque((struct prochd *)(a),(struct prochd *)(b))

extern inline void _remque(struct prochd *element)
{
	unsigned long s = spl6();
	element->ph_link->ph_rlink = element->ph_rlink;
	element->ph_rlink->ph_link = element->ph_link;
	element->ph_rlink = NULL;
	splx(s);
}
#define remque(a) _remque((struct prochd *)(a))


void sysdump(const char *msg, struct pt_regs *regs, unsigned long err);

#endif /* __KERNEL__ */
#endif /* _LINUX_SYSCOMPAT_H_ */
