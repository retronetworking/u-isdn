/*
 *
 * ISDN master program.
 *
 * Copyright (c) 1993-1995 Matthias Urlichs <urlichs@noris.de>.
 */


#define MASTER_MAIN
#include "master.h"

int
main (int argc, char *argv[])
{
	extern char *optarg;
	extern int optind;

	char *devnam = "/dev/isdnmon";
	char *execf = NULL;
	int pushlog = 0;
	int debug = 0;
	int x;

#ifdef linux
	reboot(0xfee1dead,0x17392634,1);		/* Magic to make me nonswappable */
#endif
#ifdef DO_DEBUG_MALLOC
	mcheck(NULL);
	mmtrace();
#endif
	chkall();
	setlinebuf(stdout); setlinebuf(stderr);

	progname = strrchr (*argv, '/');	/* basename */
	if (progname == NULL)
		progname = *argv;
	else
		progname++;

	while ((x = getopt (argc, argv, "iItf:x:dlLwWqQ"))!= EOF) {
		switch (x) {
		case 'l':
			pushlog |= 1;
			break;
		case 'L':
			pushlog |= 2;
			break;
		case 'w':
			pushlog |= 4;
			break;
		case 'W':
			pushlog |= 8;
			break;
		case 'q':
			pushlog |= 16;
			break;
		case 'Q':
			pushlog |= 32;
			break;
		case 'd':
			debug = 1;
			break;
		case 't':
			testonly = 1;
			break;
		case 'x':
			if (execf != NULL)
				goto usage;
			execf = optarg;
			break;
		case 'I':
			igstdin = 0;
			break;
		case 'i':
			igstdin = 1;
			break;
		case 'f':
			if (getuid ()!= 0)
				goto usage;
			devnam = optarg;
			break;
		default:
		  usage:
			fprintf (stderr, "Usage: %s -t[est] -x Data -l[ogging] -F epromfile -f devfile\n"
					"\t-M load+direct+Leo -m direct+Leo \n", progname);
			exit (1);
		}
	}
	openlog (progname, debug ? LOG_PERROR : 0, LOG_LOCAL7);

	fileargs = &argv[optind];
	read_args (NULL);

	seteuid (getuid ());

	if (!debug) {
		switch (fork ()){
		case -1:
			xquit ("fork", NULL);
		case 0:
			break;
		default:
			exit (0);
		}

#ifdef TIOCNOTTY
		{
			int a = open ("/dev/tty", O_RDWR);

			if (a >= 0) {
				(void) ioctl (a, TIOCNOTTY, NULL);
				close (a);
			}
		}
#endif
		{
			int i;
			for (i=getdtablesize()-1;i>=0;i--)
				(void) close(i);
		}
		igstdin=1;
		open("/dev/null",O_RDWR);
		dup(0); dup(0);
#ifdef HAVE_SETPGRP_2
		setpgrp (0, getpid ());
#endif
#ifdef HAVE_SETPGRP_0
		setpgrp();
#endif
	}

#ifdef linux
	{
		FILE * fd;
		if((fd = fopen("/proc/devices","r")) == NULL)
			syslog(LOG_ERR,"Reading device numbers: %m");
		else {
			char xx[80];
			int len;

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
					if(strcmp(x,"tisdn") == 0) 
						isdnterm = devnum;
					else if(strcmp(x,"isdn") == 0) 
						isdnstd = devnum;
				}
			}
			fclose(fd);
			if(isdnstd == 0)
				syslog(LOG_CRIT, "No ISDN driver found!");
			else {
				int i;

				system("rm -rf /dev/isdn /dev/isdnmon");
				mkdir("/dev/isdn",0755);
				mknod ("/dev/isdnmon", S_IFCHR | S_IRUSR | S_IWUSR, MKDEV(isdnstd,0));

				for(i=1;i<NPORT;i++) {
					mknod (idevname (i), S_IFCHR | S_IRUSR | S_IWUSR, MKDEV(isdnstd,i));
					chmod (idevname (i), S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
				}
				syslog(LOG_DEBUG,"ISDN: isdn/XX: major number %d",isdnstd);
			}
				
			if(isdnterm == 0) 
				syslog(LOG_CRIT,"No ISDN terminal device found!");
			else {
				int i;
				for(i=1;i<NPORT;i++)
					unlink(devname(i));
				syslog(LOG_DEBUG,"ISDN: ttyiXX: major number %d",isdnterm);
			}
		}
	}
