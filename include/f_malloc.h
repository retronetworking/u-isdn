#ifndef _F_MALLOC
#define _F_MALLOC
#ifdef KERNEL

#ifdef linux
#define HAVEMALLOC
#include "compat.h"
#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/malloc.h>
#define malloc(x) kmalloc((x),GFP_ATOMIC)
#define free(x) kfree((x))
#endif

/* other kernels here */

#ifndef HAVEMALLOC
#error No kernel malloc available?
#endif

#else
#include <malloc.h>
#endif /* !kernel */

#endif /* _F_MALLOC */
