
#include <sys/types.h>
#include <sys/file.h>
#include <sys/param.h>
#include <sys/errno.h>
#include "f_signal.h"
#include <sys/stropts.h>
#include "f_termio.h"
#ifdef AUX
#include <compat.h>
#endif
#include <malloc.h>
#include "streams.h"
#include "kernel.h"
#include "msgtype.h"

#include "dump.h"

main ()
{
#define d(a,b) do { if(!(a)) { printf("%s\n",b); exit(1); } } while(0)
#define dd(a,b) do { if((err=(a))!=0) { printf("E %d: %s\n",err,b); exit(1); } } while(0)
	struct xstream *xp = stropen (0);
	int err;
	int len;
	int i, j;

	{
		extern struct streamtab log_info;
		extern struct streamtab t70info;

		modregister (&log_info);
		modregister (&t70info);
	}
#ifdef AUX
	setcompat (getcompat ()| COMPAT_BSDSIGNALS);
#endif


	d (!strioctl (xp, I_PUSH, (int) "strlog"), "Damn Log 1");
	d (!strioctl (xp, I_PUSH, (int) "t70"), "Damn T70 1");
	d (!strioctl (xp, I_PUSH, (int) "strlog"), "Damn Log 2");

	{
		char *x;

#define T do { int len = strlen(x); mblk_t *mp = allocb(len,BPRI_MED); bcopy(x,mp->b_wptr,len); mp->b_wptr += len; mp->b_datap->db_type = MSG_PROTO; putnext(&xp->wl,mp); } while(0)
		x = "ms :ms t70 :mt 5";
		T;
		x = "ou";
		T;
		x = "co";
		T;
	}

	while (1) {
		char d[100];
		char *x = (char *) gets (d);

		if (x == NULL)
			break;
		len = strlen (d);
		switch (*d) {
		default:
			printf ("???\n");
			break;
		case 'w':
			len--;
			hexm ((uchar_t *) d + 1, &len);
			if (len)
				dd (strwrite (xp, (uchar_t *) d + 1, &len, 0), "WriteLow");
			else
				printf ("???\n");
			break;
		case 'W':
			len--;
			hexm ((uchar_t *) d + 1, &len);
			if (len)
				dd (strwrite (xp, (uchar_t *) d + 1, &len, 1), "WriteHigh");
			else
				printf ("???\n");
			break;

		case '\0':
			break;
		}
		runqueues ();

		for (i = 0; i < 5; i++)
			for (j = 0; j <= 1; j++) {
				if ((err = strread (xp, d, &len, j)) == 0) {
				} else {
					printf ("Err %d in pass %d, reading from %s\n", err, i, j ? "up" : "down");
					exit (1);
				}
			}
	}
	d (!strioctl (xp, I_POP, 0), "pop");
	d (!strioctl (xp, I_POP, 0), "pop");
	d (!strioctl (xp, I_POP, 0), "pop");
	strclose (xp, 0);
	printf ("Done\n");
	exit (0);
}

mblk_t *c_dupmsg(mblk_t *a, const char *b, int c) { return dupmsg(a); }
