#ifdef linux
#define ARP_PATH "/sbin/arp"
#define INET_BSD
#include <linux/config.h>
#ifdef CONFIG_INET_BSD
#define SOCK_HAS_LEN
#else
#define SET_STEP
#define ROUTE_IF
#endif
#include <linux/sockios.h>
#endif
#include "primitives.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include "f_ioctl.h"
#include "f_ip.h"
#include <sys/sysmacros.h>
#include "stropts.h"
#include "f_termio.h"
#include <sys/signal.h>
#include <sys/socket.h>
#include <errno.h>
#include <net/if.h>
#ifdef CONFIG_INET_BSD
#include <net/if_slvar.h>
#include <net/slip.h>
#else
#include <linux/if_slip.h>
#endif
#include <arpa/inet.h>
#ifdef SYSV
#include <sys/stream.h>
#endif
#include <netinet/in.h>
#include <netdb.h>
#include <syslog.h>
#ifdef AUX
#include <compat.h>
#endif
#include <fcntl.h>

#include "str_if.h"
#include "sioctl.h"

void
xquit (const char *s)
{
	syslog (LOG_WARNING, "%s: %m", s ? s : "(NULL)");
	exit (3);
}

void
squit (int sig)
{
	syslog (LOG_INFO, "Sig %d", sig);
	xquit (NULL);
}

void
quit (void)
{
	xquit ("Trouble");
}

struct hostent *
host_name (const char *name)
{
	struct hostent *h = gethostbyname (name);

	if (h == NULL) {
		static struct hostent def;
		static struct in_addr defaddr;
		static char *alist[1];
		static char namebuf[128];

		defaddr.s_addr = inet_addr (name);
		if (defaddr.s_addr == -1)
			return NULL;
		strcpy (namebuf, name);
		def.h_name = namebuf;
		def.h_addr_list = alist, def.h_addr = (char *) &defaddr;
		def.h_length = sizeof (struct in_addr);

		def.h_addrtype = AF_INET;
		def.h_aliases = 0;
		h = &def;
	}
	return h;
}

static char ifname[IFNAMSIZ];
static int s;
int devfd = 0;
char *dev = NULL;

char makeroute[2000], unmakeroute[2000];

ushort_t id = 0;
long ich = 0, du = 0, mask = 0;
char *ichaddr, *duaddr, *arpaddr = NULL;
int islinked = 0;
int ifflags = IFF_UP|IFF_POINTOPOINT;
int mtu = 0;

char *destaddr[20];
char **destxaddr = destaddr;

char *desthaddr[20];
char **desthxaddr = desthaddr;

