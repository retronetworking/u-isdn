/*
 * This file is part of the ISDN master program.
 *
 * Copyright (C) 1995 Matthias Urlichs.
 * See the file COPYING for license details.
 */

#include "master.h"
#include "isdn_12.h"

/* Handle dead processes */
void
deadkid (void)
{
	int pid, val = 0, has_dead = 0;
	struct conninfo *conn;

	while((pid = wait4 (-1,&val,WNOHANG,NULL)) > 0) {
		printf ("\n* PID %d died, %x\n", pid, val);

		chkall();
		for (conn = theconn; conn != NULL; conn = conn ? conn->next : NULL) {
			if(conn->ignore)
				continue;
			if (conn->pid == pid) {
				conn->pid = 0;
				if(conn->flags & F_PERMANENT)
					has_dead = 1;
				if (conn->minor == 0)
					dropconn (conn);
				else
					ReportConn(conn);
				break;
			} else {
				struct proginfo **prog;

				for (prog = &conn->run; prog != NULL && *prog != NULL; prog = &(*prog)->next) {
					if ((*prog)->pid == pid) {
						struct proginfo *fprog = *prog;

						*prog = fprog->next;
#if 0
						fprog->pid = 0;
						if (fprog->die) {
							struct proginfo *sprog;
							for (sprog = conn->run; sprog != NULL; sprog = sprog->next) {
								if (sprog->die)
									break;
							}
#if 0
							if (sprog == NULL)
								kill (SIGTERM, conn->pid);
#endif
						}
#endif
						free (fprog);
						conn = NULL;
						break;
					}
				}
			}
		}
	}
	if(has_dead) {
		in_boot=1;
		do_run_now++;
		timeout(run_now,NULL,3*HZ);
	}
	signal (SIGCHLD, (sigfunc__t) deadkid);
}


/* Push protocols onto stream */
int
pushprot (conngrab cg, int minor, char update)
{
	cf prot;
	char *mods = NULL;

	for (prot = cf_ML; prot != NULL; prot = prot->next) {
		if(!matchflag(cg->flags,prot->type)) continue;
		if (!wildmatch (cg->site, prot->site)) continue;
		if (!wildmatch (cg->protocol, prot->protocol)) continue;
		if (!wildmatch (cg->card, prot->card)) continue;
		if (!classmatch (cg->cclass, prot->cclass)) continue;
		break;
	}
	if (prot == NULL)
		return -ENOENT;
	if(update) 
		cg->flags = (cg->flags & ~F_SETINITIAL) | F_SETLATER;
	else
		cg->flags = (cg->flags & ~F_SETLATER) | F_SETINITIAL;
	if (minor != 0) {
		char *sp1, *sp2;
		char *sx;

		mblk_t *mj = allocb (60 + strlen (prot->args), BPRI_LO);
		int len;
		mods = prot->args;

		m_putid (mj, CMD_PROT);
		m_putsx (mj, ARG_MINOR);
		m_puti (mj, minor);
		m_putdelim (mj);
		if(update)
			m_putid (mj, PROTO_UPDATEMODLIST);
		else
			m_putid (mj, PROTO_MODLIST);
		m_putsx (mj, ARG_MODE); /* set card mode */
		m_putsz (mj, prot->arg);
		m_putdelim (mj);
		m_putsz(mj, mods);
		len = mj->b_wptr - mj->b_rptr;
		DUMPW (mj->b_rptr, len);
		(void) strwrite (xs_mon, (uchar_t *) mj->b_rptr, len, 1);
		freeb (mj);

		sx = (char *)malloc (strlen (prot->args) + 5 + strlen (PROTO_NAME));
		if (sx == NULL)
			return -ENOMEM;
		sprintf (sx, " %s %s", prot->args, PROTO_NAME);
		sp1 = sx;
		while (*sp1 != '\0' && !isspace (*sp1))
			sp1++;
		while (*sp1 != '\0' && isspace (*sp1))
			sp1++;
		for (sp2 = sp1; *sp1 != '\0'; sp1 = sp2) {
			cf cm;
			mblk_t *mi;

			while (*sp2 != '\0' && !isspace (*sp2))
				sp2++;
			if(*sp2 != '\0')
				*sp2++ = '\0';
			if ((mi = allocb (256, BPRI_MED)) == NULL) {
				free (sx);
				return -ENOMEM;
			}
			for (cm = cf_MP; cm != NULL; cm = cm->next) {
				ushort_t id;
				streamchar *newlim = NULL;
				mblk_t *mz = NULL;

				if(!matchflag(cg->flags,cm->type)) continue;
				if (!wildmatch (cg->site, cm->site)) continue;
				if (!wildmatch (cg->protocol, cm->protocol)) continue;
				if (!wildmatch (cg->card, cm->card)) continue;
				if (!classmatch (cg->cclass, cm->cclass)) continue;
				if (!wildmatch (sp1, cm->arg)) continue;

				mz = allocsb (strlen (cm->args), (streamchar *)cm->args);

				newlim = mz->b_wptr;
				while (m_getsx (mz, &id) == 0) {
					switch (id) {
					default:
						{
							ushort_t id2;
							streamchar *news = NULL;
							streamchar *olds = mi->b_rptr;

							while (m_getsx (mi, &id2) == 0) {
								if (id != id2)
									continue;
								mi->b_rptr = olds;
								goto skip;
							}
							mi->b_rptr = olds;
							m_putsx (mi, id);
							olds = mi->b_wptr;
							*olds++ = ' ';
							m_getskip (mz);
							news = mz->b_rptr;
							while (news < newlim && olds < mi->b_datap->db_lim
									&& *news != ':')
								*olds++ = *news++;
							mi->b_wptr = olds;
							mz->b_rptr = news;
						} break;
					}
				  skip:;
				}
				freeb (mz);
				if (strchr(cm->type,'X'))
					break;
			}
			if (mi->b_rptr < mi->b_wptr) {
				struct iovec io[2];
				mblk_t *mj = allocb (50, BPRI_LO);

				m_putid (mj, CMD_PROT);
				m_putsx (mj, ARG_MINOR);
				m_puti (mj, minor);
				m_putdelim (mj);
				m_putid (mj, PROTO_MODULE);
				m_putsx (mj, PROTO_MODULE);
				m_putsz (mj, (uchar_t *) sp1);	/* Delimiter to mi pushed by
												 * m_putsx */
				io[0].iov_base = mj->b_rptr;
				io[0].iov_len = mj->b_wptr - mj->b_rptr;
				io[1].iov_base = mi->b_rptr;
				io[1].iov_len = mi->b_wptr - mi->b_rptr;
				DUMPW (mj->b_rptr, io[0].iov_len);
				printf ("+ ");
				DUMPW (mi->b_rptr, io[1].iov_len);
				(void) strwritev (xs_mon, io, 2, 1);
				freeb (mj);
			}
			freeb (mi);
		}
		free (sx);
		if(!update) {
			mblk_t *mj = allocb (32, BPRI_LO);
			int len;

			if (mj == NULL)
				return -ENOMEM;
			m_putid (mj, CMD_PROT);
			m_putsx (mj, ARG_MINOR);
			m_puti (mj, minor);
			m_putdelim (mj);
			m_putc (mj, PROTO_MODE);
			len = mj->b_wptr - mj->b_rptr;
			DUMPW (mj->b_rptr, len);
			(void) strwrite (xs_mon, (uchar_t *) mj->b_rptr, len, 1);
			freeb (mj);
		}

	}
	return 0;
}


