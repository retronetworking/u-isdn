#include "f_module.h"
#include "isdn_limits.h"
#include <stddef.h>
#include <linux/sched.h>
#include "loader.h"

#ifndef CARDTYPE
#error "You have to define CARDTYPE for this to work."
#endif

extern int NAME(CARDTYPE,init)(struct cardinfo *inf);
extern void NAME(CARDTYPE,exit)(struct cardinfo *inf);

struct cardinfo inf = {0,};

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
		printf("You must name this card: insmod xxx.o name=$(cardname Foo0)\n");
		return -EINVAL;
	}
	if(irq == 2)
		irq = 9;
	inf.irq = irq;
	inf.ipl = ipl;
	inf.ioaddr = io;
	inf.memaddr = mem;
	inf.ID = name;
	inf.debug = debug;
	inf.use_count = &mod_use_count_;
	return NAME(CARDTYPE,init)(&inf);
}

static int do_exit_module(void)
{
	NAME(CARDTYPE,exit)(&inf);
	return 0;
}
#else
#error "This can only be used as a module!"
#endif

