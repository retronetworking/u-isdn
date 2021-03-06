#ifndef _TIMER_H
#define _TIMER_H

#define NTIMER  15

#include "primitives.h"

#define TIMER_INTERVAL CHAR2('t','i')
#define TIMER_PRETIME CHAR2('t','o')
#define TIMER_READMAX CHAR2('t','r')
#define TIMER_WRITEMAX CHAR2('t','w')
#define TIMER_DO_INT CHAR2('t','I')
#define TIMER_DO_DISC CHAR2('t','D')

#define TIMER_IF_IN CHAR2('l','i')
#define TIMER_IF_OUT CHAR2('l','o')
#define TIMER_IF_BOTH CHAR2('l','b')

#define TIMER_DATA_IN CHAR2('d','i')
#define TIMER_DATA_OUT CHAR2('d','o')
#define TIMER_DATA_BOTH CHAR2('d','b')
#define TIMER_DATA_NONE CHAR2('d','n')

#endif /* _TIMER_H */
