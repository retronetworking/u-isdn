
#include "primitives.h"
#include <malloc.h>
#include "f_strings.h"

#ifdef DO_NEED_STRDUP
char *
strdup (const char *xx)
{
	int len = strlen (xx) + 1;
	char *yy = malloc (len);

	if (xx != NULL)
		bcopy (xx, yy, len);
	return yy;
}

#endif


#ifdef DO_NEED_WRITEV

#include <sys/uio.h>

int writev(int fd, struct iovec *vp, int vpcount)
{
    int                 count;
    int					len;
    int					xlen;
	char				*buf;
	char				*xbuf;

    for (count = 0, len = 0; count < vpcount; len += vp[count++].iov_len)
		continue;
	if(len == 0)
		return 0;
	buf = malloc(len); 
	if(buf == NULL)
		return -1;
    for (len = 0; --vpcount >= 0; len += (vp++)->iov_len)
		memcpy(buf+len,vp->iov_base,vp->iov_len);
	for(xlen=0, xbuf=buf; len > 0; xbuf += count, len -= count, xlen += count)
        if ((count = write(fd,xbuf,len)) <= 0) {
			free(buf);
            return (xlen > 0) ? xlen : -1;
		}
	free(buf);
    return xlen;
}

#endif

