/*
 * This file is part of the ISDN master program.
 *
 * Copyright (C) 1995 Matthias Urlichs.
 * See the file COPYING for license details.
 */

#include "master.h"


/* Clone a connection info record ("conngrab" because I use it to collect
   all pertinent information to grab data out of the configuration files
   with), or create a new one. */
conngrab
Xnewgrab(conngrab master, int lin)
{
	conngrab slave;

	slave = gcxmalloc(sizeof(*slave));
	if(slave == NULL)
		return NULL;
	GrabAllocs++;
	if(master == NULL) {
		bzero(slave,sizeof(*slave));
		slave->cclass = "*";
		slave->mask = ~0;
	} else {
		if(master->refs == 0 || master->protocol == (char *)0xdeadbeef)
			panic("FreeGrab");
		*slave = *master;
		if(slave->par_out != NULL)
			slave->par_out = copybufmsg(slave->par_out);
		if(slave->par_in != NULL)
			slave->par_in = dupmsg(slave->par_in);
	}
	slave->refs = 1;
	return slave;
}

/* Forget one... */
void
Xdropgrab(conngrab cg,int lin)
{
	if(cg == NULL)
		return;
	chkone(cg);
	cg->dropline = lin;

	if(--cg->refs == 0) {
		chkone(cg);
		GrabFrees++;
		if(cg->par_out != NULL)
			freemsg(cg->par_out);
		if(cg->par_in != NULL)
			freemsg(cg->par_in);
		cg->par_out = (void *)0xdeadbeef;
		cg->par_in  = (void *)0xdeadbeef;
		cg->site    = (void *)0xdeadbeef;
		cg->protocol= (void *)0xdeadbeef;
		cg->cclass  = (void *)0xdeadbeef;
		cg->card    = (void *)0xdeadbeef;
		chkone(cg);
		gfree(cg);
		return;
	}
}

/* Set the reference number. For debugging. */
void
Xsetconnref(const char *deb_file, unsigned int deb_line, conninfo conn, int connref)
{
	if(log_34 & 2)
		printf("-%s:%d: SetConnRef.%p %d/%d/%ld -> %d\n",deb_file,deb_line,conn,conn->minor,conn->fminor,conn->connref,connref);
	conn->connref = connref;
}

/* Print the text foo onto all ATL/ channels. */
void
connreport(char *foo, char *card, int minor)
{
	conninfo conn;
	mblk_t xx;
	struct datab db;
	char ans[20];
	xx.b_rptr = ans;
	db.db_base = ans;
	db.db_lim = ans + sizeof (ans);
	xx.b_datap = &db;

	for(conn = isdn4_conn; conn != NULL; conn = conn->next) {
		struct iovec io[2];

		chkone(conn);
		if(conn->ignore < 3 || conn->minor == 0)
			continue;
		if(!wildmatch(conn->cardname,card))
			continue;
		if(minor != 0 && conn->minor != minor)
			continue;
		if(conn->lastMsg != NULL && !strcmp(conn->lastMsg,foo))
			continue;
		if(conn->lastMsg != NULL) 
			gfree(conn->lastMsg);
		conn->lastMsg = gbxmalloc(strlen(foo)+1);
		if(conn->lastMsg != NULL) 
			strcpy(conn->lastMsg,foo);

		xx.b_wptr = ans;
		m_putid (&xx, CMD_PROT);
		m_putsx (&xx, ARG_FMINOR);
		m_puti (&xx, conn->minor);
		m_putdelim (&xx);
		m_putid (&xx, PROTO_AT);
		io[0].iov_base = xx.b_rptr;
		io[0].iov_len = xx.b_wptr - xx.b_rptr;
		io[1].iov_base = foo;
		io[1].iov_len = strlen(foo);
		(void) strwritev (xs_mon, io, 2, 1);
	}
}

