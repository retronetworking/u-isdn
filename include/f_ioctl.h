#ifndef _F_IOCTL
#define _F_IOCTL

#ifdef M_UNIX
#include <sys/socket.h>
#define _IO(a,b) _IOS((a),(b))
#define _IOR(a,b,c) _IOSR((a),(b),(c))
#define _IOW(a,b,c) _IOSW((a),(b),(c))
#define _IOWR(a,b,c) _IOSWR((a),(b),(c))
#endif

#if defined(AUX) || defined(linux)
#include <sys/ioctl.h>
#endif

#endif /*  _F_IOCTL */
