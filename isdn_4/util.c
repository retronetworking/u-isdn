/*
 * This file is part of the ISDN master program.
 *
 * Copyright (C) 1995 Matthias Urlichs.
 * See the file COPYING for license details.
 */

#include "master.h"

/* Too many strings to keep track of, no time for garbage collection. */
/* Enter them in a binary tree... */
/* str_enter MUST NOT be called while any string in the tree is temporarily
   modified. No string in the tree may be permanently modified in ANY way. */
char *str_enter(char *master)
{
	struct string **str = &stringdb;
	struct string *st = *str;

	if(master == NULL)
		return NULL;
	while(st != NULL) {
		int sc;
		if(st->data == master)
			return master;
		sc = strcmp(master,st->data);
		if(sc == 0)
			return st->data;
		else if(sc < 0)
			str = &st->left;
		else
			str = &st->right;
		st = *str;
	}
	st = malloc(sizeof(struct string)+strlen(master));
	if(st == NULL)
		return NULL;

	strcpy(st->data,master);
	st->left = st->right = NULL;
	*str = st;
	chkone(st);
	return st->data;
}

/* Simpleminded, bidirectional wildmat().
   If both strings are patterns, this doesn't work. Unfortunately, pattern
   joining is obviously NP-complete, and I don't know any algorithms to do it
   anyway. */
char *wildmatch(char *a, char *b)
{
	if(a == NULL)
		return b;
	else if(b == NULL)
		return a;
	else if(wildmat(a,b))
		return a;
	else if(wildmat(b,a))
		return b;
	else
		return NULL;
}

/* Given a string aaa+bbb, return aaa. */
char *
strippat(char *a)
{
	char aa[30];
	char *aplus = strchr(a,'+');

	if(aplus == NULL)
		return a;
	*aplus = '\0';
	strcpy(aa,a);
	*aplus = '+';
	return str_enter(aa);
}

/* Given strings +aaa+bbb and +ccc+aaa, return +aaa+bbb+ccc. */
static char *
pluscat(char *a, char *b)
{
	if(a == NULL)
		return b;
	if(b == NULL)
		return a;
	if(*a != '+')
		return b;
	if(*b != '+')
		return a;
	{
		char classpat[50];
		char *classind = classpat;
		strcpy(classpat,a);
		while(*classind) {
			if(*classind == '+')
				*classind = '\0';
			classind++;
		}
		{
			char *bnext = b;
			do {
				b = bnext + 1;
				bnext = strchr(b,'+');
				if(bnext != NULL) *bnext = '\0';
				for(a = classpat+1; a < classind; a += strlen(a)+1)
					if(strcmp(a,b) == 0)
						break;
				if(a >= classind) {
					strcpy(classind+1,b);
					classind += strlen(b)+1;
				}
				if(bnext != NULL) *bnext = '+';
			} while(bnext != NULL);
			for(a = classpat; a < classind; a += strlen(a))
				*a++ = '+';
			*a = '\0';
			return str_enter(classpat);
		}
	}
}

/* "abc" "bcd" -> "bc" */
/* "abc+bc" "bcd+b" -> "bc+bc+b" */
/* "abc+bc" "bcd+d" -> NULL */
/* It is of course possible to optimize all of this, but it's not worth the
   work. */
char *
classmatch(char *a, char *b)
{
	if(a == NULL)
		return NULL;
	if(b == NULL)
		return NULL;
	if(*b == '*' || *b == '\0')
		return a;
	if(*a == '*' || *a == '\0')
		return b;
	if(*a == '+' || *b == '+') {
		if(*a == '+' && *b == '+') {
			char classpat[50];
			if(strcmp(a,b) == 0)
				return a;
			strcpy(classpat,a);
			strcat(classpat,b);
			return str_enter(classpat);
		} else {
			char *aorig,*bplus;
			if(*a != '+') {
				char *tmp = a; a = b; b = tmp;
			}
			if((bplus=strchr(b,'+')) != NULL) *bplus='\0';
			aorig = a;
			if(strlen(a) == 2) { /* Obvious optimization */
				a = classmatch(b,a+1);
				if(bplus != NULL) *bplus = '+';
				return a;
			}
			do {
				while(*++a != '\0' && *a != '+') {
					if(strchr(b,*a) != NULL)
						break;
				}
				if(*a == '\0' || *a == '+') {
					if(bplus != NULL) *bplus='+';
					return NULL;
				}
			} while((a = strchr(a,'+')) != NULL);
			{
				char classpat[50];
				strcpy(classpat,b);
				if(bplus != NULL)
					*bplus='+';
				strcat(classpat,pluscat(aorig,bplus));
				return str_enter(classpat);
			}
		}
	} else {
		char classpat[30];
		char *classind = classpat;
		char *aplus, *bplus;
		if((aplus=strchr(a,'+')) != NULL) *aplus='\0';
		if((bplus=strchr(b,'+')) != NULL) *bplus='\0';
		while(*a != 0) {
			if(strchr(b,*a) != NULL)
				*classind++ = *a;
			a++;
		}
		if(aplus != NULL) *aplus='+';
		if(bplus != NULL) *bplus='+';
		if(classpat == classind)
			return NULL;
		*classind = 0;
		/* If the class is one character long, there's not much point
		   in carrying around all the baggage. */
		if((classind > classpat+1) && (aplus != NULL || bplus != NULL))
			return classmatch(str_enter(classpat),pluscat(aplus,bplus));
		else
			return str_enter(classpat);
	}
}

/* Put something into the environment. */

void
putenv2 (const char *key, const char *val)
{
	char *xx = (char *)malloc (strlen (key) + strlen (val) + 2);

	if (xx != NULL) {
		sprintf (xx, "%s=%s", key, val);
		putenv (xx);
	}
}



void Xbreak(void) { }

void
xquit (const char *s, const char *t)
{
	if (s != NULL)
		syslog (LOG_WARNING, "%s %s: %m", s, t ? t : "");
	abort();
}

void panic(const char *x, ...)
{
	*((char *)0xdeadbeef) = 0; /* Crash */
}


void
dropdead(void)
{
	if(zzconn != NULL && zzconn->cg != NULL)
		syslog(LOG_ERR, "Startup of %s:%s cancelled --
		timeout",zzconn->cg->site,zzconn->cg->protocol);
	else 
		syslog(LOG_ERR, "Startup cancelled because of a timeout!");
	exit(9);
}

void
log_idle (void *xxx)
{
	syslog (LOG_DEBUG, "ISDN is still alive.");
	timeout (log_idle, NULL, 10 * 60 * HZ);
}

void
queue_idle (void *xxx)
{
	runqueues (); runqueues();
	timeout (queue_idle, NULL, HZ/2);
}

void
alarmsig(void)
{
	printf("Dead");
}

void
do_quitnow(void *nix)
{
	quitnow = 1;
	kill_progs(NULL);
}