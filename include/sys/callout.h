#if !defined(__sys_callout_h)
#define __sys_callout_h

#ifndef __KERNEL__
#include <sys/types.h>
#endif

struct	callout {
	int	c_time;		/* incremental time */
	void *	c_arg;		/* argument to routine */
	int	(*c_func)(void *);	/* routine */
	struct	callout *c_next;
};

#endif /* __sys_callout_h */