/* Print the state of this connection with connreport(). */
void
ReportOneConn(conninfo conn, int minor)
{
	char is1[15]="",sp[200], *spf = sp;
	char cn = 1;

	if(conn->cardname != NULL) {
		if((conn->cardname[0] == '*') && (conn->cardname[1] == '\0'))
			cn = 0;
	}
	if(conn->state != c_up) {
		if(conn->retries > 0)
			sprintf(is1,".%d",conn->retries);
		if(conn->retiming > 0)
			sprintf(is1+strlen(is1),"-%d",conn->retiming);
	}
	spf += sprintf(spf,"%s%d:%d %s %s %s %d %s%s/%s %ld %ld %s",
		conn->ignore?"!":"", conn->minor,
		conn->seqnum, conn->cg ? conn->cg->site : "-",
		conn->cg ? conn->cg->protocol : "-",
		conn->classname ? conn->classname : (conn->cg ? conn->cg->cclass : "-"),
		conn->pid, state2str(conn->state), is1,
		(cn && conn->cardname) ? conn->cardname : (conn->cg ? conn->cg->card : "-"),
		conn->charge, conn->ccharge, FlagInfo(conn->flags));
	if(conn->cg != NULL && (conn->cg->flags ^ conn->flags) != 0) {
		int foo = strlen(FlagInfo(conn->cg->flags ^ conn->flags));
		int bar = strlen(FlagInfo(conn->cg->flags));
		if(foo*3 < bar*2)
			spf += sprintf(spf, "^%s",FlagInfo(conn->cg->flags ^ conn->flags));
		else
			spf += sprintf(spf, "/%s",FlagInfo(conn->cg->flags));
	}
	if(conn->cg != NULL && conn->cg->nr != NULL)
		spf += sprintf(spf, ",%s",conn->cg->nr);
	else if(conn->cg != NULL && conn->cg->oldnr != NULL)
		spf += sprintf(spf, ",%s",conn->cg->oldnr);
	if(conn->cg != NULL && conn->cg->lnr != NULL)
		spf += sprintf(spf, ";%s",conn->cg->lnr);
	else if(conn->cg != NULL && conn->cg->oldlnr != NULL)
		spf += sprintf(spf, ";%s",conn->cg->oldlnr);
	spf += sprintf(spf," %s", CauseInfo(conn->cause, conn->causeInfo));
	connreport(sp,(conn->cg ? conn->cg->card : "*"),minor);
}

static void nodown(conninfo conn)
{
	syslog(LOG_CRIT,"Verbindung geht nicht runter! Problem! Nothalt wegen %s!", conn->cg ? conn->cg->site : "???");
	_exit(10);
}

static inline int retimeout(conninfo conn)
{
	int tim = 0;
	/* Exponential backoff. Sorry but this is necessary. */
	conn->retiming++;
	if(conn->charge != 0)
		tim = 5*60*(1<<conn->retiming);
	else if (conn->cause == ID_priv_Busy)
		tim = 5*(1<<(conn->retiming/6));
	else if(conn->flags & F_FASTREDIAL)
		tim = 2+(1<<(conn->retiming/4));
	else
		tim = 5+(1<<(conn->retiming/2));
	if((tim <= 0) || (tim > 60*60*2))
		tim = 60*60*2;
	return tim;
}

/* Sets the state of a connection; does all the housekeeping associated
   with the change. */
