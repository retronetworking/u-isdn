/*
 * This file is part of the ISDN master program.
 *
 * Copyright (C) 1995 Matthias Urlichs.
 * See the file COPYING for license details.
 */

#include "master.h"



int
matchflag(long flags, char *ts)
{
	char inc,outg,leas,prep,dial,ini,aft;

	ini = (strchr(ts,'u') != NULL);
	aft = (strchr(ts,'a') != NULL);
	inc = (strchr(ts,'i') != NULL);
	outg= (strchr(ts,'o') != NULL);
	leas= (strchr(ts,'f') != NULL);
	prep= (strchr(ts,'p') != NULL);
	dial= (strchr(ts,'d') != NULL);

	if(flags & F_SETINITIAL) { if (aft && !ini) return 0; }
	if(flags & F_SETLATER)   { if (!aft && ini) return 0; }

	if(flags & F_OUTGOING) { if (inc && !outg) return 0; }
	if(flags & F_INCOMING) { if (!inc && outg) return 0; }

	if(!(flags & (F_LEASED|F_PERMANENT|F_DIALUP))) return 1;
	if(!(leas || dial || prep)) return 1;
	if((flags & F_LEASED)   && leas) return 1;
	if((flags & F_PERMANENT)&& prep) return 1;
	if((flags & F_DIALUP)   && dial) return 1;
	return 0;
}


char *
pmatch1 (cf prot, conngrab *cgm)
{
	char *sit, *pro, *cla, *car;
	char first = 1;
	conngrab cg = *cgm;

	chkone(prot); chkone(cg);
	sit = wildmatch(cg->site,    prot->site);    if(sit == NULL) return "7ERR Match SITE";
	pro = wildmatch(cg->protocol,prot->protocol);if(pro == NULL) return "6ERR Match PROTOCOL";
	car = wildmatch(cg->card,    prot->card);    if(car == NULL) return "6ERR Match CARD";
	cla =classmatch(cg->cclass,  prot->cclass);  if(cla == NULL) return "6ERR Match CLASS";

	cg = newgrab(cg);
	if(cg == NULL)
		return "0OUT OF MEMORY";
	cg->site = sit; cg->protocol = pro; cg->card = car; cg->cclass = cla;

	for (first = 1; prot != NULL; prot = prot->next, first = 0) {
#define ARG_IN 01
#define ARG_OUT 02
		char nrt = ARG_IN|ARG_OUT;
		ushort_t id;
		conngrab cgc = NULL;
		mblk_t *cand = NULL;
		streamchar *mbs_in = NULL, *mbs_out = NULL;

		if(!matchflag(cg->flags,prot->type)) { if(first) { dropgrab(cg); return "5ERR BadFlag"; } else continue;}
		if (first) {
			if (strchr (prot->type, 'M')) {
				dropgrab(cg);
				return "7ERR FIND NotFirst";
			}
		} else {
			if (strchr (prot->type, 'R')) 
				goto Ex;

			sit = wildmatch(cg->site,    prot->site);    if(sit==NULL) continue;
			pro = wildmatch(cg->protocol,prot->protocol);if(pro==NULL) continue;
			car = wildmatch(cg->card,    prot->card);    if(car==NULL) continue;
			cla =classmatch(cg->cclass,  prot->cclass);  if(cla==NULL) continue;
		}
		cgc = newgrab(cg);
		if(cgc == NULL) {
			dropgrab(cg);
			return "0OUT OF MEMORY";
		}
		if(!first) {
			cgc->site = sit; cgc->protocol = pro; cgc->card = car; cgc->cclass = cla;
		}
		if(cgc->par_out == NULL) {
			if ((cgc->par_out = allocb(256,BPRI_LO)) == NULL) {
				dropgrab(cgc); dropgrab(cg);
				return "0OUT of MEMORY";
			}
		}
#define DG(str) { if(first) { Xbreak(); dropgrab(cgc); dropgrab(cg); return str; } goto Ex; }

		mbs_in = ((cgc->par_in !=NULL)? cgc->par_in->b_rptr : NULL);
		mbs_out= ((cgc->par_out!=NULL)? cgc->par_out->b_rptr: NULL);
			
		cand = allocsb (strlen (prot->args), (streamchar *)prot->args);
		if(cand == NULL)
			goto Ex;
		while (m_getsx (cand, &id) == 0) {

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
							if(1)printf("MatchI %x: %lx vs %lx\n",id,xx,yy);	\
							dropgrab(cgc); dropgrab(cg); return "3ERR FIND Match Arg";		\
						}													\
					}														\
				ex:; }) 														/**/
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
							if(1)printf("MatchO %x: %lx vs %lx\n",id,xx,yy);	\
							dropgrab(cgc); dropgrab(cg); return "3ERR FIND Match Arg";		\
						}													\
						if(!(cgc->flags & F_OUTCOMPLETE)) {					\
							m_putsx(cgc->par_out,id);						\
							m_put##_what(cgc->par_out,*(_t *)&yy);			\
						}													\
					}														\
				ex:; })														/**/
