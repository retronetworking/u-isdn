/*
 *
 * ISDN loader for intelligent cards.
 *
 * Copyright (c) 1993-1995 Matthias Urlichs <urlichs@noris.de>.
 */

#include "master.h"

static void
card_load_close(struct loader *ld, char success)
{
	if(ld->name != NULL)
		ld->card->name = ld->name;
    if(ld->file != NULL)
		fclose(ld->file);
	if(ld->timer)
		untimeout(card_load,ld);
	{
		struct loader **ldp = &isdn4_loader;
		while(*ldp != NULL) {
			if(*ldp == ld) {
				*ldp = ld->next;
				break;
			}
			ldp = &(*ldp)->next;
		}
	}
	if(success) {
		char newconn = 0;
		struct conninfo *conn;
		for(conn = isdn4_conn; conn != NULL; conn = conn->next) {
			if(conn->seqnum == ld->connseq)
				break;
		}
		if(conn == NULL) {
			conn = xmalloc(sizeof(*conn));
			newconn = 1;
		}
		if(conn != NULL) {
			if(newconn) {
				bzero(conn,sizeof(*conn));
				conn->cause = ID_priv_Print;
				conn->cardname = ld->card->name;
				conn->seqnum = ++connseq;
				conn->next = isdn4_conn; isdn4_conn = conn;
			}
			conn->causeInfo = "active interface";
			dropconn(conn);
		}
	}
	free(ld);
    do_run_now++;
    timeout(run_now,NULL,HZ/3);
}

void
card_load_fail(struct loader *ld, int err)
{
	mblk_t *mj = allocb (32, BPRI_LO);
	int len;
    struct isdncard **pcard, *card = ld->card;

	if(err == -EAGAIN) {
		if(ld->timer)
			untimeout(card_load,ld);
		if(ld->thislen > 0)
			ld->foffset -= ld->thislen;
		else {
			ld->thislen = 0;
			ld->nrfile--;
		}
		ld->timer = 1;
		timeout(card_load,ld,HZ/2);
		return;
	}
	
    for (pcard = &isdn4_card; *pcard != NULL; pcard = &(*pcard)->next) {
        if (card == *pcard) {
            *pcard = card->next;
			break;
		}
	}

	syslog(LOG_ERR,"Card %s was not loaded: Error %s at file %d, pos %d\n",ld->card,strerror(err),ld->nrfile,ld->foffset);

	/* Tell lower layer to forget the thing */

	if (mj != NULL) {
		m_putid (mj, CMD_NOCARD);
		m_putsx (mj, ARG_CARD);
		m_putsz (mj, ld->name);
		len = mj->b_wptr - mj->b_rptr;
		DUMPW (mj->b_rptr, len);
		(void) strwrite (xs_mon, (uchar_t *) mj->b_rptr, len, 1);
		freeb (mj);
	}

	{
		char newconn = 0;
		struct conninfo *conn;
		for(conn = isdn4_conn; conn != NULL; conn = conn->next) {
			if(conn->seqnum == ld->connseq)
				break;
		}
		if(conn == NULL) {
			conn = xmalloc(sizeof(*conn));
			newconn = 1;
		}
		if(conn != NULL) {
			if(newconn) {
				bzero(conn,sizeof(*conn));
				conn->cause = ID_priv_Print;
				conn->seqnum = ++connseq;
				conn->next = isdn4_conn; isdn4_conn = conn;
			}
			conn->causeInfo = "Load failed";
			dropconn(conn);
		}
	}
	card_load_close(ld,0);
	free(card);
}