void
enable (void)
{
	int xid;

	if (islinked)
		return;

#ifdef SIOCGIFNAME
	if(ioctl(devfd,SIOCGIFNAME,&ifname) >= 0)
		xid = atoi(strpbrk(ifname,"0123456789"));
	else
#endif
#ifdef SIOCGETU
	if (sioctl (devfd, SIOCGETU, &xid) >= 0)
		sprintf (ifname, "str%d", xid);
	else
#endif
#ifdef SLIOCGUNIT
	if(ioctl(devfd,SLIOCGUNIT,&xid) >= 0)
		sprintf (ifname, "str%d", xid);
	else
#endif
		xquit ("GetU");
	id = xid;

	syslog(LOG_DEBUG,"Setting up IF %s",ifname);
	if(makeroute[0]=='\0') {
		struct hostent *h;
		char **y;

		if ((h = host_name (ichaddr)) == NULL)
				xquit (ichaddr);
		ich = *(long *) h->h_addr;
		if ((h = host_name (duaddr)) == NULL)
				xquit (duaddr);
		du = *(long *) h->h_addr;

		makeroute[0]='\0';
#if 0
		sprintf(makeroute+strlen(makeroute),"set -xv; exec >>/tmp/sll 2>&1 </dev/null; ");
		sprintf(unmakeroute+strlen(unmakeroute),"set -xv; exec >>/tmp/sll 2>&1 </dev/null; ");
#endif

#ifdef ROUTE_IF
		sprintf(makeroute+strlen(makeroute), "%s add -host %s dev %s; ", ROUTE_PATH,ichaddr,ifname);
		sprintf(makeroute+strlen(makeroute), "%s add -host %s gw %s dev %s; ", ROUTE_PATH,duaddr,ichaddr,ifname);
		if(arpaddr != NULL)
			sprintf(makeroute+strlen(makeroute), "%s -s %s %s pub; ", ARP_PATH,duaddr,arpaddr);

		sprintf(unmakeroute+strlen(unmakeroute), "%s del %s; ",ROUTE_PATH,duaddr);
		if(arpaddr != NULL)
			sprintf(unmakeroute+strlen(unmakeroute), "%s -d %s; ",ARP_PATH,duaddr);
#endif

		for(y = destaddr;y != destxaddr;y++) {
			char *route = strchr(*y,':');
			char *foo; char xbuf[40];
			if(route) {
				*route++ = '\0';
#ifdef SET_STEP
				sprintf(xbuf, "%s netmask %s",*y,route);
#else
				sprintf(xbuf, "%s -netmask %s",*y,route);
#endif
				foo = xbuf;
				*--route = ':';
			} else {
				foo = *y;
			}
#ifdef SET_STEP
			sprintf (makeroute+strlen(makeroute), "%s add -net %s gw %s dev %s; ", ROUTE_PATH, foo,duaddr,ifname);
			sprintf (unmakeroute+strlen(unmakeroute), "%s del %s; ", ROUTE_PATH, foo);
#else
			sprintf (makeroute+strlen(makeroute), "%s -- add %s %s; ", ROUTE_PATH, foo,duaddr);
			sprintf (unmakeroute+strlen(unmakeroute), "%s -- delete %s %s; ", ROUTE_PATH, foo,duaddr);
#endif
		}
		for(y = desthaddr;y != desthxaddr;y++) {
#ifdef SET_STEP
			sprintf (makeroute+strlen(makeroute), "%s add -host %s gw %s dev %s; ", ROUTE_PATH, *y,duaddr,ifname);
			if(arpaddr != NULL)
				sprintf(makeroute+strlen(makeroute), "%s -s %s %s pub; ", ARP_PATH, *y, arpaddr);

			sprintf (unmakeroute+strlen(unmakeroute), "%s del %s; ", ROUTE_PATH, *y);
			if(arpaddr != NULL)
				sprintf(unmakeroute+strlen(unmakeroute), "%s -d %s; ",ARP_PATH, *y);
#else
			sprintf (makeroute+strlen(makeroute), "%s -- add %s %s; ", ROUTE_PATH, *y,duaddr);
			sprintf (unmakeroute+strlen(unmakeroute), "%s -- delete %s %s; ", ROUTE_PATH, *y,duaddr);
#endif
		}
	}
#ifndef SET_STEP
	{
		struct ifreq ifr;

		strncpy (ifr.ifr_name, ifname, sizeof (ifr.ifr_name));
		if (ioctl (s, SIOCGIFFLAGS, (caddr_t) & ifr) < 0) {
			syslog (LOG_ERR, "ioctl (SIOCGIFFLAGS): %m");
			quit ();
		}
		ifr.ifr_flags |= ifflags;
		if (ioctl (s, SIOCSIFFLAGS, (caddr_t) & ifr) < 0) {
			syslog (LOG_ERR, "ioctl(SIOCSIFFLAGS): %m");
			quit ();
		}
		if(mtu > 0) {
			ifr.ifr_mtu = mtu;
			if (ioctl (s, SIOCSIFMTU, (caddr_t) & ifr) < 0) {
				syslog (LOG_ERR, "ioctl(SIOCSIFMTU): %m");
				quit ();
			}
		}
	}
#endif
	{
		struct ifreq ifr;

		strncpy (ifr.ifr_name, ifname, sizeof (ifr.ifr_name));
		ifr.ifr_addr.sa_family = AF_INET;
#ifdef SOCK_HAS_LEN
		ifr.ifr_addr.sa_len = sizeof(struct sockaddr_in);
#endif
		if (ioctl (s, SIOCGIFADDR, (caddr_t) & ifr) < 0) {
			if(errno != EADDRNOTAVAIL) {
				syslog (LOG_ERR, "ioctl (SIOCGIFADDR): %m");
				quit ();
			}
		} else {
#ifndef SET_STEP
			if (ioctl (s, SIOCDIFADDR, (caddr_t) & ifr) < 0) {
				syslog (LOG_ERR, "ioctl(SIOCDIFADDR): %m");
				quit ();
			}
#endif
		}

	}
#ifdef SET_STEP
	{
		struct ifreq ifr;

		strncpy (ifr.ifr_name, ifname, sizeof (ifr.ifr_name));
		ifr.ifr_addr.sa_family = AF_INET;
#ifdef SOCK_HAS_LEN
		ifr.ifr_addr.sa_len = sizeof(struct sockaddr_in);
#endif
		((struct sockaddr_in *) & ifr.ifr_dstaddr)->sin_addr.s_addr = ich;
		if (ioctl(s, SIOCSIFADDR, &ifr) < 0) {
			syslog (LOG_ERR, "ioctl(SIOCSIFADDR %d.%d.%d.%d %d.%d.%d.%d): %m",
			((uchar_t *)&ich)[0], ((uchar_t *)&ich)[1], ((uchar_t *)&ich)[2], ((uchar_t *)&ich)[3],
			((uchar_t *)&du )[0], ((uchar_t *)&du )[1], ((uchar_t *)&du )[2], ((uchar_t *)&du )[3]);
			quit();
		}

		ifr.ifr_addr.sa_family = AF_INET;
#ifdef SOCK_HAS_LEN
		ifr.ifr_addr.sa_len = sizeof(struct sockaddr_in);
#endif
		((struct sockaddr_in *) & ifr.ifr_dstaddr)->sin_addr.s_addr = du;
		if (ioctl(s, SIOCSIFDSTADDR, &ifr) < 0) {
			syslog (LOG_ERR, "ioctl(SIOCSIFDSTADDR %d.%d.%d.%d %d.%d.%d.%d): %m",
			((uchar_t *)&ich)[0], ((uchar_t *)&ich)[1], ((uchar_t *)&ich)[2], ((uchar_t *)&ich)[3],
			((uchar_t *)&du )[0], ((uchar_t *)&du )[1], ((uchar_t *)&du )[2], ((uchar_t *)&du )[3]);
			quit();
		}

		if (mask != 0) {
			ifr.ifr_addr.sa_family = AF_INET;
#ifdef SOCK_HAS_LEN
			ifr.ifr_addr.sa_len = sizeof(struct sockaddr_in);
#endif
			((struct sockaddr_in *) & ifr.ifr_netmask)->sin_addr.s_addr = mask;
			if (ioctl(s, SIOCSIFNETMASK, &ifr) < 0) {
				syslog (LOG_ERR, "ioctl(SIOCSIFNETMASK): %m");
				quit();
			}
		}
	}
	{
		struct ifreq ifr;

		strncpy (ifr.ifr_name, ifname, sizeof (ifr.ifr_name));
		if (ioctl (s, SIOCGIFFLAGS, (caddr_t) & ifr) < 0) {
			syslog (LOG_ERR, "ioctl (SIOCGIFFLAGS): %m");
			quit ();
		}
		ifr.ifr_flags |= ifflags;
		if (ioctl (s, SIOCSIFFLAGS, (caddr_t) & ifr) < 0) {
			syslog (LOG_ERR, "ioctl(SIOCSIFFLAGS): %m");
			quit ();
		}
		if(mtu > 0) {
			ifr.ifr_mtu = mtu;
			if (ioctl (s, SIOCSIFMTU, (caddr_t) & ifr) < 0) {
				syslog (LOG_ERR, "ioctl(SIOCSIFMTU): %m");
				quit ();
			}
		}
	}
#else
	{
		struct ifaliasreq ifr;

		bzero ((char *) &ifr, sizeof (ifr));
		strncpy (ifr.ifra_name, ifname, sizeof (ifr.ifra_name));

		ifr.ifra_addr.sa_family = AF_INET;
#ifdef SOCK_HAS_LEN
		ifr.ifra_addr.sa_len = sizeof(struct sockaddr_in);
#endif
		((struct sockaddr_in *) & ifr.ifra_addr)->sin_addr.s_addr = ich;

		ifr.ifra_dstaddr.sa_family = AF_INET;
#ifdef SOCK_HAS_LEN
		ifr.ifra_dstaddr.sa_len = sizeof(struct sockaddr_in);
#endif
		((struct sockaddr_in *) & ifr.ifra_dstaddr)->sin_addr.s_addr = du;

		if (mask != 0) {

			/* ifr.ifra_mask.sa_family = AF_INET; */
#ifdef SOCK_HAS_LEN
			ifr.ifra_mask.sa_len = sizeof(struct sockaddr_in);
#endif
			((struct sockaddr_in *) & ifr.ifra_mask)->sin_addr.s_addr = mask;
	
			if (ioctl (s, SIOCAIFADDR, (caddr_t) & ifr) < 0) {
				syslog (LOG_ERR, "ioctl(SIOCAIFADDR %d.%d.%d.%d %d.%d.%d.%d): %m",
					((uchar_t *)&ich)[0], ((uchar_t *)&ich)[1], ((uchar_t *)&ich)[2], ((uchar_t *)&ich)[3],
					((uchar_t *)&du )[0], ((uchar_t *)&du )[1], ((uchar_t *)&du)[2], ((uchar_t *)&du )[3]);
				xquit ("Dead");
			}
		}
	}
#endif

	islinked = 1;
	if(fork() == 0) {
		signal(SIGCHLD,SIG_DFL);
		fprintf(stderr,"<<%s>>\n",makeroute);
		execl("/bin/sh","sh","-c",makeroute,NULL);
		/* system (makeroute); */
	}
}