#define CHK(_what,_t) { \
			if((nrt & ARG_OUT)&& (cgc->par_out!= NULL)) CHKO(_what,_t); \
			if((nrt & ARG_IN) && (cgc->par_in != NULL)) CHKI(_what,_t); } break

#define CHKVI()																\
	({ __label__ ex; int xx,yy,xm; streamchar *vx,*vy,*vm; ushort_t id2;		\
		yy = m_gethexlen(cand);												\
		if (yy <= 0 || (vy=malloc(yy))==NULL) break;						\
		if(m_gethex(cand,vy,yy) != 0) { free(vy); break; }					\
		if ((xm = m_gethexlen(cand)) > 0) {									\
			if ((vm=malloc(xm)) == NULL)									\
				{ free(vy); break; }										\
			if(m_gethex(cand,vm,xm) != 0)									\
				{ free(vy); free(vm); break; }								\
		} else																\
			{ vm=NULL; xm=0; }												\
		if(cgc->par_in != NULL) {											\
			while(m_getsx(cgc->par_in,&id2) == 0) {							\
				if(id != id2) continue;										\
				xx = m_gethexlen(cgc->par_in);								\
				if (xx <= 0 || (vx=malloc(xx))==NULL) break;				\
				if(m_gethex(cgc->par_in,vx,xx) != 0)						\
					{ free(vx); break; }									\
				if(abs(vectcmp(vx,xx,vy,yy,vm,xm)) < 5)						\
					{ free(vx); free(vy); if(xm>0)free(vm); goto ex; }		\
				if(0)if(!strchr(prot->type,'F') && !first) continue;		\
				cgc->par_in->b_rptr = mbs_in;								\
				freeb(cand);												\
				if(1)printf("MatchVI %x: %s vs %s\n",id,vx,vy);				\
				free(vx); free(vy); if(xm>0)free(vm);						\
				dropgrab(cgc); dropgrab(cg); return "3ERR FIND Match Arg";	\
			}																\
			free(vy); if(xm>0)free(vm);										\
		}																	\
	ex:; })				 														/**/