/* Set ISDN card mode */
int
pushcardprot (conngrab cg, int minor)
{
	cf prot;
	cf cmod = NULL;
	int num = 0; /* Grrr, GCC */
	struct isdncard *card;

	for(card = isdn4_card; card != NULL; card = card->next) {
		if(wildmatch(cg->card,card->name))
			break;
	}
	if(card == NULL)
		return -ENOENT;
	for (prot = cf_ML; prot != NULL; prot = prot->next) {
		if(!matchflag(cg->flags,prot->type)) continue;
		if (!wildmatch (cg->site, prot->site)) continue;
		if (!wildmatch (cg->protocol, prot->protocol)) continue;
		if (!wildmatch (cg->card, prot->card)) continue;
		if (!classmatch (cg->cclass, prot->cclass)) continue;

		if(card->cap & CHM_INTELLIGENT) {
			num = 1;
		} else {
			for (cmod = cf_CM; cmod != NULL; cmod = cmod->next) {
				if (!wildmatch (cg->card, cmod->card)) continue;
				if (!wildmatch(prot->arg,cmod->arg)) continue;
				break;
			}
			if (cmod == NULL)
				return -ENOENT;
			num = cmod->num;
			break;
		}
	}
	if (prot == NULL) 
		return -ENOENT;
	
	if (minor != 0) {
		mblk_t *mj = allocb (32, BPRI_LO);
		int len;

		if (mj == NULL)
			return -ENOMEM;
		m_putid (mj, CMD_CARDSETUP);
		m_putsx (mj, ARG_MINOR);
		m_puti (mj, minor);
		m_putdelim (mj);
		m_putc (mj, PROTO_MODE);
		m_puti (mj, num);
		m_puti (mj, prot->num);
		len = mj->b_wptr - mj->b_rptr;
		DUMPW (mj->b_rptr, len);
		(void) strwrite (xs_mon, (uchar_t *) mj->b_rptr, len, 1);
		freeb (mj);
	}
	return 0;
}