void
Xsetconnstate(const char *deb_file, unsigned int deb_line,conninfo conn, CState state)
{
	chkone(conn);
	if(log_34 & 2) {
		printf("%s:%d: State %d: %s",deb_file,deb_line,conn->minor,state2str(conn->state));
		if(conn->state != state)
			printf(" -> %s\n",state2str(state));
		else
			printf("\n");
	}
	if(conn->timer_reconn && (state == c_off || state == c_offdown || (state >= c_going_up
		&& conn->state < c_going_up))) {
		conn->timer_reconn = 0;
		untimeout(time_reconn,conn);
	} else if(!conn->timer_reconn && state != c_off && state < c_going_up && conn->state >= c_going_up) {
		if(conn->want_fast_reconn) {
			conn->want_fast_reconn = 0;
		} else {
			conn->timer_reconn = 1;
			if(conn->flags & F_FASTREDIAL)
				timeout(time_reconn,conn,HZ);
			else if(conn->flags & F_FORCEIN)
				timeout(time_reconn,conn,30*HZ);
			else
				timeout(time_reconn,conn,6*HZ);
		}
	}

	if(conn->cg != NULL) {
		if(state == c_up) {
			conn->cg->d_level = 0;
			conn->cg->d_nextlevel = 0;
		} else if(state == c_off) {
			if(conn->cg->d_level < conn->cg->d_nextlevel) {
				conn->cg->d_level = conn->cg->d_nextlevel;
				state = c_down;
				conn->retries = 0;
			} else {
				conn->cg->d_level = 0;
				conn->cg->d_nextlevel = 0;
			}
		}
	}
	if((conn->minor > 0) && (conn->flags & F_PERMANENT)) {
		if((conn->state >= c_down) && (state <= c_off) && conn->sentsetup) {
			mblk_t *mb = allocb(30,BPRI_MED);
			if(mb != NULL) {
				int xlen;

				m_putid (mb, CMD_PROT);
				m_putsx (mb, ARG_MINOR);
				m_puti (mb, conn->minor);
				m_putdelim (mb);
				m_putid (mb, PROTO_DISABLE);
				xlen = mb->b_wptr - mb->b_rptr;
				DUMPW (mb->b_rptr, xlen);
				(void) strwrite (xs_mon, (uchar_t *) mb->b_rptr, xlen, 1);
				freemsg(mb);
			}
		}
		
		if((conn->state >= c_off) && (state == c_down) && conn->sentsetup) {
			mblk_t *mb = allocb(30,BPRI_MED);
			if(mb != NULL) {
				int xlen;

				m_putid (mb, CMD_PROT);
				m_putsx (mb, ARG_MINOR);
				m_puti (mb, conn->minor);
				m_putdelim (mb);
				m_putid (mb, PROTO_ENABLE);
				xlen = mb->b_wptr - mb->b_rptr;
				DUMPW (mb->b_rptr, xlen);
				(void) strwrite (xs_mon, (uchar_t *) mb->b_rptr, xlen, 1);
				freemsg(mb);
			}
		}
		
		if((conn->state < c_up) && (state == c_up))
			pushprot(conn->cg,conn->minor,conn->connref,PUSH_UPDATE);
	}

#if 0
	if(conn->state <= c_down)
		setconnref(conn,0);
#endif
	if(state == c_up) {
		conn->cause = 0;
		conn->upwhen = time(NULL);
		conn->did_bounce = 0;
	}
	else if(state == c_going_up) {
		conn->cause = 999999;
		conn->did_bounce = 1;
	}
	if(conn->state == c_up && state < c_up) {
		int conntime = time(NULL) - conn->upwhen;
		if(conn->cg != NULL)
			syslog(LOG_INFO,"CONN %s:%s %ld",conn->cg->site,conn->cg->protocol,conntime);
		else
			syslog(LOG_WARNING,"CONN ??? %ld",conntime);
	}
	if((conn->state < c_going_down && state > c_going_down) || state < c_off) {
		if(conn->charge > 0) {
			if(conn->cg != NULL)
				syslog((state > c_going_down) ? LOG_INFO : LOG_WARNING,"COST %s:%s %ld",conn->cg->site,conn->cg->protocol,conn->charge);
			else
				syslog(LOG_WARNING,"COST ??? %ld",conn->charge);
			run_rp(conn,'l');
		}
		conn->ccharge += conn->charge;
		conn->charge = 0;
	}
	if(conn->cg != NULL) {
		if(conn->state == c_off && state > c_off) {
			kill_rp(conn,'x');
			run_rp(conn,'y');
		}
		if(conn->state > c_off && state == c_off) {
			kill_rp(conn,'y');
			run_rp(conn,'x');
		}
		if(conn->state != c_up && state == c_up) {
			kill_rp(conn,'d');
			kill_rp(conn,'f');
			run_rp(conn,'u');
		}
		if(conn->state == c_up && state != c_up) {
			kill_rp(conn,'u');
			run_rp(conn,'d');
		}
		if(conn->state == c_going_up && state <= c_going_down) {
			kill_rp(conn,'u');
			run_rp(conn,'f');
		}
		if(conn->state > c_forceoff && state <= c_forceoff && conn->pid != 0) {
			struct conninfo *xconn;
			xconn = gcxmalloc(sizeof(*xconn));
			if(xconn != NULL) {
				bzero(xconn,sizeof(*xconn));
				xconn->seqnum = ++connseq;
				xconn->cause = ID_priv_Print;
				xconn->causeInfo = "Sig TERM (force off)";
				xconn->pid = conn->pid;
				conn->cg->refs++;
				/* dropgrab(conn->cg; ** is new anyway */
				xconn->cg = conn->cg;
				xconn->next = isdn4_conn; isdn4_conn = xconn;
				dropconn(xconn);
			}
			kill(conn->pid,SIGTERM);
			conn->pid = 0;
		}
	}
	if(conn->state != c_going_down && state == c_going_down)
		timeout(nodown,conn,10*HZ);
	else if(conn->state == c_going_down && state != c_going_down)
		untimeout(nodown,conn);
	if(conn->state == c_off && state > c_off) {
		conn->retries = 0;
		conn->want_reconn = 0;
	}

	if((conn->state != c_down) || (state != c_going_down))
		conn->state=state; /* ^^- prevents spurious messages */

	if(conn->ignore < 2)
		ReportConn(conn);
	if(state <= c_down)
		conn->connref = 0;
	if(conn->ignore)
		return;
	if(state == c_down && conn->want_reconn) {
		conn->want_reconn--;
		try_reconn(conn);
	} else if(state == c_up) {
		conn->retries = 0;
		conn->retiming = 0;
		conn->want_reconn = 0;
	}
	if((state == c_off) && !conn->retime && (conn->flags & F_PERMANENT)) {
		conn->retime = 1;
		timeout(retime,conn,retimeout(conn)*HZ);
	} else if((state > c_off) && conn->retime) {
		conn->retime = 0;
		untimeout(retime,conn);
	}
}

