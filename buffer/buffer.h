#ifndef _BUF_H
#define _BUF_H

#define NT70  15
#define BUF_mtu 256

#include "primitives.h"

#define BUF_UP CHAR2('b','r')			  /* buffer size, upstream */
#define BUF_DOWN CHAR2('b','w')			  /* buffer size, downstream */
#define BUF_BOTH CHAR2('b','b')			  /* buffer size, both directions */

#endif /* _BUF_H */