/* Startup a connection... */
struct conninfo *
startconn(conngrab cg, int fminor, int connref, char **ret)
{
	struct iovec io[3];
	int iovlen = 0;
	streamchar data[MAXLINE];
	mblk_t *xx, yy;
	struct datab db;
	struct conninfo *conn;
	char *str;

	yy.b_rptr = data;
	yy.b_wptr = data;
	db.db_base = data;
	db.db_lim = data + sizeof (data);
	yy.b_datap = &db;

	if(ret == NULL)
		ret = &str;
	*ret = NULL;
	chkall();
	cg->refs++;
	for(conn = theconn; conn != NULL; conn = conn->next) {
		if(conn->ignore)
			continue;
		if(conn->minor == 0)
			continue;
		if(conn->cg == cg)
			break;
	}
	if(conn == NULL) {
		for(conn = theconn; conn != NULL; conn = conn->next) {
			char *sit,*pro,*car,*cla;

			if(conn->ignore)
				continue;
			if(conn->pid == 0 || conn->minor == 0)
				continue;
			if(!(conn->flags & F_PERMANENT))
				continue;
			if(conn->cg == NULL)
				continue;
			if((sit = wildmatch(conn->cg->site,cg->site)) == NULL) continue;
			if((pro = wildmatch(conn->cg->protocol,cg->protocol)) == NULL) continue;
			if((car = wildmatch(conn->cg->card,cg->card)) == NULL) continue;
			if((cla = classmatch(conn->cg->cclass,cg->cclass)) == NULL) continue;
			cg->site = sit; cg->protocol = pro; cg->card = car; cg->cclass = cla;
			break;
		}
	}
	if(conn == NULL) {
		for(conn = theconn; conn != NULL; conn = conn->next) {
			char *sit,*pro;

			if(conn->ignore)
				continue;
			if(conn->pid == 0 || conn->minor == 0)
				continue;
			if(!(conn->flags & F_PERMANENT))
				continue;
			if(conn->cg == NULL)
				continue;
			if((sit = wildmatch(conn->cg->site,cg->site)) == NULL) continue;
			if((pro = wildmatch(conn->cg->protocol,cg->protocol)) == NULL) continue;
			cg->site = sit; cg->protocol = pro;
			break;
		}
	}
	if(conn == NULL) {
		dropgrab(cg);
		*ret = "Internal error: Connection not found";
		return NULL;
	}

	/* Returning "+" in the first position means keep the new connection. */ 

	if(conn->state == c_forceoff) {
		dropgrab(cg);
		*ret = "-COLLISION 1a";
		return conn;
	}
	if(conn->state == c_going_down) {
		dropgrab(cg);
		*ret = "-COLLISION 1b";
		return conn;
	}
	if(conn->state > c_going_down) {
		*ret = "+COLLISION 1b";
		if((conn->state == c_going_up) && (cg->flags & F_PREFOUT))
			**ret = '-';
		if((conn->state == c_up) && (cg->flags & (F_PREFOUT | F_FORCEOUT)))
			**ret = '-';
		dropgrab(cg);
		return conn;
	}
	if((cg->flags & F_FORCEOUT) && (cg->flags & F_INCOMING)) {
		*ret = "=CALLBACK";
		dropgrab(cg);
		return conn;
	}

printf("Start: %s:%s #%s...",cg->site,cg->protocol,cg->nr);
	if(((*ret) = findit (&cg)) != NULL) {
		dropgrab(cg);
		chkall();
		return NULL;
	}
	chkall();
	dropgrab(conn->cg);
	conn->cg = cg;
	conn->flags |= cg->flags & F_MOVEFLAGS;
	chkone(cg);

	if (cg->flags & F_INCOMING) {
		m_putid (&yy, CMD_ANSWER);
		xx = cg->par_in;
	} else if(cg->flags & F_OUTGOING) {
		m_putid (&yy, CMD_DIAL);
		xx = cg->par_out;
	} else {
		*ret = "NEITHER IN NOR OUT";
		return NULL;
	}
	m_putsx (&yy, ARG_DELAY);
	m_puti (&yy, cg->delay);
	if(cg->flags & F_OUTGOING) {
		m_putsx(&yy,ARG_NOCONN);
		setconnref(conn,connrefs);
		connrefs += 2;
	} else if(connref != 0) {
		if(conn->connref != 0 && conn->state == c_up) {
			*ret = "COLLISION 2";
			return conn;
		}
		/* setconnref(conn,connref); */
	}
	
	m_putsx (&yy, ARG_MINOR);
	m_puti (&yy, conn->minor);
	if(fminor != 0)
		conn->fminor = fminor;
#if 1
	if (conn->fminor != 0) {
		m_putsx (&yy, ARG_FMINOR);
		m_puti (&yy, conn->fminor);
	}
#endif
	chkone(cg);
	if (cg->flags & F_INTERRUPT)
		m_putsx (&yy, ARG_INT);

	if (connref != 0 || conn->connref != 0) {
		m_putsx (&yy, ARG_CONNREF);
		m_puti (&yy, connref ? connref : conn->connref);
	}

	if (cg->lnrsuf != NULL) {
		char *s = cg->lnrsuf;
		m_putsx (&yy, ARG_LNUMBER);
		m_putsz (&yy, s);
	}
	if (cg->nr != NULL) {
		char *s = strip_nr(cg->nr);
		printf("Strip3 %s -> %s\n",cg->nr,s);
		if(s == NULL && cg->nrsuf != NULL) {
			s = append_nr(cg->nr,cg->nrsuf);
			printf("Append3 %s,%s -> %s\n",cg->nr,cg->nrsuf,s);
		}
		if(s != NULL) {
			m_putsx (&yy, ARG_NUMBER);
			m_putsz (&yy, s);
		}
	}
	if (cg->protocol != NULL
			&& strchr (cg->protocol, '*') == NULL) {
		m_putsx (&yy, ARG_STACK);
		m_putsz (&yy, cg->protocol);
	}
	m_putsx (&yy, ARG_CARD);
	m_putsz (&yy, cg->card);
#if 0
	if (strchr (type, 'H') != NULL)
		m_putsx (&yy, ARG_SUPPRESS);
#endif

	io[0].iov_base = yy.b_rptr;
	io[0].iov_len = yy.b_wptr - yy.b_rptr;
	if (xx != NULL) {
		io[1].iov_base = xx->b_rptr;
		io[1].iov_len = xx->b_wptr - xx->b_rptr;
		iovlen = 2;
	} else
		iovlen = 1;
#if 0
	if(cg->par_out != NULL) {
		io[iovlen].iov_base = cg->par_out->b_rptr;
		io[iovlen].iov_len = cg->par_out->b_wptr - cg->par_out->b_rptr;
		iovlen++;
	}
#endif
	DUMPW (yy.b_rptr, io[0].iov_len);
	if (iovlen > 1) {
		printf ("+ ");
		DUMPW (xx->b_rptr, io[1].iov_len);
		if(iovlen > 2) {
			printf ("+ ");
			DUMPW (cg->par_out->b_rptr, io[2].iov_len);
		}
	}
	setconnstate(conn,c_going_up);
	(void) strwritev (xs_mon, io, iovlen, 1);
	chkone(conn);
	*ret = NULL;
	return conn;
}