void
disable (void)
{
	if (!islinked)
		return;

	syslog(LOG_DEBUG,"Taking down IF %s",ifname);
	if(fork() == 0) {
		signal(SIGCHLD,SIG_DFL);
		fprintf(stderr,"<<%s>>\n",unmakeroute);
		execl("/bin/sh","sh","-c",unmakeroute,NULL);
	}
	{
		struct ifreq ifr;

		strncpy (ifr.ifr_name, ifname, sizeof (ifr.ifr_name));
		if (ioctl (s, SIOCGIFFLAGS, (caddr_t) & ifr) < 0) {
			syslog (LOG_ERR, "ioctl (SIOCGIFFLAGS): %m");
			quit ();
		}
		ifr.ifr_flags &= ~IFF_UP;
		if (ioctl (s, SIOCSIFFLAGS, (caddr_t) & ifr) < 0) {
			syslog (LOG_ERR, "ioctl(SIOCSIFFLAGS): %m");
			quit ();
		}

		strncpy (ifr.ifr_name, ifname, sizeof (ifr.ifr_name));
		ifr.ifr_addr.sa_family = AF_INET;
#ifdef SOCK_HAS_LEN
		ifr.ifr_addr.sa_len = sizeof(struct sockaddr_in);
#endif
		if (ioctl (s, SIOCGIFADDR, (caddr_t) & ifr) < 0) {
			if(errno != EADDRNOTAVAIL) 
				syslog (LOG_ERR, "ioctl (SIOCGIFADDR): %m");
		} else {
#ifndef SET_STEP
			if (ioctl (s, SIOCDIFADDR, (caddr_t) & ifr) < 0) {
				syslog (LOG_ERR, "ioctl(SIOCDIFADDR): %m");
				quit ();
			}
#endif
		}
	}

	islinked = 0;
}