#define CHKVO()																\
	({ __label__ ex; int xx,yy,xm; streamchar *vx,*vy,*vm; ushort_t id2;		\
		yy = m_gethexlen(cand);												\
		if (yy <= 0 || (vy=malloc(yy))==NULL) break;						\
		if(m_gethex(cand,vy,yy) != 0) { free(vy); break; }					\
		if ((xm = m_gethexlen(cand)) > 0) {									\
			if ((vm=malloc(xm)) == NULL)									\
				{ free(vy); break; }										\
			if(m_gethex(cand,vm,xm) != 0)									\
				{ free(vy); free(vm); break; }								\
		} else																\
			{ vm=NULL; xm=0; }												\
		if(cgc->par_out != NULL) {											\
			while(m_getsx(cgc->par_out,&id2) == 0) {						\
				if(id != id2) continue;										\
				xx = m_gethexlen(cgc->par_out);								\
				if (xx <= 0 || (vx=malloc(xx))==NULL) break;				\
				if(m_gethex(cgc->par_out,vx,xx) != 0)						\
					{ free(vx); break; }									\
				if(abs(vectcmp(vx,xx,vy,yy,vm,xm)) < 5)						\
					{ free(vx); free(vy); if(xm>0)free(vm); goto ex; }		\
				if(0)if(!strchr(prot->type,'F') && !first) continue;		\
				cgc->par_out->b_rptr = mbs_out;								\
				if(1)printf("MatchVO %x: %s vs %s\n",id,vx,vy);				\
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
				if((nrt & ARG_OUT)&& (cgc->par_out!= NULL)) CHKVO(); \
				if((nrt & ARG_IN) && (cgc->par_in != NULL)) CHKVI(); } break
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
			case ARG_INNUMS  : nrt=ARG_IN;  break;
			case ARG_OUTNUMS : nrt=ARG_OUT; break;
			case ARG_BOTHNUMS: nrt=ARG_IN|ARG_OUT; break;
			case ARG_NEEDNOLOCAL:
				if(cg->lnr != NULL)
					DG("3LocalNrGiven");
				break;
			case ARG_NEEDNOREMOTE:
				if(cg->nr != NULL)
					DG("3RemoteNrGiven");
				break;
			case ARG_NEEDLOCAL:
				if(cg->lnr == NULL)
					DG("3LocalNrRequired");
				break;
			case ARG_NEEDREMOTE:
				if(cg->nr == NULL)
					DG("3RemoteNrRequired");
				break;
			case ARG_NUMBER:
				{
					char yy[MAXNR + 2];

					if (m_getstr (cand, yy, MAXNR) != 0)
						break;
					if ((nrt & ARG_IN) && (cgc->nrsuf != NULL)) {
						if(0)printf("MatchSuffix %s and %s\n",cgc->nrsuf,yy);
						if(!match_suffix(cgc->nrsuf,yy)) { if(cgc->flags & F_OUTGOING) { printf("  SuffixBadness  "); Xbreak(); } else DG("2WrongNrSuffix 2") }
					} else if((nrt & ARG_OUT) && (cgc->nrsuf == NULL))
						cgc->nrsuf = str_enter(yy);
					if((cgc->nr != NULL) && (nrt & ARG_OUT) && !(cgc->flags & F_NRCOMPLETE)) {
						char *foo = append_nr(cgc->nr,yy);
						if(0)printf("Append1 %s,%s -> %s\n",cgc->nr,yy,foo);
						cgc->nr = foo;
						if(cgc->nr != NULL) {
							if(0)printf("Strip1 %s -> %s\n",cg->nr,strip_nr(cg->nr));
							if(strip_nr(cgc->nr) != NULL)
								cgc->flags |= F_NRCOMPLETE;
						} else { if(cgc->flags & F_OUTGOING) { printf("  SuffixBadness2  "); Xbreak(); } else DG("3WrongNrSuffix 1") }
					}
				}
				break;
			case ARG_LNUMBER:
				{
					char yy[MAXNR + 2];

					if (m_getstr (cand, yy, MAXNR) != 0)
						break;
					if ((nrt & ARG_IN) && (cgc->lnrsuf != NULL)) {
						if(0)printf("MatchLSuffix %s and %s\n",cgc->lnrsuf,yy);
						if(!match_suffix(cgc->lnrsuf,yy)) { if(cgc->flags & F_OUTGOING) { printf("  SuffixBadness3  "); Xbreak(); } else DG("3WrongLNrSuffix 2") }
					} else if((nrt & ARG_OUT) && (cgc->lnrsuf == NULL))
						cgc->lnrsuf = str_enter(yy);
					if((cgc->lnr != NULL) && (nrt & ARG_OUT) && !(cgc->flags & F_LNRCOMPLETE)) {
						char *foo = append_nr(cgc->lnr,yy);
						if(0)printf("Append2 %s,%s -> %s\n",cgc->lnr,yy,foo);
						cgc->lnr = foo;
						if(cgc->lnr != NULL) {
							if(0)printf("Strip2 %s -> %s\n",cg->lnr,strip_nr(cg->lnr));
							if(strip_nr(cgc->lnr) != NULL)
								cgc->flags |= F_LNRCOMPLETE;
						} else { if(cgc->flags & F_OUTGOING) { printf("  SuffixBadness4  "); Xbreak(); } else DG("3WrongLNrSuffix 1") }
					}
				}
				break;
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
			case ARG_FASTREDIAL: cgc->flags |= F_FASTREDIAL; goto argdup;
			case ARG_FASTDROP:   cgc->flags |= F_FASTDROP;   break;
			case ARG_IGNORELIMIT:cgc->flags |= F_IGNORELIMIT;goto argdup;
			case ARG_FORCEOUT:   cgc->flags |= F_FORCEOUT;   goto argdup;
			case ARG_BACKCALL:   cgc->flags |= F_BACKCALL;   goto argdup;
			case ARG_NOREJECT:   cgc->flags |= F_NOREJECT;   goto argdup;
			case ARG_PREFOUT:    cgc->flags |= F_PREFOUT;    goto argdup;
			case ARG_INT:        cgc->flags |= F_INTERRUPT;  break;
			case ARG_SPV:
			case ARG_FORCETALK:
			  argdup:
				CHKX();
			}
			if(cgc->par_in != NULL) cgc->par_in->b_rptr = mbs_in;
			if(cgc->par_out!= NULL) cgc->par_out->b_rptr= mbs_out;
		}
		dropgrab(cg);
		cg = cgc;
		cgc = NULL;
	  Ex:
		if(cand != NULL)
			freeb (cand);
		if (strchr(prot->type,'X'))
			break;
		if(cgc != NULL) {
			if(cgc->par_in != NULL) cgc->par_in->b_rptr = mbs_in;
			if(cgc->par_out!= NULL) cgc->par_out->b_rptr= mbs_out;
	  		dropgrab(cgc);
		}
	}
	dropgrab(*cgm);
	*cgm = cg;
	return NULL;
}

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


