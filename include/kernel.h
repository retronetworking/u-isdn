#ifndef _INC_KERNEL
#define _INC_KERNEL
#ifndef KERNEL

#if defined(linux)
#include <linux/config.h>
#ifdef CONFIG_DEBUG_LATENCY
#undef CONFIG_DEBUG_LATENCY
#endif
#endif

extern int spl6 (void);
extern int spl1 (void);
extern int splhi (void);
extern int splx (int ms);

extern void timeout (void *a, void *b, int c);
extern void untimeout (void *a, void *b);

extern void panic(const char *x, ...);

#define	KERN_EMERG	"0:"	/* system is unusable			*/
#define	KERN_ALERT	"1:"	/* action must be taken immediately	*/
#define	KERN_CRIT	"2:"	/* critical conditions			*/
#define	KERN_ERR	"3:"	/* error conditions			*/
#define	KERN_WARNING	"4:"	/* warning conditions			*/
#define	KERN_NOTICE	"5:"	/* normal but significant condition	*/
#define	KERN_INFO	"6:"	/* informational			*/
#define	KERN_DEBUG	"7:"	/* debug-level messages			*/

#endif
#endif
