/*
 * This file is part of the ISDN master program.
 *
 * Copyright (C) 1995 Matthias Urlichs.
 * See the file COPYING for license details.
 */

#include "master.h"

/* Handle dead processes */
void
deadkid (void)
{
	int pid, val = 0, has_dead = 0;
	struct conninfo *conn;

	while((pid = wait4 (-1,&val,WNOHANG,NULL)) > 0) {
		if(log_34 & 2)
			printf ("\n* PID %d died, %x\n", pid, val);

		chkall();
		for (conn = isdn4_conn; conn != NULL; conn = conn ? conn->next : NULL) {
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
		do_run_now++;
		timeout(run_now,NULL,3*HZ);
	}
}


/* Push protocols onto stream */
int
pushprot (conngrab cg, int minor, int connref, char update)
{
	cf prot;
	char *mods = NULL;
	char oupdate = update;

	for (prot = cf_ML; prot != NULL; prot = prot->next) {
		if (!matchflag (cg->flags,prot->type)) continue;
		if (!wildmatch (cg->site, prot->site)) continue;
		if (!wildmatch (cg->protocol, prot->protocol)) continue;
		if (!wildmatch (cg->card, prot->card)) continue;
		if (!maskmatch (cg->mask, prot->mask)) continue;
		if (!classmatch(theclass,classmatch (cg->cclass, prot->cclass))) continue;
		break;
	}
	if (prot == NULL)
		return -ENOENT;
	if(update & PUSH_UPDATE) 
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
		m_putid (mj, PROTO_MODLIST);
		m_puti(mj,update);
		if(cg != NULL && cg->card != NULL && strcmp(cg->card,"*") != 0) {
			m_putsx(mj,ARG_CARD);
			m_putsz(mj,cg->card);
		}
		if(connref) {
			m_putsx (mj, ARG_CONNREF);
			m_puti(mj,connref);
		}
		m_putdelim (mj);
		m_putsz(mj, mods);
		len = mj->b_wptr - mj->b_rptr;
		DUMPW (mj->b_rptr, len);
		(void) strwrite (xs_mon, (uchar_t *) mj->b_rptr, len, 1);
		freeb (mj);

		sx = (char *)xmalloc (strlen (prot->args) + 5 + strlen (PROTO_NAME));
		if (sx == NULL)
			return -ENOMEM;
		sprintf (sx, " %s %s", prot->args, PROTO_NAME);
		sp1 = sx;
		for (sp2 = sp1; *sp1 != '\0'; sp1 = sp2) {
			cf cm;
			mblk_t *mi;

			while (*sp2 != '\0' && !isspace (*sp2))
				sp2++;
			if(*sp2 != '\0')
				*sp2++ = '\0';
			if(update & PUSH_AFTER) {
				if(!strcmp(sp1,"reconn"))
					update &=~ PUSH_AFTER;
				else
					continue;
			} else if(update & PUSH_BEFORE) {
				if(!strcmp(sp1,"reconn"))
					break;
			}
			if ((mi = allocb (256, BPRI_MED)) == NULL) {
				free (sx);
				return -ENOMEM;
			}
			for (cm = cf_MP; cm != NULL; cm = cm->next) {
				ushort_t id;
				streamchar *newlim = NULL;
				mblk_t *mz = NULL;

				if (!matchflag (cg->flags,cm->type)) continue;
				if (!wildmatch (cg->site, cm->site)) continue;
				if (!wildmatch (cg->protocol, cm->protocol)) continue;
				if (!wildmatch (cg->card, cm->card)) continue;
				if (!maskmatch (cg->mask, cm->mask)) continue;
				if (!classmatch(theclass, classmatch(cg->cclass, cm->cclass))) continue;
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
				if(log_34 & 1)
					printf ("+ ");
				DUMPW (mi->b_rptr, io[1].iov_len);
				(void) strwritev (xs_mon, io, 2, 1);
				freeb (mj);
			}
			freeb (mi);
		}
		free (sx);
		if(!(oupdate & (PUSH_AFTER|PUSH_UPDATE))) {
			mblk_t *mj = allocb (64, BPRI_LO);
			int len;

			if (mj == NULL)
				return -ENOMEM;
			m_putid (mj, CMD_CARDSETUP);
			m_putsx (mj, ARG_MINOR);
			m_puti (mj, minor);
			if(cg != NULL && cg->card != NULL && strcmp(cg->card,"*") != 0) {
				m_putsx(mj,ARG_CARD);
				m_putsz(mj,cg->card);
			}
			if(connref != 0) {
				m_putsx (mj, ARG_CONNREF);
				m_puti (mj, connref);
			}
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


/* Startup a connection... */
struct conninfo *
startconn(conngrab cg, int fminor, int connref, char **ret, conngrab *retcg)
{
	struct iovec io[3];
	int iovlen = 0;
	streamchar data[MAXLINE];
	mblk_t *xx, yy;
	struct datab db;
	struct conninfo *conn;
	char *str;
	conngrab rcg = NULL;

	yy.b_rptr = data;
	yy.b_wptr = data;
	db.db_base = data;
	db.db_lim = data + sizeof (data);
	yy.b_datap = &db;

	if(ret == NULL)
		ret = &str;
	if(retcg == NULL)
		retcg = &rcg;
	if(cg->mask == 0) {
		*ret = "Internal error: bad mask";
		return NULL;
	}
	*ret = NULL;
	chkall();
	cg->refs++;
	for(conn = isdn4_conn; conn != NULL; conn = conn->next) {
		if(conn->ignore)
			continue;
		if(conn->minor == 0)
			continue;
		if(conn->cg == cg)
			break;
	}
	if(conn == NULL && (cg->flags & (F_PERMANENT|F_MULTIDIALUP|F_DIALUP))) {
		for(conn = isdn4_conn; conn != NULL; conn = conn->next) {
			char *sit,*pro,*car,*cla;
			ulong_t sub;

			if(conn->ignore)
				continue;
			if(conn->pid == 0 || conn->minor == 0)
				continue;
			if(!(conn->flags & (F_PERMANENT|F_DIALUP)))
				continue;
			if(conn->cg == NULL)
				continue;
			if((sit = wildmatch(conn->cg->site,cg->site)) == NULL) continue;
			if((pro = wildmatch(conn->cg->protocol,cg->protocol)) == NULL) continue;
			if((car = wildmatch(conn->cg->card,cg->card)) == NULL) continue;
			if((sub = maskmatch(conn->cg->mask,cg->mask)) == 0) continue;
			if((cla = classmatch(conn->cg->cclass,cg->cclass)) == NULL) continue;
			cg->site = sit; cg->protocol = pro; cg->card = car; cg->cclass = cla;
			cg->mask = sub;
			break;
		}
	}
	if(conn == NULL && (cg->flags & (F_PERMANENT|F_MULTIDIALUP|F_DIALUP))) {
		for(conn = isdn4_conn; conn != NULL; conn = conn->next) {
			char *sit,*pro;

			if(conn->ignore)
				continue;
			if(conn->pid == 0 || conn->minor == 0)
				continue;
			if(!(conn->flags & (F_PERMANENT|F_DIALUP)))
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
	/* Return "-" to keep the old connection. */
	/* Return "=" to call back. */
	if(conn->state == c_forceoff) {
		dropgrab(cg);
		*ret = "-COLLISION 1a";
		return conn;
	}
	if(conn->cg != cg) {
		if(conn->state == c_going_down) {
			if((cg->flags & (F_PREFOUT|F_FORCEOUT)) && (cg->flags & F_INCOMING)) 
				*ret = "-Hmmm, callout delayed?";
			else if(cg->flags & F_INCOMING)
				*ret = "+Incoming while old call isn't really down?";
			else
				*ret = "+Hmmm, something's not quite right...";
			dropgrab(cg);
			return conn;
		}
		if((cg->flags & F_FORCEOUT) && (cg->flags & F_INCOMING)) {
			*ret = "=Calling back";
			dropgrab(cg);
			return conn;
		}
		if(conn->state > c_going_down) {
			if(cg->flags & F_INCOMING) {
				if((conn->state == c_going_up) && (conn->flags & F_OUTGOING))
					*ret = "+Getting called back";
				if((conn->state == c_going_up) && (cg->flags & F_PREFOUT))
					*ret = "-Incoming -- toss";
				if((conn->state == c_up) && (cg->flags & (F_PREFOUT | F_FORCEOUT)))
					*ret = "-Incoming -- toss F";
			} else {
				printf("Collision in startconn out, should not happen!\n");
				*ret = "+In+Out Clash 0";
				if((conn->state == c_going_up) && (cg->flags & F_PREFOUT))
					*ret = "+In+Out Clash 1";
				if((conn->state == c_up) && (cg->flags & (F_PREFOUT | F_FORCEOUT)))
					*ret = "+In+Out Clash 2";
			}
			if(*ret != NULL) {
				dropgrab(cg);
				return conn;
			}
		}
	}
	if(conn->state > c_going_down) {
		/* *ret = "+EXISTS"; */
		return conn;
	}

	if(log_34 & 2)
		printf("Start: %s:%s #%s...",cg->site,cg->protocol,cg->nr);
	if(((*ret) = findit (&cg,0)) != NULL) {
		*retcg = cg;
		dropgrab(rcg);
		chkall();
		return NULL;
	}
	chkall();
	dropgrab(conn->cg);
	conn->cg = cg;

	syncflags(conn,0);

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
		/* if((conn->connref == 0) || !(cg->flags & F_LEASED)) */ {
			m_putsx(&yy,ARG_NOCONN);
			setconnref(conn,isdn4_connref);
			isdn4_connref += 2;
		}
	} else if(connref != 0) {
		if(conn->connref != 0 && conn->state == c_up) {
			*ret = "COLLISION 2";
			return conn;
		}
		setconnref(conn,connref);
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

	if (cg->lnr != NULL) {
		char *s = strip_nr(cg->lnr,1);
		if(log_34 & 2)
			printf("Strip3 %s -> %s\n",cg->lnr,s);
		if(s == NULL && cg->lnrsuf != NULL) {
			s = append_nr(cg->lnr,cg->lnrsuf);
			if(log_34 & 2)
				printf("Append3 %s,%s -> %s\n",cg->lnr,cg->lnrsuf,s);
		}
		if(s != NULL) {
			m_putsx (&yy, ARG_LNUMBER);
			m_putsz (&yy, s);
		}
	}
	if (cg->nr != NULL) {
		char *s = strip_nr(cg->nr,0);
		if(log_34 & 2)
			printf("Strip3 %s -> %s\n",cg->nr,s);
		if(s == NULL && cg->nrsuf != NULL) {
			s = append_nr(cg->nr,cg->nrsuf);
			if(log_34 & 2)
				printf("Append3 %s,%s -> %s\n",cg->nr,cg->nrsuf,s);
		}
		if(s != NULL) {
			m_putsx (&yy, ARG_NUMBER);
			m_putsz (&yy, s);
		}
	}
	if (cg->site != NULL && strchr (cg->site, '*') == NULL) {
		m_putsx (&yy, ARG_SITE);
		m_putsz (&yy, cg->site);
	}
	if (cg->protocol != NULL && strchr (cg->protocol, '*') == NULL) {
		m_putsx (&yy, ARG_STACK);
		m_putsz (&yy, cg->protocol);
	}
	m_putsx (&yy, ARG_CARD);
	m_putsz (&yy, cg->card);
	{
		ulong_t m;
		int i;
		for(i=1,m=1;(i < 33) && !(cg->mask & m);m<<=1,i++) ;
		m_putsx (&yy, ARG_SUBCARD);
		m_puti (&yy, i);
	}
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
		if(log_34 & 1)
			printf ("+ ");
		DUMPW (xx->b_rptr, io[1].iov_len);
		if(iovlen > 2) {
			if(log_34 & 1)
				printf ("+ ");
			DUMPW (cg->par_out->b_rptr, io[2].iov_len);
		}
	}

	if(conn->state < c_going_up) {
		setconnstate(conn,c_going_up);
		(void) strwritev (xs_mon, io, iovlen, 1);
	}
	chkone(conn);
	*ret = NULL;
	return conn;
}

/*
 * Start a new program.
 * - if foo is NULL (and conn isn't), start a subprogram.
 */
char *
runprog (cf cfr, struct conninfo **rconn, conngrab *foo, char what)
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
		{
			struct conninfo *xconn;
			int id = 0; char *ids = strchr(cfr->type,'/');
			if(ids != NULL)
				id = atoi(ids);
			if(id != 0) {
				for(xconn = isdn4_conn; xconn != NULL; xconn = xconn->next) {
					if(xconn->id == id) {
						return "Already Running";
					}
				}
			}
		}
		{
			char *err;

			if((err = findit (foo,!!(cg->flags & F_PERMANENT))) != NULL) {
				if(conn != NULL && rconn != NULL && conn != *rconn)
					free(conn);
				return err;
			}
			cg = *foo;

			if(conn == NULL) {
				conn = xmalloc(sizeof(*conn));
				if(conn == NULL) {
					return "NO MEMORY.1";
				}

				bzero(conn,sizeof(*conn));
				conn->seqnum = ++connseq;
				conn->state = c_down;
				conn->cause = 999999;
				conn->next = isdn4_conn; isdn4_conn = conn;
			}
			cg->refs++;
			dropgrab(conn->cg);
			conn->cg = cg;
			conn->flags |= conn->cg->flags;
			conn->pid = (pid_t)~0;
			ReportConn(conn);
		}
		{
			int flags = 0;
			if (strchr(cfr->type,'m') != NULL)
				flags |= F_MULTIDIALUP;
			if (strchr(cfr->type,'d') != NULL)
				flags |= F_DIALUP;
			if (strchr(cfr->type,'p') != NULL)
				flags |= F_PERMANENT;
			if (strchr(cfr->type,'f') != NULL)
				flags |= F_LEASED;
			cg->flags = (cg->flags &~ (F_MULTIDIALUP|F_DIALUP|F_PERMANENT|F_LEASED)) | flags;
		}
		if (pipe (pip) == -1) {
			syslog(LOG_CRIT,"Pipe: %m");
			return "NO PIPE";
		}
	} else {
		if(conn != NULL) {
			int id = 0; char *ids = strchr(cfr->type,'/');
			if(ids != NULL)
				id = atoi(ids);
			if(id != 0) {
				for(prog = conn->run; prog != NULL; prog = prog->next) {
					if(prog->id == id)
						return "Already Running";
				}
			}
		}
		prog = (struct proginfo *) xmalloc (sizeof (struct proginfo));

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
	switch ((pid = fork ())) {
	case -1:
		if(foo != NULL) {
			close(pip[0]);
			close(pip[1]);
		}
		return "CANNOT FORK";
	case 0:
		{					  /* child. Go to some lengths to detach us. */
			char *ap;
			int arc;
			char *arg[99];
			int devfd;

			zzconn = conn;
			alarm(15);
			signal(SIGALRM,(void *)dropdead);
			if (foo != NULL) {
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
					if(foo != NULL)
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
					strncpy (ut.ut_host, cfr->site, sizeof (ut.ut_host));
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
				if(foo != NULL) 
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
				if(what != 0) {
					static char whatstr[2];
					whatstr[0]=what;
					whatstr[1]='\0';
					putenv2 ("REASON", whatstr);
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
			if (foo != NULL)
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
	if (cfr != NULL && foo != NULL) {
		int ac=-1, devin;

		close (pip[1]);
		if(backrun(pip[0],5)
				|| ((strchr (cfr->type, 'C') != NULL)
					&& ((ac=read (pip[0], &dev2, sizeof (dev2))) != sizeof (dev2)))) {
			syslog (LOG_ERR, "%s: not opened 1: %d %m", cfr->args, ac);
			conn->minor = 0;
			if (conn->pid == 0)
				dropconn (conn);
			return "OPEN ERR";
		}
		backrun(pip[0],5);
		if ((ac=read (pip[0], &devin, sizeof (devin))) != sizeof (dev)) {
			if(ac<0)
				syslog (LOG_ERR, "%s: not opened 2: %d %m", cfr->args,ac);
			else
				syslog (LOG_ERR, "%s: not opened 2: %d", cfr->args,ac);
			conn->minor = 0;
			if (conn->pid == 0)
				dropconn (conn);
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
			return msg;
		}
#endif
		close (pip[0]);
	}
	syslog (LOG_INFO, "exec %d:%d %d %s/%s %s", dev, dev2, pid, cfr->site,cfr->protocol, cfr->args);
	if(0)printf ("* PID %d\n", pid);

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

			if((cg != NULL) /* && ((cg->flags & (F_LEASED) || !(cg->flags & (F_INCOMING|F_OUTGOING))) */ ) {
				int err = pushprot (conn->cg, conn->minor, conn->connref, PUSH_AFTER);
				if(err != 0) {
					if(log_34 & 2)
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
				conn->sentsetup = 1;
				cg->card = str_enter("*"); /* cosmetic */
				setconnstate(conn,c_down);
			}
			if((cg != NULL) && (cg->flags & (F_INCOMING|F_OUTGOING)) && !(cg->flags & F_NOSTART)) {
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
				if(startconn(conn->cg,0,0, &msg, NULL) != conn) {
					syslog(LOG_CRIT,"Bug in runprog->startconn (%s) for %s:%s",msg ? msg : "(unknown reason)", cg->site,cg->protocol);
					dropgrab(conn->cg);
					conn->cg = NULL;
					chkone(conn);
				}
			}
		}
	}
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
		if(maskmatch(pro->mask,conn->cg->mask) == 0) continue;
		if(classmatch(pro->cclass,conn->cg->cclass) == NULL) continue;

		xconn = xmalloc(sizeof(*xconn));
		if(xconn != NULL) {
			bzero(xconn,sizeof(*xconn));
			xconn->seqnum = ++connseq;
			xconn->cause = ID_priv_Print;
			xconn->causeInfo = "Signal HUP";
			xconn->pid = pro->pid;
			conn->cg->refs++;
			/* dropgrab(conn->cg; ** is new anyway */
			xconn->cg = conn->cg;
			xconn->next = isdn4_conn; isdn4_conn = xconn;
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
		ulong_t sub;
		int id = 0;
		char *ids;

		if(strchr(cfr->type,what) == NULL) continue;
		if((sit = wildmatch(conn->cg->site,cfr->site)) == NULL) continue;
		if((pro = wildmatch(conn->cg->protocol,cfr->protocol)) == NULL) continue;
		if((car = wildmatch(conn->cg->card,cfr->card)) == NULL) continue;
		if((sub = maskmatch(conn->cg->mask,cfr->mask)) == 0) continue;
		if((cla = classmatch(theclass,classmatch(conn->cg->cclass,cfr->cclass))) == NULL) continue;
		if((ids = strchr(cfr->type,'/')) != NULL)
			id = atoi(ids);

		for(pr = conn->run; pr != NULL; pr = pr->next) {
			struct conninfo *xconn;
			if(id != 0 && pr->id == id)
				break;
			if(strchr(pr->type,what) == NULL) continue;
			if(wildmatch(pr->site,sit) == NULL) continue;
			if(wildmatch(pr->protocol,pro) == NULL) continue;
			if(wildmatch(pr->card,car) == NULL) continue;
			if(maskmatch(pr->mask,sub) == 0) continue;
			if(classmatch(pr->cclass,cla) == NULL) continue;

			xconn = xmalloc(sizeof(*xconn));
			if(xconn != NULL) {
				bzero(xconn,sizeof(*xconn));
				xconn->seqnum = ++connseq;
				xconn->cause = ID_priv_Print;
				xconn->causeInfo = "Signal USR1";
				xconn->pid = pr->pid;
				conn->cg->refs++;
				/* dropgrab(conn->cg; ** is new anyway */
				xconn->cg = conn->cg;
				xconn->next = isdn4_conn; isdn4_conn = xconn;
				dropconn(xconn);
			}
			if(strchr(pr->type,'s') == NULL)
				kill (pr->pid, SIGUSR1);
			break;
		}
		if(pr == NULL) {
			runprog (cfr, &conn, NULL, what);
		}
	}
}


/* Find the next program to run */
void
run_now(void *nix)
{
	cf what;
	int spos = 0;

	if(do_run_now > 1) {
		do_run_now--;
		progidx = 0;
		return;
	}
	if(signal(SIGHUP,SIG_IGN) != SIG_IGN)
		signal (SIGHUP, SIG_DFL);
	if((in_boot < 0)|| quitnow)
		return;

	for(what = cf_R; what != NULL; what = what->next) {
		if(spos++ < progidx) {
			if(log_34 & 2)
				printf("Skip #%d; ",spos);
			continue;
		}
		progidx++;
		if(log_34 & 2)
			printf("Do #%d...",spos);
		if(what->got_err) {
			if(log_34 & 2)
				printf("StoredErr; ");
			continue;
		}
		if(strchr(what->type,'B') != NULL || strchr(what->type,'p') != NULL) {
			struct conninfo *conn;

			for(conn = isdn4_conn; conn != NULL; conn = conn->next) {
				if(conn->ignore || (conn->cg == NULL))
					continue;
				if(strcmp(conn->cg->site,what->site))
					continue;
				if(strcmp(conn->cg->protocol,what->protocol))
					continue;
				if(!classmatch(theclass,classmatch(conn->cg->cclass,what->cclass)))
					continue;
				break;
			}
			if(conn == NULL) {
				conngrab cg = newgrab(NULL);
				char *err;
			
				if(log_34 & 2)
					printf("run %s:%s; ",what->site,what->protocol);
				cg->site = what->site; cg->protocol = what->protocol;
				cg->card = what->card; cg->cclass = what->cclass;
				if (strchr(what->type,'p') != NULL)
					cg->flags |= F_PERMANENT;
				if (strchr(what->type,'f') != NULL)
					cg->flags |= F_LEASED;
				if (strchr(what->type,'B') != NULL)
					cg->flags |= F_OUTGOING;
				else
					cg->flags |= F_NOSTART;
				err = runprog(what,&conn,&cg,'B');
				cg->flags &=~ F_NOSTART;
				if(conn != NULL) {
					conn->cardname = what->card;
					conn->classname = what->cclass;
					kill_rp(conn,'t');
					run_rp(conn,'i');
				} else if(err != NULL) {
					conn = xmalloc(sizeof(*conn));
					if(conn != NULL) {
						bzero(conn,sizeof(*conn));
						conn->seqnum = ++connseq;
						conn->cause = ID_priv_Print;
						conn->causeInfo = err;
						cg->refs++;
						conn->cg = cg;
						conn->next = isdn4_conn; isdn4_conn = conn;
						dropconn(conn);
					}
				}
				chkone(cg);
				dropgrab(cg);
				if(err == NULL || !strncasecmp(err,"no free dev",11)) {
					if(err != NULL) {
						spos--;
						printf("Try again: %s",err);
					}
					if(log_34 & 2)
						printf("\n");
					progidx = spos;
					timeout(run_now,NULL,HZ);
					return;
				} else {
					if(log_34 & 2)
						printf("FAIL %s\n",err);
				}
			} else {
				if(log_34 & 2)
					printf("exist %s:%s\n",conn->cg->site,conn->cg->protocol);
				if(conn->cg != NULL && conn->minor != 0 && conn->pid != 0) {
					if(conn->cg->cclass != NULL) {
						if((classmatch(conn->cg->cclass, theclass) == NULL) && (conn->state >= c_going_up)) {
							mblk_t *mb = allocb(80,BPRI_MED);

							setconnstate(conn, c_down);
							if(log_34 & 2)printf("DisM8 ");
							if(mb != NULL) {
								int xlen;
								*mb->b_wptr++ = PREF_NOERR;
								m_putid (mb, CMD_OFF);
								m_putsx (mb, ARG_MINOR);
								m_puti (mb, conn->minor);
								xlen = mb->b_wptr - mb->b_rptr;
								DUMPW (mb->b_rptr, xlen);
								(void) strwrite (xs_mon, (uchar_t *) mb->b_rptr, xlen, 1);
								freeb(mb);
							}
						}
					}
					if(conn->state >= c_going_up)
						pushprot(conn->cg,conn->minor,conn->connref,PUSH_UPDATE);
					else
						pushprot(conn->cg,conn->minor,conn->connref,PUSH_AFTER|PUSH_UPDATE);
					progidx = spos;
					timeout(run_now,NULL,HZ/3);
					return;
				}
			}
		}
	}
	if(signal(SIGHUP,SIG_IGN) != SIG_IGN)
    	signal (SIGHUP, (sigfunc__t) read_args_run);
	progidx = 0;
	do_run_now = 0;
}


/* Any programs still running? */
int
has_progs(void)
{
	struct conninfo *conn;
	if(!quitnow)
		return 1;
	for(conn = isdn4_conn; conn != NULL; conn = conn->next) {
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
	for(conn = isdn4_conn; conn != NULL; conn = nconn) {
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