/*
 * Start a new program.
 * - if foo is NULL (and conn isn't), start a subprogram.
 */
char *
runprog (cf cfr, struct conninfo **rconn, conngrab *foo)
{
	int pid = 0;
	int pip[2];
	int dev2;
	int xlen;
	struct conninfo *conn;

	streamchar data[MAXLINE];
	mblk_t yy;
	struct datab db;
	struct proginfo *prog = NULL;
	int dev = 0;
	conngrab cg;

	yy.b_rptr = data;
	yy.b_wptr = data;
	db.db_base = data;
	db.db_lim = data + sizeof (data);
	yy.b_datap = &db;

	chkall();
	if(foo != NULL)
		cg = *foo;
	else
		cg = NULL;
	if(*rconn != NULL)
		conn = *rconn;
	else
		conn = NULL;

	if ((foo != NULL) || (strchr(cfr->type,'c') != NULL)) {
		static int sdev = 0;
		if(sdev == 0) sdev = getpid()%(NPORT/2)+(NPORT/4);
		for(; sdev<NPORT*2; sdev++) {
			if((sdev % NPORT) == 0)
				continue;
			if(lockdev(sdev % NPORT,0)<0)
				continue;
			/* We have a device */
			break;
		}
		if (sdev == NPORT*2) {
			dev = sdev = sdev % NPORT;
			syslog(LOG_ERR,"No free devices for ISDN (no spool dir?)");
			return "NO FREE DEVICES";
		}
		dev = sdev = sdev % NPORT;
		sdev++;
	}
	if (foo != NULL) {		  /* We're launching a master program */
		if (pipe (pip) == -1) {
			syslog(LOG_CRIT,"Pipe: %m");
			return "NO PIPE";
		}

		{
			char *err;

			if((err = findit (foo)) != NULL) {
				if(conn != NULL)
					free(conn);
				return err;
			}
			cg = *foo;

			if(conn == NULL) {
				conn = malloc(sizeof(*conn));
				if(conn == NULL) {
					return "NO MEMORY.1";
				}

				bzero(conn,sizeof(*conn));
				conn->seqnum = ++connseq;
				conn->state = c_down;
				conn->cause = 999999;
				conn->next = theconn;
				theconn = conn;
			}
			cg->refs++;
			dropgrab(conn->cg);
			conn->cg = cg;
			conn->flags |= conn->cg->flags;
			conn->pid = (pid_t)~0;
			ReportConn(conn);
		}
	} else {
		prog = (struct proginfo *) malloc (sizeof (struct proginfo));

		if (prog == NULL)
			return "NO MEMORY.2";
		bzero (prog, sizeof (prog));
		prog->master = conn;
		prog->site = cfr->site;
		prog->protocol = cfr->protocol;
		prog->cclass = cfr->cclass;
		prog->card = cfr->card;
		prog->type = cfr->type;
	}
	callout_sync ();
	if (foo != NULL && conn != NULL) {
		cg->refs++;
		dropgrab(conn->cg);
		conn->cg = cg;
		conn->locked = 1;
		conn->minor = dev;
	}
	backrun(-1,0); backrun(-1,0);
    signal (SIGCHLD, SIG_DFL);
	switch ((pid = fork ())) {
	case -1:
		if(0)callout_async ();
		return "CANT FORK";
	case 0:
		{					  /* child. Go to some lengths to detach us. */
			char *ap;
			int arc;
			char *arg[99];
			int devfd;

			zzconn = conn;
			alarm(15);
			signal(SIGALRM,(void *)dropdead);
			if (cfr != NULL && cg != NULL) {
				close (pip[0]);
				fcntl (pip[1], F_SETFD, 1);		/* close-on-exec */
			}
			close (fd_mon);
			close (0);
			close (1);
			if(strchr(cfr->type,'S') != NULL)
				close (2);
			if (strchr (cfr->type, 'Q') != NULL) {	/* Trace */
				if(fork() == 0) {
					char *svarg[10], **sarg = svarg; int sarc = 0;
					char pid[10], pfile[15];

					alarm(0);
					sprintf(pid,"%d",getppid());
					sprintf(pfile,"/tmp/follow.%d",getppid());

					if (fork() != 0) _exit(0);
					sleep(1);

					sarg[sarc++] = "/bin/strace";
					sarg[sarc++] = "-s";
					sarg[sarc++] = "240";
					sarg[sarc++] = "-f";
					sarg[sarc++] = "-p";
					sarg[sarc++] = pid;
					sarg[sarc] = NULL;
					if(strchr(cfr->type,'S') == NULL)
						close(2);
					open("/dev/null",O_RDWR);
					open(pfile,O_WRONLY|O_CREAT,0600);
					dup(1);
					execv("/bin/strace",sarg);
				}
				sleep(3);
			}
#ifdef HAVE_SETPGRP_2
			setpgrp (0, conn->minor ? conn->pid : getpid ());
#endif
#ifdef HAVE_SETPGRP_0
			setpgrp();
#endif
#ifdef DO_HAVE_SETSID
			setsid();
#endif
#ifdef TIOCNOTTY
			{
				int a = open ("/dev/tty", O_RDWR);

				if (a >= 0) {
					(void) ioctl (a, TIOCNOTTY, NULL);
					(void) close (a);
				}
			}
#endif
			if (cfr != NULL && ((cg != NULL) || strchr(cfr->type,'c') != NULL)) {
				if (strchr(cfr->type,'T') != NULL) {
					mknod (devname (dev), S_IFCHR | S_IRUSR | S_IWUSR, MKDEV(isdnterm,dev));
					chown (devname (dev), cfr->num, cfr->num2);
					chmod (devname (dev), S_IRUSR | S_IWUSR | S_IWGRP);
					devfd = open(devname(dev), O_RDWR | O_EXCL
#ifdef O_GETCTTY
										| O_GETCTTY
#endif
						);
				} else {
					devfd = open(idevname(dev), O_RDWR | O_EXCL
#ifdef O_NOCTTY
										| O_NOCTTY
#endif
						);
				}
				if (devfd < 0) {
					syslog(LOG_CRIT,"unable to open %s: %m",(strchr(cfr->type,'T') != NULL)?devname(dev):idevname(dev));
					unlockdev(dev);
					_exit (1);
				}
				if (strchr (cfr->type, 'C') != NULL) {	/* Open a second device
														 * for talking. Pass
														 * minor of first
														 * device. */
					int dev2;
					for(dev2=1; dev2<NPORT; dev2++) {
						if(lockdev(dev2,0)<0)
							continue;
						/* We have a device */
						break;
					}
					if (dev2 == NPORT) {
						syslog(LOG_ERR,"No free devices2 for ISDN");
						_exit(1);
					}
					write (pip[1], &dev, sizeof (dev));
					if ((devfd = open (devname(dev2), O_RDWR
#ifdef O_NOCTTY
											| O_NOCTTY
#endif
									))!= 1) {
						syslog(LOG_CRIT,"unable to open %s: %m",devname(dev2));
						_exit (1);
					}
				} else
					dup2 (devfd, 1);
			} else {
				if ((devfd = open ("/dev/null", O_RDWR)) != 0) {
					syslog(LOG_CRIT,"Unable to open /dev/null: %m");
					_exit (1);
				}
				dup2 (devfd, 1);
			}
			if (strchr (cfr->type, 'S') != NULL) 
				dup2 (devfd, 2);
			if (cfr != NULL && cg != NULL) {
				if (strchr (cfr->type, 'T') != NULL) {
					struct termio tt = {IGNBRK | ICRNL, OPOST | ONLCR, B38400 | CS8 | CREAD | HUPCL, ICANON | ECHO | ECHOE | ECHOK | ISIG, 0, {0x03, 0x02, 0x7F, 0x15, 0x04, 0, 0, 0}};
					(void) ioctl (0, TCSETA, &tt);
#ifndef linux
					if(ioctl (0, I_PUSH, "line") != 0)
						syslog(LOG_ERR,"Unable to push line discipline! %m");
#endif
				} else {
#if 0
					struct termio tt =
					{IGNBRK, 0, B38400 | CS8 | CREAD | HUPCL, 0, 0, { 0, }};
					(void) ioctl (0, TCSETA, &tt);
#endif
				}

				if (strchr (cfr->type, 'U') != NULL) {
					struct utmp ut;

					bzero (&ut, sizeof (ut));
					strncpy (ut.ut_id, sdevname (dev), sizeof (ut.ut_id));
					strncpy (ut.ut_line, mdevname (dev), sizeof (ut.ut_line));
#ifndef M_UNIX
					strncpy (ut.ut_host, cfr->protocol, sizeof (ut.ut_host));
#endif
					ut.ut_pid = getpid ();
					ut.ut_type = LOGIN_PROCESS;
					ut.ut_time = time(NULL);
					getutline (&ut);
					pututline (&ut);
					endutent ();
					{
						int wf = open ("/etc/wtmp", O_WRONLY | O_APPEND);

						if (wf >= 0) {
							(void) write (wf, &ut, sizeof (ut));
							close (wf);
						}
					}
				}
				write (pip[1], &dev, sizeof (dev));
			}
			setregid (cfr->num2, cfr->num2);
			setreuid (cfr->num, cfr->num);
			if(conn != NULL && conn->cg != NULL) {
				if(conn->cg->site != NULL)
					putenv2 ("SITE", conn->cg->site);
				if(conn->cg->protocol != NULL)
					putenv2 ("PROTOCOL", conn->cg->protocol);
				if(conn->cg->cclass != NULL)
					putenv2 ("CLASS", strippat(conn->cg->cclass));
				if(conn->cg->nr != NULL)
					putenv2 ("PHONE", conn->cg->nr);
				if(conn->cg->lnr != NULL)
					putenv2 ("LPHONE", conn->cg->lnr);
				if(conn->ccharge != 0) {
					static char coststr[20];
					sprintf(coststr,"%ld",conn->ccharge);
					putenv2 ("CCOST", coststr);
				}
				if(conn->charge != 0) {
					static char coststr[20];
					sprintf(coststr,"%ld",conn->charge);
					putenv2 ("COST", coststr);
				}
				putenv2 ("CAUSE",CauseInfo(conn->cause, conn->causeInfo));
				putenv2 ("DIRECTION", (conn->flags & F_INCOMING) ? "IN" : "OUT");
				if(conn->minor != 0)
					putenv2 ("DEVICE", devname (conn->minor));
			}

			arc = 0;
			ap = cfr->args;
			if (strchr (cfr->type, '$') == NULL) {
				while (*ap != '\0') {
					arg[arc++] = ap;
					while (*ap != '\0' && !isspace (*ap))
						ap++;
					if (*ap == '\0')
						break;
					*ap++ = '\0';
					while (*ap != '\0' && isspace (*ap))
						ap++;
				}
			} else {
				arg[arc++] = "/bin/sh";
				arg[arc++] = "-c";
				arg[arc++] = ap;
			}
			arg[arc] = NULL;
			ap = *arg;
			if ((*arg = strrchr (ap, '/')) == NULL)
				*arg = ap;
			else
				(*arg)++;
			alarm(0);
			execvp (ap, arg);
			syslog (LOG_ERR, "Could not execute %s for %s/%s: %m", *arg,cg->site,cg->protocol);
			if (cfr != NULL && cg != NULL)
				write (pip[1], "EXEC", 4);
			_exit (2);
		}
	default:;
	}

	if(cfr != NULL) {
#ifdef HAVE_SETPGRP_2
		setpgrp (pid, conn->minor ? conn->pid : pid);
#endif
#ifdef HAVE_SETPGRP_0
		setpgrp();
#endif
		if(foo == NULL) {
			/* prog->minor == dev; */
			prog->pid = pid;
		} else {
			conn->minor = dev;
			conn->pid = pid;
		}
	}
	backrun(-1,0); backrun(-1,0);
	if (cfr != NULL) {
		int ac=-1, devin;

		close (pip[1]);
		if(backrun(pip[0],5)
				|| ((strchr (cfr->type, 'C') != NULL)
					&& ((ac=read (pip[0], &dev2, sizeof (dev2))) != sizeof (dev2)))) {
			syslog (LOG_ERR, "%s: not opened 1: %d %m", cfr->args, ac);
			if(0)callout_async ();
			conn->minor = 0;
			if (conn->pid == 0)
				dropconn (conn);
			deadkid(); return "OPEN ERR";
		}
		backrun(pip[0],5);
		if ((ac=read (pip[0], &devin, sizeof (devin))) != sizeof (dev)) {
			if(ac<0)
				syslog (LOG_ERR, "%s: not opened 2: %d %m", cfr->args,ac);
			else
				syslog (LOG_ERR, "%s: not opened 2: %d", cfr->args,ac);
			if(0)callout_async ();
			conn->minor = 0;
			if (conn->pid == 0)
				dropconn (conn);
			deadkid();
			return "OPEN ERR";
		}
		if(devin != dev)
			syslog (LOG_ERR, "%s: device %d != %d", cfr->args,devin,dev);
		if (strchr (cfr->type, 'C') == NULL)
			dev2 = dev;
#if 0
		if ((ylen = read (pip[0], msg, sizeof (msg) - 1)) > 0) {
			msg[ylen] = '\0';
			close (pip[0]);
			syslog (LOG_ERR, "%s: not executed", cfr->args);
			if(0)callout_async ();
			deadkid(); return msg;
		}
#endif
		close (pip[0]);
	}
	syslog (LOG_INFO, "exec %x:%x %d %s/%s %s", dev, dev2, pid, cfr->site,cfr->protocol, cfr->args);
	printf ("* PID %d\n", pid);

	if(prog != NULL) {
		prog->next = conn->run;
		conn->run = prog;
	} else if (conn != NULL) {
		conn->locked--;
		if(conn->state > c_off)
			setconnstate(conn, c_down);
		if(conn->minor == 0 && conn->pid == 0) 
			dropconn(conn);
		else if (conn->minor == 0 || conn->pid == 0)
			conn = NULL;
		if(conn != NULL) {
			conn->fminor = (dev == dev2) ? 0 : dev2;

			m_putid (&yy, CMD_FAKEOPEN);
			m_putsx (&yy, ARG_MINOR);
			m_puti (&yy, conn->minor);
			xlen = yy.b_wptr - yy.b_rptr;
			DUMPW (yy.b_rptr, xlen);
			(void) strwrite (xs_mon, (uchar_t *) yy.b_rptr, xlen, 1);
			yy.b_wptr = yy.b_rptr;

			if (conn->fminor != 0 && conn->fminor != conn->minor) {
				m_putid (&yy, CMD_FAKEOPEN);
				m_putsx (&yy, ARG_MINOR);
				m_puti (&yy, conn->fminor);
				xlen = yy.b_wptr - yy.b_rptr;
				DUMPW (yy.b_rptr, xlen);
				(void) strwrite (xs_mon, (uchar_t *) yy.b_rptr, xlen, 1);
				yy.b_wptr = yy.b_rptr;
			}
			m_putid (&yy, CMD_PROT);
			m_putsx (&yy, ARG_FMINOR);
			m_puti (&yy, conn->minor);
			m_putdelim (&yy);
			m_putid (&yy, PROTO_MODULE);
			m_putsx (&yy, PROTO_MODULE);
			m_putsz (&yy, (uchar_t *) "proto");
			m_putsx (&yy, PROTO_ONLINE);
			m_putsx (&yy, PROTO_CARRIER);
			m_puti (&yy, (cfr != NULL && strchr(cfr->type,'B')) ? 0 : 1);
			m_putsx (&yy, PROTO_BREAK);
			m_puti (&yy, 0);

			xlen = yy.b_wptr - yy.b_rptr;
			DUMPW (yy.b_rptr, xlen);
			(void) strwrite (xs_mon, (uchar_t *) yy.b_rptr, xlen, 1);
			yy.b_wptr = yy.b_rptr;

			if(conn->fminor != 0 && conn->fminor != conn->minor) {
				m_putid (&yy, CMD_PROT);
				m_putsx (&yy, ARG_FMINOR);
				m_puti (&yy, conn->fminor);
				m_putdelim (&yy);
				m_putid (&yy, PROTO_MODULE);
				m_putsx (&yy, PROTO_MODULE);
				m_putsz (&yy, (uchar_t *) "proto");
				m_putsx (&yy, PROTO_ONLINE);
				xlen = yy.b_wptr - yy.b_rptr;
				DUMPW (yy.b_rptr, xlen);
				(void) strwrite (xs_mon, (uchar_t *) yy.b_rptr, xlen, 1);
				yy.b_wptr = yy.b_rptr;
			}

			if(cg != NULL && (cg->flags & (F_INCOMING|F_OUTGOING))) {
				char *msg = NULL;

				cg->refs++;
				dropgrab(conn->cg);
				conn->cg = cg;
				{
					char *xd;

					if ((xd = strchr (cfr->type, '.')) != NULL && (conn->flags & F_INCOMING))
						cg->delay = atoi (xd + 1);
					else
						cg->delay = 0;
				}
				if(startconn(conn->cg,0,0, &msg) == conn) 
					setconnstate(conn,c_going_up);
				else {
					syslog(LOG_CRIT,"Bug in runprog->startconn (%s) for %s:%s",msg ? msg : "(unknown reason)", cg->site,cg->protocol);
					dropgrab(conn->cg);
					conn->cg = NULL;
					chkone(conn);
				}
			} else {
				int err = pushprot (conn->cg, conn->minor, 0);
				if(err != 0) {
printf("NoProtoEnable NotPushprot\n");
					m_putid (&yy, CMD_CLOSE);
					m_putsx (&yy, ARG_MINOR);
					m_puti (&yy, conn->minor);
					xlen = yy.b_wptr - yy.b_rptr;
					DUMPW (yy.b_rptr, xlen);
					(void) strwrite (xs_mon, (uchar_t *) yy.b_rptr, xlen, 1);
					conn->minor = 0;
					if(conn->pid == 0)
						dropconn(conn);
					else
						kill(conn->pid,SIGHUP);
					return "CANT PUSHPROT";
				}
				if(conn->flags & F_PERMANENT) {
					m_putid (&yy, CMD_PROT);
					m_putsx (&yy, ARG_MINOR);
					m_puti (&yy, conn->minor);
					m_putdelim (&yy);
					m_putid(&yy,PROTO_ENABLE);
					xlen = yy.b_wptr - yy.b_rptr;
					DUMPW (yy.b_rptr, xlen);
					(void) strwrite (xs_mon, (uchar_t *) yy.b_rptr, xlen, 1);
				}
else printf("NoProtoEnable NotPermanent\n");
				cg->card = str_enter("*"); /* cosmetic */
				ReportConn(conn); /* even more cosmetic... */
			}
		}
	}
	deadkid();
	if(0)callout_async ();
	if(rconn != NULL)
		*rconn = conn;
	return NULL;
}


