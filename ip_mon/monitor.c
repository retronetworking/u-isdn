#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <signal.h>
#include <setjmp.h>
#include <errno.h>
#include "f_strings.h"
#include <syslog.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <ctype.h>
#include "ip_mon.h"
#include <sys/stropts.h>
#include <sys/file.h>
#include <fcntl.h>
#ifdef linux
#include <linux/fs.h>
#endif

#if 0
#define FD_SETSIZE      (sizeof(fd_set) * 8)
#define FD_SET(n, p)    (((fd_set *) (p))->fds_bits[0] |= (1 << ((n) % 32)))
#define FD_CLR(n, p)    (((fd_set *) (p))->fds_bits[0] &= ~(1 << ((n) % 32)))
#define FD_ISSET(n, p)  (((fd_set *) (p))->fds_bits[0] & (1 << ((n) % 32)))
#define FD_ZERO(p)      bzero((char *)(p), sizeof(*(p)))
#endif

void chkall(void) { } 

void
Usage (char *progname)
{
	fprintf (stderr, "Usage: %s [ -l ]\n", progname);
	exit (2);
}

int notlocal = 0, notremote = 0, notprotocol = 0, logtime = 0, lastpri;
int dosetent = 1, dohostsetent = 1;

#define MAXINVAL 100
unsigned long inval[MAXINVAL];
uchar_t invalc[MAXINVAL];
int next_inval = 0, max_inval = 0;

static int
is_inval (unsigned long inv)
{
	int i;

	for (i = 0; i < max_inval; i++) {
		if (inv == inval[i]) {
			if(--invalc[i])
				return 1;
			bzero(&inval[i], sizeof(inval[i]));
			return 0;
		}
	}
	return 0;
}

static void
set_inval (unsigned long inv)
{
	if (is_inval (inv))
		return;

	inval[next_inval] = inv;
	invalc[next_inval] = 5;
	if (++next_inval > max_inval)
		max_inval = next_inval;
	if (next_inval >= MAXINVAL)
		next_inval = 0;
}

