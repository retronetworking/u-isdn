#ifndef _F_SIGNAL
#define _F_SIGNAL

#include "primitives.h"
#if defined(KERNEL) && defined(linux)
#include <linux/signal.h>
#else
#include <signal.h>
#endif

#ifdef SYSV_SIGTYPE

#define SIG_TYPE sigset_t 
#define SIG_SUSPEND(_var,_sig) do { sigset_t _new; sigemptyset(&_new); sigaddset(&_new,_sig);\
								  sigprocmask(SIG_BLOCK,&_new,&_var); } while(0)
#define SIG_RESUME(_var) sigprocmask(SIG_SETMASK,&_var,NULL)

#endif

#ifdef BSD_SIGTYPE

#define SIG_TYPE int
#define SIG_SUSPEND(_var,_sig) (_var) = sigblock (sigmask (_sig))
#define SIG_RESUME(_var) sigsetmask(_var)

#endif

#ifdef NONE_SIGTYPE

#define SIG_TYPE struct { sigfunc__t __sig; int __sign; }
#ifdef SIG_HOLD
#define SIG_SUSPEND(_var,_sig) do { (_var).__sign = _sig;  (_var).__sig = signal(_sig,SIG_HOLD); } while(0)
#else
#define SIG_SUSPEND(_var,_sig) do { (_var).__sign = _sig;  (_var).__sig = signal(_sig,SIG_IGN); } while(0)
#endif
#define SIG_RESUME(_var) do { (void) signal((_var).__sign, (_var).__sig); } while(0)

#endif

#ifndef SIG_TYPE
error You did not define a valid SIGTYPE in config/config.data
#endif

#endif /* _F_SIGNAL */
