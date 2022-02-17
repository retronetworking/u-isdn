
#include <sys/types.h>
#include <sys/file.h>
#include <sys/param.h>
#include <sys/errno.h>
#include "f_signal.h"
#include <sys/stropts.h>
#include "f_termio.h"
#include <compat.h>
#include <sys/conf.h>
#include <malloc.h>
#include "streams.h"
#include "kernel.h"

#include "dump.h"

extern int x75_T1 ();
extern int x75_T3 ();
int t1arg;
int t3arg;

timeout (a, b, c)
{
	if (a == (int) &x75_T1) {
		printf ("Timer T1: %d.%02d\n", c / 60, c % 60 * 100 / 60);
		t1arg = b;
	} else if (a == (int) &x75_T3) {
		printf ("Timer T3: %d.%02d\n", c / 60, c % 60 * 100 / 60);
		t3arg = b;
	} else
		printf ("Unknown Timer %x/%x %d.%02d\n", a, b, c / 60, c % 60 * 100 / 60);
}

untimeout (a, b)
{
	if (a == (int) &x75_T1) {
		if (b != t1arg)
			printf ("Bad (%x vs %x) ", b, t1arg);
		printf ("Untimer T1\n");
	} else if (a == (int) &x75_T3) {
		if (b != t3arg)
			printf ("Bad (%x vs %x) ", b, t3arg);
		printf ("Untimer T3\n");
	} else
		printf ("Unknown Untimer %x/%x\n", a, b);

}

main ()
{
#define d(a,b) do { if(!(a)) { printf("%s\n",b); exit(1); } } while(0)
#define dd(a,b) do { if((err=(a))!=0) { printf("E %d: %s\n",err,b); exit(1); } } while(0)
	struct xstream *xp = stropen (0);
	int err;
	int len;
	int i, j;

	{
		extern struct streamtab loginfo;
		extern struct streamtab x75info;

		modregister (&loginfo);
		modregister (&x75info);
	}
	setcompat (getcompat ()| COMPAT_BSDSIGNALS);


	d (!strioctl (xp, I_PUSH, (int) "strlog"), "Damn Log 1");
	d (!strioctl (xp, I_PUSH, (int) "x75"), "Damn X75 1");
	d (!strioctl (xp, I_PUSH, (int) "strlog"), "Damn Log 2");

	{
		char *x;
#define T do { int len = strlen(x); mblk_t *mp = allocb(len,BPRI_MED); bcopy(x,mp->b_wptr,len); mp->b_wptr += len; mp->b_datap->db_type = M_PROTO; putnext(&xp->wl,mp); } while(0)
		x = "x75 :po :nk 1 :t3 25 :ad 60 60 :cm 0"; T;
		x = "*ou"; T;
		x = "*co"; T;
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

		case 't':
			x75_T1 (t1arg);
			break;
		case 'T':
			x75_T3 (t3arg);
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
