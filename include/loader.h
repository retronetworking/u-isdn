#ifndef LOADER_H
#define LOADER_H
#include <stream.h>

struct cardinfo {
	long memaddr;
	short ioaddr;
	unsigned char irq, ipl;
	unsigned long ID;
	unsigned int debug;
	long *use_count;
};

#define xxappxx(a,b) a##b
#define NAME(a,b) xxappxx(a,b) /* isn't C wonderful */
#define xxstrxx(a) #a
#define STRING(a) xxstrxx(a) /* ditto */

#endif