/* "rdrop" means "really drop". */
void rdropconn (struct conninfo *conn, int deb_line) {
	conn->ignore=2; dropconn(conn); }

void
Xdropconn (struct conninfo *conn, const char *deb_file, unsigned int deb_line)
{
	chkone(conn);
	if(conn->locked) {
		if(log_34 & 2)printf ("DropConn %s:%d: LOCK %d/%d/%ld\n", deb_file,deb_line, conn->minor, conn->fminor, conn->connref);
		return;
	}
	if(log_34 & 2)printf ("DropConn %s:%d: %d/%d/%ld\n", deb_file,deb_line, conn->minor, conn->fminor, conn->connref);
	if(!conn->ignore) {
		conn->ignore=1;
		setconnstate(conn,c_unknown);
#if 0
		if(conn->state > c_off)
			setconnstate(conn, c_off);
		else
			ReportConn(conn);
#endif
		timeout(rdropconn,conn,HZ*60*5); /* Drop the record after five minutes */
		return;
	} else if(conn->ignore == 1) { /* already going to drop it */
		setconnstate(conn,c_unknown);
		return;
	} else
		setconnstate(conn,c_unknown);

	{	/* unchain the conn from the list */
		/* Could use a doubly-linked list here, but what the ... */
		struct conninfo **pconn = &isdn4_conn;
		while(*pconn != NULL) {
			if(*pconn == conn) {
				*pconn = conn->next;
				break;
			}
			pconn = &(*pconn)->next;
		}

	}
	{
		struct proginfo *run;

		for (run = conn->run; run != NULL; run = run->next) {
			if(strchr(run->type,'s') == NULL)
				kill(run->pid,SIGQUIT);
			run->master = NULL;
		}
	}
	{	/* Say that we forgot the thing. */
		char xs[10];
		sprintf(xs,"-%d",conn->seqnum);
		connreport(xs,conn->cg ? conn->cg->card : "*",0);
	}
	printf("Drop %p %s:%d\n",conn,deb_file,deb_line);
	dropgrab(conn->cg);
	if(conn->lastMsg != NULL)
		gfree(conn->lastMsg);
	gfree(conn);
}