/* Kill applicable subprograms */
void
kill_rp(struct conninfo *conn, char whatnot)
{
	struct proginfo *pro;
	for(pro = conn->run; pro != NULL; pro = pro->next) {
		struct conninfo *xconn;
		if(strchr(pro->type,whatnot) == NULL) continue;
		if(wildmatch(pro->site,conn->cg->site) == NULL) continue;
		if(wildmatch(pro->protocol,conn->cg->protocol) == NULL) continue;
		if(wildmatch(pro->card,conn->cg->card) == NULL) continue;
		if(classmatch(pro->cclass,conn->cg->cclass) == NULL) continue;

		xconn = malloc(sizeof(*xconn));
		if(xconn != NULL) {
			bzero(xconn,sizeof(*xconn));
			xconn->seqnum = ++connseq;
			xconn->cause = ID_priv_Print;
			xconn->causeInfo = "Signal HUP";
			xconn->pid = pro->pid;
			conn->cg->refs++;
			/* dropgrab(conn->cg; ** is new anyway */
			xconn->cg = conn->cg;
			xconn->next = theconn;
			theconn = xconn;
			dropconn(xconn);
		}
		if(strchr(pro->type,'s') == NULL)
			kill (pro->pid, SIGHUP);
		break;
	}
}

/* Start applicable subprograms. */
void
run_rp(struct conninfo *conn, char what)
{
	cf cfr;
	struct proginfo *pr;
	for(cfr = cf_RP; cfr != NULL; cfr = cfr->next) {
		char *sit,*pro,*car,*cla;

		if(strchr(cfr->type,what) == NULL) continue;
		if((sit = wildmatch(conn->cg->site,cfr->site)) == NULL) continue;
		if((pro = wildmatch(conn->cg->protocol,cfr->protocol)) == NULL) continue;
		if((car = wildmatch(conn->cg->card,cfr->card)) == NULL) continue;
		if((cla = classmatch(conn->cg->cclass,cfr->cclass)) == NULL) continue;

		for(pr = conn->run; pr != NULL; pr = pr->next) {
			struct conninfo *xconn;
			if(strchr(pr->type,what) == NULL) continue;
			if(wildmatch(pr->site,cfr->site) == NULL) continue;
			if(wildmatch(pr->protocol,cfr->protocol) == NULL) continue;
			if(wildmatch(pr->card,cfr->card) == NULL) continue;
			if(classmatch(pr->cclass,cfr->cclass) == NULL) continue;

			xconn = malloc(sizeof(*xconn));
			if(xconn != NULL) {
				bzero(xconn,sizeof(*xconn));
				xconn->seqnum = ++connseq;
				xconn->cause = ID_priv_Print;
				xconn->causeInfo = "Signal USR1";
				xconn->pid = pr->pid;
				conn->cg->refs++;
				/* dropgrab(conn->cg; ** is new anyway */
				xconn->cg = conn->cg;
				xconn->next = theconn;
				theconn = xconn;
				dropconn(xconn);
			}
			if(strchr(pr->type,'s') == NULL)
				kill (pr->pid, SIGUSR1);
			break;
		}
		if(pr == NULL) {
			runprog (cfr, &conn, NULL);
		}
	}
}


