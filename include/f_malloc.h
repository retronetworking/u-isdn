#ifndef _F_MALLOC
#define _F_MALLOC
#ifdef KERNEL

#ifdef linux
#define HAVEMALLOC
#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/malloc.h>
#undef malloc
#undef free
#ifdef CONFIG_MALLOC_NAMES
#define malloc(x) deb_kmalloc(__FILE__,__LINE__,(x),GFP_ATOMIC)
#define free(x) deb_kfree_s(__FILE__,__LINE__,(x),0)
#else
#define malloc(x) kmalloc((x),GFP_ATOMIC)
#define free(x) kfree((x))
#endif
#endif

/* other kernels here */

#ifndef HAVEMALLOC
#error No kernel malloc available?
#endif

#else
#include <malloc.h>
#endif /* !kernel */

#endif /* _F_MALLOC */