void
card_load(struct loader *ld)
{
	cf lf;
	int do_again, thisseq;

	ld->timer = 0;
  again:
    do_again = 0;
	thisseq = 0;
	/* Now find the next applicable entry. ld->seqnum is the cf number of the
	   next file to load. */
	for(lf = cf_LF; lf != NULL; lf = lf->next) {
		thisseq++;
		if(!wildmatch(ld->name,lf->card))
			continue;
		if(ld->seqnum != 0 && ld->seqnum != thisseq) {
			if(ld->seqnum < thisseq && (do_again == 0 || do_again < thisseq))
				do_again = thisseq;
			continue;
		}
		break;
	}
	if(lf == NULL) {
		if(ld->seqnum == 0) {
			syslog(LOG_ERR, "Card loader for %s has a config problem (seq %d)!\n",ld->card,ld->seqnum);
			goto ex_load;
		}
		if(do_again) {
			ld->seqnum = do_again;
			goto again;
		}
	} else
		do_again = 1;
	if(ld->thislen >= 0) {
		if (!do_again || (ld->file != NULL)) {
			char *buf = NULL;
			int len, xlen, foffset;
			mblk_t xx;
			struct datab db;
			struct iovec io[3];
			streamchar ans[50];

			if(ld->file != NULL) {
				buf = xmalloc(lf->num);
				if(buf == NULL) {
					syslog(LOG_ERR, "Card loader for %s !\n",ld->card);
					goto ex_load;
				}

				if(fseek(ld->file,ld->foffset,0) < 0) {
					syslog(LOG_ERR, "Card loader for %s: fseek returned %m\n",ld->card);
					goto ex_load;
				}
				len = fread(buf,1,lf->num,ld->file);
				if(len < 0) {
					syslog(LOG_ERR, "Card loader for %s: read returned %m\n",ld->card);
					free(buf);
					goto ex_load;
				}
				foffset = ld->foffset;
				ld->foffset += len;
				ld->thislen = len;
			} else {
				len = 0;
				foffset = 0;
				ld->foffset = 0;
			}
			if(!do_again && (ld->file == NULL))
				++ld->nrfile; /* last file; we're finished */
			xx.b_rptr = xx.b_wptr = ans;
			xx.b_datap = &db;
			db.db_base = ans;
			db.db_lim = ans + sizeof (ans);
			m_putid (&xx, CMD_LOADFILE);
			m_putsx(&xx,ARG_CARD);
			m_putsz(&xx,ld->name);
			m_putsx(&xx,ARG_SEQNUM);
			m_puti(&xx,ld->nrfile);
			m_putsx(&xx,ARG_OFFSET);
			m_puti(&xx,foffset);
			
			*xx.b_wptr++ = ' ';
			xlen = xx.b_wptr - xx.b_rptr;
			DUMPW (ans, xlen);
			io[0].iov_base = ans;
			io[0].iov_len = xlen;
			io[1].iov_base = "::";
			io[1].iov_len = 2;
			if(len > 0) {
				io[2].iov_base = buf;
				io[2].iov_len  = len;
				len = 3;
			} else {
				len = 2;
				if(ld->file != NULL) {
					fclose(ld->file);
					ld->file = NULL;
					ld->seqnum++;
				}
			}
			(void) strwritev (xs_mon, io,len, 1);
			if(buf != NULL)
				free(buf);
			ld->timer = 1;
			timeout(card_load,ld,(ld->file || !lf) ? HZ : (HZ*lf->num2+HZ/3));
			if(!do_again)
				ld->thislen = -1;
			return;
		} else {
			ld->seqnum = thisseq;
			ld->nrfile++;
			ld->file = fopen(lf->arg,"r");
			ld->foffset = 0;
			if(ld->file == NULL) {
				syslog(LOG_ERR, "Card loader for %s: file %s: %m\n",ld->card,lf->arg);
				goto ex_load;
			}
			ld->timer = 1;
			timeout(card_load,ld,HZ/3);
			return;
		}
	} else {
		cf dl;

		for(dl = cf_DL; dl != NULL; dl = dl->next) {
			struct iovec io[3];
			int len;
			mblk_t xx;
			struct datab db;
			streamchar ans[30];

			if(!wildmatch(dl->card,ld->name))
				continue;
			xx.b_rptr = xx.b_wptr = ans;
			xx.b_datap = &db;
			db.db_base = ans;
			db.db_lim = ans + sizeof (ans);
			m_putid (&xx, CMD_DOCARD);
			m_putsx(&xx,ARG_CARD);
			m_putsz(&xx,ld->name);

			*xx.b_wptr++ = ' ';
			len = xx.b_wptr - xx.b_rptr;
			DUMPW (ans, len);
			io[0].iov_base = ans;
			io[0].iov_len = len;
			len = 1;
			if(dl->args != NULL) {
				printf ("+ ");
				io[len].iov_base = "::";
				io[len].iov_len = 2;
				len++;
				io[len].iov_base = dl->args;
				io[len].iov_len  = strlen(dl->args);
				DUMPW (dl->args,io[len].iov_len);
				len++;
			}
			(void) strwritev (xs_mon, io,len, 1);
			card_load_close(ld,1);
			return;
		}
		card_load_fail(ld,-EIO);
	}
	return;

  ex_load:
  	card_load_fail(ld,-EIO);
	return;
}
