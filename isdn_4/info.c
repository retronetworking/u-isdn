/*
 * This file is part of the ISDN master program.
 *
 * Copyright (C) 1995 Matthias Urlichs.
 * See the file COPYING for license details.
 */

#include "master.h"
#include "isdn_12.h"

/* Once upon a time, do_info() was a very big function. */

#ifndef __GNUC__
#error "Sorry, but you need GCC's nested functions for this."
#endif

void
do_info (streamchar * data, int len)
{
mblk_t xx;
struct datab db;
ushort_t id;
ushort_t ind;
streamchar ans[MAXLINE];
char *msgbuf;
char *resp;
long fminor;
long minor;
long callref;
struct conninfo *conn;
char crd[5];
char prot[20];
char nr[MAXNR + 2];
char lnr[MAXNR + 2];
long uid;
long connref;
char dialin;
long charge;
ushort_t cause;
int xlen;
int has_force;
long bchan;
long hdrval;
char no_error;

int
parse_arg(void) 
{
	switch (id) {
	case ARG_CAUSE:
		(void)m_getid(&xx,&cause);
		break;
	case ARG_CHARGE:
		(void)m_geti(&xx,&charge);
		break;
	case ARG_FORCE:
		has_force=1;
		break;
	case ARG_LLC:
	case ARG_ULC:
	case ARG_BEARER:
		break;
	case ARG_NUMBER:
		{
			int err2;

			if ((err2 = m_getstr (&xx, nr, MAXNR)) != 0) {
				if (err2 != ENOENT && err2 != ESRCH) {
					printf (" XErr 0\n");
					return 2;
				}
			}
		} break;
	case ARG_LNUMBER:
		{
			int err2;

			if ((err2 = m_getstr (&xx, lnr, MAXNR)) != 0) {
				if (err2 != ENOENT && err2 != ESRCH) {
					printf (" XErr 02\n");
					return 2;
				}
			}
		} break;
	case ARG_CARD:
		if (m_getstr (&xx, crd, 4) != 0) {
			printf (" XErr 1\n");
			return 2;
		}
		break;
	case PROTO_INCOMING:
		dialin = 1;
		break;
	case PROTO_OUTGOING:
		dialin = 0;
		break;
	case ARG_MINOR:
		if (m_geti (&xx, &minor) != 0) {
			printf (" XErr 3\n");
			return 2;
		}
		if (minor < 0 || minor >= NMINOR) {
			printf (" XErr 4a\n");
			return 2;
		}
		break;
	case ARG_UID:
		if (m_geti (&xx, &uid) != 0) {
			printf (" XErr u\n");
			return 2;
		}
		break;
	case ARG_ERRHDR:
		if (m_geti (&xx, &hdrval) != 0) {
			printf (" XErr 34\n");
			if(0)return 2;
		}
		break;
	case ARG_FMINOR:
		{
			long fmi;
			if (m_geti (&xx, &fmi) != 0) {
				printf (" XErr 3\n");
				return 2;
			}
			if (fminor != 0 && fminor != fmi) {
				resp = "ILLEGAL FMINOR";
				return 1;
			}
			fminor = fmi;
			if (fminor < 0 || fminor >= NMINOR) {
				printf (" XErr 4b\n");
				return 2;
			}
		} break;
	case ARG_CALLREF:
		if (m_geti (&xx, &callref) != 0) {
			printf (" XErr 5x\n");
			return 2;
		}
		break;
	case ARG_CONNREF:
		if (m_geti (&xx, &connref) != 0) {
			printf (" XErr 5\n");
			return 2;
		}
		break;
	case ARG_CHANNEL:
		if (m_geti (&xx, &bchan) != 0) {
			printf (" XErr 9a\n");
			return 2;
		}
		break;
	case ARG_STACK:
		if (m_getstr (&xx, prot, sizeof(prot)-1) != 0) {
			printf (" XErr 9\n");
			return 2;
		}
		break;
	}
	return 0;
}

void
find_conn(void)
{
	struct conninfo *xconn = NULL;
	if(0)printf ("Check Conn %ld/%ld/%ld: ", minor, fminor, connref);
	if(fminor == 0) fminor = minor;
	for (conn = theconn; conn != NULL; conn = conn->next) {
		if(conn->ignore)
			continue;
		if(0)printf ("%d/%d/%ld ", conn->minor, conn->fminor, conn->connref);
		if ((connref != 0) && (conn->connref != 0)) {
			if (conn->connref == connref)
				break;
			else
				continue; /* the connection was taken over... */
		}
		if ((minor != 0) && (conn->minor != 0)) {
			if (conn->minor == minor)
				break;
			else
				continue;
		}
		if ((fminor != 0) && (conn->fminor != 0)) {
			if (conn->fminor == fminor)
				xconn = conn;
		}
#if 0
		if (*(ulong_t *) crd != 0 && conn->cg->card != 0 && conn->cg->card != *(ulong_t *) crd)
			continue;
#endif
	}
	if(0)printf ("\n");
	if(conn == NULL)
		conn = xconn;
	if(conn != NULL) {
		printf("Found Conn %d/%d, cref %ld\n",conn->minor,conn->fminor,conn->connref);
	}
	if (conn != NULL && ind != IND_OPEN) {
		if (conn->minor == 0 && minor != 0) {
			conn->minor = minor;
		}
		if (conn->fminor == 0 && fminor != 0) {
			conn->fminor = fminor;
		}
		if (conn->connref == 0 && connref != 0) {
			setconnref(conn,connref);
		}
		if (conn->cg != NULL && conn->cg->card == NULL)
			conn->cg->card = str_enter(crd);
		if(conn->flags & F_INCOMING)
			dialin = 1;
		if(charge > 0) {
			conn->charge = charge;
			if(conn->state <= c_going_down) {
				if (++conn->chargecount == 3) {
					if(conn->cg != NULL)
						syslog(LOG_ALERT,"Cost Runaway, connection not closed for %s:%s",conn->cg->site,conn->cg->protocol);
					else
						syslog(LOG_ALERT,"Cost Runaway, connection not closed for ???");
				}
			} else
				conn->chargecount = 0;
			ReportConn(conn);
		}
		if(cause != 0)
			conn->cause = cause;
	}
}

int
init_vars (void)
{
	msgbuf = NULL;
	resp = NULL;
	fminor = 0;
	minor = 0;
	callref = 0;
	conn = NULL;
	prot[0] = '*';
	prot[1] = '\0';
	nr[0] = '\0';
	lnr[0] = '\0';
	uid = -1;
	connref = 0;
	dialin = -1;
	charge = 0;
	cause = 0;
	has_force = 0;
	bchan = -1;
	hdrval = -1;
	no_error = 0;

	*(ulong_t *) crd = 0;
	crd[4] = '\0';
	printf ("HL R ");
	dumpascii (data, len);
	printf ("\n");
	data[len] = '\0';
	xx.b_rptr = data;
	xx.b_wptr = data + len;
	xx.b_cont = NULL;
	db.db_base = data;
	db.db_lim = data + len;
	xx.b_datap = &db;
	if (m_getid (&xx, &ind) != 0)
		return 1;
	(char *) data = (char *) xx.b_rptr;
	return 0;
}

int
do_card(void)
{
	short cpos;
	cf dl;
	long nbchan;
	long cardcap;
	int ret;

	if ((ret = m_getstr (&xx, crd, 4)) != 0)
		return ret;
	if ((ret = m_geti (&xx, &nbchan)) != 0)
		return ret;
	if ((ret = m_getx (&xx, &cardcap)) != 0)
		return ret;
	for (cpos = 0; cpos < cardnum; cpos++) {
		if (!strcmp(cardlist[cpos], crd))
			return -EEXIST;
	}
	if (cardnum >= NCARDS)
		return -ENOSPC;
	cardlist[cardnum] = str_enter(crd);
	cardnrbchan[cardnum] = nbchan;
	cardnum++;
	if(cardcap & CHM_INTELLIGENT) {
		struct loader *ld = malloc(sizeof(struct loader));
		if(ld == NULL)
			return -errno;
		bzero(ld,sizeof(*ld));
		ld->card = str_enter(crd);

		card_load(ld);
	} else for(dl = cf_DL; dl != NULL; dl = dl->next) {
		struct iovec io[3];
		int len;

		if(!wildmatch(crd,dl->card))
			continue;
		xx.b_rptr = xx.b_wptr = ans;
		db.db_base = ans;
		db.db_lim = ans + sizeof (ans);
		m_putid (&xx, CMD_DOCARD);
		m_putsx(&xx,ARG_CARD);
		m_putsz(&xx,crd);

		*xx.b_wptr++ = ' ';
		xlen = xx.b_wptr - xx.b_rptr;
		DUMPW (ans, xlen);
		io[0].iov_base = ans;
		io[0].iov_len = xlen;
		len = 1;
		if(dl->args != NULL) {
			printf ("+ ");
			io[len].iov_base = ":: ";
			io[len].iov_len = 3;
			len++;
			io[len].iov_base = dl->args;
			io[len].iov_len  = strlen(dl->args);
			DUMPW (dl->args,io[len].iov_len);
			len++;
		}
		(void) strwritev (xs_mon, io,len, 1);
		break;
	}
	do_run_now++;
	timeout(run_now,NULL,3*HZ);

	return 0;
}

int
do_nocard(void)
{
	short cpos;
	int ret;

	if ((ret = m_getstr(&xx, crd, 4)) != 0)
		return ret;
	for (cpos = 0; cpos < cardnum; cpos++) {
		if (!strcmp(cardlist[cpos], crd)) {
			--cardnum;
			cardlist[cpos] = cardlist[cardnum];
			cardnrbchan[cpos] = cardnrbchan[cardnum];
			if(cardidx >= cardnum)
				cardidx = 0;
			return 0;
		}
	}
	return -ENOENT;
}

int
do_cardproto(void)
{
	if (crd[0] == '\0' || connref == 0 || minor == 0) {
		printf ("\n*** NoProto: Card %p, callref %ld, minor %ld\n", crd, callref, minor);
		return -EINVAL;
	}
	if (conn == NULL) {
		printf ("\n*** Warn NoConnProto: Card %p, callref %ld, minor %ld\n", crd, callref, minor);
	}
	{
		conngrab cg = newgrab(conn ? conn->cg : NULL);
		if(cg == NULL) {
			resp = "OutOfMem";
			return 1;
		}
		cg->card = str_enter(crd);
		cg->protocol = str_enter(prot);
		if ((resp = findsite (&cg)) != NULL) {
			dropgrab(cg);
			if(conn != NULL) {
				conn->want_reconn = 0;
				setconnstate(conn, c_off);
				if(conn->pid == 0) {
					dropconn(conn);
					conn = NULL;
				}
			}

			syslog (LOG_ERR, "ISDN NoProtocol1 %ld %s", minor, data);
			xx.b_rptr = xx.b_wptr = ans;
			db.db_base = ans;
			db.db_lim = ans + sizeof (ans);
printf("Dis10 ");
			m_putid (&xx, CMD_OFF);
			if(minor > 0) {
				m_putsx (&xx, ARG_MINOR);
				m_puti (&xx, minor);
			}
			if(connref != 0) {
				m_putsx (&xx, ARG_CONNREF);
				m_puti (&xx, connref);
			}
			if(crd[0] != '\0') {
				m_putsx(&xx,ARG_CARD);
				m_putsz(&xx,crd);
			}
			if(callref != 0) {
				m_putsx (&xx, ARG_CALLREF);
				m_puti (&xx, callref);
			}

			xlen = xx.b_wptr - xx.b_rptr;
			DUMPW (ans, xlen);
			(void) strwrite (xs_mon, ans, &xlen, 1);

			return 2;
		}
		if (pushcardprot (cg, minor) == 0) {
			dropgrab(cg);
			/* Success */
			return 0;
		} else {
			dropgrab(cg);
			syslog (LOG_ERR, "ISDN NoProtocol2 %ld %s", minor, data);
			xx.b_rptr = xx.b_wptr = ans;
			db.db_base = ans;
			db.db_lim = ans + sizeof (ans);
printf("Dis11 ");
			m_putid (&xx, CMD_OFF);
			if(minor > 0) {
				m_putsx (&xx, ARG_MINOR);
				m_puti (&xx, minor);
			}
			if(connref != 0) {
				m_putsx (&xx, ARG_CONNREF);
				m_puti (&xx, connref);
			}
			if(crd[0] != '\0') {
				m_putsx(&xx,ARG_CARD);
				m_putsz(&xx,crd);
			}
			if(callref != 0) {
				m_putsx (&xx, ARG_CALLREF);
				m_puti (&xx, callref);
			}

			xlen = xx.b_wptr - xx.b_rptr;
			DUMPW (ans, xlen);
			(void) strwrite (xs_mon, ans, &xlen, 1);


			resp = "ERROR";
			return 1;
		}
	}
	return 0;
}

int
do_proto(void)
{
	if (connref == 0 || minor == 0) {
		printf ("\n*** NoProto: Card %p, callref %ld, minor %ld\n", crd, callref, minor);
		return 2;
	}
	if (conn == NULL) {
		printf ("\n*** Warn NoConnProto: Card %p, callref %ld, minor %ld\n", crd, callref, minor);
	}
	{
		conngrab cg = newgrab(conn ? conn->cg : NULL);
		if(cg == NULL) {
			resp = "NoMemErrErr";
			return 1;
		}
		cg->protocol = str_enter(prot);
		if(cg->par_in != NULL)
			freemsg(cg->par_in);
		cg->par_in = copymsg(&xx);

		if (crd[0] != '\0')
			cg->card = str_enter(crd);

		if ((resp = findsite (&cg)) != NULL) {
			dropgrab(cg);
			syslog (LOG_ERR, "ISDN NoProtocol3 %ld %s", minor, data);

			xx.b_rptr = xx.b_wptr = ans;
			db.db_base = ans;
			db.db_lim = ans + sizeof (ans);
			m_putid (&xx, CMD_CLOSE);
			m_putsx (&xx, ARG_MINOR);
			m_puti (&xx, minor);
			if(conn->minor == minor) {
				conn->minor = 0;
				if(conn->pid == 0)
					dropconn(conn);
				else
					kill(conn->pid,SIGHUP);
			}

			xlen = xx.b_wptr - xx.b_rptr;
			DUMPW (ans, xlen);
			(void) strwrite (xs_mon, ans, &xlen, 1);
			return 2;
		}
		if (pushprot (cg, minor, ind == IND_PROTO_AGAIN) == 0) {
			/* Success */
			dropgrab(cg);
			return 0;
		} else {
			dropgrab(cg);
			syslog (LOG_ERR, "ISDN NoProtocol4 %ld %s", minor, data);

			xx.b_rptr = xx.b_wptr = ans;
			db.db_base = ans;
			db.db_lim = ans + sizeof (ans);
			m_putid (&xx, CMD_CLOSE);
			m_putsx (&xx, ARG_MINOR);
			m_puti (&xx, minor);

			xlen = xx.b_wptr - xx.b_rptr;
			DUMPW (ans, xlen);
			(void) strwrite (xs_mon, ans, &xlen, 1);

			resp = "ERROR";
			return 1;
		}
	}
	return 0;
}

int
do_incoming(void)
{
	cf cfr;
	mblk_t *cinf;
	conngrab cg = newgrab(NULL);
	if(cg == NULL) {
		resp = "OutOfMemFoo";
		goto inc_err;
	}
	cg->flags = F_INCOMING|F_DIALUP|F_PERMANENT|F_NRCOMPLETE|F_LNRCOMPLETE;
	cinf = allocb(len,BPRI_LO);
	if(cinf == NULL) {
		resp = "OutOfMemFoo";
		goto inc_err;
	}

	syslog (LOG_INFO, "ISDN In %ld %s", minor, data);
	bcopy (data, cinf->b_wptr, len);
	cinf->b_wptr += len;
	cg->par_in = cinf;
	cg->card = str_enter(crd);
	if ((resp = findit (&cg)) != NULL) 
		goto inc_err;
	if (quitnow) {
		resp = "SHUTTING DOWN";
		goto inc_err;
	}
	if (in_boot) {
		resp = "STARTING UP";
		goto inc_err;
	}
	{
		char *sit = NULL,*pro = NULL,*car = NULL,*cla = NULL; /* GCC */
printf("Hunt for %s/%s/%s/%s/%o\n",cg->site,cg->protocol,cg->card,cg->cclass,cg->flags);
		for (cfr = cf_R; cfr != NULL; cfr = cfr->next) {
			if(cfr->got_err) continue;
			if (!matchflag(cg->flags,cfr->type)) continue;
			if ((sit = wildmatch (cg->site, cfr->site)) == NULL) continue;
			if ((pro = wildmatch (cg->protocol, cfr->protocol)) == NULL) continue;
			if ((car = wildmatch (cg->card, cfr->card)) == NULL) continue;
			if ((cla =classmatch (cg->cclass, cfr->cclass)) == NULL) continue;
			break;
		}
		if (cfr == NULL) {
			resp = "NO PROGRAM";
			goto inc_err;
		}
		cg->site = sit; cg->protocol = pro; cg->cclass = cla; cg->card = car;
	}
	if(((conn = startconn(cg,fminor,connref,&resp)) != NULL) && (resp != NULL)) {
		mblk_t *mz;
		if(conn->state == c_forceoff) {
			goto cont;
		} else if ((conn->connref == connref || conn->connref == 0))  {
			if(*resp != '=')
				goto cont;
		}

		printf("\n*** ConnRef Clash! old is %ld, new %ld\n",conn->connref,connref);

		mz = allocb(40,BPRI_HI); if(mz == NULL) goto cont;

		if(*resp != '+') {
printf("Dis1 ");
			m_putid (mz, CMD_OFF);
			m_putsx (mz, ARG_NODISC);
			m_putsx (mz, ARG_FORCE);
			m_putsx (mz, ARG_CONNREF);
			m_puti (mz, connref);
			m_putsx (mz, ARG_CAUSE);
			m_putsx2(mz, ID_N1_CallRejected);
			if(crd[0] != '\0') {
				m_putsx(mz,ARG_CARD);
				m_putsz(mz,crd);
			}
			if(connref != 0) {
				m_putsx (mz, ARG_CONNREF);
				m_puti (mz, connref);
			}
			if(callref != 0) {
				m_putsx (mz, ARG_CALLREF);
				m_puti (mz, callref);
			}
			if(cg != NULL)
				syslog (LOG_WARNING, "DropIn '%s' for %s/%s/%s", cg->site, cg->protocol, cg->cclass, nr);
			else
				syslog (LOG_WARNING, "DropIn '??' for ???/%s", nr);
			xlen = mz->b_wptr - mz->b_rptr;
			DUMPW (mz->b_rptr, xlen);
			(void) strwrite (xs_mon, mz->b_rptr, &xlen, 1);
			freeb(mz);

			if(*resp == '=') {
				setconnref(conn,connref);
				conn->want_reconn = MAX_RECONN; /* Wait for confirmation before doing anything */
				conn->want_fast_reconn = 1;
			}

			conn = malloc(sizeof(*conn));
			if(conn != NULL) {
				bzero(conn,sizeof(*conn));
				conn->seqnum = ++connseq;
				conn->cause = ID_priv_Print;
				conn->causeInfo = "Drop Incoming";
				cg->refs++;
				/* dropgrab(conn->cg; ** is new anyway */
				conn->cg = cg;
				conn->next = theconn;
				theconn = conn;
				dropconn(conn);
			}
			resp = NULL;
		} else {
#if 0
		  dropother:
#endif
printf("Dis2 ");
			m_putid (mz, CMD_OFF);
			m_putsx (mz, ARG_NODISC);
			m_putsx (mz, ID_N0_cause);
			m_putsx2(mz, ID_N1_CallRejected);
			if(conn->cg != NULL && conn->cg->card != NULL && conn->cg->card[0] != '\0') {
				m_putsx(mz,ARG_CARD);
				m_putsz(mz,conn->cg->card);
			}
			if(conn->connref != 0) {
				m_putsx (mz, ARG_CONNREF);
				m_puti (mz, conn->connref);
			}
			setconnstate(conn,c_down);
			setconnref(conn,connref);
			if(cg != NULL)
				syslog (LOG_WARNING, "DropOut '%s' for %s/%s/%s", cg->site, cg->protocol, cg->cclass, nr);
			else
				syslog (LOG_WARNING, "DropOut '??' for ??/??/%s", nr);
			xlen = mz->b_wptr - mz->b_rptr;
			DUMPW (mz->b_rptr, xlen);
			(void) strwrite (xs_mon, mz->b_rptr, &xlen, 1);
			freeb(mz);
			resp = NULL;
			dropgrab(conn->cg); cg->refs++; conn->cg = cg;
			ReportConn(conn);
#if 1
			/* cg->flags &=~ F_INCOMING; */
			/* cg->flags |= F_OUTGOING; */
			if((conn = startconn(cg,fminor,connref,NULL)) != conn)
				resp = "ClashRestart Failed";
#endif
			conn = malloc(sizeof(*conn));
			if(conn != NULL) {
				bzero(conn,sizeof(*conn));
				conn->seqnum = ++connseq;
				conn->cause = ID_priv_Print;
				conn->causeInfo = "Drop Outgoing";
				cg->refs++;
				/* dropgrab(conn->cg; ** is new anyway */
				conn->cg = cg;
				conn->next = theconn;
				theconn = conn;
				dropconn(conn);
			}
		}
		goto cont;
	} else if(conn != NULL)
		goto cont;
	conn = (struct conninfo *)malloc (sizeof (struct conninfo));

	if (conn == NULL) {
		resp = "NO MEMORY.5";
		goto inc_err;
	}
	bzero (conn, sizeof (struct conninfo));
	conn->seqnum = ++connseq;

	cg->refs++;
	/* dropgrab(conn->cg; ** is new anyway */
	conn->cg = cg;
	conn->flags = cg->flags;
	setconnref(conn,connref);
	conn->cause = 999999;
	setconnstate(conn, c_down);
	ReportConn(conn);
	resp = runprog (cfr, &conn, &cg);
	chkone(cg); chkone(conn);
  cont:
#if 0
	if (conn != NULL) {
		conn->cg->nr = str_enter(nr);
	}
#endif
  inc_err:
	if (resp != NULL) {
		xx.b_wptr = xx.b_rptr = ans;
		xx.b_datap = &db;
		db.db_base = ans;
		db.db_lim = ans + sizeof (ans);

printf("Dis3 ");
		m_putid (&xx, CMD_OFF);
		if(connref != 0) {
			m_putsx (&xx, ARG_CONNREF);
			m_puti (&xx, connref);
		}

		/* BUSY-if-no-channel is very ugly but unavoidable when
			sharing the bus with brain-damaged devices (there are
			many out there) */
		m_putsx (&xx, ARG_CAUSE);
		if((bchan < 0) || !strcmp(resp,"0BUSY")) {
			m_putsx2 (&xx, ID_N1_UserBusy);

			if(conn != NULL && (conn->flags & F_BACKCALL)) {
				if(conn->want_reconn == 0)
					conn->want_reconn = MAX_RECONN - (MAX_RECONN >> 1);
				setconnstate(conn,conn->state);
			}
		}
		else if(cg->flags & F_NOREJECT)
				m_putsx2 (&xx, ID_N1_NoChans);
			else
				m_putsx2 (&xx, ID_N1_CallRejected);
			if(cg->flags & F_FASTDROP)
				m_putsx(&xx,ARG_FASTDROP);
		if(crd[0] != '\0') {
			m_putsx(&xx,ARG_CARD);
			m_putsz(&xx,crd);
		}
		if(callref != 0) {
			m_putsx (&xx, ARG_CALLREF);
			m_puti (&xx, callref);
		}

		if(cg != NULL) {
			syslog (LOG_WARNING, "Got '%s' for %s/%s/%s/%s,%s", resp, cg->site, cg->protocol, cg->card, cg->cclass, nr);
		} else
			syslog (LOG_WARNING, "Got '%s' for ???,%s", resp, nr);
		xlen = xx.b_wptr - xx.b_rptr;
		DUMPW (ans, xlen);
		(void) strwrite (xs_mon, ans, &xlen, 1);

		conn = malloc(sizeof(*conn));
		if(conn != NULL) {
			bzero(conn,sizeof(*conn));
			conn->seqnum = ++connseq;
			conn->cause = ID_priv_Print;
			conn->causeInfo = resp;
			cg->refs++;
			/* dropgrab(conn->cg; ** is new anyway */
			conn->cg = cg;
			conn->next = theconn;
			theconn = conn;
			run_rp(conn,'r');
			dropconn(conn);
		}
	}
	dropgrab(cg);
	return 0;
}

int
do_conn(void)
{
	if (conn != NULL)
		setconnstate(conn, c_going_up);
	if(conn == NULL || (conn->flags & F_OUTGOING)) {
		syslog (LOG_INFO, "ISDN Out %ld %s", minor, data);

		if(1 /* conn == NULL ** || !(conn->dialin & 2) */ ) {
			resp = "CARRIER";

			xx.b_rptr = xx.b_wptr = ans;
			db.db_base = ans;
			db.db_lim = ans + sizeof (ans);
			m_putid (&xx, CMD_PROT);
			m_putsx (&xx, ARG_FMINOR);
			m_puti (&xx, minor);
			m_putdelim (&xx);
			m_putid (&xx, PROTO_AT);
			m_putsz (&xx, (uchar_t *) resp);
			xlen = xx.b_wptr - xx.b_rptr;
			DUMPW (ans, xlen);
			(void) strwrite (xs_mon, ans, &xlen, 1);
		}
	}
#if 0 /* not yet */
	xx.b_rptr = xx.b_wptr = ans;
	db.db_base = ans;
	db.db_lim = ans + sizeof (ans);
	m_putid (&xx, CMD_PROT);
	m_putsx (&xx, ARG_FMINOR);
	m_puti (&xx, minor);
	m_putdelim (&xx);
	m_putid (&xx, PROTO_MODULE);
	m_putsx (&xx, PROTO_MODULE);
	m_putsz (&xx, (uchar_t *) "proto");
	printf("On1\n");
	m_putsx (&xx, PROTO_ONLINE);
	xlen = xx.b_wptr - xx.b_rptr;
	DUMPW (xx.b_rptr, xlen);
	(void) strwrite (xs_mon, (uchar_t *) xx.b_rptr, &xlen, 1);
#endif
	return 0;
}

int
do_hasenable(void)
{
	if(conn != NULL) {
		if(conn->state == c_off)
			setconnstate(conn, c_down);
		else if(conn->state == c_offdown)
			setconnstate(conn, c_going_down);
	}
#if 0
	if(do_run_now) {
		untimeout(run_now,NULL);
		run_now(NULL);
	}
#endif
	return 0;
}

int
do_hasdisable(void)
{
	if(conn != NULL) {
		if(conn->state == c_down)
			setconnstate(conn, c_off);
		else if(conn->state == c_going_down)
			setconnstate(conn, c_offdown);
	}
	return 0;
}

int
do_disc(void)
{
	if (conn != NULL) {
		conn->got_id = 1;
			conn->cg->oldnr = conn->cg->nr;
			conn->cg->oldlnr = conn->cg->lnr;
		if(!conn->want_fast_reconn)
			conn->cg->nr = NULL;
		conn->cg->lnr = NULL;
#if 0
		after_ind:
#endif
		conn->fminor = 0;
		if(conn->got_hd) { /* really down */
			switch(conn->state) {
			case c_offdown:
			case c_off:
			  conn_off:
				{
					mblk_t *mb = allocb(30,BPRI_MED);

					if(conn->state >= c_down) {
						if(conn->cg != NULL)
							syslog(LOG_ERR,"OFF %s:%s %s",conn->cg->site,conn->cg->protocol,CauseInfo(conn->cause, conn->causeInfo));
						else
							syslog(LOG_ERR,"OFF ??? %s",CauseInfo(conn->cause, conn->causeInfo));
					}
					setconnstate(conn,c_off);
					if(mb != NULL) {
						m_putid (mb, CMD_PROT);
						m_putsx (mb, ARG_MINOR);
						m_puti (mb, minor);
						m_putdelim (mb);
						m_putid (mb, PROTO_DISABLE);
						xlen = mb->b_wptr - mb->b_rptr;
						DUMPW (mb->b_rptr, xlen);
						(void) strwrite (xs_mon, (uchar_t *) mb->b_rptr, &xlen, 1);
						freemsg(mb);
					}
				}
				break;
			case c_going_up:
				if(conn->charge > 0 || (++conn->retries > cardnum*2 && !(conn->flags & F_FASTREDIAL))
							|| (conn->retries > cardnum*10 && (conn->flags & F_FASTREDIAL)))
					goto conn_off;
				/* else FALL THRU */
			case c_up:
			case c_going_down:
#if 0
				if(conn->got_id == 1)
					setconnstate(conn,c_going_down);
				else
#endif
				hitme:
				if(has_force) {
					mblk_t *mb = allocb(30,BPRI_MED);

					if(mb != NULL) {
						m_putid (mb, CMD_PROT);
						m_putsx (mb, ARG_MINOR);
						m_puti (mb, minor);
						m_putdelim (mb);
						m_putid (mb, PROTO_DISABLE);
						xlen = mb->b_wptr - mb->b_rptr;
						DUMPW (mb->b_rptr, xlen);
						(void) strwrite (xs_mon, (uchar_t *) mb->b_rptr, &xlen, 1);
						freemsg(mb);
					}
					setconnstate(conn,c_off);
				} else 
					setconnstate(conn,c_down);
				if((conn->flags & F_PERMANENT) && (conn->minor != 0 || minor != 0 || fminor != 0)) {
					mblk_t *mb = allocb(30,BPRI_MED);

					if(mb != NULL) {
						m_putid (mb, CMD_PROT);
						m_putsx (mb, ARG_MINOR);
						m_puti (mb, conn->minor ? conn->minor : minor ? minor : fminor);
						m_putdelim (mb);
						m_putid (mb, PROTO_ENABLE);
						xlen = mb->b_wptr - mb->b_rptr;
						DUMPW (mb->b_rptr, xlen);
						(void) strwrite (xs_mon, (uchar_t *) mb->b_rptr, &xlen, 1);
						freemsg(mb);
					}
				}
				break;
			default:;
			}
		} else  { /* not wholly there yet */
			switch(conn->state) {
			case c_going_up:
				if((conn->flags & F_INCOMING) && !(conn->flags & F_PERMANENT)) {
					xx.b_rptr = xx.b_wptr = ans;
					db.db_base = ans;
					db.db_lim = ans + sizeof (ans);
printf("Dis4d ");
					m_putid (&xx, CMD_CLOSE);
					m_putsx (&xx, ARG_MINOR);
					m_puti (&xx, minor);
					m_putsx (&xx, ARG_NOCONN);
					xlen = xx.b_wptr - xx.b_rptr;
					DUMPW (xx.b_rptr, xlen);
					(void) strwrite (xs_mon, (uchar_t *) xx.b_rptr, &xlen, 1);
					if(conn->minor == minor) {
						conn->minor = 0;
						if(conn->pid == 0)
							dropconn(conn);
						else
							kill(conn->pid,SIGHUP);
					}

				}					
				if(conn->charge > 0 || ++conn->retries > 3) 
					goto conn_off;
				else
					goto hitme;
				break;
			case c_up:
				setconnstate(conn,c_going_down);
				break;
			case c_down:
				setconnstate(conn,c_down);
				break;
			default:;
			}
			if(conn->cg != NULL)
				syslog(LOG_INFO,"DOWN %s:%s",conn->cg->site,conn->cg->protocol);
			else
				syslog(LOG_INFO,"DOWN ???");
		} 
		if (conn->pid == 0)
			dropconn (conn);
		else {
			if(fminor != 0 && minor != fminor && minor != 0) {
				syslog(LOG_ERR,"fMinor does not match -- closing (%ld/%ld)",minor,fminor);
				xx.b_rptr = xx.b_wptr = ans;
				db.db_base = ans;
				db.db_lim = ans + sizeof (ans);
				m_putid (&xx, CMD_CLOSE);
				m_putsx (&xx, ARG_MINOR);
				m_puti (&xx, minor);
				if(conn->flags & F_PERMANENT)
					m_putsx (&xx, ARG_NOCONN);
				xlen = xx.b_wptr - xx.b_rptr;
				DUMPW (xx.b_rptr, xlen);
				(void) strwrite (xs_mon, (uchar_t *) xx.b_rptr, &xlen, 1);
				if(conn->minor == minor) {
					conn->minor = 0;
					if(conn->pid == 0)
						dropconn(conn);
					else
						kill(conn->pid,SIGHUP);
				}

			}
		}
	} else {
		if(charge > 0)
			syslog(LOG_WARNING,"COST ??:?? %ld",charge);
	}
#if 0
	if(minor > 0) {
		xx.b_rptr = xx.b_wptr = ans;
		db.db_base = ans;
		db.db_lim = ans + sizeof (ans);
printf("Dis4 ");
		m_putid (&xx, CMD_OFF);
		m_putsx (&xx, ARG_MINOR);
		m_puti (&xx, minor);
		m_putsx (&xx, ARG_NOCONN);
		xlen = xx.b_wptr - xx.b_rptr;
		DUMPW (xx.b_rptr, xlen);
		(void) strwrite (xs_mon, (uchar_t *) xx.b_rptr, &xlen, 1);
	}
#endif
#if 1
	if(conn != NULL && conn->state <= c_going_down && ind == IND_DISC) {
		resp = "NO CARRIER";
		return 1;
	}
#endif
	return 0;
}

int
do_close(void)
{
	if (minor == 0)
		return 2;
	{
		if (conn != NULL) {
			conn->minor = 0;
			if(conn->fminor == minor)
				conn->fminor = 0;
			if (conn->pid == 0)
				dropconn (conn);
			else {
				printf("PID still %d\n",conn->pid);
				kill(conn->pid,SIGHUP);
			}
		}
		{
			for(conn = theconn; conn != NULL; conn = conn->next) {
				if(conn->minor == minor && conn->ignore == 3) {
					dropconn(conn);
					continue;
				}
				if(conn->minor == minor && conn->ignore == 0) {
					conn->minor = 0;
					if(conn->fminor == minor)
						conn->fminor = 0;
					if (conn->pid == 0)
						dropconn (conn);
					else {
						printf("PID still %d\n",conn->pid);
						kill(conn->pid,SIGHUP);
					}
				}
			}
		}
	}
	unlockdev(minor);
	{
		struct utmp ut;

		bzero (&ut, sizeof (ut));
		strncpy (ut.ut_id, sdevname (minor), sizeof (ut.ut_id));
		strncpy (ut.ut_line, mdevname (minor), sizeof (ut.ut_line));
		ut.ut_pid = getpid ();
		ut.ut_type = DEAD_PROCESS;
		ut.ut_time = time(NULL);
		if (getutline (&ut) != 0) {
			int wf = open ("/etc/wtmp", O_WRONLY | O_APPEND);

			if (wf >= 0) {
				(void) write (wf, &ut, sizeof (ut));
				close (wf);
			}
			pututline (&ut);
		}
		endutent ();
	}
	unlink (devname (minor));
	chmod (idevname (minor), S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
	chown (idevname (minor), 0,0);
	return 0;
}

int
do_open(void)
{
	if (minor == 0)
		return 2;

	user[fminor] = uid;
	chmod (idevname (minor), S_IRUSR | S_IWUSR);
	chown (idevname (minor), uid,-1);

	if(conn != NULL)
		return 0;

	xx.b_rptr = xx.b_wptr = ans;
	db.db_base = ans;
	db.db_lim = ans + sizeof (ans);
	m_putid (&xx, CMD_PROT);
	m_putsx (&xx, ARG_FMINOR);
	m_puti (&xx, minor);
	m_putdelim (&xx);
	m_putid (&xx, PROTO_MODULE);
	m_putsx (&xx, PROTO_MODULE);
	m_putsz (&xx, (uchar_t *) "proto");
	m_putsx (&xx, PROTO_CR);
	m_puti (&xx, 13);
	m_putsx (&xx, PROTO_LF);
	m_puti (&xx, 10);
	m_putsx (&xx, PROTO_BACKSPACE);
	m_puti (&xx, 8);
	m_putsx (&xx, PROTO_ABORT);
	m_puti (&xx, 3);
	m_putsx (&xx, PROTO_CARRIER);
	m_puti (&xx, 1);
	m_putsx (&xx, PROTO_BREAK);
	m_puti (&xx, 1);
	printf("On2 %p %d\n",conn,dialin);
	m_putsx (&xx, ((conn != NULL) || (dialin > 0)) ? PROTO_ONLINE : PROTO_OFFLINE);

	len = xx.b_wptr - xx.b_rptr;
	DUMPW (xx.b_rptr, len);
	(void) strwrite (xs_mon, (uchar_t *) xx.b_rptr, &len, 1);
	return 0;
}

int
do_hasconnected(void)
{
	if (conn != NULL) {
		if (conn->cg != NULL)
			conn->flags |= conn->cg->flags & F_MOVEFLAGS;
		setconnstate(conn,c_up);
	}
#if 1
	resp = "CONNECT";
#endif

	if(conn != NULL && conn->cg != NULL)
		syslog(LOG_INFO,"UP %s:%s",conn->cg->site,conn->cg->protocol);
	else
		syslog(LOG_INFO,"UP ??:??");

	if(resp != NULL) {
		xx.b_rptr = xx.b_wptr = ans;
		db.db_base = ans;
		db.db_lim = ans + sizeof (ans);
		m_putid (&xx, CMD_PROT);
		m_putsx (&xx, ARG_FMINOR);
		m_puti (&xx, fminor);
		m_putdelim (&xx);
		m_putid (&xx, PROTO_AT);
		*xx.b_wptr++ = ' ';
		m_putsz (&xx, (uchar_t *) resp);
		xlen = xx.b_wptr - xx.b_rptr;
		DUMPW (ans, xlen);
		(void) strwrite (xs_mon, ans, &xlen, 1);
	}

	xx.b_rptr = xx.b_wptr = ans;
	db.db_base = ans;
	db.db_lim = ans + sizeof (ans);
	m_putid (&xx, CMD_PROT);
	m_putsx (&xx, ARG_MINOR);
	m_puti (&xx, minor);
	m_putdelim (&xx);
	m_putid (&xx, PROTO_MODULE);
	m_putsx (&xx, PROTO_MODULE);
	m_putsz (&xx, (uchar_t *) "proto");
	printf("On3\n");
	m_putsx (&xx, PROTO_ONLINE);
	xlen = xx.b_wptr - xx.b_rptr;
	DUMPW (xx.b_rptr, xlen);
	(void) strwrite (xs_mon, (uchar_t *) xx.b_rptr, &xlen, 1);
	return 0;
}

int
do_disconnect(void)
{
	if (conn != NULL)
		if(conn->state > c_down) {
			if(conn->state == c_going_up && conn->charge > 0)
				setconnstate(conn, c_offdown);
			else
				setconnstate(conn, c_going_down);
		}

#if 0
	xx.b_rptr = xx.b_wptr = ans;
	db.db_base = ans;
	db.db_lim = ans + sizeof (ans);
printf("Dis5 ");
	m_putid (&xx, CMD_OFF);
	m_putsx (&xx, ARG_MINOR);
	m_puti (&xx, minor);
	xlen = xx.b_wptr - xx.b_rptr;
	DUMPW (xx.b_rptr, xlen);
	(void) strwrite (xs_mon, (uchar_t *) xx.b_rptr, &xlen, 1);
#endif
	resp = NULL;
	return 0;
}
	
int
do_hasdisconnect(void)
{
	if (minor == fminor) {
		xx.b_rptr = xx.b_wptr = ans;
		db.db_base = ans;
		db.db_lim = ans + sizeof (ans);
		m_putid (&xx, CMD_PROT);
		m_putsx (&xx, ARG_MINOR);
		m_puti (&xx, minor);
		m_putdelim (&xx);
		m_putid (&xx, PROTO_MODULE);
		m_putsx (&xx, PROTO_MODULE);
		m_putsz (&xx, (uchar_t *) "proto");
		printf("On4 %p %d\n",conn,dialin);
		m_putsx (&xx, ((conn != NULL) || (dialin > 0)) ?  PROTO_ONLINE : PROTO_OFFLINE);
		xlen = xx.b_wptr - xx.b_rptr;
		DUMPW (xx.b_rptr, xlen);
		(void) strwrite (xs_mon, (uchar_t *) xx.b_rptr, &xlen, 1);
	}
#if 1
	if(minor > 0) {
		xx.b_rptr = xx.b_wptr = ans;
		db.db_base = ans;
		db.db_lim = ans + sizeof (ans);
printf("Dis6 ");
		m_putid (&xx, CMD_OFF);
		m_putsx (&xx, ARG_MINOR);
		m_puti (&xx, minor);
		if((conn != NULL) && (conn->flags & F_PERMANENT))
			m_putsx (&xx, ARG_NOCONN);
		xlen = xx.b_wptr - xx.b_rptr;
		DUMPW (xx.b_rptr, xlen);
		(void) strwrite (xs_mon, (uchar_t *) xx.b_rptr, &xlen, 1);
	}
#endif
	if(conn != NULL) {
		conn->got_hd = 1;
		if(conn->got_id) { /* really down */
			switch(conn->state) {
			case c_offdown:
				setconnstate(conn,c_off);
				break;
			case c_up:
			case c_going_up:
			case c_going_down:
#if 0
				if(conn->got_id == 1)
					setconnstate(conn,c_going_down);
				else
#endif
					setconnstate(conn,c_down);
				break;
			default:;
			}
		} else  { /* not wholly down yet */
			syslog(LOG_INFO,"DOWN %s:%s",conn->cg->site,conn->cg->protocol);

			switch(conn->state) {
			case c_up:
			case c_going_up:
				setconnstate(conn,c_going_down);
				break;
			default:;
			}
		}
	}
#if 0
	resp = "NO CARRIER";
	return 1;
#endif
	return 0;
}

int
do_wantconnect(void)
{
	if (!quitnow && conn != NULL && (conn->state > c_off || (conn->state == c_off && has_force))) {
		if ((conn->state == c_off) && (conn->minor != 0)) {
			mblk_t *mb = allocb(30,BPRI_MED);

			if(mb != NULL) {
				m_putid (mb, CMD_PROT);
				m_putsx (mb, ARG_MINOR);
				m_puti (mb, conn->minor);
				m_putdelim (mb);
				m_putid (mb, PROTO_ENABLE);
				xlen = mb->b_wptr - mb->b_rptr;
				DUMPW (mb->b_rptr, xlen);
				(void) strwrite (xs_mon, (uchar_t *) mb->b_rptr, &xlen, 1);
				freemsg(mb);
			}
		}
		if(conn->state < c_going_up) {
			setconnref(conn,0);
			try_reconn(conn);
		}
	} else {
		if(minor > 0) {
			xx.b_rptr = xx.b_wptr = ans;
			db.db_base = ans;
			db.db_lim = ans + sizeof (ans);
printf("Dis7 ");
			m_putid (&xx, CMD_OFF);
			m_putsx (&xx, ARG_MINOR);
			m_puti (&xx, minor);
			m_putsx (&xx, ARG_NOCONN);
			xlen = xx.b_wptr - xx.b_rptr;
			DUMPW (xx.b_rptr, xlen);
			(void) strwrite (xs_mon, (uchar_t *) xx.b_rptr, &xlen, 1);
		}
	}
	return 0;
}

int
do_atcmd(void)
{
	for (;;) {
		while (xx.b_rptr < xx.b_wptr && *xx.b_rptr != 'a' && *xx.b_rptr != 'A')
			xx.b_rptr++;
		if (xx.b_rptr == xx.b_wptr)
			return 0;
		while (xx.b_rptr < xx.b_wptr && (*xx.b_rptr == 'a' || *xx.b_rptr == 'A'))
			xx.b_rptr++;
		if (xx.b_rptr < xx.b_wptr && (*xx.b_rptr == 't' || *xx.b_rptr == 'T'))
			break;
	}
	{
	for(conn = theconn; conn != NULL; conn = conn->next) {
		if(conn->minor == minor && conn->ignore == 3) {
			dropconn(conn);
			break;
		}
	}
	}
	/* AT recognized */
	xx.b_rptr++;
	resp = "OK";
	while (1) {
		int dodrop;
	at_again:
		dodrop = 0;
		m_getskip (&xx);
		if (xx.b_rptr >= xx.b_wptr)
			return 1;
		printf ("AT %c\n", *xx.b_rptr);
		switch (*xx.b_rptr++) {
		case '/':
			m_getskip (&xx);
			switch (*xx.b_rptr++) {
			default:
				return 3;
			case 'k':
			case 'K': /* Kill running programs. Restart in half a minute. */
				if(user[fminor] != 0) {
					resp = "NO PERMISSION";
					return 1;
				}
				/* Kick a connection. */
				if(m_geti(&xx,&minor) != 0) {
					kill_progs(NULL);
				} else {
					for(conn = theconn; conn != NULL; conn = conn->next) {
						if(conn->ignore)
							continue;
						if(conn->minor == minor)
							break;
					}
					if(conn != NULL) {
						kill_progs(conn);
						break;
					}
					resp = "NOT FOUND";
					return 1;
				}
				break;
			case 'r':
			case 'R': /* Reload database. */
				if(user[fminor] != 0) {
					resp = "NO PERMISSION";
					return 1;
				}
				read_args(NULL);
				do_run_now++;
				run_now(NULL);
				break;
			case 'q':
			case 'Q': /* Shutdown. */
				if(user[fminor] != 0) {
					resp = "NO PERMISSION";
					return 1;
				}
				do_quitnow(NULL);
				break;
			case 'b':
			case 'B': { /* Reenable a connection. */
					if(m_geti(&xx,&minor) != 0) {
						return 3;
					}
					for(conn = theconn; conn != NULL; conn = conn->next) {
						if(conn->ignore)
							continue;
						if(conn->minor == minor)
							break;
					}
					if(conn != NULL) {
						if(conn->state == c_off) {
							if ((conn->flags & F_PERMANENT) && (conn->minor != 0)) {
								mblk_t *mb = allocb(30,BPRI_MED);

								setconnstate(conn, c_down);
								m_putid (mb, CMD_PROT);
								m_putsx (mb, ARG_MINOR);
								m_puti (mb, conn->minor);
								m_putdelim (mb);
								m_putid (mb, PROTO_ENABLE);
								xlen = mb->b_wptr - mb->b_rptr;
								DUMPW (mb->b_rptr, xlen);
							(void) strwrite (xs_mon, (uchar_t *) mb->b_rptr, &xlen, 1);
								freeb(mb);
							}
							conn->retries = conn->retiming = 0;
							goto at_again;
						} else {
							resp = "BAD STATE";
							return 1;
						}
					}
					resp = "NOT FOUND";
					return 1;
				} break;
			case 'f':
			case 'F': /* freeze the connection, i.e. state = OFF */
				dodrop = 1;
				/* FALL THRU */
			case 'x':
			case 'X':
				{ /* Drop a connection. */
					if(user[fminor] != 0) {
						resp = "NO PERMISSION";
						return 1;
					}
					if(m_geti(&xx,&minor) != 0) {
						resp = "ERROR";
						return 1;
					}
					for(conn = theconn; conn != NULL; conn = conn->next) {
						if(conn->ignore)
							continue;
						if(conn->minor == minor)
							break;
					}
					if(conn != NULL) {
						if(conn->state >= c_going_up || (dodrop && conn->state == c_down)) {
							mblk_t *mb = allocb(30,BPRI_MED);

							if(dodrop)
								setconnstate(conn, c_forceoff);
							else
								setconnstate(conn, c_down);
printf("Dis8 ");
							if(mb != NULL) {

{
								m_putid (mb, CMD_OFF);
								m_putsx (mb, ARG_MINOR);
								m_puti (mb, minor);
								xlen = mb->b_wptr - mb->b_rptr;
								DUMPW (mb->b_rptr, xlen);
								(void) strwrite (xs_mon, (uchar_t *) mb->b_rptr, &xlen, 1);

								if(dodrop) {
									m_putid (mb, CMD_PROT);
									m_putsx (mb, ARG_MINOR);
									m_puti (mb, minor);
									m_putdelim (mb);
									m_putid (mb, PROTO_DISABLE);
									xlen = mb->b_wptr - mb->b_rptr;
									DUMPW (mb->b_rptr, xlen);
									(void) strwrite (xs_mon, (uchar_t *) mb->b_rptr, &xlen, 1);
									mb->b_wptr=mb->b_rptr;
								}

								freeb(mb);
							}
							goto at_again;
						} else {
							resp = "BAD STATE";
							return 1;
						}
					}
					resp = "NOT FOUND";
					return 1;
					}
				}
				break;
			case 'l':
			case 'L':
				{ /* List connections. */
					struct conninfo *fconn;
					char *sp;
					msgbuf = malloc(10240);
					if(msgbuf == NULL) {
						resp = "NO MEMORY.6";
						return 1;
					}
					sp = resp = msgbuf;
					sp += sprintf(sp,"#:ref id site protocol class pid state/card cost total flags,rem.nr;loc.nr cause\r\n");
					for(fconn = theconn; fconn != NULL; fconn = fconn->next)  {
						if(fconn->ignore < 3) {
							sp += sprintf(sp,"%s%d:%d %s %s %s %d %s/%s %ld %ld %s %s\r\n",
								fconn->ignore?"!":"", fconn->minor, fconn->seqnum,
								(fconn->cg && fconn->cg->site) ? fconn->cg->site : "-",
								(fconn->cg && fconn->cg->protocol) ? fconn->cg->protocol : "-",
								(fconn->cg && fconn->cg->cclass) ? fconn->cg->cclass : "-",
								fconn->pid, state2str(fconn->state),
								(fconn->cg && fconn->cg->card) ? fconn->cg->card : "-",
								fconn->charge, fconn->ccharge, FlagInfo(fconn->flags),
								CauseInfo(fconn->cause, fconn->causeInfo));
						}
					}
					conn = malloc(sizeof(*conn));
					if(conn != NULL) {
						bzero(conn,sizeof(*conn));
						conn->seqnum = ++connseq;
						conn->ignore = 3;
						conn->minor = minor;
						conn->next = theconn;
						theconn = conn;
					}
					sprintf(sp,"OK");

					return 1;
				}
				break;

			case 'i':
			case 'I':
				{ /* List state. */
					char *sp;
#if LEVEL < 4
					extern int l3print(char *);
#endif
					msgbuf = malloc(10240);
					if(msgbuf == NULL) {
						resp = "NO MEMORY.6";
						return 1;
					}
					sp = resp = msgbuf;
					{
						int cd;
						for(cd=0;cd<cardnum;cd++) {
							sp += sprintf(sp,"%s(%d) ",cardlist[cd],cardnrbchan[cd]);
						}
						*sp++ = '\r'; *sp++ = '\n';
					}
#if LEVEL < 4
					sp += l3print(sp);
					*sp++ = '\r'; *sp++ = '\n';
#endif
					sprintf(sp,"OK");

					return 1;
				}
				break;

			case 'm':
			case 'M':
				{
					m_getskip (&xx);
					if (xx.b_rptr == xx.b_wptr)
						return 3;
					xlen = xx.b_wptr - xx.b_rptr;
					DUMPW (xx.b_rptr, xlen);
					(void) strwrite (xs_mon, xx.b_wptr, &xlen, 1);
					return 0;
				}
				break;
#if 0
			case 'f':
			case 'F':
				{	/* Forward call. Untested. */
					mblk_t yy;

					m_getskip (&xx);
					yy.b_rptr = yy.b_wptr = ans;
					db.db_base = ans;
					db.db_lim = ans + sizeof (ans);
					m_putid (&yy, CMD_PROT);
					m_putsx (&yy, ARG_FMINOR);
					m_puti (&yy, minor);
					if (xx.b_rptr < xx.b_wptr && isdigit (*xx.b_rptr)) {
						m_putsx (&yy, ARG_EAZ);
						m_puti (&yy, *xx.b_rptr++);
						if (xx.b_rptr < xx.b_wptr && isdigit (*xx.b_rptr)) {
							m_putsx (&yy, ARG_EAZ2);
							m_puti (&yy, *xx.b_rptr++);
						}
					}
					if (xx.b_rptr < xx.b_wptr && *xx.b_rptr == '.') {
					}
					xlen = yy.b_wptr - yy.b_rptr;
					DUMPW (ans, xlen);
					(void) strwrite (xs_mon, ans, &xlen, 1);
					resp = NULL;
				}
				break;
#endif
			}
			break;
		case 'o':
		case 'O':
			{
				mblk_t yy;

				m_getskip (&xx);
				if (xx.b_rptr < xx.b_wptr) {
					resp = "ERROR";
					return 1;
				}
				resp = "CONNECT";

				yy.b_rptr = yy.b_wptr = ans;
				db.db_base = ans;
				db.db_lim = ans + sizeof (ans);
				m_putid (&yy, CMD_PROT);
				m_putsx (&yy, ARG_FMINOR);
				m_puti (&yy, minor);
				m_putdelim (&yy);
				m_putid (&yy, PROTO_AT);
				*yy.b_wptr++ = ' ';
				m_putsz (&yy, (uchar_t *) resp);
				xlen = yy.b_wptr - yy.b_rptr;
				DUMPW (ans, xlen);
				(void) strwrite (xs_mon, ans, &xlen, 1);

				yy.b_rptr = yy.b_wptr = ans;
				db.db_base = ans;
				db.db_lim = ans + sizeof (ans);
				m_putid (&yy, CMD_PROT);
				m_putsx (&yy, ARG_MINOR);
				m_puti (&yy, minor);
				m_putdelim (&yy);
				m_putid (&yy, PROTO_MODULE);
				m_putsx (&yy, PROTO_MODULE);
				m_putsz (&yy, (uchar_t *) "proto");
				printf("On5\n");
				m_putsx (&yy, PROTO_ONLINE);
				xlen = yy.b_wptr - yy.b_rptr;
				DUMPW (yy.b_rptr, xlen);
				(void) strwrite (xs_mon, (uchar_t *) yy.b_rptr, &xlen, 1);
				resp = NULL;
			}
			return 0;
		case 'H':
		case 'h':
			{
				mblk_t yy;
				struct datab dbb;

				yy.b_datap = &dbb;
				yy.b_rptr = yy.b_wptr = ans;
				dbb.db_base = ans;
				dbb.db_lim = ans + sizeof (ans);
printf("Dis9 ");
				m_putid (&yy, CMD_OFF);
				m_putsx (&yy, ARG_MINOR);
				m_puti (&yy, minor);
				xlen = yy.b_wptr - yy.b_rptr;
				DUMPW (yy.b_rptr, xlen);
				(void) strwrite (xs_mon, (uchar_t *) yy.b_rptr, &xlen, 1);
				resp = NULL;
			}
			break;
		case 'D':
		case 'd':
			{
				/* cf cfr; */
				streamchar *m1, *m2, *m3;
				/* char *cclass = NULL; */
				mblk_t *md = allocb (256, BPRI_MED);
				conngrab cg = newgrab(NULL);

				if (md == NULL || cg == NULL) {
					if(md != NULL)
						freeb(md);
					if(cg != NULL)
						dropgrab(cg);
					resp = "NO MEMORY.7";
					return 1;
				}
				if(quitnow) {
					freeb(md);
					dropgrab(cg);
					resp = "SHUTTING DOWN";
					return 1;
				}
				if(in_boot) {
					freemsg(md);
					dropgrab(cg);
					resp = "STARTING UP";
					return 1;
				}
				if(fminor != 0) {
					m_putid (md, CMD_NOPROT);
					m_putsx (md, ARG_FMINOR);
					m_puti (md, fminor);
					xlen=md->b_wptr-md->b_rptr;
					DUMPW (md->b_rptr, xlen);
					(void) strwrite (xs_mon, md->b_rptr, &xlen, 1);
					md->b_wptr=md->b_rptr;
				}

				m_getskip (&xx);
				if (xx.b_rptr == xx.b_wptr) {
					freemsg (md);
					return 3;
				}
				m1 = m3 = m2 = xx.b_rptr;
				if (*m1 == '/') {
					m2++;
					m3++;
				}
				while (m2 < xx.b_wptr && *m2 != '/' && *m2 != ';') {
					if (*m1 == '/') {		/* site name */
						if (isspace (*m2))
							break;
						*m3++ = *m2++;
					} else {		/* site number */
						if (isdigit (*m2))
							*m3++ = *m2;
						m2++;
					}
				}
				if (m2 < xx.b_wptr && *m2 == '/') {
					*m3 = '\0';
					m_getskip (&xx);
					m3 = ++m2;
					while (m3 < xx.b_wptr && !isspace (*m3) && *m3 != '/' && *m3 != ';')
						m3++;
					/* if (m3 < xx.b_rptr) */
					if (m3 == m2)
						m2 = NULL;
					if (*m3 == '/') {
						*m3++ = '\0';
						m_getskip(&xx);
						xx.b_rptr = m3+4;
					} else {
						*m3++ = '\0';	/* Probably unsafe */
						xx.b_rptr = m3;
						m3 = NULL;
					}
				} else {
					*m2++ = '\0';
					xx.b_rptr = m2;
					m2 = NULL;
					m3 = NULL;
				}
				if(conn == NULL) {
					conn = malloc(sizeof(*conn));
					if(conn != NULL) {
						bzero(conn,sizeof(*conn));
						conn->seqnum = ++connseq;
						conn->fminor = fminor;
						conn->minor = minor;
						conn->next = theconn; theconn = conn;
					}
				}

				if(conn != NULL) {
					setconnref(conn,connrefs);
					connrefs += 2;
					cg->refs++;
					dropgrab(conn->cg);
					conn->cg = cg;
				}

				cg->flags = F_OUTGOING|F_DIALUP;
				if(m3 != NULL)
					cg->card = str_enter(m3);
				cg->protocol = str_enter(m2 ? m2 : (streamchar *)"login");

				if (*m1 == '/') {
					cg->site = str_enter(m1+1);
				} else {
					cg->nr = str_enter(m1);
					cg->flags |= F_NRCOMPLETE;
				}
				resp = findit (&cg);
				if (resp != NULL) {
					freeb (md);
					dropgrab(cg);
					return 1;
				}
				cg->refs++;
				dropgrab(conn->cg);
				conn->cg = cg;
				setconnstate(conn,c_down);
				if((conn = startconn(cg,fminor,0,NULL)) != NULL) {
					freeb(md);
					dropgrab(cg);
					break;
				}
				dropgrab(cg);
				freeb (md);
				resp = "ERROR (internal)";
				return 1;
			}
			break;
		case 'A':
		case 'a':
			return 3;
		default:
			return 3;
		}
	} /* end while */
	return 0;
}

int
do_getinfo(void)
{
	if(conn != NULL && conn-charge != 0 && (conn->charge % 10) == 0)
		syslog(LOG_INFO,"Cost %s:%s %ld",conn->cg->site,conn->cg->protocol,conn->charge);
	{
		mblk_t *mb = allocb(30,BPRI_MED);

		if(mb != NULL) {
			m_putid (mb, CMD_PROT);
			m_putsx (mb, ARG_MINOR);
			m_puti (mb, minor ? minor : fminor);
			m_putdelim (mb);
			m_putid (mb, PROTO_TICK);
			xlen = mb->b_wptr - mb->b_rptr;
			DUMPW (mb->b_rptr, xlen);
			(void) strwrite (xs_mon, (uchar_t *) mb->b_rptr, &xlen, 1);
			freemsg(mb);
		}
		setconnstate(conn,conn->state);
	}
	return 0;
}

int
do_error(void)
{
printf("GotAnError: Minor %ld, connref %ld, hdr %s\n",minor,connref,HdrName(hdrval));
	if(hdrval == HDR_CLOSE) /* Ignore? */
		return 0;
	if(conn == NULL && connref != 0) {
		for(conn = theconn; conn != NULL; conn = conn->next) {
			if(conn->connref == connref)
				break;
		}
	}
	if(conn != NULL) {
		if(conn->cg != NULL) {
			cf cfr;

			for (cfr = cf_R; cfr != NULL; cfr = cfr->next) {
				if (!matchflag(conn->cg->flags,cfr->type)) continue;
				if (wildmatch (conn->cg->site, cfr->site) == NULL) continue;
				if (wildmatch (conn->cg->protocol, cfr->protocol) == NULL) continue;
				if (wildmatch (conn->cg->card, cfr->card) == NULL) continue;
				if (classmatch (conn->cg->cclass, cfr->cclass) == NULL) continue;
				break;
			}
			if(cfr != NULL) {
				struct conninfo *xconn;
				if(cfr->got_err)
					goto conti;
				if(strchr(cfr->type,'E'))
					cfr->got_err = 1;

				xconn = malloc(sizeof(*xconn));
				if(xconn != NULL) {
					bzero(xconn,sizeof(*xconn));
					xconn->seqnum = ++connseq;
					xconn->cause = ID_priv_Print;
					xconn->causeInfo = "Program Error";
					conn->cg->refs++;
					/* dropgrab(conn->cg; ** is new anyway */
					xconn->cg = conn->cg;
					xconn->next = theconn;
					theconn = xconn;
					dropconn(xconn);
				}
			}
		}
		xx.b_rptr = xx.b_wptr = ans;
		db.db_base = ans;
		db.db_lim = ans + sizeof (ans);

		*xx.b_wptr++ = PREF_NOERR;
		m_putid (&xx, CMD_OFF);
		m_putsx(&xx,ARG_FORCE);
		if(minor > 0) {
			m_putsx (&xx, ARG_MINOR);
			m_puti (&xx, minor);
		}
		if(connref != 0) {
			m_putsx (&xx, ARG_CONNREF);
			m_puti (&xx, connref);
		}
		if(crd[0] != '\0') {
			m_putsx(&xx,ARG_CARD);
			m_putsz(&xx,crd);
		}

		xlen = xx.b_wptr - xx.b_rptr;
		DUMPW (xx.b_rptr, xlen);
		(void) strwrite (xs_mon, ans, &xlen, 1);

	}
	if((minor ? minor : fminor) != 0) {
		char dats[30];
		xx.b_rptr = xx.b_wptr = dats;
		db.db_base = dats;
		db.db_lim = dats + sizeof(dats);

		*xx.b_wptr++ = PREF_NOERR;
		m_putid (&xx, CMD_CLOSE);
		m_putsx (&xx, ARG_MINOR);
		m_puti (&xx, (minor ? minor : fminor));
		xlen = xx.b_wptr - xx.b_rptr;
		DUMPW (xx.b_rptr, xlen);
		(void) strwrite (xs_mon, (uchar_t *) xx.b_rptr, &xlen, 1);
		if(conn != NULL && conn->minor == (minor ? minor : fminor)) {
			conn->minor = 0;
			if(conn->pid == 0)
				dropconn(conn);
			else
				kill(conn->pid,SIGHUP);
		}

	}
	conti:
	no_error=1;
	return 3;
}

	int ret;

	if(init_vars())
		return;
	while (m_getsx (&xx, &id) == 0) { 
		switch(parse_arg()) {
		case 0: break;
		case 1: goto print;
		case 2: goto err;
		default:goto error;
		}
	}
	find_conn();
	/* if(ind != IND_OPEN) */ 
	(char *) xx.b_rptr = (char *) data;
  redo:
  	if (conn != NULL && conn->cg != NULL) {
		if (conn->cg->nr != NULL && nr[0] == '\0')
			strcpy(nr, conn->cg->nr);
	}
	chkall();
	switch (ind) {
	case IND_CARD:             ret = do_card();          break;
	case IND_NOCARD:           ret = do_nocard();        break;
	case IND_CARDPROTO:        ret = do_cardproto();     break;
	case IND_PROTO:
	case IND_PROTO_AGAIN:      ret = do_proto();         break;
	case IND_INCOMING:         ret = do_incoming();      break;
	case IND_CONN:             ret = do_conn();          break;
	case PROTO_HAS_ENABLE:     ret = do_hasenable();     break;
	case PROTO_HAS_DISABLE:    ret = do_hasdisable();    break;
	case ID_N1_INFO:           ret = do_getinfo();       break;
	case ID_N1_REL:
	case IND_DISC:             ret = do_disc();          break;
	case IND_CLOSE:            ret = do_close();         break;
	case IND_OPEN:             ret = do_open();          break;
	case PROTO_HAS_CONNECTED:  ret = do_hasconnected();  break;
	case PROTO_DISCONNECT:     ret = do_disconnect();    break;
	case PROTO_HAS_DISCONNECT: ret = do_hasdisconnect(); break;
	case PROTO_WANT_CONNECTED: ret = do_wantconnect();   break;
	case PROTO_AT:             ret = do_atcmd();         break;
	case IND_ERR:              ret = do_error();         break;

	case IND_INFO:
		if (m_getid (&xx, &ind) != 0)
			goto err;
		goto redo;
	case ID_N1_FAC_ACK:
		resp = "OK";
		goto print;
	case ID_N1_FAC_REJ:
		resp = "ERROR";
		goto print;
	case ID_N1_ALERT:
		resp = "RRING";
		goto print;
#if 0
	case IND_NEEDSETUP:
		{
		} break;
	case IND_EXPAND:
		{
		} break;
#endif
	default:
	  err:
		chkall();
		return;
	  /* ok: */
		resp = "OK";
		goto print;
	  error:
		resp = "ERROR";
		goto print;
	  print:
		chkall();
		if (fminor == 0)
			fminor = minor;
		if (((fminor != minor) || !no_error) && resp != NULL && fminor > 0) {
			xx.b_rptr = xx.b_wptr = ans;
			db.db_base = ans;
			db.db_lim = ans + sizeof (ans);

			if(no_error)
				*xx.b_wptr++ = PREF_NOERR;
			m_putid (&xx, CMD_PROT);
			m_putsx (&xx, ARG_FMINOR);
			m_puti (&xx, fminor);
			m_putdelim (&xx);
			m_putid (&xx, PROTO_AT);
			*xx.b_wptr++ = ' ';
			m_putsz (&xx, (uchar_t *) resp);
			xlen = xx.b_wptr - xx.b_rptr;
			DUMPW (ans, xlen);
			(void) strwrite (xs_mon, ans, &xlen, 1);
		}
		if(msgbuf != NULL && resp == msgbuf)
			free(msgbuf);
		return;
	}
	if(ret < 0 || ret == 2)
		goto err;
	else if(ret == 1)
		goto print;
	else if(ret == 3)
		goto error;
}