#endif

	signal (SIGALRM, (sigfunc__t) alarmsig);
	signal (SIGPIPE, SIG_IGN);
	if(signal(SIGHUP,SIG_IGN) != SIG_IGN)
		signal (SIGHUP, SIG_DFL);
	if(signal(SIGINT,SIG_IGN) != SIG_IGN)
		signal (SIGINT, do_quitnow);
	signal (SIGQUIT, (sigfunc__t) do_quitnow);
	signal (SIGUSR1, (sigfunc__t) kill_progs);
	signal (SIGCHLD, (sigfunc__t) deadkid);

	chkall();
	xs_mon = stropen (0);
	if (xs_mon == NULL)
		xquit ("xs_mon = NULL", "");

	if (!testonly) {
	fd_mon = open (devnam, O_RDWR);
	if (fd_mon < 0)
		xquit ("Open Dev", devnam);
		if (ioctl (fd_mon, I_SRDOPT, RMSGN) < 0)
			if (errno != ENOTTY)
		xquit ("SetStrOpt", "");

	if (pushlog & 2)
		if (ioctl (fd_mon, I_PUSH, "strlog") < 0)
				if (errno != ENOTTY)
			xquit ("Push", "strlog 1");
	if (pushlog & 8)
		if (ioctl (fd_mon, I_PUSH, "logh") < 0)
				if (errno != ENOTTY)
			xquit ("Push", "strlog 1");
	if (pushlog & 32)
		if (ioctl (fd_mon, I_PUSH, "qinfo") < 0)
				if (errno != ENOTTY)
			xquit ("Push", "strlog 1");
#if LEVEL < 4
		if (xnkernel) {
			if (ioctl (fd_mon, I_PUSH, "isdn_3") < 0) {
				syslog (LOG_ERR, "No kernel ISDN module: %m");
				xnkernel = 0;
			} else {
				if (pushlog & 1)
					if (ioctl (fd_mon, I_PUSH, "strlog") < 0)
						xquit ("Push", "strlog 1");
				if (pushlog & 4)
					if (ioctl (fd_mon, I_PUSH, "logh") < 0)
						xquit ("Push", "strlog 1");
				if (pushlog & 16)
					if (ioctl (fd_mon, I_PUSH, "qinfo") < 0)
						xquit ("Push", "strlog 1");
			}
		}
#endif
	} {
		typedef int (*F) ();
		extern void isdn3_init (void);
		extern void isdn_init (void);
		extern qf_put strrput;
		extern qf_srv strwsrv;
		extern struct streamtab strloginfo, loghinfo;
#if LEVEL < 4
		extern struct streamtab isdn3_info;
#endif
		extern struct qinit stwhdata, stwldata;
		extern struct module_info strhm_info;
		static struct qinit xhr =
		{strrput, do_h, NULL, NULL, NULL, &strhm_info, NULL};
		static struct qinit xlr =
		{strrput, do_l, NULL, NULL, NULL, &strhm_info, NULL};

#if LEVEL < 4
		isdn3_init ();
#endif
		setq (&xs_mon->rh, &xhr, &stwhdata);
		setq (&xs_mon->wl, &stwldata, &xlr);
#if LEVEL < 4
		register_strmod (&isdn3_info);
#endif
		register_strmod (&strloginfo);
		register_strmod (&loghinfo);
#if LEVEL < 4
		if (!xnkernel || testonly) {
		if (pushlog & 1)
				strioctl (xs_mon, I_PUSH, (long) "log");
		if (pushlog & 4)
			strioctl (xs_mon, I_PUSH, (long) "logh");
		if (pushlog & 16)
			strioctl (xs_mon, I_PUSH, (long) "qinfo");
		strioctl (xs_mon, I_PUSH, (long) "isdn_3");
		}
#endif
	}

	if(0)callout_async ();
	timeout (log_idle, NULL, 5 * HZ);
	timeout (queue_idle, NULL, HZ);
	if (execf != NULL) {
		FILE *efile = fopen (execf, "r");

		if (efile == NULL)
			xquit ("Exec File", execf);
	}
	syspoll ();

	strclose (xs_mon, 0);
	exit (0);
	return 0;
}