void
disableq (void)
{
	disable ();
	syslog(LOG_INFO,"Exit DisableQ");
	exit (0);
}

void
sigon (void)
{
	syslog (LOG_INFO, "Sig USR1");
	enable ();
}

void
sigoff (void)
{
	syslog (LOG_INFO, "Sig USR2");
	disable ();
}

int
main (int argc, char *argv[])
{
	extern char *optarg;
	extern int optind;
	char *progname = *argv;
	int pid = getpid ();
	int dodial = 0, domon = 0, dovanj = 0, dofakeh = 0, dolog = 0, dologmodem = 0,
	docount = 0, doqinfo = 0, noslip = 0, doenable = 0, dodebug = 0;
	int speed = 0;

#ifdef AUX
	setcompat (COMPAT_BSD);
#endif
	alarm(0);

	openlog (progname, LOG_PID|LOG_PERROR, LOG_LOCAL7);

	signal (SIGUSR1, (sigfunc__t) enable);
	signal (SIGUSR2, (sigfunc__t) disable);
	signal (SIGPWR, (sigfunc__t) disable);
	signal (SIGHUP, (sigfunc__t) disableq);
	signal (SIGINT, (sigfunc__t) disableq);
	signal (SIGTERM, (sigfunc__t) disableq);
	signal (SIGQUIT, (sigfunc__t) disableq);
	signal (SIGCHLD, SIG_IGN);
	signal (SIGTTOU, SIG_IGN);

	while ((s = getopt (argc, argv, "Ddr:R:lEMm:vfLop:s:S:A:")) != EOF)
		switch (s) {
		case 'D':
			dodebug = 1;
			break;
		case 'S':
			noslip = 1;
			break;
		case 's':
			speed = atoi(optarg);
			break;
		case 'l':
			dolog ++;
			break;
		case 'L':
			dologmodem = 1;
			break;
		case 'E':
			doenable = 1;
			break;
		case 'm':
			mtu = atoi(optarg);
			break;
		case 'M':
			domon = 1;
			break;
		case 'f':
			dofakeh = 1;
			break;
		case 'v':
			dovanj = 1;
			break;
		case 'o':
			dodial = 1;
			break;
		case 'p':
			dev = optarg;
			break;
		case 'd':
			*destxaddr++ = "default";
			break;
		case 'R':
			*desthxaddr++ = optarg;
			break;
		case 'r':
			*destxaddr++ = optarg;
			break;
        case 'A': /* Proxy ARP */
            arpaddr = optarg;
            break;
		case '?':
		default:
		  usage:
			fprintf (stderr, "Usage: %s [-d {-r route}... -p device -o -l -m -v -A xx:xx:xx:xx:xx:xx ] from to\n", progname);
			syslog(LOG_ERR,"Usage!");
			exit (99);
		}

	argv += optind;
	argc -= optind;
	if (argc != 2)
		goto usage;

	ichaddr = *argv++;
	duaddr = *argv++;

	if(!dodebug) 
		freopen(stderr,"w","/dev/null");

	if ((s = socket (AF_INET, SOCK_DGRAM, IPPROTO_IP)) < 0)
		xquit ("socket");

	if (dev != NULL) {
		if (dodial)
			if ((devfd = open ("/dev/tty", O_RDWR)) >= 0) {
#ifdef TIOCNOTTY
				(void) ioctl (devfd, TIOCNOTTY, 0);
#endif
				close (devfd);
			}
		if ((devfd = open (dev, O_RDWR)) < 0) {
			syslog (LOG_ERR,"%s: %m",dev);
			exit (1);
		} {
			struct termios ts;

#ifdef UIOCFLOW
			ioctl(devfd,UIOCFLOW,0);
#endif
#ifdef UIOCMODEM
			ioctl(devfd,UIOCMODEM,0);
#endif
			if (ioctl (devfd, TCGETS, &ts) == 0) {
				int i;
				long ocflag = ts.c_cflag;
				for(i = 0; i < NCCS; i++)
                	ts.c_cc[i] = '\0';
				ts.c_cc[VMIN] = 1;
				ts.c_cc[VTIME] = 0;
				ts.c_iflag = (BRKINT);
				ts.c_oflag = (0);
				ts.c_lflag = (0);
				ts.c_cflag = (CRTSCTS|HUPCL|CREAD|CS8);
				if(speed == 0)
					ts.c_cflag |= ocflag&CBAUD;
				else {
					cfsetispeed(&ts,speed);
					cfsetospeed(&ts,speed);
				}
				if (ioctl (devfd, TCSETSF, &ts) != 0) {
					syslog (LOG_ERR,"%s TCSETSF: %m",dev);
					exit (1);
				}
			}
		}
		if (dodial) {
			ioctl (devfd, I_PUSH, "strlog");
			write (devfd, "AT&L1A\r", 7);
			sleep (1);
			ioctl (devfd, TCFLSH, 0);
			ioctl (devfd, I_FLUSH, FLUSHR);
		  dor:
			{
				char c;

				if (read (devfd, &c, 1) != 1)
					syslog (LOG_ERR,"read: %m");
				if (c == '\r' || c == '\n')
					goto dor;
				if (c != 'C') {
					syslog (LOG_ERR,"got %02x: %m",c);
					exit (1);
				}
			}
			sleep (1);
			ioctl (devfd, TCFLSH, 0);
			ioctl (devfd, I_FLUSH, FLUSHR);
			ioctl (devfd, I_POP, NULL);
		}
		if(dologmodem)
			if (ioctl (devfd, I_PUSH, "strlog") != 0) {
				syslog (LOG_ERR,"strlog %m");
				exit (1);
			}
		if(!noslip) {
			if (ioctl (devfd, I_PUSH, "slip") != 0) {
				if(0)syslog (LOG_ERR,"slip: %m");
			}
		}
	}
	if(dolog) {
		doqinfo++; dolog--;
	}
	if(dolog) {
		docount++; dolog--;
		}
	if (dolog) {
		--dolog;
		if (ioctl (devfd, I_PUSH, "strlog") != 0) {
			syslog (LOG_ERR,"strlog: %m");
			exit (1);
		}
	}
	if (dofakeh) {
		if (ioctl (devfd, I_PUSH, "fakeh") != 0) {
			syslog (LOG_ERR,"fakeh: %m");
			exit (1);
		}
	}
	if (dovanj) {
		if (ioctl (devfd, I_PUSH, "van_j") < 0) {
#ifdef SIOCSIFENCAP
			int i = SL_MODE_CSLIP|SL_OPT_ADAPTIVE;
			ioctl(devfd,SIOCSIFENCAP,&i);
#endif
#ifdef SC_COMPRESS
			ifflags |= SC_COMPRESS;
#endif
		}
	}
	if (dolog) {
		--dolog;
		if (ioctl (devfd, I_PUSH, "strlog") != 0) {
			syslog (LOG_ERR,"strlog: %m");
			exit (1);
		}
	}
	if (domon)
		if (ioctl (devfd, I_PUSH, "ip_mon") != 0) {
			syslog (LOG_ERR,"ip_mon: %m");
			exit (1);
		}
	if (dolog) {
		--dolog;
		if (ioctl (devfd, I_PUSH, "strlog") != 0) {
			syslog (LOG_ERR,"strlog: %m");
			exit (1);
		}
	}
	if (docount)
		if (ioctl (devfd, I_PUSH, "count") != 0) {
			syslog (LOG_ERR,"count: %m");
			exit (1);
		}
	if(doqinfo)
		if (ioctl (devfd, I_PUSH, "qinfo") != 0) {
			syslog (LOG_ERR,"qinfo: %m");
			exit (1);
		}
	if(dev != NULL) {
		if (ioctl (devfd, I_PUSH, "str_if") != 0) {
			int oerrno = errno;
			int disc = N_SLIP;

			if(ioctl(devfd,TIOCSETD,&disc) < 0) {
				syslog (LOG_ERR,"str_if: %m");
				errno = oerrno;
				syslog (LOG_ERR,"str_if: %m");
				exit (1);
			}
		}
		if (ioctl (devfd, I_PUSH, "strlog") != 0) {
			syslog (LOG_ERR,"strlog: %m");
		}
		if (ioctl (devfd, I_PUSH, "pr_on") != 0) {
			syslog (LOG_ERR,"pr_on: %m");
		}
		if (0) {
			if (ioctl (devfd, I_PUSH, "strlog") != 0) {
				syslog (LOG_ERR,"strlog: %m");
				exit (1);
			}
		}
	}
	if(dev != NULL || doenable)
		enable ();

#ifdef HAVE_SETPGRP_2
	setpgrp (0, pid);
#endif
#ifdef HAVE_SETPGRP_0
	setpgrp();
#endif
#ifdef TIOCSPGRP
	(void) ioctl (0, TIOCSPGRP, &pid);
#endif

	alarm(0);
	for (;;) {
		char c;
		int i = read (devfd, &c, 1);
		int oerrno;

		switch (i) {
		case -1:
			oerrno = errno;
			syslog (LOG_DEBUG, "Driver: %m");
			if (oerrno != EAGAIN && oerrno != EINTR) {
				if(oerrno == EIO) pause();
				goto Out;
			}
			break;
		case 0:
			goto Out;
		default:
			syslog (LOG_DEBUG, "Got Data %02x from driver -- down?", c);
			(void) ioctl(devfd,I_FLUSH,FLUSHR);
		}
	}
	/* At this point, we have decided to shut down */
  Out:
	syslog (LOG_DEBUG, "Terminating");
	disable();
	exit(1);
}