char *
findsite (conngrab *foo)
{
	cf dp = NULL;
	cf dl = NULL;
	cf d = NULL;
	char *errstr = "8ERR FIND";
	char *errstrx;
	int numwrap = 1;
	conngrab cg = *foo;
	conngrab errcg = NULL;

	chkone(cg);
	cg->refs++;
	for (dl = cf_DL; dl != NULL; dl = dl->next) {
		char *matcrd;

if(0)printf("%s.%s.!.",cg->site,cg->card);
		if ((matcrd = wildmatch (cg->card, dl->card)) == NULL)
			continue;
		if(!(cg->flags & F_LEASED)) {
			char *crd;
			for (dp = cf_DP; dp != NULL; dp = dp->next) {
				if ((crd = wildmatch (cg->card, dp->card)) != NULL)
					break;
			}
			if (dp == NULL) {
				errstr = "9CARD UNKNOWN";
				continue;
			}
			matcrd = crd;
		}
		for (d = cf_D, numwrap = 1; d != NULL || numwrap > 0; d = (numwrap ? d->next : d)) {
			char *matcla;
			char *matsit;
			char *matcar;
			char *matpro;

			if(d == NULL) {
				numwrap = 0;
				numidx = 0;
				d = cf_D;
			} 
			if (numwrap > 0 && numidx >= numwrap++)
				continue;
			else if(numwrap == 0)
				numwrap = -1;
				numidx++;

			dropgrab(cg);
			cg = *foo;
			cg->refs++;

			if((matsit = wildmatch(cg->site,d->site)) == NULL) continue;
			if((matpro = wildmatch(cg->protocol,d->protocol)) == NULL) continue;
			if((matcar = wildmatch(matcrd,d->card)) == NULL) continue;
			if((matcla = classmatch(cg->cclass,d->cclass)) == NULL) continue;
			if(!matchflag(cg->flags,d->type)) continue;

			dropgrab(cg);
			cg = newgrab(*foo);
			if(cg == NULL) return "0OUT OF MEM";

			cg->site = matsit; cg->cclass = matcla;
			cg->card = matcar; cg->protocol = matpro;
			if(0)printf("%s...",matsit);

			if(!(cg->flags & F_LEASED)) {
				if(cg->nr != NULL) {
					cg->nrsuf = match_nr(cg->nr,d->arg, ((cg->flags&F_INCOMING) && (dp->args != NULL)) ? dp->args : dp->arg);
					if(0)printf("Match %s,%s,%s -> %s\n",cg->nr,d->arg, ((cg->flags&F_INCOMING) && (dp->args != NULL)) ? dp->args : dp->arg, cg->nrsuf);
					if(cg->nrsuf == NULL) {
						if(*errstr > '8') {
							dropgrab(errcg); errcg = cg; cg->refs++;
							errstr = "8NrRemMatch";
						}
						continue;
					}
				} else if(!(cg->flags & F_INCOMING)) {
					cg->nr = build_nr(d->arg,dl->arg,((cg->flags&F_INCOMING) && (dp->args != NULL)) ? dp->args : dp->arg, 0);
					if(0)printf("Build %s,%s,%s,%d -> %s\n",d->arg,dl->arg,((cg->flags&F_INCOMING) && (dp->args != NULL)) ? dp->args : dp->arg, 0, cg->nr);
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
					if(0)printf("MatchL %s,%s,%s -> %s\n",cg->lnr,dl->arg, ((cg->flags&F_INCOMING) && (dp->args != NULL)) ? dp->args : dp->arg, cg->lnrsuf);
					if(cg->lnrsuf == NULL) {
						if(*errstr > '3') {
							dropgrab(errcg); errcg = cg; cg->refs++;
							errstr = "6NrLocMatch";
						}
						continue;
					}
				} else if(0) { /* Hmmm... */
					cg->lnr = build_nr(dl->arg,dl->arg,((cg->flags&F_INCOMING) && (dp->args != NULL)) ? dp->args : dp->arg, 0);
					if(0)printf("BuildL %s,%s,%s,%d -> %s\n",dl->arg,dl->arg,((cg->flags&F_INCOMING) && (dp->args != NULL)) ? dp->args : dp->arg, 0, cg->lnr);
					if(cg->lnr == NULL) {
						if(*errstr > '4') {
							dropgrab(errcg); errcg = cg; cg->refs++;
							errstr="6LocNrMatch";
						}
						continue;
					}
				}
			}

			if ((errstrx = pmatch (&cg)) == NULL) {
				goto gotit;
			}
			if(*errstr > *errstrx) {
				errstr = errstrx;
				errcg = cg; cg->refs++;
			}
			/* p->b_rptr = olds; */
		}
		dropgrab(cg);
		if(errcg != NULL) {
			dropgrab(*foo); *foo = errcg;
		}
		if(errstr != NULL)
			printf("A>%s; ",errstr);
		return errstr;

	gotit:
		{
				struct conninfo *conn;
				cf cl = NULL;
				int nrconn = 0, naconn = 0;
				int nrbchan = 0;

				for(cl = cf_CL; cl != NULL; cl = cl->next) {
					if(wildmatch(cg->card, cl->card))
						break;
				}
				{
				int ci;
				for(ci=0; ci < cardnum; ci++) {
					if(!strcmp(cg->card, cardlist[ci])) {
						nrbchan = cardnrbchan[ci];
							break;
						}
					}
				}
			if(cl != NULL) {
printf("Limit for %s:%d:%d %s:%s:%s %s\n",cg->card,cl->num,nrbchan,cg->site,cg->protocol,cg->cclass,cg->nr ? cg->nr : "-");
				for(conn = theconn; conn != NULL; conn = conn->next) {
					if(conn->ignore || !conn->cg)
						continue;
					if(wildmatch(conn->cg->site,cg->site) &&
							wildmatch(conn->cg->protocol,cg->protocol))
						continue;
					if((conn->state >= c_going_up) && wildmatch(conn->cg->card, cg->card)) {
printf("Share line with %s:%d:%d %s:%s:%s %s\n",conn->cg->card,cl->num,nrbchan,conn->cg->site,conn->cg->protocol,conn->cg->cclass,conn->cg->nr ? conn->cg->nr : "-");
						nrconn ++;
						if(!(conn->flags & F_IGNORELIMIT))
							naconn++;
					}
				}
				if(((nrbchan > 0) && (nrconn >= nrbchan)) || ((naconn >= cl->num) && !(cg->flags & F_IGNORELIMIT))) {
printf("BUSY: nrb %d, nrc %d, nac %d, num %d, flag %o ::",nrbchan,nrconn,naconn,cl->num,cg->flags);
					errstr = "0BUSY";
					dropgrab(errcg); errcg = cg; cg->refs++;
					continue;
				}
			}
				if (cg->par_out != NULL && strchr(d->type, 'H') != NULL && !(cg->flags & F_OUTCOMPLETE))
					m_putsx (cg->par_out, ARG_SUPPRESS);
				dropgrab(errcg);
				dropgrab(*foo); *foo = cg;
				return NULL;
			}
		}
	dropgrab(cg);
	if(errcg != NULL) {
		dropgrab(*foo);
		*foo = errcg;
	}
	if(errstr != NULL)
		printf("B>%s; ",errstr);
	return errstr;
}