/* Find the next program to run */
void
run_now(void *nix)
{
	cf what;
	static int npos = 0;
	int spos = 0;

	if(do_run_now > 1) {
		do_run_now--;
		return;
	}
	if(signal(SIGHUP,SIG_IGN) != SIG_IGN)
		signal (SIGHUP, SIG_DFL);
	if(quitnow)
		return;

	for(what = cf_R; what != NULL; what = what->next) {
		if(spos++ < npos) {
printf("Skip #%d; ",spos);
			continue;
		}
		npos++;
printf("Do #%d...",spos);
		if(what->got_err) {
printf("StoredErr; ");
			continue;
		}
		if(strchr(what->type,'B') != NULL || strchr(what->type,'p') != NULL) {
			struct conninfo *conn;

			for(conn = theconn; conn != NULL; conn = conn->next) {
				if(conn->ignore || (conn->cg == NULL))
					continue;
				if(strcmp(conn->cg->site,what->site))
					continue;
				if(strcmp(conn->cg->protocol,what->protocol))
					continue;
				if(!classmatch(conn->cg->cclass,what->cclass))
					continue;
				break;
			}
			if(conn == NULL) {
				conngrab cg = newgrab(NULL);
				char *err;
printf("run %s:%s; ",what->site,what->protocol);
				cg->site = what->site; cg->protocol = what->protocol;
				cg->card = what->card; cg->cclass = what->cclass;
				if (strchr(what->type,'p') != NULL)
					cg->flags |= F_PERMANENT;
				if (strchr(what->type,'f') != NULL)
					cg->flags |= F_LEASED;
				if (strchr(what->type,'B') != NULL)
					cg->flags |= F_OUTGOING;
				err = runprog(what,&conn,&cg);
				if(conn != NULL) {
					kill_rp(conn,'t');
					run_rp(conn,'i');
				}
				chkone(cg);
				dropgrab(cg);
				if(err == NULL || !strncasecmp(err,"no free dev",11)) {
					if(err != NULL) {
						spos--;
						printf("Try again: %s",err);
					}
printf("\n");
					timeout(run_now,(void *)spos,2*HZ);
					return;
				} else {
printf("FAIL %s\n",err);
				}
			} else {
printf("exist %s:%s\n",conn->cg->site,conn->cg->protocol);
				if(conn->cg != NULL && conn->minor != 0 && conn->pid != 0)
					pushprot(conn->cg,conn->minor,1);
			}
		}
	}
	in_boot = 0;
	if(signal(SIGHUP,SIG_IGN) != SIG_IGN)
    	signal (SIGHUP, (sigfunc__t) read_args_run);
	npos = 0;
	do_run_now = 0;
}


