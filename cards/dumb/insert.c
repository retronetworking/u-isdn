#include "f_module.h"
#include "isdn_limits.h"
#include <stddef.h>

#ifdef linux
#include <linux/sched.h>
#endif

#include "shell.h"

#ifndef CARDTYPE
#error "You have to define CARDTYPE for this to work."
#endif

#define xxappxx(a,b) a##b
#define NAME(a,b) xxappxx(a,b) /* isn't C wonderful */
#define xxstrxx(a) #a
#define STRING(a) xxstrxx(a) /* ditto */

extern int NAME(CARDTYPE,init)(struct _dumb *dumb);
extern void NAME(CARDTYPE,exit)(struct _dumb *dumb);

struct _dumb dumb;

int irq = 0;
int mem = 0;
int io = 0;
int ipl = 0;
int name = 0;
int debug = 0;

#ifdef MODULE
static int do_init_module(void)
{
	if(name == 0) {
		printf("You must name this card: insmod xxx.o name=$(name Foo0)\n");
		return -EINVAL;
	}
	dumb.irq = irq;
	dumb.ipl = ipl;
	dumb.ioaddr = io;
	dumb.memaddr = mem;
	dumb.ID = name;
	dumb.debug = debug;
	return NAME(CARDTYPE,init)(&dumb);
}

static int do_exit_module(void)
{
	NAME(CARDTYPE,exit)(&dumb);
	return 0;
}
#else
#error "This can only be used as a module!"
#endif