char *
findit (conngrab *foo)
{
	ushort_t id;
	mblk_t *p;
	char *errstr = "9NO CARD";
	char *errstrx;
	short c;
	char *card;
	conngrab cg = newgrab(*foo);
	conngrab errcg = NULL;
	int cardlim;

	if(cg == NULL)
		return "NoMemFoo";
	p = cg->par_in;
	card = cg->card;

	if(p != NULL) {
		streamchar *olds = p->b_rptr;
		char st[MAXNR + 2];
		char *card;

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
				if((card = wildmatch(st,cg->card)) == NULL) {
					dropgrab(cg);
					return "CARD MISMATCH";
				}
				break;
			}
		}
		p->b_rptr = olds;
		if(cg->site == NULL && cg->nr == NULL)
			cg->site = str_enter("unknown");
	}

	cardlim = cardnum+cardidx;
	for (c = cardidx; c < cardlim; c++) {
			cf crd;
		if(!wildmatch(card,cardlist[c % cardnum]))
			continue;
		cg->card = cardlist[c % cardnum];
		if(cg->flags & F_INCOMING)
			numidx = 1;
		if ((errstrx = findsite (&cg)) == NULL) {
			cardidx = (c+1)%cardnum;
				dropgrab(*foo);
				*foo = cg;
				cg->flags |= F_OUTCOMPLETE;
				for (crd = cf_CM; crd != NULL; crd = crd->next) {
				if (!wildmatch (cardlist[c % cardnum], crd->card))
						continue;
					return NULL;
				}
			errstrx = "0CARDMATCH";
			}
			if(*errstrx < *errstr) {
				errstr = errstrx;
				dropgrab(errcg); errcg = cg;
				errcg->refs++;
			}
		}
	if(errcg != NULL) {
		dropgrab(*foo);
		*foo = errcg;
	}
	dropgrab(cg);
	return errstr;
}
