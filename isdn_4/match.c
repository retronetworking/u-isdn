/*
 * This file is part of the ISDN master program.
 *
 * Copyright (C) 1995 Matthias Urlichs.
 * See the file COPYING for license details.
 */

#include "master.h"
#include "isdn_12.h" /* CHM_INTELLIGENT */

void
syncflags(conninfo conn, char set)
{
	if(conn->cg == NULL)
		return;
	
	if(((conn->cg->flags & F_MASKFLAGS) == 0) || ((conn->cg->flags & F_MASKFLAGS) == F_DIALFLAGS)) {
		conn->flags = (conn->flags & F_MASKFLAGS) | (conn->cg->flags & ~F_MASKFLAGS);
	} else {
		if(set)
			conn->flags |= (conn->cg->flags & F_MOVEFLAGS & ~F_DIALFLAGS);
		else
			conn->flags = conn->cg->flags;
	}
}


/* Verify that the connection mode matches the stated flags. */
long
matchflag(long flags, char *ts)
{
	char inc,outg,leas,perm,dial,mdial,ini,aft;

	ini = (strchr(ts,'u') != NULL);
	aft = (strchr(ts,'a') != NULL);
	inc = (strchr(ts,'i') != NULL);
	outg= (strchr(ts,'o') != NULL);
	leas= (strchr(ts,'f') != NULL);
	perm= (strchr(ts,'p') != NULL);
	dial= (strchr(ts,'d') != NULL);
	mdial=(strchr(ts,'m') != NULL);

	if(flags & F_SETINITIAL) { if (aft && !ini) return 0; }
	if(flags & F_SETLATER)   { if (!aft && ini) return 0; }

	if(flags & F_OUTGOING) { if (inc && !outg) return 0; }
	if(flags & F_INCOMING) { if (!inc && outg) return 0; }

	if(!(leas || dial || perm)) goto set;
	if(!(flags & (F_LEASED|F_PERMANENT|F_MULTIDIALUP|F_DIALUP))) goto set;
	if((flags & F_LEASED)      && leas) goto set;
	if((flags & F_PERMANENT)   && perm) goto set;
	if((flags & F_DIALUP)      && dial) goto set;
	if((flags & F_MULTIDIALUP) && mdial) goto set;
	return 0;
  set:
  	if(ini)  flags |= F_SETINITIAL;
  	if(aft)  flags |= F_SETLATER;
  	if(inc)  flags |= F_INCOMING;
  	if(outg) flags |= F_OUTGOING;
  	if(leas) flags |= F_LEASED;
  	if(perm) flags |= F_PERMANENT;
  	if(dial) flags |= F_DIALUP;
  	if(mdial)flags |= F_MULTIDIALUP;
  	return   flags |  F_FOOBAR;
}