int main (int argc, char *argv[])
{
	int c;
	extern int optind;
	extern char *optarg;
	int sock;
	int dolog = 0;
	int ignstdin = 1;
	char *progname;
	fd_set fd, fd1;
	char bf1[512];
	int b1;

	setvbuf (stdout, NULL, _IOLBF, 512);
	if ((progname = strchr (argv[0], '/')) != NULL)
		progname++;
	else
		progname = argv[0];
	while ((c = getopt (argc, argv, "Iilabctmnh")) != EOF)
		switch (c) {
		case '?':
		default:
			Usage (argv[0]);
		case 'I':
			ignstdin=0;
			break;
		case 'i':
			ignstdin=1;
			break;
		case 't':
			logtime = 1;
			break;
		case 'l':
			dolog++;
			break;
		case 'a':
			notlocal = 1;
			break;
		case 'b':
			notremote = 1;
			break;
		case 'c':
			notprotocol = 1;
			break;
		case 'h':
			dohostsetent = 0;
			break;
		case 'n':
			dosetent = 0;
			break;
		}
	if (argc - optind != 0)
		Usage (argv[0]);
	argc -= optind;
	argv += optind;

	if(dosetent) {
		setprotoent(1); setservent(1);
	} else {
		setprotoent(0); setservent(0);
	}
	if(dohostsetent) {
		sethostent(1);
	} else {
		sethostent(0);
	}

	openlog (progname, LOG_PID, LOG_USER);
	if(logtime) 
		printf("*%ld\n",lastpri = time(NULL));
#ifdef linux
	{
		FILE * fd;
		if((fd = fopen("/proc/devices","r")) == NULL)
			syslog(LOG_ERR,"Reading device numbers: %m");
		else {
			char xx[80];
			int len, monitordev = 0;

			while(fgets(xx,sizeof(xx)-1,fd) != NULL) {
				char *x = xx;
				len = strlen(xx);
				if(len>0 && xx[len-1]=='\n')
					xx[len-1]='\0';
				while(*x != '\0' && isspace(*x))
					x++;
				if(isdigit(*x)) {
					int devnum = atoi(x);
					while(*x != '\0' && isdigit(*x)) x++;
					while(*x != '\0' && isspace(*x)) x++;
					if((strcmp(x,"ip_mon") == 0) || (strcmp(x,"ipmon") == 0))
						monitordev = devnum;
				}
			}
			fclose(fd);
			if(monitordev == 0)
				syslog(LOG_CRIT, "No IP monitor driver found!");
			else {
				int i;

				unlink(IP_MON_NAME);
				mknod (IP_MON_NAME, S_IFCHR | S_IRUSR | S_IWUSR, MKDEV(monitordev,0));
				syslog(LOG_DEBUG,"ISDN: monitor: major number %d\n",monitordev);
			}
		}
	}
#endif
	if ((sock = open (IP_MON_NAME, O_RDWR)) < 0) {
		syslog (LOG_ERR, " Open %s: %m\n", IP_MON_NAME);
		exit (1);
	}
	ioctl (sock, I_SRDOPT, RMSGN);
	if (dolog--) {
		if (dolog--)
			(void) ioctl (sock, I_PUSH, "strlog");
		(void) ioctl (sock, I_PUSH, "qinfo");
	}
	FD_ZERO (&fd);
	if(!ignstdin)
		FD_SET (0, &fd);
	FD_SET (sock, &fd);
	b1 = 0;
	while ((ignstdin || FD_ISSET (0, &fd)) && FD_ISSET (sock, &fd)) {
		bcopy (&fd, &fd1, sizeof (fd));
		if (select (sock + 1, &fd1, NULL, NULL, NULL) < 0) {
			syslog (LOG_ERR, "Select: %m\n");
			exit (1);
		}
		if (FD_ISSET (0, &fd1)) {
			struct _monitor mon;
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
							bf1[i] = '\0';
							if (i > 0 && bf1[i - 1] == '\r')
								bf1[i - 1] = '\0';
							if (tomon (bf1, &mon) < 0)
								continue;
							if (write (sock, &mon, sizeof (mon)) != sizeof (mon)) {
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
			struct _monitor mon;
			int i = read (sock, &mon, sizeof (mon));

			if (i != sizeof (mon)) {
				FD_CLR (sock, &fd);
				if (i < 0)
					syslog (LOG_ERR, "< < ERROR %m\n");
				else if (i > 0)
					syslog (LOG_ERR, "< < %d bytes?", i);
				else
					syslog (LOG_INFO, "< < CLOSED\n");
				close (sock);
			} else {
				char buf[512];

				if (tostr (buf, &mon) < 0)
					continue;
				strcat(buf,"\n");
				write (1, buf, strlen (buf));
			}
		}
	}
	return 0;
}

int
tomon (char *bf, struct _monitor *mon)
{
	struct sockaddr_in server;
	struct hostent *hp, *gethostbyname ();

	char a[256], b[256];
	int c, d;

	if (sscanf (bf, "%s %s %d %d", a, b, &c, &d) != 4)
		return -1;
	bzero (mon, sizeof (mon));
	mon->known = 1;
	mon->cap_p = c;
	mon->cap_b = d;

	if (isdigit (a[0]))
		mon->local = inet_addr (a);
	else {
		hp = gethostbyname (a);
		if (hp == NULL)
			return -1;
		bcopy (hp->h_addr, &mon->local, hp->h_length);
	}

	if (isdigit (b[0])) {
		mon->remote = inet_addr (b);
	} else {
		hp = gethostbyname (a);
		if (hp == NULL)
			return -1;
		bcopy (hp->h_addr, &mon->remote, hp->h_length);
	}
	return 0;
}

jmp_buf jp;
void alju(int nix) { longjmp(jp,1); }

int
tostr (char *bf, struct _monitor *mon)
{
	struct sockaddr_in server;
	struct hostent *hp;
	struct protoent *proto;
	struct servent *serv1, *serv2;
	char a[256], b[256];

	if (mon->sofar_p == 0 && mon->sofar_b == 0)
		return -1;

	signal(SIGALRM,alju);
	alarm(30);

	if (!notlocal && !is_inval (mon->local) && !setjmp(jp) && (hp = gethostbyaddr ((char *)&mon->local, sizeof (mon->local), AF_INET)) != NULL) {
		strncpy (a, hp->h_name, sizeof (a));
	} else {
		if (!notlocal)
			set_inval (mon->local);
		sprintf (a, "_%d.%d.%d.%d",
				(uchar_t)((ntohl(mon->local) >> 24) & 0xFF),
				(uchar_t)((ntohl(mon->local) >> 16) & 0xFF),
				(uchar_t)((ntohl(mon->local) >> 8) & 0xFF),
				(uchar_t)(ntohl(mon->local) & 0xFF));
	}

	signal(SIGALRM,alju);
	(void)alarm(30);
	if (!notremote && !is_inval (mon->remote) && !setjmp(jp) && (hp = gethostbyaddr ((char *)&mon->remote, sizeof (mon->remote), AF_INET)) != NULL) {
		strncpy (b, hp->h_name, sizeof (b));
	} else {
		if (!notremote)
			set_inval (mon->remote);
		sprintf (b, "_%d.%d.%d.%d",
				(uchar_t)((ntohl(mon->remote) >> 24) & 0xFF),
				(uchar_t)((ntohl(mon->remote) >> 16) & 0xFF),
				(uchar_t)((ntohl(mon->remote) >> 8) & 0xFF),
				(uchar_t)(ntohl(mon->remote) & 0xFF));
	}
	signal(SIGALRM,SIG_IGN);
	alarm(0);
	if(logtime) {
		unsigned long thispri = time(NULL);
		bf += sprintf(bf,"%d:",thispri-lastpri);
		lastpri=thispri;
	}
	bf += sprintf (bf, "%s %s %d %d  ", a, b, mon->sofar_p, mon->sofar_b);
	if(!notprotocol && (proto = getprotobynumber(mon->p_protocol)) != NULL) {
		if((serv1 = getservbyport((mon->p_local), proto->p_name)) != NULL)
			strcpy(a, serv1->s_name);
		else 
			sprintf(a,"_%u",ntohs(mon->p_local));
		if((serv2 = getservbyport((mon->p_remote), proto->p_name)) != NULL) 
			strcpy(b, serv2->s_name);
		else
			sprintf(b,"_%u",ntohs(mon->p_remote));
		bf += sprintf (bf, "%s %s %s",proto->p_name,a,b);
	} else {
		bf += sprintf (bf, "_%d _%u _%u", mon->p_protocol, ntohs(mon->p_local), ntohs(mon->p_remote));
	}
	return 0;
}
