#ifndef _LINUX_STROPTS_H
#define _LINUX_STROPTS_H

#include <sys/ioctl.h>

#ifndef FMNAMESZ
#define FMNAMESZ	16
#endif FMNAMESZ

/*
 * Read options.
 */
#define RNORM 	0	/* Message boundaries are ignored */
#define RMSGD	1	/* Message tails are droped */
#define RMSGN	2	/* Message tails are preserved */

/*
 * Flush options.
 */
#define FLUSHR 01	/* flush read queue */
#define FLUSHW 02	/* flush write queue */
#define FLUSHRW (FLUSHR|FLUSHW)

/*
 *  Ioctls.
 */
#define I_NREAD		_IOR('S', 01, int)
#define I_PUSH		_IOW('S', 02, char[FMNAMESZ+1])
#define I_POP		_IO('S', 03)
#define I_LOOK		_IOR('S', 04, char[FMNAMESZ+1])
#define I_FLUSH		_IO('S', 05)
#define I_SRDOPT	_IO('S', 06)
#define I_GRDOPT	_IOR('S', 07, int)
#define I_STR		_IOWR('S', 010, struct strioctl)

#define I_FIND		_IOW('S', 011, char[FMNAMESZ+1])

/*
 * ioctl control block. Needed for ioctl commands which do not correctly encode their data, and for compatibility with stupid systems like S5R4.
 * Sensible code can just call ioctl(2) directly.
 */
struct strioctl {
	int 	ic_cmd;			/* command */
	int	ic_timout;		/* timeout value */
	int	ic_len;			/* length of data */
	void	*ic_dp;			/* pointer to data */
};

#endif /* _LINUX_STROPTS_H */
