#ifndef _LINUX_SYSCOMPAT_H_
#define _LINUX_SYSCOMPAT_H_

#include "config.h"
#include <linux/types.h>

#ifdef DO_DEBUGGING
#define CONFIG_DEBUG_STREAMS
#define CONFIG_DEBUG_ISDN
#define CONFIG_MALLOC_NAMES
#endif

#ifdef __KERNEL__

#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/interrupt.h>
#include <asm/segment.h>

#ifdef DO_DEBUGGING
#include <linux/malloc.h>

#ifndef COMPAT_C

#ifdef kfree
#undef kfree
#endif
#define kfree(a) deb_kfree((a),__FILE__,__LINE__)
#ifdef kmalloc
#undef kmalloc
#endif
#define kmalloc(a,b) deb_kmalloc((a),(b),__FILE__,__LINE__)
#ifdef kfree_s
#undef kfree_s
#endif
#define kfree_s(a,b) deb_kfree((a),__FILE__,__LINE__)

#endif /* COMPAT_C */

void deb_kfree(void *fo, const char *deb_file, unsigned int deb_line);
void *deb_kmalloc(size_t sz, int prio, const char *deb_file, unsigned int deb_line);
int deb_kcheck(void *fo, const char *deb_file, unsigned int deb_line);
#define deb_kfree_s(a,b,c,d) deb_kfree((a),(c),(d))
#define deb_kcheck_s(a,b,c,d) deb_kcheck((a),(c),(d))
#ifdef kcheck
#undef kcheck
#endif
#define kcheck(a) deb_kcheck((a),__FILE__,__LINE__)

#else /* DO_DEBUGGING */

#ifdef kcheck
#undef kcheck
#endif
#define kcheck(x) do { } while(0)
#ifdef deb_kcheck
#undef deb_kcheck
#endif
#define deb_kcheck(x,y,z) do { } while(0)

#endif /* DO_DEBUGGING */


#include <linux/wait.h>
#include <linux/limits.h>
#include <linux/timer.h>
#include <sys/cdefs.h>

#ifndef MAX_PATH
#define MAX_PATH PATH_MAX
#endif

#else /* __KERNEL__ */
#include <string.h>
#endif /* KERNEL */

#if 1
typedef unsigned char uchar_t;
typedef unsigned short ushort_t;
typedef unsigned int uint_t;
typedef unsigned long ulong_t;
#endif

#ifdef __KERNEL__

#define spl(x) ({ long flags; save_flags(flags); cli(); flags;})
#define splx(i) restore_flags(i)

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


/* Should be memmove() or something... */
static inline void bcopy(void *a, void *b, int c) { memcpy(b,a,c); }
static inline void bzero(void *a, int b) { memset(a,0,b); }
static inline int bcmp(void *a, void *b, int c) { return memcmp(a,b,c); }

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

#define printf printk


extern inline void psignal(int sig, struct task_struct *p) {
	send_sig(sig,p,1); }
extern inline void gsignal(int sig, int pg) {
	kill_pg(pg, sig, 1); }


void sysdump(const char *msg, struct pt_regs *regs, unsigned long err);

#endif /* __KERNEL__ */
#endif /* _LINUX_SYSCOMPAT_H_ */
