/*
 *
 * ISDN loader for intelligent cards.
 *
 * Copyright (c) 1993-1995 Matthias Urlichs <urlichs@noris.de>.
 */

#include "master.h"

#if 0
struct loader {
	char *card;
	FILE *file;
	long seqnum; /* number in config file */
	int nrfile; /* number loaded to card */
};

/* cf_LF: num2: sequence number; num: block size */
#endif

void card_load(struct loader *ld)
{
	cf dl,lf;
	int do_again;

  again:
    do_again = 0;
	for(lf = cf_LF; lf != NULL; lf++) {
		if(!wildmatch(ld->card,lf->card))
			continue;
		if(ld->seqnum != 0 && ld->seqnum != lf->num2) {
			if(ld->seqnum < lf->num2 && (do_again == 0 || do_again > lf->num2))
				do_again = ld->seqnum;
			continue;
		}
		break;
	}
	if(lf == NULL) {
		if(ld->seqnum == 0) {
			syslog(LOG_ERR, "Card loader for %s dropped us (seq %d)!\n",ld->card,ld->seqnum);
			goto ex_load;
		}
		if(do_again) {
			ld->seqnum = do_again;
			goto again;
		}
	} else
		do_again = 1;
	if(!do_again || (ld->file != NULL)) {
		char *buf = NULL; /* shut up GCC */
		int len, xlen, foffset;
		mblk_t xx;
		struct datab db;
		struct iovec io[3];
		streamchar ans[30];

		if(ld->file != NULL) {
			buf = malloc(lf->num);
			if(buf == NULL) {
				syslog(LOG_ERR, "Card loader for %s !\n",ld->card);
				goto ex_load;
			}

			foffset = ftell(ld->file);
			if(foffset < 0) {
				syslog(LOG_ERR, "Card loader for %s: ftell returned %m\n",ld->card);
				goto ex_load;
			}
			len = fread(buf,1,lf->num,ld->file);
			if(len < 0) {
				syslog(LOG_ERR, "Card loader for %s: read returned %m\n",ld->card);
				free(buf);
				goto ex_load;
			}
		} else {
			len = 0;
			foffset = 0;
		}
		xx.b_rptr = xx.b_wptr = ans;
		db.db_base = ans;
		db.db_lim = ans + sizeof (ans);
		m_putid (&xx, CMD_LOADFILE);
		m_putsx(&xx,ARG_CARD);
		m_putsz(&xx,ld->card);
		m_putsx(&xx,ARG_SEQNUM);
		m_puti(&xx,ld->seqnum);
		m_putsx(&xx,ARG_OFFSET);
		m_puti(&xx,foffset);
		
		*xx.b_wptr++ = ' ';
		xlen = xx.b_wptr - xx.b_rptr;
		DUMPW (ans, xlen);
		io[0].iov_base = ans;
		io[0].iov_len = xlen;
		io[1].iov_base = ":: ";
		io[1].iov_len = 3;
		if(len > 0) {
			io[2].iov_base = buf;
			io[2].iov_len  = len;
			len = 3;
		} else {
			len = 2;
			if(ld->file != NULL) {
				fclose(ld->file);
				ld->file = NULL;
			}
		}
		(void) strwritev (xs_mon, io,len, 1);
		free(buf);
		if(do_again) {
			timeout(card_load,ld,HZ/5);
			return;
		}
	} else {
		ld->seqnum = lf->num2;
		ld->nrfile++;
		ld->file = fopen(lf->arg,"r");
		if(ld->file == NULL) {
			syslog(LOG_ERR, "Card loader for %s: file %s: %m\n",ld->card,lf->arg);
			goto ex_load;
		}
		timeout(card_load,ld,HZ);
		return;
	}

	for(dl = cf_DL; dl != NULL; dl = dl->next) {
		struct iovec io[3];
		int len;
		mblk_t xx;
		struct datab db;
		streamchar ans[30];

		if(!wildmatch(dl->card,ld->card))
			continue;
		xx.b_rptr = xx.b_wptr = ans;
		db.db_base = ans;
		db.db_lim = ans + sizeof (ans);
		m_putid (&xx, CMD_DOCARD);
		m_putsx(&xx,ARG_CARD);
		m_putsz(&xx,ld->card);

		*xx.b_wptr++ = ' ';
		len = xx.b_wptr - xx.b_rptr;
		DUMPW (ans, len);
		io[0].iov_base = ans;
		io[0].iov_len = len;
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

  ex_load:
    if(ld->file != NULL)
		fclose(ld->file);
	free(ld);
	return;
}
