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
conngrab Xnewgrab(conngrab master, int lin)
{
	conngrab slave;

	slave = malloc(sizeof(*slave));
	if(slave == NULL)
		return NULL;
	if(master == NULL) {
		bzero(slave,sizeof(*slave));
		slave->cclass = str_enter("*");
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
void Xdropgrab(conngrab cg,int lin)
{
	if(cg == NULL)
		return;
	chkone(cg);
	cg->dropline = lin;

	if(--cg->refs == 0) {
		chkone(cg);
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
		free(cg);
		return;
	}
}

/* Set the reference number. For debugging. */
void Xsetconnref(const char *deb_file, unsigned int deb_line, conninfo conn, int connref)
{
	if(0)printf("-%s:%d: SetConnRef.%p %d/%d/%ld -> %d\n",deb_file,deb_line,conn,conn->minor,conn->fminor,conn->connref,connref);
	conn->connref = connref;
}

/* Print the text foo onto all ATL/ channels. */
void connreport(char *foo)
{
	conninfo conn;
	mblk_t xx;
	struct datab db;
	char ans[20];
	xx.b_rptr = ans;
	db.db_base = ans;
	db.db_lim = ans + sizeof (ans);
	xx.b_datap = &db;

	for(conn = theconn; conn != NULL; conn = conn->next) {
		struct iovec io[2];

		chkone(conn);
		if(conn->ignore != 3 || conn->minor == 0)
			continue;

		xx.b_wptr = ans;
		m_putid (&xx, CMD_PROT);
		m_putsx (&xx, ARG_FMINOR);
		m_puti (&xx, conn->minor);
		m_putdelim (&xx);
		m_putid (&xx, PROTO_AT);
		*xx.b_wptr++ = '*';
		io[0].iov_base = xx.b_rptr;
		io[0].iov_len = xx.b_wptr - xx.b_rptr;
		io[1].iov_base = foo;
		io[1].iov_len = strlen(foo);
		DUMPW (xx.b_rptr, io[0].iov_len);
		printf ("+ ");
		DUMPW (foo,strlen(foo));
		(void) strwritev (xs_mon, io, 2, 1);
	}
}

/* Print the state of this connection with connreport(). */
void ReportConn(conninfo conn)
{
	char sp[200], *spf = sp;
	spf += sprintf(spf,"%s%d:%d %s %s %s %d %s/%s %ld %ld %s",
		conn->ignore?"!":"", conn->minor,
		conn->seqnum, conn->cg ? conn->cg->site : "-",
		conn->cg ? conn->cg->protocol : "-", conn->cg ? conn->cg->cclass : "-", conn->pid,
		state2str(conn->state), conn->cg ? conn->cg->card : "-", conn->charge,
		conn->ccharge, FlagInfo(conn->flags));
	if(conn->cg != NULL && (conn->cg->flags ^ conn->flags) != 0) {
		int foo = strlen(FlagInfo(conn->cg->flags ^ conn->flags));
		int bar = strlen(FlagInfo(conn->cg->flags));
		if(foo < bar)
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
	connreport(sp);
}

/* Sets the state of a connection; does all the housekeeping associated
   with the change. */
void Xsetconnstate(const char *deb_file, unsigned int deb_line,conninfo conn, CState state)
{
	chkone(conn);
	printf("%s:%d: State %d: %s",deb_file,deb_line,conn->minor,state2str(conn->state));
	if(conn->state != state)
		printf(" -> %s\n",state2str(state));
	else
		printf("\n");
	if(conn->timer_reconn && (state == c_offdown || (state >= c_going_up
		&& conn->state < c_going_up))) {
		conn->timer_reconn = 0;
		untimeout(time_reconn,conn);
	} else if(!conn->timer_reconn && state < c_going_up && conn->state >= c_going_up) {
		if(conn->want_fast_reconn) {
			conn->want_fast_reconn = 0;
		} else {
			conn->timer_reconn = 1;
			if(conn->flags & F_FASTREDIAL)
				timeout(time_reconn,conn,HZ);
			else
				timeout(time_reconn,conn,5*HZ);
		}
	}
	if(conn->state <= c_down)
		setconnref(conn,0);
	if(state == c_up)
		conn->cause = 0;
	else if(state == c_going_up)
		conn->cause = 999999;
	if((conn->state < c_going_down && state > c_going_down) || state <= c_off) {
		if(conn->charge > 0) {
			if(conn->cg != NULL)
				syslog(LOG_WARNING,"COST %s:%s %ld",conn->cg->site,conn->cg->protocol,conn->charge);
			else
				syslog(LOG_WARNING,"COST ??? %ld",conn->charge);
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
		if(conn->state != c_forceoff && state == c_forceoff && conn->pid != 0) {
			struct conninfo *xconn;
			xconn = malloc(sizeof(*xconn));
			if(xconn != NULL) {
				bzero(xconn,sizeof(*xconn));
				xconn->seqnum = ++connseq;
				xconn->cause = ID_priv_Print;
				xconn->causeInfo = "Sig TERM (force off)";
				xconn->pid = conn->pid;
				conn->cg->refs++;
				/* dropgrab(conn->cg; ** is new anyway */
				xconn->cg = conn->cg;
				xconn->next = theconn;
				theconn = xconn;
				dropconn(xconn);
			}
			kill(conn->pid,SIGTERM);
			conn->pid = 0;
		}
	}
	conn->state=state;
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
	if(state >= c_going_up) {
		conn->got_id = 0;
		conn->got_hd = 0;
	}
	if((state == c_off) && !conn->retime && (conn->flags & F_PERMANENT)) {
		conn->retime = 1;
		timeout(retime,conn,((conn->charge != 0) ? 5*60*++conn->retiming : (conn->cause == ID_priv_Busy) ? 5 : (conn->flags & F_FASTREDIAL) ? 2 : 5)*HZ);
	} else if((state != c_off) && conn->retime) {
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
		if(1)printf ("DropConn %s:%d: LOCK %d/%d/%ld\n", deb_file,deb_line, conn->minor, conn->fminor, conn->connref);
		return;
	}
	if(1)printf ("DropConn %s:%d: %d/%d/%ld\n", deb_file,deb_line, conn->minor, conn->fminor, conn->connref);
	if(!conn->ignore) {
		conn->ignore=1;
		setconnstate(conn,c_forceoff);
#if 0
		if(conn->state > c_off)
			setconnstate(conn, c_off);
		else
			ReportConn(conn);
#endif
		timeout(rdropconn,conn,HZ*60*5); /* Drop the record after five minutes */
		return;
	} else if(conn->ignore == 1) { /* already going to drop it */
		setconnstate(conn,c_forceoff);
		return;
	} else
		setconnstate(conn,c_forceoff);

	{	/* unchain the conn from the list */
		/* Could use a doubly-linked list here, but what the ... */
		struct conninfo **pconn = &theconn;
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
		connreport(xs);
	}
	dropgrab(conn->cg);
	free(conn);
}


void retime(struct conninfo *conn)
{
	if(conn->retime) {
		int xlen;

		if (conn->flags & F_PERMANENT) {
			mblk_t *mb = allocb(30,BPRI_MED);

			if(mb != NULL) {
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

		conn->retime = 0;
		setconnstate(conn,c_down);

	}
}

void time_reconn(struct conninfo *conn)
{
	if(conn->timer_reconn) {
		conn->timer_reconn = 0;
		if(conn->want_reconn) 
			try_reconn(conn);
	}
}

/* Reestablish a connection, eg. because data are to be transmitted. */
void try_reconn(struct conninfo *conn)
{
	mblk_t *md;
	int xlen;

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

		cg->nr = NULL; cg->nrsuf = NULL;
		cg->lnr = NULL; cg->lnrsuf = NULL;
		cg->card = str_enter("*");;
		cg->cclass = str_enter("*");;
		cg->flags &=~(F_MOVEFLAGS|F_INCOMING|F_OUTCOMPLETE|F_NRCOMPLETE|F_LNRCOMPLETE);
		cg->flags |= F_OUTGOING;
		if((cg->flags & (F_PERMANENT|F_LEASED)) == F_PERMANENT)
			cg->flags |= F_DIALUP;
		if(cg->par_out != NULL)
			freemsg(cg->par_out);
		if((cg->par_out = allocb(256,BPRI_LO)) == NULL) {
			dropgrab(cg);
			freeb(md);
			return;
		}
		if(cg->par_in != NULL) {
			freemsg(cg->par_in);
			cg->par_in = NULL;
		}

		/* anything else is added by startconn */

		if((xconn = startconn(cg,0,0, &ret)) == conn) {
			dropgrab(cg);
			freeb(md);
			return;
		}
		dropgrab(cg);
		if(ret != NULL) {
			setconnstate(conn,c_going_up);
			if(!strcmp(ret,"0BUSY")) {
				conn->cause = ID_priv_Busy;
				if ((conn->flags & F_PERMANENT) && (conn->minor != 0)) {
					mblk_t *mb = allocb(30,BPRI_MED);

					setconnstate(conn, c_down);
					m_putid (mb, CMD_PROT);
					m_putsx (mb, ARG_MINOR);
					m_puti (mb, conn->minor);
					m_putdelim (mb);
					m_putid (mb, PROTO_DISABLE);
					xlen = mb->b_wptr - mb->b_rptr;
					DUMPW (mb->b_rptr, xlen);
					(void) strwrite (xs_mon, (uchar_t *) mb->b_rptr, xlen, 1);
					freeb(mb);
				}
			} else {
				conn->cause = ID_priv_Print;
				conn->causeInfo = ret;
			}
printf("DropThis, %s\n",ret);
			setconnstate(conn,c_off);
			return;
		}

		md->b_rptr = md->b_wptr = md->b_datap->db_base;
		m_putid(md,CMD_PROT);
		m_putsx(md,ARG_MINOR);
		m_puti(md,conn->minor);
		m_putdelim (md);
		m_putid (md, PROTO_DISABLE);
		setconnstate(conn,c_off);

		xlen=md->b_wptr-md->b_rptr;
		DUMPW (md->b_rptr, xlen);
		(void) strwrite (xs_mon, md->b_rptr, xlen, 1);
		freeb(md);
	} else
		setconnstate(conn,c_off);
}

