/*
 * This file is part of the ISDN master program.
 *
 * Copyright (C) 1995 Matthias Urlichs.
 * See the file COPYING for license details.
 */

#include "master.h"


/**
 ** Number String Stuff 
 **/


char *match_nr (char *extnr, char *locnr, char *locpref)
/* Vergleicht ankommende Nummern mit dem Eintrag einer D-Zeile. */
/* liefert evtl. ein ungematchtes Suffix zurück */
/* 09119599131,+911-959913/[1-3],=00+0-   ->   /[1-3] */
/* 91234, +*, =00+0-  -> "" */
/* +9119599131,+911-959913/[1-3],=00+0-   ->   /[1-3] */
/* +9119599131,+911-959923/[1-3],=00+0-   ->   NULL */
/* +9119599134,+911-959913/[1-3],=00+0-   ->   NULL */
{
	char *extpos, *locpos;
	if(isdigit(*extnr)) {
		/* finde das passende Präfix */
		while(*locpref != '\0') {
			extpos=extnr;
			if(isdigit(*locpref)) {
				locpref++;
				continue;
			}
			locpos = strchr(locnr,*locpref);
			if(locpos == NULL) {
				locpref++;
				continue;
			}
			locpref++;
			while(isdigit(*locpref) && (*extpos == *locpref) && (*extpos != '\0')) {
				extpos++; locpref++;
			}
			if(isdigit(*locpref))
				continue;
			goto DoMatch;
		}
		return NULL;
	} else {
		/* Wirf zusammenpassende Präfixe raus */
		extpos = extnr;
		locpref = strchr(locpref,*extpos);
		if(locpref == NULL) {
			if((*extpos == '/' || *extpos == '.') && ((locpos = strchr(locnr,*extpos)) != NULL)) 
				goto DoMatch;
			return NULL;
		}
		locpos = strchr(locnr,*extpos);
		if(locpos == NULL)
			return NULL;
		extpos++;
		locpos++;
	}
  DoMatch:
	/* Matche den Rest der Nummer */
	while(*extpos != '\0') {
		if(!isdigit(*locpos)) {
			if(*locpos == '*')
				return str_enter(extnr);
			if(*locpos == '.' || *locpos == '/') {
				char destnr[10], *destpos=destnr;
				if(*extpos == *locpos) {
					extpos++;
					if(*extpos == '\0')
						return "";
					if(!isdigit(*extpos)) {
						if(strcmp(extpos,locpos+1) && wildmat(extpos,locpos+1) <= 0)
							return NULL;
					}
				} else {
					if(locpos[1] != '\0' && wildmat(extpos,locpos+1) <= 0)
						return NULL;
				}
				*destpos++=*locpos;
				while(*extpos != '\0')
					*destpos++ = *extpos++;
				*destpos = '\0';
				return str_enter(destnr);
			} else {
				if(*extpos == *locpos) {
					extpos++; locpos++;
					continue;
				}
				locpos++;
				if(*locpos == '*')
					return str_enter(extnr);
			}
		}
		if(*extpos != *locpos)
			return NULL;
		extpos++; locpos++;
	}
	if(*locpos != '\0')
		return NULL;
	return "";
}


int match_suffix(char *extsuf, char *extnr)
/* Vergleicht das obige Suffix mit einem :nr-Eintrag, true wenn OK. */
/* /[1-3],/4 -> false */
/* /[1-3],/2 -> true */
/* .[1-3],/2 -> false */
{
	if(*extsuf != *extnr)
		return 0;
	if(*extsuf=='\0')
		return 1;
	extsuf++; extnr++;
	return (wildmat(extsuf,extnr));
}

char *build_nr (char *extnr, char *locnr, char *locpref, int islocal)
/* baut eine zu wählende Nummer zusammmen, bzw. deren Anfang 
 * =911-959913.[1-3],-959913.[1-3],+00=0-,0  -> .[1-3]
 * =911-959913/[1-3],=911-959913/[1-3],+00=0-,0  -> 959913/[1-3]
 * =911-959913/[1-3],=911-959913/[1-3],+00=0-,1  -> /[1-3]
 * =911-123456/[1-3],=911-959913/[1-3],+00=0-,x  -> 123456/[1-3]
 * =9131-123456/[1-3],=911-959913/[1-3],+00=0-,x  -> 09131123456/[1-3]
 */
{
	char destnr[40];
	char *destpos=destnr;

	char *locpos,*extpos,*prefpos=locpref,*lastprefpos=NULL;
	while(*prefpos != '\0') {
		if(isdigit(*prefpos)) {
			prefpos++;
			continue;
		}
		locpos=strchr(locnr,*prefpos);
		extpos=strchr(extnr,*prefpos);
		if(locpos == NULL || extpos == NULL) {
			prefpos++;
			continue;
		}
		lastprefpos = prefpos;
		locpos++; extpos++;
		while(*locpos != '\0' && *locpos == *extpos && isdigit(*locpos)) {
			locpos++; extpos++;
		}
		if(*locpos != *extpos)
			break;
		prefpos++;
	}
	if(lastprefpos==NULL)
		return NULL;
	if(*prefpos == '\0' && (islocal & 1)) {
		char *xextpos = strchr(extnr,'/');
		char *xlocpos = strchr(locnr,'/');
		if(xextpos != NULL && xlocpos != NULL && 
				(!strcmp(xextpos,xlocpos) || wildmatch(xextpos+1,xlocpos+1) > 0))
			lastprefpos="/";
	}
	
	locpos=strchr(locnr,*lastprefpos);
	extpos=strchr(extnr,*lastprefpos);
	if(islocal & 2)
		*destpos++ = *lastprefpos++;
	else
		lastprefpos++;
	while(*lastprefpos != '\0' && isdigit(*lastprefpos)) {
		*destpos++ = *lastprefpos;
		lastprefpos++;
	}
	while(*extpos != '\0' && *extpos != '/' && *extpos != '.') {
		if(*extpos != '+' && *extpos != '=' && *extpos != '-')
			*destpos++ = *extpos;
		extpos++;
	}
	while(*extpos != '\0') {
		*destpos++ = *extpos++;
	}

	*destpos='\0';
	return str_enter(destnr);
}

