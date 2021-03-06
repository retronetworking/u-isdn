#ifndef _INC_KERNEL
#define _INC_KERNEL
#if defined(linux)
#include <linux/config.h>
#endif

#ifndef KERNEL
#include <malloc.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/errno.h>
#include <sys/uio.h>

extern int spl6 (void);
extern int spl1 (void);
extern int splhi (void);
extern int splx (int ms);
extern int spl (int ms);

extern void timeout (void *a, void *b, int c);
extern void untimeout (void *a, void *b);

int min (int a, int b);
int max (int a, int b);

extern void panic(const char *x, ...);

#define	KERN_EMERG	"0:"	/* system is unusable			*/
#define	KERN_ALERT	"1:"	/* action must be taken immediately	*/
#define	KERN_CRIT	"2:"	/* critical conditions			*/
#define	KERN_ERR	"3:"	/* error conditions			*/
#define	KERN_WARNING	"4:"	/* warning conditions			*/
#define	KERN_NOTICE	"5:"	/* normal but significant condition	*/
#define	KERN_INFO	"6:"	/* informational			*/
#define	KERN_DEBUG	"7:"	/* debug-level messages			*/

#define kmalloc(a,b) malloc((a))
#define kfree(a) free((a))
#define kfree_s(a,b) free((a))

#undef HZ
#define HZ 10 /* don't need more granularity */

#else /* KERNEL */
#ifndef __KERNEL__
#define __KERNEL__
#endif
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/time.h>
#include <linux/param.h>
#include <linux/errno.h>
#include <linux/uio.h>

char *loghdr(char level);
#undef KERN_EMERG
#undef KERN_ALERT
#undef KERN_CRIT
#undef KERN_ERR
#undef KERN_WARNING
#undef KERN_NOTICE
#undef KERN_INFO
#undef KERN_DEBUG
#define KERN_EMERG  loghdr(0)
#define KERN_ALERT  loghdr(1)
#define	KERN_CRIT	loghdr(2)
#define	KERN_ERR	loghdr(3)
#define	KERN_WARNING	loghdr(4)
#define	KERN_NOTICE	loghdr(5)
#define	KERN_INFO	loghdr(6)
#define	KERN_DEBUG	loghdr(7)

#endif

#ifndef minor
#define minor(a) ((a)&0xFF)
#endif

#endif
