#ifndef LOADER_H
#define LOADER_H
#include <stream.h>

struct cardinfo {
	long memaddr;
	short ioaddr;
	unsigned char irq, ipl;
	unsigned long ID;
	unsigned int debug;

};

#endif