char *append_nr(char *extnr, char *extext)
/* erweitert eine Nummer um ein Suffix */
/* 12345.[78],.6 -> NULL */
/* 12345.[67],.6 -> 123456 */
/* 12345.,.6 -> 123456 */
{
	char destnr[40];
	char *destpos = destnr;
	char *extpos;

	if(extnr == NULL)
		return NULL;
	if((extpos = strchr(extnr,*extext)) == NULL)
		return NULL;
	if(extpos[1] != '\0' && wildmatch(extpos,extext) <= 0)
		return NULL;
	while(extnr != extpos)
		*destpos++ = *extnr++;
	extext++;
	while(*extext != '\0' && !isspace(*extext))
		*destpos++ = *extext++;
	*destpos='\0';
	return str_enter(destnr);
}

char *strip_nr(char *extnr, int keepfirst)
/* entfernt die Spezialzeichen aus einer vollständigen Nummer,
   zwecks Dialout; NULL wenn die Nummer unvollständig ist */
/* 123.45 -> 12345 */
/* 123.[45] -> NULL */
/* 123. -> NULL */
{
	char destnr[40];
	char *destpos = destnr;
	int lastspc=1;

	if(keepfirst && (*extnr != '\0'))
		*destpos++ = *extnr++;
	while(*extnr != '\0') {
		if(isdigit(*extnr)) {
			lastspc=0;
			*destpos++ = *extnr;
		} else if(*extnr == '[' || *extnr == ']' || *extnr == '?' || *extnr ==
				'*')
			return NULL;
		else
			lastspc=1;
		extnr++;
	}
	if(lastspc)
		return NULL;
	*destpos='\0';
	return str_enter(destnr);
}

#ifdef TESTING

char *nam;
void pri(char *x) {
	if(x==NULL)
		printf("(NULL) ");
	else
		printf("'%s' ",x);
}
void strt(char *a,char *z) {
	pri("strip_nr");
	pri(a);
	pri("->");
	pri(strip_nr(a));
	pri("||");
	pri(z);
	printf("\n");
}
void appt(char *a,char *b,char *z) {
	pri("append_nr");
	pri(a);
	pri(b);
	pri("->");
	pri(append_nr(a,b));
	pri("||");
	pri(z);
	printf("\n");
}
void matn(char *a,char *b,char *c,char *z) {
	pri("match_nr");
	pri(a); pri(b); pri(c);
	pri("->");
	pri(match_nr(a,b,c));
	pri("||");
	pri(z);
	printf("\n");
}
void buit(char *a,char *b,char *c,int d,char *z) {
	pri("build_nr");
	pri(a); pri(b); pri(c); pri(d?"true":"false");
	pri("->");
	pri(build_nr(a,b,c,d));
	pri("||");
	pri(z);
	printf("\n");
}
void matt(char *a,char *b,int z) {
	pri("match_suffix");
	pri(a); pri(b);
	pri("->");
	pri(match_suffix(a,b)?"true":"false");
	pri("||");
	pri(z?"true":"false");
	printf("\n");
}

main()
{
matn("09119599131","+911-959913/[1-3]","=00+0-","/1");
matn("09119599131","+911-959913/1","=00+0-","");
matn("09119599131","+911-9599131","=00+0-","");
matn("+9119599131","+911-959913/[1-3]","=00+0-","/1");
matn("+9119599131","+911-959923/[1-3]","=00+0-","NULL");
matn("+9119599134","+911-959913/[1-3]","=00+0-","NULL");
matt("/[1-3]","/4",0);
matt("/[1-3]","/2",1);
matt(".[1-3]","/2",0);
buit("=911-959913.[1-3]","-959913.[1-3]","+00=0-",0,"959913.[1-3]");
buit("=911-959913.[1-3]","-959913.[1-3]","+000=00-0.",0,".[1-3]");
buit("=911-959913/[1-3]","=911-959913/[1-3]","+00=0-",0,"959913/[1-3]");
buit("=911-959913/[1-3]","=911-959913/[1-3]","+00=0-",1,"/[1-3]");
buit("=911-123456/[1-3]","=911-959913/[1-3]","+00=0-",1,"123456/[1-3]");
buit("=9131-123456/[1-3]","=911-959913/[1-3]","+00=0-",0,"09131123456/[1-3]");
appt("12345.[78]",".6","NULL");
appt("12345.[67]",".6","123456");
appt("12345.",".6","123456");
strt("123.45","12345");
strt("123.[45]","NULL");
strt("123.","NULL");
}
#endif

