#include "primitives.h"
#include "f_ioctl.h"
#include "stropts.h"
#include <sys/errno.h>
#ifdef linux
#include <linux/ioctl.h>
#endif

extern int errno;

int
sioctl (int fd, ulong_t type, void *data)
{
	long ret;

#if 0
	if ((ret = ioctl (fd, type, data)) != -1 || errno != EINVAL)
		return ret;
#endif
	{
		struct strioctl ctl;

		ctl.ic_cmd = type;
		ctl.ic_timout = -1;
		ctl.ic_dp = data;
		ctl.ic_len = (type & IOC_IN) ? ((type >> 16) & IOCCMD_MASK) : 0;

		if ((ret = ioctl (fd, I_STR, &ctl)) != -1 || errno != EINVAL)
			return ret;
	}
	return ioctl (fd, type, data);
}

#define ioctl(a,b,c) sioctl(a,b,c)