/* Check if this P-line matches a connection request */
char *
pmatch1 (cf prot, conngrab *cgm)
{
	char *sit, *pro, *cla, *car;
	long flg;
	ulong_t sub;
	char first = 1;
	conngrab cg = *cgm;

	chkone(prot); chkone(cg);
	/* Basic preprocessing */
	sit = wildmatch(cg->site,    prot->site);    if(sit == NULL) return "7ERR Match SITE";
	pro = wildmatch(cg->protocol,prot->protocol);if(pro == NULL) return "7ERR Match PROTOCOL";
	car = wildmatch(cg->card,    prot->card);    if(car == NULL) return "5ERR Match CARD";
	cla =classmatch(cg->cclass,  prot->cclass);  if(cla == NULL) return "4ERR Match CLASS";
	sub = maskmatch(cg->mask,    prot->mask);    if(sub == 0)    return "6ERR Match SUBCARD";

	/* OK, that fits. Make a copy to assign the values to. */
	cg = newgrab(cg);
	if(cg == NULL)
		return "0OUT OF MEMORY";
	cg->site = sit; cg->protocol = pro; cg->card = car; cg->cclass = cla;
	cg->mask = sub;

	/* Now scan this line's, and all matching followup lines', flags. */
	for (first = 1; prot != NULL; prot = prot->next, first = 0) {
#define ARG_IN 01
#define ARG_OUT 02
		char nrt = ARG_IN|ARG_OUT;
		ushort_t id;
		conngrab cgc = NULL;
		mblk_t *cand = NULL;
		streamchar *mbs_in = NULL, *mbs_out = NULL;

		if((flg = matchflag(cg->flags,prot->type)) == 0) { if(first) { 
			if(1)printf(":BadFlag %s for 0%o\n",prot->type,cg->flags);
			dropgrab(cg); return "5ERR BadFlag"; } else continue;}
		if (first) {
			if (strchr (prot->type, 'M')) { /* First Match not allowed. */
				dropgrab(cg);
				return "7ERR FIND NotFirst";
			}
		} else {
			if (strchr (prot->type, 'R')) /* First Match only. */
				goto Ex;

			sit = wildmatch(cg->site,    prot->site);    if(sit==NULL) continue;
			pro = wildmatch(cg->protocol,prot->protocol);if(pro==NULL) continue;
			car = wildmatch(cg->card,    prot->card);    if(car==NULL) continue;
			cla =classmatch(cg->cclass,  prot->cclass);  if(cla==NULL) continue;
			sub = maskmatch(cg->mask,    prot->mask);    if(sub==0)    continue;
		}
		/* Now make another copy for the parameters. If they don't fit
		   we'll have to undo everything. This is stupid but there is no
		   other way to do it, for now -- we'll use a cache sometime soon. */
		cgc = newgrab(cg);
		if(cgc == NULL) {
			dropgrab(cg);
			return "0OUT OF MEMORY";
		}
		if(!first) {
			cgc->site = sit; cgc->protocol = pro; cgc->card = car; cgc->cclass = cla;
			cgc->mask = sub; cgc->flags = flg;
		}
		if(cgc->retries == 0 && prot->num != 0)
			cgc->retries = prot->num;
		if(cgc->par_out == NULL) { /* No outgoing parameter list? Yet! */
			if ((cgc->par_out = allocb(256,BPRI_LO)) == NULL) {
				dropgrab(cgc); dropgrab(cg);
				return "0OUT of MEMORY";
			}
		}
#define DG(str) do { if(first) { dropgrab(cgc); dropgrab(cg); return str; } goto Ex; } while(0)

		/* Remember pointers into the parameter strings. */
		mbs_in = ((cgc->par_in !=NULL)? cgc->par_in->b_rptr : NULL);
		mbs_out= ((cgc->par_out!=NULL)? cgc->par_out->b_rptr: NULL);
			
		/* Allocate a new candidate string for outgoing parameters. */
		cand = allocsb (strlen (prot->args), (streamchar *)prot->args);
		if(cand == NULL)
			goto Ex;
		while (m_getsx (cand, &id) == 0) {

		/* Check that an incoming parameter matches. */
#define CHKI(_what,_t)														\
				({ __label__ ex; long xx,yy; ushort_t id2;					\
					yy = 0;													\
					if(m_get##_what(cand,(_t *)&yy) != 0) break;			\
					if(cgc->par_in != NULL) {								\
						while(m_getsx(cgc->par_in,&id2) == 0) {				\
							if(id != id2) continue;							\
							xx = 0;											\
							if(m_get##_what (cgc->par_in,(_t *)&xx) != 0) continue;	\
							if(xx == yy) goto ex;							\
							if(0)if(!strchr(prot->type,'F') && !first) break;\
							cgc->par_in->b_rptr = mbs_in;					\
							freeb(cand);									\
							if(0)printf("MatchI %x: %lx vs %lx\n",id,xx,yy);	\
							dropgrab(cgc); dropgrab(cg); return "3ERR FIND Match Arg";		\
						}													\
					}														\
				ex:; }) 														/**/
		
		/* Dto., outgoing parameter; if it's not there yet and we need it, add it. */
#define CHKO(_what,_t)														\
				({ __label__ ex; long xx,yy; ushort_t id2;					\
					yy = 0;													\
					if(m_get##_what(cand,(_t *)&yy) != 0) break;			\
					if(cgc->par_out != NULL) {								\
						while(m_getsx(cgc->par_out,&id2) == 0) {			\
							if(id != id2) continue;							\
							xx = 0;											\
							if(m_get##_what (cgc->par_out,(_t *)&xx) != 0) continue;	\
							if(xx == yy) goto ex;							\
							if(0)if(!strchr(prot->type,'F') && !first) break;\
							cgc->par_out->b_rptr = mbs_out;					\
							freeb(cand);									\
							if(0)printf("MatchO %x: %lx vs %lx\n",id,xx,yy);	\
							dropgrab(cgc); dropgrab(cg); return "3ERR FIND Match Arg";		\
						}													\
						if(!(cgc->flags & F_OUTCOMPLETE)) {					\
							m_putsx(cgc->par_out,id);						\
							m_put##_what(cgc->par_out,*(_t *)&yy);			\
						}													\
					}														\
				ex:; })														/**/

		/* Put it all together. */
#define CHK(_what,_t) { \
			if((nrt & ARG_OUT)&& (cgc->flags & F_OUTGOING) && (cgc->par_out!= NULL)) CHKO(_what,_t); \
			if((nrt & ARG_IN) && (cgc->flags & F_INCOMING) && (cgc->par_in != NULL)) CHKI(_what,_t); } break

	/* Same as above, but for vectorized parameters with optional bitmasks. */
	/* You are not supposed to understand this code. */
#define CHKVI()																\
	({ __label__ ex; int xx,yy,xm; streamchar *vx,*vy,*vm; ushort_t id2;		\
		yy = m_gethexlen(cand);												\
		if (yy <= 0 || (vy=xmalloc(yy))==NULL) break;						\
		if(m_gethex(cand,vy,yy) != 0) { free(vy); break; }					\
		if ((xm = m_gethexlen(cand)) > 0) {									\
			if ((vm=xmalloc(xm)) == NULL)									\
				{ free(vy); break; }										\
			if(m_gethex(cand,vm,xm) != 0)									\
				{ free(vy); free(vm); break; }								\
		} else																\
			{ vm=NULL; xm=0; }												\
		if(cgc->par_in != NULL) {											\
			while(m_getsx(cgc->par_in,&id2) == 0) {							\
				if(id != id2) continue;										\
				xx = m_gethexlen(cgc->par_in);								\
				if (xx <= 0 || (vx=xmalloc(xx))==NULL) break;				\
				if(m_gethex(cgc->par_in,vx,xx) != 0)						\
					{ free(vx); break; }									\
				if(abs(vectcmp(vx,xx,vy,yy,vm,xm)) < 5)						\
					{ free(vx); free(vy); if(xm>0)free(vm); goto ex; }		\
				if(0)if(!strchr(prot->type,'F') && !first) continue;		\
				cgc->par_in->b_rptr = mbs_in;								\
				freeb(cand);												\
				if(0)printf("MatchVI %x: %s vs %s\n",id,vx,vy);				\
				free(vx); free(vy); if(xm>0)free(vm);						\
				dropgrab(cgc); dropgrab(cg); return "3ERR FIND Match Arg";	\
			}																\
			free(vy); if(xm>0)free(vm);										\
		}																	\
	ex:; })				 														/**/
#define CHKVO()																\
	({ __label__ ex; int xx,yy,xm; streamchar *vx,*vy,*vm; ushort_t id2;		\
		yy = m_gethexlen(cand);												\
		if (yy <= 0 || (vy=xmalloc(yy))==NULL) break;						\
		if(m_gethex(cand,vy,yy) != 0) { free(vy); break; }					\
		if ((xm = m_gethexlen(cand)) > 0) {									\
			if ((vm=xmalloc(xm)) == NULL)									\
				{ free(vy); break; }										\
			if(m_gethex(cand,vm,xm) != 0)									\
				{ free(vy); free(vm); break; }								\
		} else																\
			{ vm=NULL; xm=0; }												\
		if(cgc->par_out != NULL) {											\
			while(m_getsx(cgc->par_out,&id2) == 0) {						\
				if(id != id2) continue;										\
				xx = m_gethexlen(cgc->par_out);								\
				if (xx <= 0 || (vx=xmalloc(xx))==NULL) break;				\
				if(m_gethex(cgc->par_out,vx,xx) != 0)						\
					{ free(vx); break; }									\
				if(abs(vectcmp(vx,xx,vy,yy,vm,xm)) < 5)						\
					{ free(vx); free(vy); if(xm>0)free(vm); goto ex; }		\
				if(0)if(!strchr(prot->type,'F') && !first) continue;		\
				cgc->par_out->b_rptr = mbs_out;								\
				if(0)printf("MatchVO %x: %s vs %s\n",id,vx,vy);				\
				freeb(cand);												\
				free(vx); free(vy); if(xm>0)free(vm);						\
				dropgrab(cgc); dropgrab(cg); return "3ERR FIND Match Arg";	\
			}																\
			if(!(cgc->flags & F_OUTCOMPLETE)) {								\
				m_putsx(cgc->par_out,id);									\
				m_puthex(cgc->par_out,vy,yy);								\
			}																\
			free(vy); if(xm>0)free(vm);										\
		}																	\
	ex:; })			 														/**/

#define CHKV() { \
				if((nrt & ARG_OUT)&& (cgc->flags & F_OUTGOING) && (cgc->par_out!= NULL)) CHKVO(); \
				if((nrt & ARG_IN) && (cgc->flags & F_INCOMING) && (cgc->par_in != NULL)) CHKVI(); } break

	/* Simple one-shot labels that can't be undone (and don't need to be). */
#define CHKX()																\
				({ __label__ ex; ushort_t id2;								\
					if(cgc->par_out != NULL) {								\
						while(m_getsx(cgc->par_out,&id2) == 0) {			\
							if(id != id2) continue;							\
							goto ex;										\
						}													\
						if(!(cgc->flags & F_OUTCOMPLETE))					\
							m_putsx(cgc->par_out,id);						\
					}														\
				ex:; }); break	  											/**/

			switch (id) {
				/* Direction-specific arguments. */
			case ARG_INNUMS  : nrt=ARG_IN;  break;
			case ARG_OUTNUMS : nrt=ARG_OUT; break;
			case ARG_BOTHNUMS: nrt=ARG_IN|ARG_OUT; break;

				/* Flags, ob lokale / entfernte Nummern präsent sein sollen
				   oder nicht. Logischerweise nur bei ankommenden Anrufen
				   interessant. */
			case ARG_NEEDNOLOCAL:
				if((cg->flags & F_INCOMING) && (cg->lnr != NULL))
					DG("3LocalNrGiven");
				break;
			case ARG_NEEDNOREMOTE:
				if((cg->flags & F_INCOMING) && (cg->nr != NULL))
					DG("3RemoteNrGiven");
				break;
			case ARG_NEEDLOCAL:
				if((cg->flags & F_INCOMING) && (cg->lnr == NULL))
					DG("3LocalNrRequired");
				break;
			case ARG_NEEDREMOTE:
				if((cg->flags & F_INCOMING) && (cg->nr == NULL))
					DG("3RemoteNrRequired");
				break;

				/* Suffix fuer entfernte Nummer. */
			case ARG_NUMBER:
				{
					char yy[MAXNR + 2];

					if (m_getstr (cand, yy, MAXNR) != 0)
						break;
					if ((nrt & ARG_IN) && (cgc->flags & F_INCOMING)) {
						if (cgc->nrsuf != NULL) {
							if(0)printf("MatchSuffix %s and %s\n",cgc->nrsuf,yy);
							if(match_suffix(cgc->nrsuf,yy) <= 0) DG("3WrongNrSuffix 2");
						} else
							cgc->nrsuf = str_enter(yy);
					}
					if((nrt & ARG_OUT) &&(cgc->flags & F_OUTGOING)) {
						if (cgc->nrsuf == NULL) 
							cgc->nrsuf = str_enter(yy);
						else if(match_suffix(cgc->nrsuf,yy) <= 0)
							DG("4NrOutMatch");
						if((cgc->nr != NULL) && !(cgc->flags & F_NRCOMPLETE)) {
							char *foo = append_nr(cgc->nr,yy);
							if(1)printf("Append1 %s,%s -> %s\n",cgc->nr,yy,foo);
							cgc->nr = foo;
							if(cgc->nr != NULL) {
								if(0)printf("Strip1 %s -> %s\n",cg->nr,strip_nr(cg->nr,0));
								if(strip_nr(cgc->nr,0) != NULL)
									cgc->flags |= F_NRCOMPLETE;
							} else DG("3WrongNrSuffix 1");
						}
					}
				}
				break;

				/* Suffix fuer lokale Nummer. */
			case ARG_LNUMBER:
				{
					char yy[MAXNR + 2];
					int suf;

					if (m_getstr (cand, yy, MAXNR) != 0)
						break;
					if ((nrt & ARG_IN) && (cgc->flags & F_INCOMING)) {
						if(cgc->lnrsuf != NULL) {
							if(0)printf("MatchLSuffix %s and %s\n",cgc->lnrsuf,yy);
							if((suf = match_suffix(cgc->lnrsuf,yy)) <= 0) 
								DG(suf ? "1LNrIncompSuffix 2" : "2WrongLNrSuffix 2");
						} else
							DG("4LNrIncompSuffix 3");
					}
					if((nrt & ARG_OUT) && (cgc->flags & F_OUTGOING)) {
						if(cgc->lnrsuf == NULL)
							cgc->lnrsuf = str_enter(yy);
						else if(match_suffix(cgc->lnrsuf,yy) <= 0)
							DG("4LNrOutMatch");

						if((cgc->lnr != NULL) && !(cgc->flags & F_LNRCOMPLETE)) {
							char *foo = append_nr(cgc->lnr,yy);
							if(0)printf("Append2 %s,%s -> %s\n",cgc->lnr,yy,foo);
							cgc->lnr = foo;
							if(cgc->lnr != NULL) {
								if(0)printf("Strip2 %s -> %s\n",cg->lnr,strip_nr(cg->lnr,1));
								if(strip_nr(cgc->lnr,1) != NULL)
									cgc->flags |= F_LNRCOMPLETE;
							} else {
								if((cgc->lnrsuf != 0) && (match_suffix(cgc->lnrsuf,yy) < 0))
									DG("2LNrIncompSuffix 4");
								else
									DG("2WrongLNrSuffix 4");
							}
						}
					}
				}
				break;

				/* Protokollkrempel... */
			case ARG_LLC:
				CHKV ();
			case ARG_ULC:
				CHKV ();
			case ARG_BEARER:
				CHKV ();
			case ARG_SERVICE:
				CHK (x, ulong_t);
			case ARG_PROTOCOL:
				CHK (i, long);
			case ARG_SUBPROT:
				CHK (i, long);
			case ARG_CHANNEL:
				CHK (i, long);

				/* Flags... */
			case ARG_CHANBUSY:   cgc->flags |= F_CHANBUSY;   goto argdup;
			case ARG_FASTREDIAL: cgc->flags |= F_FASTREDIAL; goto argdup;
			case ARG_FASTDROP:   cgc->flags |= F_FASTDROP;   break;
			case ARG_IGNORELIMIT:cgc->flags |= F_IGNORELIMIT;goto argdup;
			case ARG_FORCEOUT:   cgc->flags |= F_FORCEOUT;   goto argdup;
			case ARG_BACKCALL:   cgc->flags |= F_BACKCALL;   goto argdup;
			case ARG_NOREJECT:   cgc->flags |= F_NOREJECT;   goto argdup;
			case ARG_PREFOUT:    cgc->flags |= F_PREFOUT;    goto argdup;
			case ARG_INT:        cgc->flags |= F_INTERRUPT;  break;
			case ARG_SPV:
			  argdup:
				CHKX();
			}
			/* Reset der Stringpointer. */
			if(cgc->par_in != NULL) cgc->par_in->b_rptr = mbs_in;
			if(cgc->par_out!= NULL) cgc->par_out->b_rptr= mbs_out;
		}
		/* At this point, everything fit (so far). Kill the intermediate copy. */
		dropgrab(cg);
		cg = cgc;
		cgc = NULL;
	  Ex:
		if(cand != NULL)
			freeb (cand);
		if (strchr(prot->type,'X')) /* Cut off here. */
			break;
		if(cgc != NULL) { /* If we have an intermediate copy, toss it. */
			if(cgc->par_in != NULL) cgc->par_in->b_rptr = mbs_in;
			if(cgc->par_out!= NULL) cgc->par_out->b_rptr= mbs_out;
	  		dropgrab(cgc);
		}
	}

	/* Done. Return the result. */
	dropgrab(*cgm);
	*cgm = cg;
	return NULL;
}

/* Scan all the P lines, search for a matching block of entries and incorporate into *cgm. */
char *
pmatch (conngrab *cgm)
{
	char *errstr = "8no P config entries";
	cf prot;

	chkone(*cgm);
	for (prot = cf_P; prot != NULL; prot = prot->next) {
		char *errstrx;
		if ((errstrx = pmatch1 (prot, cgm)) != NULL) {
			if(*errstrx < *errstr)
				errstr = errstrx;
			continue;
		}
		return NULL;
	}
	return errstr;
}

/* Scan the configuration, incorporate matching entries into *foo. */
char *
findsite (conngrab *foo, int ignbusy)
{
	cf dp = NULL;
	cf dl = NULL;
	cf d = NULL;
	char *errstr = "8No matching ISDN card / DL entry";
	char *errstrx;
	int numwrap;
	conngrab cg = *foo;
	conngrab errcg = NULL;

	chkone(cg);
	cg->refs++;
	for (dl = cf_DL; dl != NULL; dl = dl->next) { /* find a matching local number. */
		char *matcrd;
		char *matclass;
		ulong_t matsub;

if(0)printf("%s.%s.!.",cg->site,cg->card); /* I hate debugging. */

		if ((matclass = classmatch (cg->cclass, dl->cclass)) == NULL)
			continue;
		if ((matcrd = wildmatch (cg->card, dl->card)) == NULL)
			continue;
		if ((matsub = maskmatch (cg->mask, dl->mask)) == 0)
			continue;
		if(!(cg->flags & F_LEASED)) { /* ... and a working dial prefix. */
			char *crd;
			ulong_t sub;
			for (dp = cf_DP; dp != NULL; dp = dp->next) {
				if ((crd = wildmatch (cg->card, dp->card)) == NULL)
					continue;
				if ((sub = maskmatch (cg->mask, dp->mask)) == 0)
					continue;
				break;
			}
			if (dp == NULL) {
				errstr = "8No matching DP entry";
				continue;
			}
			matcrd = crd;
			matsub = sub;
		}

		/* Now find a site to call out to. */
		/* The numwrap stuff makes sure that we restart where we left off last
		   time, which ensures that calling a system with more than one
		   number does what it's supposed to. */
		/* Incoming calls start at the beginning and don't hit the
		   wraparound. */
		/* "numidx" holds the position of the next number to try. */
		for (d = cf_D, numwrap = ((cg->flags & F_INCOMING) == 0);
				(d != NULL) || (numwrap > 0);
				d = (numwrap ? d->next : d)) {
			char *matcla;
			char *matsit;
			char *matcar;
			char *matpro;
			ulong_t matsub;
			long matflg;

			if(d == NULL) { /* Restart at the beginning */
				numwrap = 0;
				numidx = 0;
				if((d = cf_D) == NULL)
					break; /* no D lines at all... */
			} 
			if (numwrap > 0 && numidx >= numwrap++)
				continue; /* scan and skip */
			else if(numwrap == 0)
				numwrap = -1;
			if(!(cg->flags & F_INCOMING)) 
				numidx++;

			/* Yes, we did increment the refcount, above. */
			dropgrab(cg);
			cg = *foo;
			cg->refs++;

			if((matflg = matchflag(cg->flags,d->type)) == 0) continue;
			if((matsit = wildmatch(cg->site,d->site)) == NULL) continue;
			if((matpro = wildmatch(cg->protocol,d->protocol)) == NULL) continue;
			if((matcar = wildmatch(matcrd,d->card)) == NULL) continue;
			if((matcla = classmatch(matclass,d->cclass)) == NULL) continue;
			if((matsub = maskmatch(cg->mask,d->mask)) == 0) continue;
			if(cg->d_level != d->num) {
				if((cg->d_level < d->num) && (cg->d_nextlevel < d->num))
					cg->d_nextlevel = d->num;
				continue;
			}

			/* Preliminary match OK, remember the data so far. */
			dropgrab(cg);
			cg = newgrab(*foo);
			if(cg == NULL) return "0OUT OF MEM";

			cg->site = matsit; cg->cclass = matcla;
			cg->card = matcar; cg->protocol = matpro;
			cg->mask = matsub; cg->flags = matflg;

			if(!(cg->flags & F_LEASED)) {
				/* Now figure out the numbers... */
				if(cg->nr != NULL) {
					cg->nrsuf = match_nr(cg->nr,d->arg, ((cg->flags&F_INCOMING) && (dp->args != NULL)) ? dp->args : dp->arg);
					if(cg->nrsuf == NULL) {
						if(*errstr > '8') {
							dropgrab(errcg); errcg = cg; cg->refs++;
							errstr = "8NrRemMatch";
						}
						continue;
					}
				} else if(!(cg->flags & F_INCOMING)) {
					cg->nr = build_nr(d->arg,dl->arg,((cg->flags&F_INCOMING) && (dp->args != NULL)) ? dp->args : dp->arg, 0);
					if(cg->nr == NULL) {
						if(*errstr > '8') {
							dropgrab(errcg); errcg = cg; cg->refs++;
							errstr="8RemNrMatch";
						}
						continue;
					}
				} else { /* dialin, but no number given */
					if(strcmp(cg->site,"unknown"))
						continue;
				}
				if(cg->lnr != NULL) {
					cg->lnrsuf = match_nr(cg->lnr,dl->arg, ((cg->flags&F_INCOMING) && (dp->args != NULL)) ? dp->args : dp->arg);
					if(cg->lnrsuf == NULL) {
						if(*errstr > '3') {
							dropgrab(errcg); errcg = cg; cg->refs++;
							errstr = "6NrLocMatch";
						}
						continue;
					}
				} else if(!(cg->flags & F_INCOMING)) { /* Hmmm... */
					cg->lnr = build_nr(dl->arg,dl->arg,((cg->flags&F_INCOMING) && (dp->args != NULL)) ? dp->args : dp->arg, 2);
					if(cg->lnr == NULL) {
						if(*errstr > '4') {
							dropgrab(errcg); errcg = cg; cg->refs++;
							errstr="6LocNrMatch";
						}
						continue;
					}
				}
			}

			/* Do we have a matching P line? */
			if ((errstrx = pmatch (&cg)) == NULL) {
			/* We should have what we need. Now figure out if we can use it... */
				cf cl = NULL;
				int nrbchan = 0;

				if(cg->nr != NULL && (cg->flags & (F_INCOMING|F_OUTGOING)) && !(cg->flags & F_NRCOMPLETE)) {
					if(strip_nr(cg->nr,0) != NULL)
						cg->flags |= F_NRCOMPLETE;
					else {
						if(*errstr > '3') {
							errstr = "3RemoteNr incomplete";
							errcg = cg; cg->refs++;
						}
						continue;
					}
				}
				if(cg->lnr != NULL && (cg->flags & (F_INCOMING|F_OUTGOING)) && !(cg->flags & F_LNRCOMPLETE)) {
					if(strip_nr(cg->lnr,1) != NULL)
						cg->flags |= F_LNRCOMPLETE;
					else {
						if(*errstr > '3') {
							errstr = "3LocalNr incomplete";
							errcg = cg; cg->refs++;
						}
						continue;
					}
				}

				/* Check if we know how many B channels the card has */
				{
					struct isdncard *ca;

					for(ca = isdn4_card; ca != NULL; ca = ca->next) {
						if(!strcmp(cg->card, ca->name)) {
							nrbchan = ca->nrbchan;
							break;
						}
					}
				}

				if((nrbchan > 0) && !ignbusy) {
					int nrconn = 0;
					struct conninfo *conn;
					for(conn = isdn4_conn; conn != NULL; conn = conn->next) {
						if(conn->ignore || !conn->cg)
							continue;
						if(conn->state >= c_going_up) {
							if(wildmatch(conn->cg->card, cg->card) == NULL)
								continue;
							nrconn ++;
						}
					}
					if(nrconn >= nrbchan) {
						errstr = "0BUSY";
						dropgrab(errcg); errcg = cg; cg->refs++;
						continue; /* try the next D line */
					}
				}
				/* Check if there's a limiter. Actually, there may be more
				   than one. */
				for(cl = ignbusy ? NULL : cf_CL; cl != NULL; cl = cl->next) {
					struct conninfo *conn;
					int naconn = 0;

					if(classmatch(cg->cclass,cl->cclass) == NULL)
						continue;
					if(wildmatch(cg->card, cl->card) == NULL)
						continue;
					if(wildmatch(cg->site, cl->site) == NULL)
						continue;
					if(wildmatch(cg->protocol, cl->protocol) == NULL)
						continue;
					if(maskmatch(cg->mask, cl->mask) == 0)
						continue;
					
					for(conn = isdn4_conn; conn != NULL; conn = conn->next) {
						char *prot,*sit;
						if(conn->ignore || !conn->cg)
							continue;
						if(conn->state < c_going_up) 
							continue;
						if(wildmatch(conn->cg->card, cl->card) == NULL)
							continue;
						if((prot = wildmatch(conn->cg->protocol, cl->protocol)) == NULL)
							continue;
						if((sit = wildmatch(conn->cg->site, cl->site)) == NULL)
							continue;
						if(classmatch(conn->cg->cclass, cl->cclass) == NULL)
							continue;
						if(maskmatch(conn->cg->mask,cl->mask) == 0)
							continue;
						if(conn->state == c_going_up &&
								wildmatch(sit,cg->site) != NULL &&
								wildmatch(prot,cg->protocol) != NULL)
							continue;
						if(conn->flags & F_IGNORELIMIT)
							continue;
						naconn++;
					}
					if((cl != NULL) && (naconn >= cl->num) && !(cg->flags & F_IGNORELIMIT)) {
						errstr = "0BUSY";
						dropgrab(errcg); errcg = cg; cg->refs++;
						break;
					}
				}
				if(cl == NULL) { /* checked all of them */ 
					if (cg->par_out != NULL && strchr(d->type, 'H') != NULL && !(cg->flags & F_OUTCOMPLETE))
						m_putsx (cg->par_out, ARG_SUPPRESS);
					dropgrab(errcg);
					dropgrab(*foo); *foo = cg;
					return NULL;
				}
			}
			/* No go. Remember the error, if appropriate. */
			if(*errstr > *errstrx) {
				errstr = errstrx;
				errcg = cg; cg->refs++;
			}
			/* p->b_rptr = olds; */
		}
	}
	/* Nothing matched. Grrr. */
	dropgrab(cg);
	if(errcg != NULL) {
		dropgrab(*foo);
		*foo = errcg;
	}
	return errstr;
}

/* Wrapper stuff. Take numbers out of the incoming argument vector, find
   the card, et al. */
char *
findit (conngrab *foo, int ignbusy)
{
	ushort_t id;
	mblk_t *p;
	char *errstr = "9No card known";
	char *errstrx;
	struct isdncard *c;
	conngrab cg = newgrab(*foo), cgc = NULL;
	conngrab errcg = NULL;
	int cardwrap;

	if(cg == NULL)
		return "NoMemFoo";
	cg->cclass = classmatch(cg->cclass,theclass);
	if(cg->cclass == NULL) {
		dropgrab(cg);
		return "0Not Now";
	}
	p = cg->par_in;

	if(p != NULL) {
		streamchar *olds = p->b_rptr;
		char st[MAXNR + 2];
		long x;

		while (m_getsx (p, &id) == 0) {
			switch (id) {
			case ARG_NUMBER:
				if(cg->nr == NULL) {
					m_getstr (p, st, MAXNR);
					cg->nr = str_enter(st);
				}
				break;
			case ARG_LNUMBER:
				if(cg->lnr == NULL) {
					m_getstr (p, st, MAXNR);
					cg->lnr = str_enter(st);
				}
				break;
			case ARG_CARD:
				m_getstr (p, st, 4);
				if((cg->card = wildmatch(str_enter(st),cg->card)) == NULL) {
					dropgrab(cg);
					return "0CARD MISMATCH";
				}
				break;
			case ARG_SUBCARD:
				if((m_geti(p,&x) == 0) && (x > 0))
					cg->mask = 1<<(x-1);
				break;
			}
		}
		p->b_rptr = olds;
		if(cg->site == NULL && cg->nr == NULL)
			cg->site = "unknown";
	}

	for(c = isdn4_card, cardwrap = ((cg->flags & F_INCOMING) == 0);
		(c != NULL) || (cardwrap > 0);
			c = (cardwrap ? c->next : c)) {
		if(c == NULL) { /* Restart at the beginning */
			cardwrap = 0;
			cardidx = 0;
			if((c = isdn4_card) == NULL)
				break; /* no D lines at all... */
		} 
		if (cardwrap > 0 && cardidx >= cardwrap++)
			continue; /* scan and skip */
		else if(cardwrap == 0)
			cardwrap = -1;
		if(!(cg->flags & F_INCOMING)) 
			cardidx++;

		cg->mask = 1; /* XXX TODO: enhance for multiple interfaces */
		dropgrab(cgc);
		cgc = newgrab(cg);
		if(cgc == NULL) return "0NoMemFind";

		cgc->card = c->name;
		if ((errstrx = findsite (&cgc,ignbusy)) == NULL) { /* Found it */
			cf crd;
			cgc->flags |= F_OUTCOMPLETE;
			if(c->cap & CHM_INTELLIGENT) {
				dropgrab(*foo); dropgrab(cg);
				*foo = cgc;
				return NULL;
			}
			for (crd = cf_CM; crd != NULL; crd = crd->next) {
				if (!wildmatch (c->name, crd->card))
					continue;
				dropgrab(*foo); dropgrab(cg);
				*foo = cgc;
				return NULL;
			}
			errstrx = "0CM line missing";
		}
		if(*errstrx < *errstr) {
			errstr = errstrx;
			dropgrab(errcg); errcg = cgc;
			errcg->refs++;
		}
	}
	
	if(errcg != NULL) {
		dropgrab(*foo);
		*foo = errcg;
	}
	dropgrab(cg); dropgrab(cgc);
	return errstr;
}

