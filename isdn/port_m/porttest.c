#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <errno.h>
#include "f_strings.h"
#include <syslog.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <ctype.h>
#include "port_m.h"
#include <sys/stropts.h>
#include <sys/file.h>

#if 0
#define FD_SETSIZE      (sizeof(fd_set) * 8)
#define FD_SET(n, p)    (((fd_set *) (p))->fds_bits[0] |= (1 << ((n) % 32)))
#define FD_CLR(n, p)    (((fd_set *) (p))->fds_bits[0] &= ~(1 << ((n) % 32)))
#define FD_ISSET(n, p)  (((fd_set *) (p))->fds_bits[0] & (1 << ((n) % 32)))
#define FD_ZERO(p)      bzero((char *)(p), sizeof(*(p)))
#endif

void 
Usage (char *progname)
{
    fprintf (stderr, "Usage: %s [ -l ]\n", progname);
    exit (2);
}

int notlocal = 0, notremote = 0;

#define MAXINVAL 100
struct in_addr inval[MAXINVAL];
int next_inval = 0, max_inval = 0;

static int 
is_inval (struct in_addr inv)
{
    int i;

    for (i = 0; i < max_inval; i++) {
	if (inv.s_addr == inval[i].s_addr)
	    return 1;
    }
    return 0;
}

static void 
set_inval (struct in_addr inv)
{
	if(is_inval(inv)) return;

    inval[next_inval] = inv;
    if (++next_inval > max_inval)
	max_inval = next_inval;
    if (next_inval >= MAXINVAL)
	next_inval = 0;
}

main (int argc, char *argv[])
{
    return mann (argc, argv);
}

mann (int argc, char *argv[])
{
    int c;
    extern int optind;
    extern char *optarg;
    int sock;
    int dolog = 0;
    char *progname;
    fd_set fd, fd1;
    char bf1[512];
    int b1;

    if ((progname = strchr (argv[0], '/')) != NULL)
	progname++;
    else
	progname = argv[0];
    while ((c = getopt (argc, argv, "lab")) != EOF)
	switch (c) {
	case '?':
	default:
	    Usage (argv[0]);
	case 'l':
	    dolog = 1;
	    break;
	case 'a':
	    notlocal = 1;
	    break;
	case 'b':
	    notremote = 1;
	    break;
	}
    if (argc - optind != 0)
	Usage (argv[0]);
    argc -= optind;
    argv += optind;

    openlog (progname, LOG_PID, LOG_USER);
    if ((sock = open (NAME_PORTMAN, O_RDWR)) < 0) {
	syslog (LOG_ERR, " Open %s: %m\n", NAME_PORTMAN);
	exit (1);
    }
    ioctl(sock,I_SRDOPT,RMSGN);
    if (dolog)
	(void) ioctl (sock, I_PUSH, "strlog");
    FD_ZERO (&fd);
    FD_SET (0, &fd);
    FD_SET (sock, &fd);
    b1 = 0;
    while (FD_ISSET (0, &fd) && FD_ISSET (sock, &fd)) {
	bcopy (&fd, &fd1, sizeof (fd));
	if (select (sock + 1, &fd1, NULL, NULL, NULL) < 0) {
	    syslog (LOG_ERR, "Select: %m\n");
	    exit (1);
	}
	if (FD_ISSET (0, &fd1)) {
	    int i = read (0, bf1 + b1, sizeof (bf1) - b1 - 1);

	    if (i <= 0) {
		FD_CLR (0, &fd);
		if (i < 0)
		    syslog (LOG_ERR, "> > ERROR %m\n");
		else
		    syslog (LOG_INFO, "> > CLOSED");
		close (0);
	    } else {
		b1 += i;
		for (i = 0; i < b1; i++) {
		    if (bf1[i] == '\n' || bf1[i] == '\0' || i == sizeof (bf1)) {
			if (i > 0) {
			    int j = i;
			    bf1[i] = '\0';
			    if (i > 0 && bf1[i - 1] == '\r')
				bf1[--j] = '\0';
			    if (write (sock, bf1, j) != j) {
				perror ("Write Mon: %m");
				exit (3);
			    }
			}
			bcopy (bf1 + i + 1, bf1, b1 - i - 1);
			b1 -= i + 1;
			i = -1;
		    }
		}
	    }
	}
	if (FD_ISSET (sock, &fd1)) {
	    char bf2[256];
	    int i = read (sock, bf2,sizeof(bf2));

	    if (i < 1) {
		FD_CLR (sock, &fd);
		if (i < 0)
		    syslog (LOG_ERR, "< < ERROR %m\n");
		else
		    syslog (LOG_INFO, "< < CLOSED\n");
		close (sock);
	    } else {
		write (1, "* ",2);
		write (1, bf2, i);
		write (1, "\n", 1);
	    }
	}
    }
}

