#ifndef _STR_IF_H
#define _STR_IF_H

#include <sys/types.h>
#include <sys/ioctl.h>
#include "primitives.h"

#ifndef SIOCGETU
#define	SIOCGETU	_IOR('p', 136, int)	/* get unit number */
#endif

#define STRIF_MTU CHAR2('m','t')
#define STRIF_ETHERTYPE CHAR2('e','T')

#endif /* _STR_IF_H */

