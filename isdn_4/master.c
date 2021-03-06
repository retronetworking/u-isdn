/*
 *
 * ISDN master program.
 *
 * Copyright (c) 1993-1995 Matthias Urlichs <urlichs@noris.de>.
 */


#define MASTER_MAIN
#include "master.h"
#include <sys/mman.h>

#if __GNU_LIBRARY__ - 0 == 6 /* NOCH notwendig, fehlt in glibc */
#include <syscall.h>
_syscall1(int,mlockall,int,what);
#endif

int
main (int argc, char *argv[])
{
	extern char *optarg;
	extern int optind;

	char *devnam = "/dev/isdnmon";
	int pushlog = 0;
	int debug = 0;
	int x;
	
	mlockall(MCL_CURRENT | MCL_FUTURE);
	MALLOC_INIT();

#ifdef DO_DEBUG_MALLOC
	mcheck(NULL);
	mmtrace();
#endif
	{
		struct passwd *pw;

		if ((pw = getpwnam (ROOTUSER)) != NULL) {
			rootuser  = pw->pw_uid;
			/* rootgroup = pw->pw_gid; */
		}
	}

	chkall();
	setlinebuf(stdout); setlinebuf(stderr); /* no(t much) buffering */

	progname = strrchr (*argv, '/');	/* basename */
	if (progname == NULL)
		progname = *argv;
	else
		progname++;

	while ((x = getopt (argc, argv, "iIf:dlLwWqQmM"))!= EOF) {
		switch (x) {
		/*
		 * Logging. Small letters are usermode, capitals log in the kernel.
		 * 'l' is raw data, 'w' is interpreted data, 'q' is queue info.
		 */
		case 'm':
			log_34 |= 1;
			break;
		case 'M':
			log_34 |= 2;
			break;
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

		case 'd': /* do not fork */
			debug = 1;
			break;
		case 'I': /* for testing */
			igstdin = 0;
			break;
		case 'i':
			igstdin = 1;
			break;
		case 'f': /* alternate device to open */
			if (getuid ()!= 0)
				goto usage;
			devnam = optarg;
			break;
		default:
		  usage:
			fprintf (stderr, "Usage: %s -iIdlLwWqQmM -f masterfile cf-datei...\n", progname);
			fprintf (stderr, "   (READ THE DOCUMENTATION if you want to know what the options mean.)\n");
			exit (1);
		}
	}
	if(argv[optind] == NULL)
		goto usage;

	openlog (progname, debug ? LOG_PERROR : 0, LOG_LOCAL7);

	/* Remember all files to scan */
	fileargs = &argv[optind];
	read_args (NULL); /* This also schedules starting all configured programs */

	seteuid (getuid ());

	if (!debug) { /* Disassociate */
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
		open("/dev/null",O_RDWR);
		dup(0); dup(0);
#ifdef HAVE_SETPGRP_2
		setpgrp (0, getpid ());
#endif
#ifdef HAVE_SETPGRP_0
		setpgrp();
#endif
	} 
	{ /* Switch stdout and stderr */
		int fd = dup(1);
		dup2(2,1);
		dup2(fd,2);
		close(fd);
	}
	/* Two lockdev() calls. The first may clear a stale lock. */
	if((lockdev(0,0) < 0) && (sleep(2),lockdev(0,0) < 0)) {
		syslog(LOG_ERR,"Unable to lock the master device: %m");
		_exit(1);
	}

#ifdef linux
	{	/* (Re)Create all our device files */
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
				mknod ("/dev/isdnmon", S_IFCHR | S_IRUSR | S_IWUSR, makedev(isdnstd,0));

				for(i=1;i<NPORT;i++) {
					mknod (idevname (i), S_IFCHR | S_IRUSR | S_IWUSR, makedev(isdnstd,i));
					chmod (idevname (i), S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
				}
				if(0)syslog(LOG_DEBUG,"ISDN: isdn/XX: major number %d",isdnstd);
			}
				
			if(isdnterm == 0) 
				syslog(LOG_CRIT,"No ISDN terminal device found!");
			else {
				int i;
				for(i=1;i<NPORT;i++)
					unlink(devname(i));
				if(0)syslog(LOG_DEBUG,"ISDN: ttyiXX: major number %d",isdnterm);
			}
		}
	}
#endif

	/* Standard signal handling -- TODO: Use sigaction() instead. */
	bsd_signal (SIGALRM, (sigfunc__t) alarmsig);
	bsd_signal (SIGPIPE, SIG_IGN);
	bsd_signal (SIGHUP, (void *)do_quitnow);
	bsd_signal (SIGINT, (void *)do_quitnow); /* Always these "incompatible" pointers... */
	bsd_signal (SIGTERM, (void *)do_quitnow); /* Always these "incompatible" pointers... */
	bsd_signal (SIGQUIT, (sigfunc__t) do_quitnow);
	bsd_signal (SIGUSR1, (sigfunc__t) kill_progs);

	/* Create a stream within the program */
	xs_mon = stropen (0);
	if (xs_mon == NULL)
		xquit ("xs_mon = NULL", "");

	/* Open the device and push kernel stream modules */
	fd_mon = open (devnam, O_RDWR);
	if (fd_mon < 0)
		xquit ("Open Dev", devnam);
	if (ioctl (fd_mon, I_SRDOPT, RMSGN) < 0) /* Message mode */
		xquit ("SetStrOpt", "");
	if (ioctl (fd_mon, I_PUSH, "buffer") < 0)
		syslog(LOG_WARNING,"Buffer module not found -- unreliable");

	if (pushlog & 2)
		if (ioctl (fd_mon, I_PUSH, "strlog") < 0)
			xquit ("Push", "strlog 1");
	if (pushlog & 8)
		if (ioctl (fd_mon, I_PUSH, "logh") < 0)
			xquit ("Push", "strlog 1");
	if (pushlog & 32)
		if (ioctl (fd_mon, I_PUSH, "qinfo") < 0)
			xquit ("Push", "strlog 1");
#if LEVEL > 3
	if (ioctl (fd_mon, I_PUSH, "isdn_3") < 0)
		xquit ("ISDN_3 module");
#endif
	{	/* Associate the kernel stream with the user stream */
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
		/* push user-mode streams modules */
		if (pushlog & 1)
			strioctl (xs_mon, I_PUSH, (long) "strlog");
		if (pushlog & 4)
			strioctl (xs_mon, I_PUSH, (long) "logh");
		if (pushlog & 16)
			strioctl (xs_mon, I_PUSH, (long) "qinfo");
		strioctl (xs_mon, I_PUSH, (long) "isdn_3");
#endif
	}

	if(0)callout_async (); /* We prefer to do everything synchronously */
	timeout (log_idle, NULL, 5 * HZ); /* Tell everybody that we're still alive */
	timeout (queue_idle, NULL, HZ/3); /* process the user-mode streams */
	syspoll ();	/* Now go and do some work */

	/* Shut down. Ungracefully. Graceful shutdown of active cards TODO. */
	strclose (xs_mon, 0);
	unlockdev(0);
	return 0; /* -> exit(0) */
}