void
retime(struct conninfo *conn)
{
	if(conn->retime) {
		conn->retime = 0;
		if(conn->state == c_off)
			setconnstate(conn,c_down);
	}
}

void
time_reconn(struct conninfo *conn)
{
	if(conn->timer_reconn) {
		conn->timer_reconn = 0;
		if(conn->want_reconn) 
			try_reconn(conn);
	}
}

/* Reestablish a connection, eg. because data are to be transmitted. */
void
try_reconn(struct conninfo *conn)
{
	mblk_t *md;

	chkone(conn);
	if(conn == NULL || conn->state <= c_off)
		return;
	if(conn->state != c_down && conn->state != c_up) {
		if(!(conn->want_reconn))
			conn->want_reconn = 2;
		return;
	}
	if(conn->timer_reconn) {
		if(!(conn->want_reconn))
			conn->want_reconn = 2;
		return;
	}

	if(conn->state != c_down)
		return;

	md = allocb(256,BPRI_LO);
	if(md != NULL) {
		conngrab cg = conn->cg;
		struct conninfo *xconn;
		char *ret = NULL;

		if(cg == NULL)
			return;

		chkone(cg);
		cg->refs++;

		if(conn->want_reconn < MAX_RECONN) { /* if ==, we have a direct callback */
			cg->nr = NULL; cg->nrsuf = NULL;
			cg->flags &=~F_NRCOMPLETE;
		}
		cg->lnr = NULL; cg->lnrsuf = NULL;
		cg->card = conn->cardname ? conn->cardname : "*";
		cg->cclass = conn->classname ? conn->classname : "*";
		cg->flags &=~(F_INCOMING|F_OUTCOMPLETE|F_LNRCOMPLETE);
		cg->flags |= F_OUTGOING;

		/* If more than one bit is set, this is true. */
		if((cg->flags & F_MASKFLAGS) & ((cg->flags & F_MASKFLAGS) - 1))
			cg->flags &= ~F_MASKFLAGS;
		if(cg->par_out != NULL)
			freemsg(cg->par_out);
		if((cg->par_out = allocb(256,BPRI_LO)) == NULL) {
			dropgrab(cg);
			freeb(md);
			conn->cause = ID_priv_Print;
			conn->causeInfo = "NoMem";
			setconnstate(conn,c_off);
		}
		if(cg->par_in != NULL) {
			freemsg(cg->par_in);
			cg->par_in = NULL;
		}

		/* anything else is added by startconn */

		if((xconn = startconn(cg,0,0, &ret, NULL)) == conn) {
			dropgrab(cg);
			freeb(md);
			return;
		}
		dropgrab(cg);
		if(ret != NULL) {
			if(!strcmp(ret,"0BUSY")) {
				conn->cause = ID_priv_Busy;
			} else {
				conn->cause = ID_priv_Print;
				conn->causeInfo = ret;
			}
		}
		setconnstate(conn,c_off);
		freeb(md);
	} else
		setconnstate(conn,c_off);
}


