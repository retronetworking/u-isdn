/*
 * This file is part of the ISDN master program.
 *
 * Copyright (C) 1995 Matthias Urlichs.
 * See the file COPYING for license details.
 */

#include "master.h"
#include "isdn_12.h" /* for the header keys */

/* Once upon a time, do_info() was a very big function. */

#ifndef __GNUC__
#error "Sorry, but you need GCC's nested functions for this."
#endif

extern void log_printmsg (void *log, const char *, mblk_t *, const char *);

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
long foffset;
long thelength;
long seqnum;
struct conninfo *conn;
char crd[5];
char prot[40];
char site[40];
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
long subcard;
long hdrval;
char no_error;
struct loader *loader;
long errnum;
char incomplete;


/* Take the incoming arguments and put them into their variables. */
int
parse_arg(void) 
{
	switch (id) {
	case ARG_CAUSE:
		(void)m_getid(&xx,&cause);
		break;
	case ARG_INCOMPLETE:
		incomplete = 1;
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
				if (err2 != -ENOENT && err2 != -ESRCH) {
					printf (" XErr 0\n");
					return 2;
				}
			}
		} break;
	case ARG_LNUMBER:
		{
			int err2;

			if ((err2 = m_getstr (&xx, lnr, MAXNR)) != 0) {
				if (err2 != -ENOENT && err2 != -ESRCH) {
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
		for(loader = isdn4_loader; loader != NULL; loader = loader->next) {
			if(!strcmp(crd,loader->name))
				break;
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
	case ARG_LENGTH:
		if (m_geti (&xx, &thelength) != 0) {
			printf (" XErr len\n");
			return 2;
		}
		break;
	case ARG_OFFSET:
		if (m_geti (&xx, &foffset) != 0) {
			printf (" XErr off\n");
			return 2;
		}
		break;
	case ARG_SEQNUM:
		if (m_geti (&xx, &seqnum) != 0) {
			printf (" XErr seq\n");
			return 2;
		}
		break;
	case ARG_UID:
		if (m_geti (&xx, &uid) != 0) {
			printf (" XErr u\n");
			return 2;
		}
		break;
	case ARG_ERRNO:
		if (m_geti (&xx, &errnum) != 0) {
			printf (" XErr en\n");
			if(0)return 2;
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
	case ARG_SUBCARD:
		if (m_geti (&xx, &subcard) != 0) {
			printf (" XErr 9s\n");
			return 2;
		}
		break;
	case ARG_CHANNEL:
		if (m_geti (&xx, &bchan) != 0) {
			printf (" XErr 9a\n");
			return 2;
		}
		break;
	case ARG_SITE:
		if (m_getstr (&xx, site, sizeof(site)-1) != 0) {
			printf (" XErr 9s\n");
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


/* Get the best-matching connection for the arguments we found. */
void
find_conn(void)
{
	struct conninfo *xconn = NULL;
	if(0)printf ("Check Conn %ld/%ld/%ld: ", minor, fminor, connref);
	if(fminor == 0) fminor = minor;
	for (conn = isdn4_conn; conn != NULL; conn = conn->next) {
		if(conn->ignore)
			continue;
		if(0)printf ("%d/%d/%ld ", conn->minor, conn->fminor, conn->connref);
		if ((connref != 0) && (conn->connref != 0)) {
			if (conn->connref == connref)
				break;
			else
				continue; /* the connection was taken over... */
		}
		if (((minor ? minor : fminor) != 0) && (conn->minor != 0)) {
			if (conn->minor == (minor ? minor : fminor))
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
		if(0)printf("Found Conn %d/%d, cref %ld\n",conn->minor,conn->fminor,conn->connref);
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
			if((conn->retries > 0) && (conn->charge > 0)) {
				if(conn->cg != NULL)
					syslog(LOG_ALERT,"Cost Runaway, connection not closed for %s:%s",conn->cg->site,conn->cg->protocol);
				else
					syslog(LOG_ALERT,"Cost Runaway, connection not closed for ???");
			}
			conn->charge = charge;
			ReportConn(conn);
			run_rp(conn,'k');
		}
		if(cause != 0)
			conn->cause = cause;
	}
}

/* Self-explanatory... */
int
init_vars (void)
{
	msgbuf = NULL;
	resp = NULL;
	fminor = 0;
	minor = 0;
	callref = 0;
	conn = NULL;
	site[0] = '*';
	site[1] = '\0';
	prot[0] = '*';
	prot[1] = '\0';
	nr[0] = '\0';
	lnr[0] = '\0';
	uid = -1;
	connref = 0;
	incomplete = 0;
	dialin = -1;
	charge = 0;
	cause = 0;
	has_force = 0;
	bchan = -1;
	hdrval = -1;
	no_error = 0;
	seqnum = 0;
	foffset = -1;
	thelength = -1;
	errnum = 0;
	subcard = 0;

	*(ulong_t *) crd = 0;
	crd[4] = '\0';
	if(log_34 & 1) {
		printf ("HL R ");
		dumpascii (data, len);
		printf ("\n");
	}
	data[len] = '\0';
	xx.b_rptr = data;
	xx.b_wptr = data + len;
	xx.b_cont = NULL;
	db.db_base = data;
	db.db_lim = data + len;
	db.db_type = M_DATA;
	xx.b_datap = &db;
	if (m_getid (&xx, &ind) != 0)
		return 1;
	(char *) data = (char *) xx.b_rptr;
	return 0;
}

void do_updown(CState state)
{
	int lim;

	if(conn->cg && conn->cg->retries)
		lim = conn->cg->retries;
	else if(conn->flags & F_FORCEIN)
		lim = 2;
	else if(conn->retiming) /* we had a problem earlier */
		lim = 5;
	else if(conn->flags & F_FASTREDIAL)
		lim = 20;
	else
		lim = 10;
	if((conn->charge > 0) || (++conn->retries > lim)) {
		if(conn->cg != NULL)
			syslog(LOG_ERR,"OFF %s:%s %s",conn->cg->site,conn->cg->protocol,CauseInfo(conn->cause, conn->causeInfo));
		else
			syslog(LOG_ERR,"OFF ??? %s",CauseInfo(conn->cause, conn->causeInfo));
		setconnstate(conn,c_off);
	} else
		setconnstate(conn,has_force ? c_off : state);
}

void do_down(CState state)
{
	if(conn->cg != NULL && conn->cg->mintime > 0) {
		if(time(NULL) - conn->upwhen < conn->cg->mintime) {
			syslog(LOG_ERR,"OFF %s:%s %s",conn->cg->site,conn->cg->protocol,CauseInfo(conn->cause, conn->causeInfo));
			setconnstate(conn,c_off);
			return;
		}
	}
	setconnstate(conn,state);
}


/* Incoming IND_CARD -- a new ISDN card is recognized, or somebody put the
   ISDN cable back in. */
int
do_card(void)
{
	cf dl;
	long nbchan, ndchan;
	long cardcap;
	int ret;
	struct isdncard *card;
	struct loader *ld;

	if ((ret = m_getstr (&xx, crd, 4)) != 0)
		return ret;
	if ((ret = m_geti (&xx, &ndchan)) != 0)
		return ret;
	if ((ret = m_geti (&xx, &nbchan)) != 0)
		return ret;
	if ((ret = m_getx (&xx, &cardcap)) != 0)
		return ret;
	if(in_boot < 0)
		in_boot = 0;
	for(ld = isdn4_loader; ld != NULL; ld = ld->next) {
		if (!strcmp(ld->name, crd))
			return -EEXIST;
	}
	for(card = isdn4_card; card != NULL; card = card->next) {
		if (!strcmp(card->name, crd))
			return -EEXIST;
	}
	card = xmalloc(sizeof(*card));
	if(card == NULL)
		return -ENOMEM;
	bzero(card,sizeof(*card));
	card->name = str_enter(crd);
	card->nrbchan = nbchan;
	card->nrdchan = ndchan;
	card->mask = (1<<ndchan)-1; /* mask of all subcards */
	card->cap = cardcap;
	card->next = isdn4_card; isdn4_card = card;

	if(cardcap & CHM_INTELLIGENT) {
		ld = xmalloc(sizeof(struct loader));
		if(ld == NULL)
			return -errno;
		bzero(ld,sizeof(*ld));
		ld->card = card;
		ld->name = str_enter(crd);

		ld->next = isdn4_loader;
		isdn4_loader = ld;

		card->name = str_enter("NULL");
		ld->card = card;
		in_boot++;
	} else {
		struct iovec io[3];
		int len;
		for(dl = cf_DL; dl != NULL; dl = dl->next) {
			if(wildmatch(crd,dl->card))
				break;
		}
		if(dl == NULL) 
			return -ENXIO;
		card->name = str_enter(crd);

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
			if(log_34 & 1)printf ("+ ");
			io[len].iov_base = ":: ";
			io[len].iov_len = 3;
			len++;
			io[len].iov_base = dl->args;
			io[len].iov_len  = strlen(dl->args);
			DUMPW (dl->args,io[len].iov_len);
			len++;
		}
		(void) strwritev (xs_mon, io,len, 1);
	}

	conn = xmalloc(sizeof(*conn));
	if(conn != NULL) {
		bzero(conn,sizeof(*conn));
		conn->seqnum = ++connseq;
		conn->causeInfo = ld ? "loading" : "passive interface";
		conn->cause = ID_priv_Print;
		conn->cardname = ld ? ld->name : card->name;
		conn->next = isdn4_conn; isdn4_conn = conn;
		if(ld != NULL)
			ld->connseq = conn->seqnum;
		dropconn(conn);
	}
	if(ld != NULL)
		card_load(ld);
	do_run_now++;
	timeout(run_now,NULL,3*HZ);

	return 0;
}

/* IND_NOCARD; a card got deconfigured. */
int
do_nocard(void)
{
	int ret;
	struct isdncard **pcard;

	if ((ret = m_getstr(&xx, crd, 4)) != 0)
		return ret;
	for (pcard = &isdn4_card; *pcard != NULL; pcard = &(*pcard)->next) {
		if (!strcmp((*pcard)->name, crd)) {
			struct loader *ld;
			struct isdncard *card = *pcard;
			*pcard = card->next;

			for(ld = isdn4_loader; ld != NULL; ld = ld->next) {
				if (ld->card == card) {
					card_load_fail(ld,-EIO);
					break;
				}
			}
			conn = xmalloc(sizeof(*conn));
			if(conn != NULL) {
				bzero(conn,sizeof(*conn));
				conn->seqnum = ++connseq;
				if(cause == 0) {
					conn->causeInfo = "interface died";
					conn->cause = ID_priv_Print;
				} else
					conn->cause = cause;
				conn->cardname = card->name;
				conn->next = isdn4_conn; isdn4_conn = conn;
				dropconn(conn);
			}
			resp = NULL;
			break;
		}
	}
	return -ENOENT;
}


int
do_offcard(void)
{
	int ret;
	struct isdncard *card;

	if ((ret = m_getstr(&xx, crd, 4)) != 0)
		return ret;

	for(card = isdn4_card; card != NULL; card = card->next) {
		if (!strcmp(card->name, crd))
			card->is_down = 1;
	}
	conn = xmalloc(sizeof(*conn));
	if(conn != NULL) {
		bzero(conn,sizeof(*conn));
		conn->seqnum = ++connseq;
		conn->causeInfo = "card is offline";
		conn->cause = ID_priv_Print;
		conn->cardname = str_enter(crd);
		conn->next = isdn4_conn; isdn4_conn = conn;
		dropconn(conn);
	}
	return 0;
}


/* IND_RECARD; an ISDN cable got reattached. */
int
do_recard(void)
{
	int ret;
	struct isdncard *card;

	if ((ret = m_getstr(&xx, crd, 4)) != 0)
		return ret;
	for(card = isdn4_card; card != NULL; card = card->next) {
		if (!strcmp(card->name, crd))
			card->is_down = 0;
	}
	conn = xmalloc(sizeof(*conn));
	if(conn != NULL) {
		bzero(conn,sizeof(*conn));
		conn->seqnum = ++connseq;
		conn->causeInfo = "card is back online";
		conn->cause = ID_priv_Print;
		conn->cardname = str_enter(crd);
		conn->next = isdn4_conn; isdn4_conn = conn;
		dropconn(conn);
	}

	for(conn=isdn4_conn; conn != NULL; conn = conn->next) {
		if(conn->ignore >= 3)
			continue;
		if(!(conn->flags & F_LEASED))
			continue;
		if(conn->cg == NULL)
			continue;
		if(wildmatch(crd,conn->cg->card) == NULL)
			continue;
		if(conn->state != c_off)
			continue;
		setconnstate(conn,c_down);
		try_reconn(conn);
	}
	return 0;
}


/* IND_CARDPROTO: request to set the mode of a channel. */
/* Maps to a call of pushcardprot(). */
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
		cg->site = str_enter(site);
		cg->protocol = str_enter(prot);
		if ((resp = findsite (&cg,1)) != NULL) {
			dropgrab(cg);
			if(conn != NULL) {
				conn->want_reconn = 0;
				setconnstate(conn, c_off);
				if(conn->pid == 0) {
					dropconn(conn);
					conn = NULL;
				}
			}

			syslog (LOG_ERR, "ISDN NoProtocol1 %s %ld %s", resp, minor, data);
			if(conn != NULL) {
				conn->cause = ID_priv_Print;
				conn->causeInfo = resp;
				ReportConn(conn);
			}
			xx.b_rptr = xx.b_wptr = ans;
			db.db_base = ans;
			db.db_lim = ans + sizeof (ans);
			if(log_34 & 2)printf("Dis10 ");
			*xx.b_wptr++ = PREF_NOERR;
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
			(void) strwrite (xs_mon, ans, xlen, 1);

			return 2;
		}
		if (pushprot (cg, minor, connref, PUSH_BEFORE) == 0) {
			dropgrab(cg);
			/* Success */
			return 0;
		} else {
			dropgrab(cg);
			syslog (LOG_ERR, "ISDN NoProtocol2 %ld %s", minor, data);
			if(conn != NULL) {
				conn->cause = ID_priv_Print;
				conn->causeInfo = "Error in PushCardProt";
				ReportConn(conn);
			}
			xx.b_rptr = xx.b_wptr = ans;
			db.db_base = ans;
			db.db_lim = ans + sizeof (ans);
			if(log_34 & 2)printf("Dis11 ");
			*xx.b_wptr++ = PREF_NOERR;
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
			(void) strwrite (xs_mon, ans, xlen, 1);


			resp = "ERROR";
			return 1;
		}
	}
	return 0;
}


/* IND_INCOMING: Incoming call processing. */
int
do_incoming(void)
{
	cf cfr;
	mblk_t *cinf;
	conngrab cg = newgrab(NULL);
	if(cg == NULL) {
		resp = "OutOfMemFoo";
		incomplete = 0;
		goto inc_err;
	}
	cg->flags = F_INCOMING|F_DIALFLAGS|F_NRCOMPLETE|F_LNRCOMPLETE;
	cinf = allocb(len,BPRI_LO);
	if(cinf == NULL) {
		resp = "OutOfMemFoo";
		incomplete = 0;
		goto inc_err;
	}

	syslog (LOG_INFO, "ISDN In %ld %s", minor, data);
	bcopy (data, cinf->b_wptr, len);
	cinf->b_wptr += len;
	cg->par_in = cinf;
	cg->card = str_enter(crd);
	if ((resp = findit (&cg,0)) != NULL) {
		if(incomplete && !strncmp(resp+1,"LNrIncomp",8)) 
			resp = "waiting for number";
		goto inc_err;
	}
	incomplete = 0;
	if (quitnow) {
		resp = "SHUTTING DOWN";
		goto inc_err;
	}
	{
		char *sit = NULL,*pro = NULL,*car = NULL,*cla = NULL; /* GCC */
		long flg;
		ulong_t sub = 0;
		if(0)printf("Hunt for %s/%s/%s/%s/%o\n",cg->site,cg->protocol,cg->card,cg->cclass,cg->flags);
		/* Figure out which program to run. */
		for (cfr = cf_R; cfr != NULL; cfr = cfr->next) {
			if(cfr->got_err) continue;
			if ((flg = matchflag (cg->flags,cfr->type)) == 0) continue;
			if ((sit = wildmatch (cg->site, cfr->site)) == NULL) continue;
			if ((pro = wildmatch (cg->protocol, cfr->protocol)) == NULL) continue;
			if ((car = wildmatch (cg->card, cfr->card)) == NULL) continue;
			if ((sub = maskmatch (cg->mask, cfr->mask)) == 0) continue;
			if ((cla =classmatch (cg->cclass, cfr->cclass)) == NULL) continue;
			break;
		}
		if (cfr == NULL) { /* None. Sorry, won't do; ATA isn't implemented yet... */
			resp = "NO PROGRAM";
			goto inc_err;
		}
		cg->site = sit; cg->protocol = pro; cg->cclass = cla; cg->card = car;
		cg->mask = sub;
	}
	if((bchan < 0) && (cg->flags & F_CHANBUSY)) {
		resp = "0BUSY other";
		goto inc_err;
	}
	if(((conn = startconn(cg,fminor,connref,&resp, NULL)) != NULL) && (resp != NULL)) {
		/* An existing connection feels responsible for this. */
		mblk_t *mz;
		if(conn->state <= c_forceoff) {
			goto cont;
		} else if ((conn->connref == connref || conn->connref == 0))  {
			if(*resp != '=')
				goto cont;
		}

		mz = allocb(80,BPRI_HI); if(mz == NULL) goto cont;

		if(*resp != '+') {
			/* Throw away this call, there's either an old one active
			   or we want to call back */
			if(log_34 & 2)printf("Dis1 ");
			*mz->b_wptr++ = PREF_NOERR;
			m_putid (mz, CMD_OFF);
			m_putsx (mz, ARG_NODISC);
			m_putsx (mz, ARG_FORCE);
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
			(void) strwrite (xs_mon, mz->b_rptr, xlen, 1);
			freeb(mz);

			if(*resp == '=') {
				setconnref(conn,connref);
				conn->want_reconn = MAX_RECONN; /* Wait for confirmation before doing anything */
				conn->want_fast_reconn = 1;
			}

			conn = xmalloc(sizeof(*conn));
			if(conn != NULL) {
				bzero(conn,sizeof(*conn));
				conn->seqnum = ++connseq;
				conn->cause = ID_priv_Print;
				conn->causeInfo = resp+1;
				cg->refs++;
				/* dropgrab(conn->cg; ** is new anyway */
				conn->cg = cg;
				conn->next = isdn4_conn; isdn4_conn = conn;
				dropconn(conn);
			}
			resp = NULL;
		} else { /* Throw away the old call */
			if(log_34 & 2)printf("Dis2 ");
			*mz->b_wptr++ = PREF_NOERR;
			m_putid (mz, CMD_OFF);
			m_putsx (mz, ARG_NODISC);
			m_putsx (mz, ARG_FORCE);
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
			(void) strwrite (xs_mon, mz->b_rptr, xlen, 1);
			freeb(mz);

			cg->refs++; dropgrab(conn->cg); conn->cg = cg;
			ReportConn(conn);

			conn = xmalloc(sizeof(*conn));
			if(conn != NULL) {
				bzero(conn,sizeof(*conn));
				conn->seqnum = ++connseq;
				conn->cause = ID_priv_Print;
				conn->causeInfo = resp+1;
				cg->refs++;
				/* dropgrab(conn->cg; ** is new anyway */
				conn->cg = cg;
				conn->next = isdn4_conn; isdn4_conn = conn;
				dropconn(conn);
			}
			resp = NULL;
#if 1
			/* cg->flags &=~ F_INCOMING; */
			/* cg->flags |= F_OUTGOING; */
			startconn(cg,fminor,connref,&resp, NULL);
			if(resp != NULL) {
				printf("OhNo %s\n",resp);
				conn = xmalloc(sizeof(*conn));
				if(conn != NULL) {
					bzero(conn,sizeof(*conn));
					conn->seqnum = ++connseq;
					conn->cause = ID_priv_Print;
					conn->causeInfo = resp+1;
					cg->refs++;
					/* dropgrab(conn->cg; ** is new anyway */
					conn->cg = cg;
					conn->next = isdn4_conn; isdn4_conn = conn;
					dropconn(conn);
				}
			}
#endif
		}
		goto cont;
			resp = NULL;
	} else if(conn != NULL) /* existing connection record */
		goto cont;
	
	/* At this point we don't have a connection. The call is valid, so
	   record the thing and start the program for it. */
	conn = (struct conninfo *)xmalloc (sizeof (struct conninfo));

	if (conn == NULL) {
		resp = "NO MEMORY.5";
		goto inc_err;
	}
	bzero (conn, sizeof (struct conninfo));
	conn->seqnum = ++connseq;

	cg->refs++;
	/* dropgrab(conn->cg; ** is new anyway */
	conn->cg = cg;
	syncflags(conn,0);
	setconnref(conn,connref);
	conn->cause = 999999;
	setconnstate(conn, c_down);
	ReportConn(conn);
	conn->next = isdn4_conn; isdn4_conn = conn;
	resp = runprog (cfr, &conn, &cg,'I');
	if(resp != NULL)
		dropconn(conn);
	else 
		chkone(conn);
	chkone(cg);
  cont:
	if (resp != NULL) {
      inc_err:
		if(!incomplete) {
			xx.b_wptr = xx.b_rptr = ans;
			xx.b_datap = &db;
			db.db_base = ans;
			db.db_lim = ans + sizeof (ans);

			if(log_34 & 2)printf("Dis3 ");
			*xx.b_wptr++ = PREF_NOERR;
			m_putid (&xx, CMD_OFF);
			if(connref != 0) {
				m_putsx (&xx, ARG_CONNREF);
				m_puti (&xx, connref);
			}

			/* BUSY-if-no-channel is very ugly but unavoidable when
				sharing the bus with brain-damaged devices (there are
				many out there) which don't answer at all when they're busy.
				Grr. The PBX should catch this case. */
			/* We send the BUSY fast if _we_re busy, else we have to send it slow
			because somebody else might in fact answer... */
			m_putsx (&xx, ARG_CAUSE);
			if((bchan < 0) || !strncmp(resp+1,"BUSY",4)) {
				m_putsx2 (&xx, ID_N1_UserBusy);
				if(!strcmp(resp+1,"BUSY") || (cg->flags & F_FASTDROP))
					m_putsx(&xx,ARG_FASTDROP);

				if(conn != NULL && (conn->flags & F_BACKCALL)) {
					if(conn->want_reconn == 0)
						conn->want_reconn = MAX_RECONN - (MAX_RECONN >> 1);
					setconnstate(conn,conn->state);
				}
			} else {
				if(cg->flags & F_NOREJECT)
					m_putsx2 (&xx, ID_N1_NoChans);
				else
					m_putsx2 (&xx, ID_N1_CallRejected);
				if(cg->flags & F_FASTDROP)
					m_putsx(&xx,ARG_FASTDROP);
			}
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
			(void) strwrite (xs_mon, ans, xlen, 1);
		}
		conn = xmalloc(sizeof(*conn));
		if(conn != NULL) {
			bzero(conn,sizeof(*conn));
			conn->seqnum = ++connseq;
			conn->cause = ID_priv_Print;
			conn->causeInfo = resp;
			cg->refs++;
			/* dropgrab(conn->cg); ** is new anyway */
			conn->cg = cg;
			run_rp(conn,'r');
			conn->next = isdn4_conn; isdn4_conn = conn;
			dropconn(conn);
		}
	}
	dropgrab(cg);
	return 0;
}

/* IND_CONN: The B channel is connected, but the protocols may not be
   synced yet. */
int
do_conn(void)
{
	if (conn != NULL)
		setconnstate(conn, c_going_up);
	if(conn == NULL || (conn->flags & F_OUTGOING)) {
		syslog (LOG_INFO, "ISDN Out %ld %s", minor, data);

		if(minor > 0 || fminor > 0 /* conn == NULL ** || !(conn->dialin & 2) */ ) {
			resp = "CARRIER";

			xx.b_rptr = xx.b_wptr = ans;
			db.db_base = ans;
			db.db_lim = ans + sizeof (ans);
			m_putid (&xx, CMD_PROT);
			m_putsx (&xx, ARG_FMINOR);
			m_puti (&xx, minor ? minor : fminor);
			m_putdelim (&xx);
			m_putid (&xx, PROTO_AT);
			m_putsz (&xx, (uchar_t *) resp);
			xlen = xx.b_wptr - xx.b_rptr;
			DUMPW (ans, xlen);
			(void) strwrite (xs_mon, ans, xlen, 1);
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
	(void) strwrite (xs_mon, (uchar_t *) xx.b_rptr, xlen, 1);
#endif
	return 0;
}

/* PROTO_ENABLE was successful -> mark a connection as DOWN. */
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

/* PROTO_DISABLE was successful -> mark a connection as OFF. */
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
do_discrun(CState state)
{
	if(conn != NULL) {
		switch(conn->state) {
		case c_going_up:
			do_updown(state);
			if((conn->flags & F_INCOMING) && !(conn->flags & F_PERMANENT)) {
				xx.b_rptr = xx.b_wptr = ans;
				db.db_base = ans;
				db.db_lim = ans + sizeof (ans);
				if(log_34 & 2)printf("Dis4d ");
				m_putid (&xx, CMD_CLOSE);
				m_putsx (&xx, ARG_MINOR);
				m_puti (&xx, minor);
				m_putsx (&xx, ARG_NOCONN);
				xlen = xx.b_wptr - xx.b_rptr;
				DUMPW (xx.b_rptr, xlen);
				(void) strwrite (xs_mon, (uchar_t *) xx.b_rptr, xlen, 1);
				if(conn->minor == minor) {
					conn->minor = 0;
					if(conn->pid == 0)
						dropconn(conn);
					else
						kill(conn->pid,SIGHUP);
				}
			}					
			break;
		case c_up:
			do_down(state);
			break;
		case c_down:
			setconnstate(conn,state);
			break;
		default:;
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
				(void) strwrite (xs_mon, (uchar_t *) xx.b_rptr, xlen, 1);
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
		if(log_34 & 2)printf("Dis4 ");
		*xx.b_wptr++ = PREF_NOERR;
		m_putid (&xx, CMD_OFF);
		m_putsx (&xx, ARG_MINOR);
		m_puti (&xx, minor);
		xlen = xx.b_wptr - xx.b_rptr;
		DUMPW (xx.b_rptr, xlen);
		(void) strwrite (xs_mon, (uchar_t *) xx.b_rptr, xlen, 1);
	}
#endif
	return 0;
}

/* IND_DISC: Connection broken. */
int
do_disc(void)
{
	if (conn != NULL) {
		if(conn->cg->nr != NULL)
			conn->cg->oldnr = conn->cg->nr;
		if(conn->cg->lnr != NULL)
			conn->cg->oldlnr = conn->cg->lnr;
		if(!conn->want_fast_reconn) {
			conn->cg->nr = NULL;
			conn->cg->flags &=~ F_NRCOMPLETE;
		}
		conn->cg->lnr = NULL;
		conn->cg->flags &=~ F_LNRCOMPLETE;
		conn->fminor = 0;

		if(conn->cg != NULL)
			syslog(LOG_INFO,"DOWN %s:%s",conn->cg->site,conn->cg->protocol);
		else
			syslog(LOG_INFO,"DOWN ???");

		switch(conn->state) {
		case c_offdown:
			setconnstate(conn,c_down);
			break;
		case c_off:
			break;
		case c_going_up:
			/* If the connect cost us anything, or if the limit was
				reached, punt. */
			(void)do_discrun(c_down);
			break;
		case c_up:
			(void)do_discrun(c_down);
			break;
		case c_going_down:
			setconnstate(conn,has_force ? c_off : c_down);
			break;
		case c_down:
			setconnstate(conn,c_down);
			break;
		default:;
		}
	}
#if 1
	if(conn != NULL && conn->sentsetup && conn->state <= c_going_down) {
		resp = "NO CARRIER";
		return 1;
	}
#endif
	return 0;
}

/* IND_CLOSE: a /dev/isdn/isdnXX or /dev/ttyiXX port got closed. */
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
				if(log_34 & 2)printf("PID still %d\n",conn->pid);
				kill(conn->pid,SIGHUP);
			}
		}
		{
			struct conninfo *nconn;
			for(conn = isdn4_conn; conn != NULL; conn = nconn) {
				nconn = conn->next;
				if(conn->minor == minor && conn->ignore >= 3) {
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
						if(log_34 & 2)printf("PID still %d\n",conn->pid);
						kill(conn->pid,SIGHUP);
					}
				}
			}
		}
	}
	unlockdev(minor);

	{	/* Now update our UTMP record, if we have one. */
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


/* IND_TRACE: something happened... */
int
do_trace(void)
{
	mblk_t xy;
	struct datab db;
	char ans[20];
	char bufstr[4000], *bufi = bufstr;
	int len = xx.b_wptr-xx.b_rptr;

	xy.b_rptr = ans;
	db.db_base = ans;
	db.db_lim = ans + sizeof (ans);
	xy.b_datap = &db;

	if(subcard == 0)
		return 0;

	bufi += sprintf(bufi,"%s/%ld %s",crd[0] ? crd : "????",subcard, dialin ? " in." : "out.");

    *bufi++ = '<';
    while (len--) {
        bufi += sprintf (bufi,"%02x", *xx.b_rptr++);
        if (len)
            *bufi++ = (' ');
    }
    *bufi++ = '>';

	for(conn = isdn4_conn; conn != NULL; conn = conn->next) {
		struct iovec io[2];

		chkone(conn);
		if(conn->ignore < 4 || conn->minor == 0)
			continue;

		xy.b_wptr = ans;
		m_putid (&xy, CMD_PROT);
		m_putsx (&xy, ARG_FMINOR);
		m_puti (&xy, conn->minor);
		m_putdelim (&xy);
		m_putid (&xy, PROTO_AT);
		io[0].iov_base = xy.b_rptr;
		io[0].iov_len = xy.b_wptr - xy.b_rptr;
		io[1].iov_base = bufstr;
		io[1].iov_len = bufi-bufstr;
		DUMPW (xy.b_rptr, io[0].iov_len);
		(void) strwritev (xs_mon, io, 2, 1);
	}
	return 0;
}

/* PROTO_TELLME -- log what's happening... */
int
do_tellme(void)
{
	mblk_t xy;
	struct datab db;
	char ans[20];
	int len = xx.b_wptr-xx.b_rptr;

	xy.b_rptr = ans;
	db.db_base = ans;
	db.db_lim = ans + sizeof (ans);
	xy.b_datap = &db;

	if(subcard == 0)
		return 0;

	for(conn = isdn4_conn; conn != NULL; conn = conn->next) {
		struct iovec io[3];

		chkone(conn);
		if(conn->ignore < 4 || conn->minor == 0)
			continue;

		xy.b_wptr = ans;
		m_putid (&xy, CMD_PROT);
		m_putsx (&xy, ARG_FMINOR);
		m_puti (&xy, conn->minor);
		m_putdelim (&xy);
		m_putid (&xy, PROTO_AT);
		io[0].iov_base = xy.b_rptr;
		io[0].iov_len = xy.b_wptr - xy.b_rptr;
		io[1].iov_base = xx.b_rptr;
		io[1].iov_len = len;
		DUMPW (xy.b_rptr, io[0].iov_len);
		(void) strwritev (xs_mon, io, 2, 1);
	}
	return 0;
}


/* IND_OPEN: a /dev/isdn/isdnXX device was openend. */
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

	/* Configure the thing to be an AT command interpreter. */
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
	m_putsx (&xx, ((conn != NULL) || (dialin > 0)) ? PROTO_ONLINE : PROTO_OFFLINE);

	len = xx.b_wptr - xx.b_rptr;
	DUMPW (xx.b_rptr, len);
	(void) strwrite (xs_mon, (uchar_t *) xx.b_rptr, len, 1);
	return 0;
}

/* Disconnecting.. */
int
do_disconnect(void)
{
#if 1
	if (conn != NULL) {
		switch(conn->state) {
		case c_going_up:
			do_updown(c_going_down);
			break;
		case c_up:
			do_down(c_going_down);
			break;
		default:;
		}
	}
	xx.b_rptr = xx.b_wptr = ans;
	db.db_base = ans;
	db.db_lim = ans + sizeof (ans);
	if(log_34 & 2)printf("Dis5 ");
	*xx.b_wptr++ = PREF_NOERR;
	m_putid (&xx, CMD_OFF);
	m_putsx (&xx, ARG_MINOR);
	m_puti (&xx, minor);
	xlen = xx.b_wptr - xx.b_rptr;
	DUMPW (xx.b_rptr, xlen);
	(void) strwrite (xs_mon, (uchar_t *) xx.b_rptr, xlen, 1);
#endif
	resp = NULL;
	return 0;
}

/* A connection has been fully established. */
int
do_hasconnected(void)
{
	if (conn != NULL) {
		syncflags(conn,1);
		setconnstate(conn,c_up);
		if(conn->flags & F_FORCEIN) {
			setconnstate(conn,c_forceoff);
			do_disconnect();
			resp = "NO CARRIER";
			return 1;
		}
	}
	resp = "CONNECT";

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
		(void) strwrite (xs_mon, ans, xlen, 1);
	}

	/* Tell the command interpreter to be transparent. */
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
	m_putsx (&xx, PROTO_ONLINE);
	xlen = xx.b_wptr - xx.b_rptr;
	DUMPW (xx.b_rptr, xlen);
	(void) strwrite (xs_mon, (uchar_t *) xx.b_rptr, xlen, 1);
	return 0;
}
	
/* ... disconnect complete. */
int
do_hasdisconnect(void)
{
	if (minor == fminor) { /* Tell the interpreter to revert to command line. */
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
		if(log_34 & 2)printf("On4 %p %d\n",conn,dialin);
		m_putsx (&xx, ((conn != NULL) || (dialin > 0)) ?  PROTO_ONLINE : PROTO_OFFLINE);
		xlen = xx.b_wptr - xx.b_rptr;
		DUMPW (xx.b_rptr, xlen);
		(void) strwrite (xs_mon, (uchar_t *) xx.b_rptr, xlen, 1);
	}
#if 1
	if(minor > 0) {
		xx.b_rptr = xx.b_wptr = ans;
		db.db_base = ans;
		db.db_lim = ans + sizeof (ans);
		if(log_34 & 2)printf("Dis6 ");
		*xx.b_wptr++ = PREF_NOERR;
		m_putid (&xx, CMD_OFF);
		m_putsx (&xx, ARG_MINOR);
		m_puti (&xx, minor);
#if 0
		if((conn != NULL) && (conn->flags & F_PERMANENT))
			m_putsx (&xx, ARG_NOCONN);
#endif
		xlen = xx.b_wptr - xx.b_rptr;
		DUMPW (xx.b_rptr, xlen);
		(void) strwrite (xs_mon, (uchar_t *) xx.b_rptr, xlen, 1);
	}
#endif
	if(conn != NULL) {
		switch(conn->state) {
		case c_going_up:
			do_updown(c_going_down);
			break;
		case c_up:
			do_down(c_going_down);
			break;
		default:;
		}
	}
#if 0
	resp = "NO CARRIER";
	return 1;
#endif
	return 0;
}

/* Some interface has data to transmit -> open a connection. */
int
do_wantconnect(void)
{
	if (!quitnow && conn != NULL && (conn->state > c_off || (conn->state == c_off && has_force))) {
		if(conn->state < c_going_up) {
			if(conn->state == c_off)
				setconnstate(conn,c_down);
			setconnref(conn,0);
			try_reconn(conn);
		}
	} else { /* No way right now. Disable. */
		if(minor > 0) {
			xx.b_rptr = xx.b_wptr = ans;
			db.db_base = ans;
			db.db_lim = ans + sizeof (ans);
			if(log_34 & 2)printf("Dis7 ");
			*xx.b_wptr++ = PREF_NOERR;
			m_putid (&xx, CMD_OFF);
			m_putsx (&xx, ARG_MINOR);
			m_puti (&xx, minor);
			m_putsx (&xx, ARG_NOCONN);
			xlen = xx.b_wptr - xx.b_rptr;
			DUMPW (xx.b_rptr, xlen);
			(void) strwrite (xs_mon, (uchar_t *) xx.b_rptr, xlen, 1);
		}
	}
	return 0;
}

/* Somebody typed a command line. */
int
do_atcmd(void)
{
	for (;;) { /* Find the AT prefix. */
		while (xx.b_rptr < xx.b_wptr && *xx.b_rptr != 'a' && *xx.b_rptr != 'A')
			xx.b_rptr++;
		if (xx.b_rptr == xx.b_wptr)
			return 0;
		while (xx.b_rptr < xx.b_wptr && (*xx.b_rptr == 'a' || *xx.b_rptr == 'A'))
			xx.b_rptr++;
		if (xx.b_rptr < xx.b_wptr && (*xx.b_rptr == 't' || *xx.b_rptr == 'T'))
			break;
	}
	/* AT recognized */
	/* If we're AT/Listening, stop it. */
	for(conn = isdn4_conn; conn != NULL; conn = conn->next) {
		if(conn->minor == minor && conn->ignore >= 3) {
			dropconn(conn);
			break;
		}
	}
	xx.b_rptr++;
	resp = "OK";
	while (1) {
		int dodrop;
	at_again:
		dodrop = 0;
		m_getskip (&xx);
		if (xx.b_rptr >= xx.b_wptr)
			return 1;
		switch (*xx.b_rptr++) {
		case '/': /* AT/ */
			m_getskip (&xx);
			switch (*xx.b_rptr++) {
			default:
				return 3;
			case 'k': /* AT/K#, kill one program; AT/K, kill all programs */
			case 'K': /* Restart in half a minute. */
				if(user[fminor] != 0 && user[fminor] != rootuser) {
					resp = "NO PERMISSION";
					return 1;
				}
				/* Kick a connection. */
				if(m_geti(&xx,&minor) != 0) {
					kill_progs(NULL);
				} else {
					for(conn = isdn4_conn; conn != NULL; conn = conn->next) {
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
			case 's': case 'S':
				if(m_geti(&xx,&minor) >= 0) {
					mblk_t xy;
					struct datab db;
					char ans[20];

					xy.b_rptr = ans;
					db.db_base = ans;
					db.db_lim = ans + sizeof (ans);
					xy.b_datap = &db;
					xy.b_wptr = ans;

					m_putid (&xy, CMD_PROT);
					m_putsx (&xy, ARG_FMINOR);
					m_puti (&xy, minor);
					m_putdelim (&xy);
					m_putid (&xy, PROTO_TELLME);
					DUMPW (xy.b_rptr, xy.b_wptr-xy.b_rptr);
					(void) strwrite (xs_mon, (uchar_t *) xy.b_rptr, xy.b_wptr-xy.b_rptr, 1);
				} else
					minor = -1; /* flag */
				goto enable_watch;
			case 'r': /* AT/R */
			case 'R': /* Reload database. */
				if(user[fminor] != 0 && user[fminor] != rootuser) {
					resp = "NO PERMISSION";
					return 1;
				}
				read_args(NULL);
				do_run_now++;
				run_now(NULL);
				break;
			case 'q': /* AT/Q */
			case 'Q': /* Shutdown the ISDN system. */
				if(user[fminor] != 0 && user[fminor] != rootuser) {
					resp = "NO PERMISSION";
					return 1;
				}
				do_quitnow(NULL);
				break;
			case 'b': /* AT/B# */
			case 'B': /* Reenable a connection. */
				{
					if(m_geti(&xx,&minor) != 0) {
						return 3;
					}
					for(conn = isdn4_conn; conn != NULL; conn = conn->next) {
						if(conn->ignore)
							continue;
						if(conn->minor == minor)
							break;
					}
					if(conn != NULL) {
						if(conn->state == c_off) {
							conn->retries = conn->retiming = 0;
							setconnstate(conn,c_down);
							goto at_again;
						} else {
							resp = "BAD STATE";
							return 1;
						}
					}
					resp = "NOT FOUND";
					return 1;
				}
				break;
			case 'f': /* AT/F# */
			case 'F': /* freeze the connection, i.e. state = OFF */
				dodrop = 1;
				/* FALL THRU */
			case 'x': /* AT#X# */
			case 'X': /* Drop a connection, i.e. state = DOWN */
				{
					if(user[fminor] != 0 && user[fminor] != rootuser) {
						resp = "NO PERMISSION";
						return 1;
					}
					if(m_geti(&xx,&minor) != 0) {
						resp = "ERROR";
						return 1;
					}
					for(conn = isdn4_conn; conn != NULL; conn = conn->next) {
						if(conn->ignore)
							continue;
						if(conn->minor == minor)
							break;
					}
					if(conn != NULL) {
						if(conn->state >= c_going_up || (dodrop && conn->state == c_down)) {
							mblk_t *mb = allocb(80,BPRI_MED);

							if(dodrop)
								setconnstate(conn, c_forceoff);
							else
								setconnstate(conn, c_down);
							if(log_34 & 2)printf("Dis8 ");
							if(mb != NULL) {

{
								*mb->b_wptr++ = PREF_NOERR;
								m_putid (mb, CMD_OFF);
								m_putsx (mb, ARG_MINOR);
								m_puti (mb, minor);
								xlen = mb->b_wptr - mb->b_rptr;
								DUMPW (mb->b_rptr, xlen);
								(void) strwrite (xs_mon, (uchar_t *) mb->b_rptr, xlen, 1);
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
			case 'l': /* AT/L */
			case 'L': /* List connections and state changes. */
				minor = 0;
			  enable_watch:
				{
					struct conninfo *fconn;
					char buf[30];

					conn = xmalloc(sizeof(*conn));
					if(conn == NULL) {
						free(msgbuf);
						resp = "NoMem";
						return 1;
					}
					bzero(conn,sizeof(*conn));
					if(!minor) {
						streamchar *m1, *m2, m3;
						m_getskip (&xx);
						if (xx.b_rptr != xx.b_wptr) {
							m1 = m2 = xx.b_rptr;
							while (m2 < xx.b_wptr && *m2 != '/' && *m2 != ';') {
								if (isspace (*m2))
									break;
								m2++;
							}
							if(m1 == m2)
								conn->cardname = "*";
							else {
								m3 = *m2;
								*m2 = '\0';
								conn->cardname = str_enter(m1);
								*m2 = m3;
								xx.b_rptr = m2;
								m_getskip (&xx);
							}
						} else
							conn->cardname = "*";
					} else
						conn->cardname = "*";
					conn->seqnum = ++connseq;
					conn->ignore = 3;
					conn->minor = fminor;
					conn->next = isdn4_conn; isdn4_conn = conn;

					if(!minor) {
						connreport("#:ref id site protocol class pid state/card cost total flags,remNr;locNr cause","*",fminor);
						for(fconn = isdn4_conn; fconn != NULL; fconn = fconn->next)  {
							if(fconn->ignore >= 3) 
								continue;
							if((fconn->cg != NULL) && (fconn->cg->card != NULL))
								if(!wildmatch(fconn->cg->card,conn->cardname))
									continue;
							ReportOneConn(fconn,fminor);
						}
					}
					if(minor > 0)
						sprintf(buf,"# Dump&Wait %ld...", minor);
					else
						sprintf(buf,"# Waiting %s...", conn->cardname);
					resp = str_enter(buf);

					return 1;
				}
				break;

			case 'w': /* AT/W */
			case 'W': /* Monitor D channels */
				{
					char buf[30];
					conn = xmalloc(sizeof(*conn));
					if(conn == NULL) {
						resp = "NoMemConn";
						return 1;
					}
					bzero(conn,sizeof(*conn));
					conn->seqnum = ++connseq;
					conn->ignore = 4;
					conn->minor = minor;
					conn->next = isdn4_conn; isdn4_conn = conn;
					{
						streamchar *m1, *m2, m3;
						m_getskip (&xx);
						if (xx.b_rptr != xx.b_wptr) {
							m1 = m2 = xx.b_rptr;
							while (m2 < xx.b_wptr && *m2 != '/' && *m2 != ';') {
								if (isspace (*m2))
									break;
								m2++;
							}
							if(m1 == m2)
								conn->cardname = "*";
							else {
								m3 = *m2;
								*m2 = '\0';
								conn->cardname = str_enter(m1);
								*m2 = m3;
								xx.b_rptr = m2;
								m_getskip (&xx);
							}
						} else
							conn->cardname = "*";
					}
					sprintf(buf,"# Monitoring %s...",conn->cardname);
					resp = str_enter(buf);
					return 1;
				}
				break;
			case 'i': /* AT/I */
			case 'I': /* List of active cards, and Level 3 state. */
				{
					char *sp;
#if LEVEL < 4
					extern int l3print(char *);
#endif
					msgbuf = xmalloc(10240);
					if(msgbuf == NULL) {
						resp = "NO MEMORY.6";
						return 1;
					}
					sp = resp = msgbuf;
					{
						struct isdncard *card;
						for(card = isdn4_card;card != NULL; card = card->next) 
							sp += sprintf(sp,"%s(%d) ",card->name,card->nrbchan);
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

			case 'm': /* AT/M foo */
			case 'M': /* send "foo" to L3. */
				{
					m_getskip (&xx);
					if (xx.b_rptr == xx.b_wptr)
						return 3;
					xlen = xx.b_wptr - xx.b_rptr;
					DUMPW (xx.b_rptr, xlen);
					(void) strwrite (xs_mon, xx.b_wptr, xlen, 1);
					return 0;
				}
				break;
#if 0
			case 'f':
			case 'F':
				{	/* Forward call. Untested; only works with 1TR6, if at all. */
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
					(void) strwrite (xs_mon, ans, xlen, 1);
					resp = NULL;
				}
				break;
#endif
			}
			break;
		case 'o': /* ATO */
		case 'O': /* go online again */
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
				(void) strwrite (xs_mon, ans, xlen, 1);

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
				if(log_34 & 2)printf("On5\n");
				m_putsx (&yy, PROTO_ONLINE);
				xlen = yy.b_wptr - yy.b_rptr;
				DUMPW (yy.b_rptr, xlen);
				(void) strwrite (xs_mon, (uchar_t *) yy.b_rptr, xlen, 1);
				resp = NULL;
			}
			return 0;
		case 'H': /* ATH */
		case 'h':  /* hang up. */
			{
				mblk_t yy;
				struct datab dbb;

				yy.b_datap = &dbb;
				yy.b_rptr = yy.b_wptr = ans;
				dbb.db_base = ans;
				dbb.db_lim = ans + sizeof (ans);
				if(log_34 & 2)printf("Dis9 ");
				*yy.b_wptr++ = PREF_NOERR;
				m_putid (&yy, CMD_OFF);
				m_putsx (&yy, ARG_MINOR);
				m_puti (&yy, minor);
				xlen = yy.b_wptr - yy.b_rptr;
				DUMPW (yy.b_rptr, xlen);
				(void) strwrite (xs_mon, (uchar_t *) yy.b_rptr, xlen, 1);
			}
			break;
		case 'D': /* ATD###, ATD/site, .../protocol, .../card */
		case 'd': /* Dial out. */
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
				if(fminor != 0) {
					m_putid (md, CMD_NOPROT);
					m_putsx (md, ARG_FMINOR);
					m_puti (md, fminor);
					xlen=md->b_wptr-md->b_rptr;
					DUMPW (md->b_rptr, xlen);
					(void) strwrite (xs_mon, md->b_rptr, xlen, 1);
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
				m_getskip (&xx);
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
				conn = xmalloc(sizeof(*conn));
				if(conn == NULL) {
					dropgrab(cg);
					resp = "NoMemConn";
					return 1;
				}
				bzero(conn,sizeof(*conn));
				conn->seqnum = ++connseq;
				conn->fminor = fminor;
				conn->minor = minor;
				conn->next = isdn4_conn; isdn4_conn = conn;

				setconnref(conn,isdn4_connref);
				isdn4_connref += 2;
				cg->refs++;
				dropgrab(conn->cg);
				conn->cg = cg;

				cg->flags = F_OUTGOING|F_MULTIDIALUP|F_DIALUP;
				if(m3 != NULL)
					cg->card = str_enter(m3);
				else
					cg->card = "*";
				if(m2 != NULL)
					cg->protocol = str_enter(m2);
				else
					cg->protocol = "login";

				if (*m1 == '/') {
					cg->site = str_enter(m1+1);
				} else {
					cg->nr = str_enter(m1);
					cg->flags |= F_NRCOMPLETE;
				}
				resp = findit (&cg,0);
				if (resp != NULL) {
					freeb (md);
					conn->cause = ID_priv_Print;
					conn->causeInfo = isdigit(*resp) ? resp+1 : resp;
					dropconn(conn);
					return 1;
				}
				if(!(cg->flags & F_NRCOMPLETE)) {
					freeb(md);
					resp = "RemoteNr incomplete";
					conn->cause = ID_priv_Print;
					conn->causeInfo = resp;
					dropconn(conn);
					return 1;
				}
				cg->refs++;
				dropgrab(conn->cg);
				conn->cg = cg;
				setconnstate(conn,c_down);
				conn->sentsetup = 1; /* Not really, but... */
				if(startconn(cg,fminor,0,&resp, NULL) != NULL) {
					freeb(md);
					break;
				}
				freeb (md);
				if(resp == NULL) 
					resp = "ERROR (internal)";
				conn->cause = ID_priv_Print;
				conn->causeInfo = isdigit(*resp) ? resp+1 : resp;
				dropconn(conn);
				return 1;
			}
			break;
		case 'A': /* ATA */
		case 'a': /* Sorry, no answer yet. */
			return 3;
		default:
			return 3;
		}
	} /* end while */
	return 0;
}

/* IND_INFO -- informational message received */
int
do_getinfo(void)
{
	int DoReport = 0;
	if(conn == NULL) {
		/* What to do? */
		return -ENXIO;
	}
	if(conn->charge != 0 && (conn->charge % 10) == 0)
		syslog(LOG_INFO,"Cost %s:%s %ld",conn->cg->site,conn->cg->protocol,conn->charge);
	if(cause != 0) {
		conn->cause = cause;
		DoReport = 1;
	}
	if((charge > 0) && (conn->state >= c_going_up)) {
			/* Send a TICK messge to the protocol stack to sync timers. */
		mblk_t *mb = allocb(80,BPRI_MED);

		if(mb != NULL) {
			m_putid (mb, CMD_PROT);
			m_putsx (mb, ARG_MINOR);
			m_puti (mb, minor ? minor : fminor);
			m_putdelim (mb);
			m_putid (mb, PROTO_TICK);
			xlen = mb->b_wptr - mb->b_rptr;
			DUMPW (mb->b_rptr, xlen);
			(void) strwrite (xs_mon, (uchar_t *) mb->b_rptr, xlen, 1);
			freemsg(mb);
		}
		setconnstate(conn,conn->state);
		DoReport = 0;
	}
	if(DoReport)
		ReportConn(conn);
	return 0;
}

/* IND_ERROR. An error has occurred. Oh no... */
int
do_error(void)
{
printf("GotAnError: Minor %ld, connref %ld, hdr %s\n",minor,connref,HdrName(hdrval));
	if(hdrval == HDR_CLOSE) /* Ignore if uncloseable */
		return 0;
	if(hdrval == HDR_LOAD) {
		if(loader != NULL) {
			if ((foffset >= 0) && (errnum == -EAGAIN) && (seqnum == loader->nrfile)) {
				loader->foffset = foffset;
				if(loader->timer) 
					untimeout(card_load,loader);
				loader->timer = 1;
				timeout(card_load,loader,HZ*5);
				return 0;
			}
			card_load_fail(loader,(errnum < 0) ? errnum : -errnum);
		}
		return 0;
	}
	if(conn == NULL && connref != 0) {
		for(conn = isdn4_conn; conn != NULL; conn = conn->next) {
			if(conn->connref == connref)
				break;
		}
	}
	if(conn != NULL) {
		if(conn->cg != NULL) {
			cf cfr;

			for (cfr = cf_R; cfr != NULL; cfr = cfr->next) {
				if (matchflag (conn->cg->flags,cfr->type) == 0) continue;
				if (wildmatch (conn->cg->site, cfr->site) == NULL) continue;
				if (wildmatch (conn->cg->protocol, cfr->protocol) == NULL) continue;
				if (wildmatch (conn->cg->card, cfr->card) == NULL) continue;
				if (maskmatch (conn->cg->mask, cfr->mask) == 0) continue;
				if (classmatch (conn->cg->cclass, cfr->cclass) == NULL) continue;
				break;
			}
			if(cfr != NULL) {
				struct conninfo *xconn;
				if(cfr->got_err)
					goto conti;
				if(strchr(cfr->type,'E'))
					cfr->got_err = 1;

				xconn = xmalloc(sizeof(*xconn));
				if(xconn != NULL) {
					bzero(xconn,sizeof(*xconn));
					xconn->seqnum = ++connseq;
					xconn->cause = ID_priv_Print;
					xconn->causeInfo = "Program Error";
					conn->cg->refs++;
					/* dropgrab(conn->cg; ** is new anyway */
					xconn->cg = conn->cg;
					xconn->next = isdn4_conn; isdn4_conn = xconn;
					dropconn(xconn);
				}
			}
		}
		xx.b_rptr = xx.b_wptr = ans;
		db.db_base = ans;
		db.db_lim = ans + sizeof (ans);

		if(log_34 & 2)printf("DisA ");
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
		(void) strwrite (xs_mon, ans, xlen, 1);

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
		(void) strwrite (xs_mon, (uchar_t *) xx.b_rptr, xlen, 1);
		if(conn != NULL && conn->minor == (minor ? minor : fminor)) {
			conn->minor = 0;
			if(conn->pid == 0)
				dropconn(conn);
			else
				kill(conn->pid,SIGHUP);
		}

		for(conn = isdn4_conn; conn != NULL; conn = conn->next) {
			if((conn->ignore >= 3) && (conn->minor == (minor ? minor : fminor))) {
				dropconn(conn);
				break;
			}
		}
	}
	conti:
	no_error=1;
	return 3;
}


/* No error. Wow... */
int
do_noerror(void)
{
	if(hdrval == HDR_CLOSE) /* Ignore if uncloseable */
		return 0;
	if((hdrval == HDR_LOAD) && (loader != NULL)) {
		if(thelength == 0) { /* End of file. Do now if there's no timeout set. */
			if(!loader->timer)
				card_load(loader);
		} else { /* Continue file. Kick now. */
			if(loader->timer)
				untimeout(card_load,loader);
			card_load(loader);
		}
	}
	return 0;
}

/* Main code of do_info() follows. Finally... */
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
	if(ind != IND_TRACE)
		(char *) xx.b_rptr = (char *) data;
  redo:
  	if (conn != NULL && conn->cg != NULL) {
		if (conn->cg->nr != NULL && nr[0] == '\0')
			strcpy(nr, conn->cg->nr);
	}
	chkall();
	switch (ind) {
	case IND_CARD:             ret = do_card();          break;
	case IND_RECARD:           ret = do_recard();        break;
	case IND_OFFCARD:          ret = do_offcard();       break;
	case IND_NOCARD:           ret = do_nocard();        break;
	case IND_CARDPROTO:        ret = do_cardproto();     break;
	case IND_INCOMING:         ret = do_incoming();      break;
	case IND_CONN:             ret = do_conn();          break;
	case PROTO_HAS_ENABLE:     ret = do_hasenable();     break;
	case PROTO_HAS_DISABLE:    ret = do_hasdisable();    break;
	case ID_N1_INFO:           ret = do_getinfo();       break;
	case ID_ET_FAC:            ret = do_getinfo();       break;
	case IND_DISCONNECT:       ret = do_disc();          break;
	case IND_DISCONNECTING:    ret = do_discrun(c_going_down); break;
	case IND_CLOSE:            ret = do_close();         break;
	case IND_OPEN:             ret = do_open();          break;
	case IND_TRACE:            ret = do_trace();         break;
	case PROTO_TELLME:         ret = do_tellme();        break;
	case PROTO_HAS_CONNECTED:  ret = do_hasconnected();  break;
	case PROTO_DISCONNECT:     ret = do_disconnect();    break;
	case PROTO_HAS_DISCONNECT: ret = do_hasdisconnect(); break;
	case PROTO_WANT_CONNECTED: ret = do_wantconnect();   break;
	case PROTO_AT:             ret = do_atcmd();         break;
	case IND_ERR:              ret = do_error();         break;
	case IND_NOERR:            ret = do_noerror();       break;

	case IND_INFO: /* Skip the INFO thing and parse what's beneach. */
		if (m_getid (&xx, &ind) != 0)
			goto err;
		goto redo;
	case ID_N1_FAC_ACK: /* Something or other... */
		resp = "OK";
		goto print;
	case ID_N1_FAC_REJ:
		resp = "ERROR";
		goto print;
	case ID_N1_ALERT: /* Hey, there is somebody at the other end! */
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
			(void) strwrite (xs_mon, ans, xlen, 1);
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
