#include "f_module.h"
#include "isdn_limits.h"
#include <stddef.h>

#ifdef linux
#include <linux/sched.h>
#endif

#include "bintec.h"

#ifndef REALNAME
#error "You have to define REALNAME for this to work."
#endif

#define xxappxx(a,b) a##b
#define NAME(a,b) xxappxx(a,b) /* isn't C wonderful */
#define xxstrxx(a) #a
#define STRING(a) xxstrxx(a) /* ditto */

extern int NAME(REALNAME,init)(struct _bintec *bp);
extern void NAME(REALNAME,exit)(struct _bintec *bp);

struct _bintec bintec;

int irq = 0;
int mem = 0;
int io = 0;
int name = 0;
int debug = 0;

#ifdef MODULE
static int do_init_module(void)
{
	if(name == 0) {
		printf("You must name this card: insmod xxx.o name=$(cardname Foo0)\n");
		return -EINVAL;
	}
	bintec.irq = irq;
	bintec.ipl = 5;
	bintec.ioaddr = io;
	bintec.memaddr = mem;
	bintec.ID = name;
	bintec.debug = debug;
	return NAME(REALNAME,init)(&bintec);
}

static int do_exit_module(void)
{
	NAME(REALNAME,exit)(&bintec);
	return 0;
}
#else
#error "This can only be used as a module!"
#endif