/* Any programs still running? */
int
has_progs(void)
{
	struct conninfo *conn;
	if(!quitnow)
		return 1;
	for(conn = theconn; conn != NULL; conn = conn->next) {
		if(conn->ignore)
			continue;
		if(conn->pid != 0)
			return 1;
		if(conn->minor != 0)
			return 1;
	}
	return 0;
}

/* Kill one program / all programs */
void
kill_progs(struct conninfo *xconn)
{
	struct conninfo *conn, *nconn;
	if(!quitnow)
		in_boot = 1;
	for(conn = theconn; conn != NULL; conn = nconn) {
		nconn = conn->next;
		if(conn->ignore)
			continue;
		if(conn->pid != 0 && conn->minor != 0 && (xconn == NULL || conn == xconn)) {
			mblk_t *mb = allocb(30,BPRI_LO);
			int xlen;
			if(mb == NULL)
				continue;
			if(conn->cg != NULL)
				syslog (LOG_INFO, "ISDN drop %d %d %s:%s:%s", conn->pid, conn->minor, conn->cg->site,conn->cg->protocol,conn->cg->cclass);
			else
				syslog (LOG_INFO, "ISDN drop %d %d <unknown>", conn->pid, conn->minor);
			
			m_putid (mb, CMD_CLOSE);
			m_putsx (mb, ARG_MINOR);
			m_puti (mb, conn->minor);

			xlen = mb->b_wptr - mb->b_rptr;
			DUMPW (mb->b_rptr, xlen);
			(void) strwrite (xs_mon, mb->b_rptr, xlen, 1);
			freemsg(mb);
			conn->minor = 0;
			if(xconn != NULL)
				break;
		}
	}
	if(!quitnow) {
		do_run_now++;
		timeout(run_now,NULL,5*HZ);
	}
}