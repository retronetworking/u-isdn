/*
 * This file is part of the ISDN master program.
 *
 * Copyright (C) 1995 Matthias Urlichs.
 * See the file COPYING for license details.
 */

#include "master.h"


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

char *classmatch(char *a, char *b)
{
	if(a == NULL)
		return b;
	else if(b == NULL)
		return a;
	else if(*b == '*')
		return a;
	else if(*a == '*')
		return b;
	else {
		char classpat[30];
		char *classind = classpat;
		while(*a != 0) {
			if(strchr(b,*a) != NULL)
				*classind++ = *a;
			a++;
		}
		if(classpat == classind)
			return NULL;
		*classind = 0;
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
	exit (4);
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
