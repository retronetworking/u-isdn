/*
 * This file is part of the ISDN master program.
 *
 * Copyright (C) 1995 Matthias Urlichs.
 * See the file COPYING for license details.
 */

#include "master.h"

static int seqnum;

/* Read a line, handle empty lines and continuations. */
struct _cf *
read_line (FILE * ffile, int *theLine)
{
	char line[MAXLINE];
	char *sofar = line;
	struct _cf *out;
	int remain = MAXLINE;
	int now;

	do {
		now = 0;
		while (remain > 3 && !feof (ffile) && fgets (sofar, remain - 1, ffile) != NULL) {
			now = strlen (sofar);
			if (now == 0)
				break;
			if (sofar[now - 1] != '\n')
				return NULL;
			(*theLine)++;
			if (sofar[now - 2] == '\\') {
				sofar += now - 2;
				remain -= now - 2;
			} else {
				sofar += now - 1;
				*sofar = '\0';
				break;
			}
		}
		if (*line == '#' || *line == '\n') {
			sofar = line;
			*line = '\0';
			remain = MAXLINE;
		}
	} while (sofar == line && now > 0);
	if (sofar == line || remain <= 3 || now == 0)
		return NULL;
	*sofar = '\0';
	out = (struct _cf *)malloc (sizeof (struct _cf) + (now = sofar - line + 1));

	bcopy (line, (char *) (out + 1), now);
	bzero ((char *) out, sizeof (struct _cf));
	return out;
}

/* Append a config entry to a list of config entries. */
static void
app (cf * where, cf who)
{
	char *x = (char *) who + 1;

	while (*x != '\0' && !isspace (*x)) {
		x++;
	}
#if 1
	while (*where != NULL)
		where = &((*where)->next);
	*where = who;
	who->next = NULL;
#else
	who->next = *where;
	*where = who;
#endif
}

/* Skip spaces */
static int
skipsp (char **li)
{
	char *x = *li;

	while (*x != ' ' && *x != '\t' && *x != '\0')
		x++;
	if (*x == '\0')
		return 1;
	*x++ = '\0';
	while (*x == ' ' || *x == '\t')
		x++;
	*li = x;
	return 0;
}

#ifdef unused
static void
skipword (char **li)
{
	char *x = *li;

	while (*x != ' ' && *x != '\t' && *x != '\0')
		x++;
	*li = x;
}
#endif

void
do_subclass(cf c)
{
	char *info;
	if((info = strchr(c->card,'/')) != NULL) {
		char neg;
		int x = 0;
		*info++ = '\0';
		neg = (*info == '-');
		c->mask = neg ? ~0 : 0;
		while(*info != 0) {
			if(*info >= '0' && *info <= '9') {
				x = x * 10 + *info - '0';
			} else if(x != 0) {
				if(neg)
					c->mask |= 1<<(x-1);
				else
					c->mask &=~ (1<<(x-1));
				x = 0;
			}
			info++;
		}
	} else 
		c->mask = ~0;
}

/* Read a config file */
void
read_file (FILE * ffile, char *errf)
{
	cf c;
	int errl = 0;

	syslog (LOG_INFO, "Reading %s", errf);
	while ((c = read_line (ffile, &errl)) != NULL) {
		char *li = (char *) (c + 1);

		switch (CHAR2 (li[0], li[1])) {
		case CHAR2 ('P', ' '):
		case CHAR2 ('P', '_'):
		case CHAR2 ('P', '\t'):
				/* P <Art> <Partner> <Key> <Karte> <Mod> <Parameter...> */
			{
				char *pri;
				if (skipsp (&li)) break; c->protocol = li;
				if (skipsp (&li)) break; c->site = li;
				if (skipsp (&li)) break; c->cclass = li;
				if (skipsp (&li)) break; c->card = li;
				if (skipsp (&li)) break; c->type = li;
				if (skipsp (&li)) c->args = ""; else c->args = li;
				if((pri = strchr(c->type,',')) != NULL) {
					*pri++ = '\0';
					c->num = atoi(pri);
				}
				do_subclass(c);
				chkone(c);
				c->protocol = str_enter(c->protocol);
				c->site     = str_enter(c->site);
				c->cclass   = str_enter(c->cclass);
				c->card     = str_enter(c->card);
				c->type     = str_enter(c->type);
				c->args     = str_enter(c->args);
				app (&cf_P, c);
			}
			continue;
		case CHAR2 ('M', 'L'):
			/* ML <Art> <Partner> <Key> <Mod,#> <Modus> <Module...> */
			if (skipsp (&li)) break; c->protocol = li;
			if (skipsp (&li)) break; c->site = li;
			if (skipsp (&li)) break; c->cclass = li;
			if (skipsp (&li)) break; c->card = li;
			if (skipsp (&li)) break; c->type = li;
			if (skipsp (&li)) break; c->arg = li;
			if (skipsp (&li)) break; c->args = li;
			{
				char *sp = strchr(c->type,',');
				if(sp != NULL) {
					*sp++=0;
					if ((c->num = atoi (sp)) == 0 && sp[0] != '0')
						break;
				}
			}
			do_subclass(c);
			chkone(c);
			c->protocol = str_enter(c->protocol);
			c->site     = str_enter(c->site);
			c->cclass   = str_enter(c->cclass);
			c->card     = str_enter(c->card);
			c->type     = str_enter(c->type);
			c->arg      = str_enter(c->arg);
			c->args     = str_enter(c->args);
			app (&cf_ML, c);
			continue;
		case CHAR2 ('M', 'P'):
			/* ML <Art> <Partner> <Key> <Mod> <Modus> <Module...> */
			if (skipsp (&li)) break; c->protocol = li;
			if (skipsp (&li)) break; c->site = li;
			if (skipsp (&li)) break; c->cclass = li;
			if (skipsp (&li)) break; c->card = li;
			if (skipsp (&li)) break; c->type = li;
			if (skipsp (&li)) break; c->arg = li;
			if (skipsp (&li)) break; c->args = li;
			chkone(c);
			do_subclass(c);
			c->protocol = str_enter(c->protocol);
			c->site     = str_enter(c->site);
			c->cclass   = str_enter(c->cclass);
			c->card     = str_enter(c->card);
			c->type     = str_enter(c->type);
			c->arg      = str_enter(c->arg);
			c->args     = str_enter(c->args);
			app (&cf_MP, c);
			continue;
		case CHAR2 ('D', ' '):
		case CHAR2 ('D', '_'):
		case CHAR2 ('D', '\t'):
			{
				char *pri;
				/* D <Art> <Partner> <Key> <Karte> <Mod> <Nr> */
				if (skipsp (&li)) break; c->protocol = li;
				if (skipsp (&li)) break; c->site = li;
				if (skipsp (&li)) break; c->cclass = li;
				if (skipsp (&li)) break; c->card = li;
				if (skipsp (&li)) break; c->type = li;
				if (!skipsp (&li)) c->arg = li;
				if((pri = strchr(c->type,',')) != NULL) {
					*pri++ = '\0';
					c->num = atoi(pri);
				}
				chkone(c);
				do_subclass(c);
				c->protocol = str_enter(c->protocol);
				c->site     = str_enter(c->site);
				c->cclass   = str_enter(c->cclass);
				c->card     = str_enter(c->card);
				c->type     = str_enter(c->type);
				c->arg      = str_enter(c->arg);
				app (&cf_D, c);
			}
			continue;
		case CHAR2 ('T', 'M'):
			/* TM <Key> <String> */
			if (skipsp (&li)) break; c->cclass = li;
			if (skipsp (&li)) break; c->arg = li;
			if (!skipsp (&li)) break;
			if (isintime(c->arg) < 0) break;
			chkone(c);
			do_subclass(c);
			c->cclass   = str_enter(c->cclass);
			c->arg      = str_enter(c->arg);
			app (&cf_TM, c);
			continue;
		case CHAR2 ('D', 'L'):
			/* DL <Key> <Karte> <Nummer> <Protokolle> */
			if (skipsp (&li)) break; c->cclass = li;
			if (skipsp (&li)) break; c->card = li;
			if (skipsp (&li)) break; c->arg = li;
			if (skipsp (&li)) c->args = ""; else c->args = li;
			chkone(c);
			do_subclass(c);
			c->cclass   = str_enter(c->cclass);
			c->card     = str_enter(c->card);
			c->arg      = str_enter(c->arg);
			c->args     = str_enter(c->args);
			app (&cf_DL, c);
			continue;
		case CHAR2 ('D', 'P'):
			/* DP <Karte> <Nummernpräfixe-Dialout> <Nummernpräfixe-Dialin> */
			if (skipsp (&li)) break; c->card = li;
			if (skipsp (&li)) break; c->arg = li;
			if (!skipsp (&li)) c->args = li; else c->args = c->arg;
			if(c->args[0] == '\0') c->args = c->arg;
			chkone(c);
			do_subclass(c);
			c->card     = str_enter(c->card);
			c->arg      = str_enter(c->arg);
			c->args     = str_enter(c->args);
			app (&cf_DP, c);
			continue;
		case CHAR2 ('R', ' '):
		case CHAR2 ('R', '_'):
		case CHAR2 ('R', '\t'):
			{
				char *username;
				struct passwd *pw;

				if (skipsp (&li)) break; c->protocol = li;
				if (skipsp (&li)) break; c->site = li;
				if (skipsp (&li)) break; c->cclass = li;
				if (skipsp (&li)) break; c->card = li;
				if (skipsp (&li)) break; username = li;
				if (skipsp (&li)) break; c->type = li;
				if (skipsp (&li)) break; c->args = li;
				if ((pw = getpwnam (username)) == NULL)
					break;
				chkone(c);
				do_subclass(c);
				c->num = pw->pw_uid;
				c->num2 = pw->pw_gid;
				c->protocol = str_enter(c->protocol);
				c->site     = str_enter(c->site);
				c->cclass   = str_enter(c->cclass);
				c->card     = str_enter(c->card);
				c->type     = str_enter(c->type);
				c->args     = str_enter(c->args);
				app (&cf_R, c);
			} continue;
		case CHAR2 ('R', 'P'):
			{
				char *username;
				struct passwd *pw;

				if (skipsp (&li)) break; c->protocol = li;
				if (skipsp (&li)) break; c->site = li;
				if (skipsp (&li)) break; c->cclass = li;
				if (skipsp (&li)) break; c->card = li;
				if (skipsp (&li)) break; username = li;
				if (skipsp (&li)) break; c->type = li;
				if (skipsp (&li)) break; c->args = li;
				if ((pw = getpwnam (username)) == NULL)
					break;
				chkone(c);
				do_subclass(c);
				c->num      = pw->pw_uid;
				c->num2     = pw->pw_gid;
				c->protocol = str_enter(c->protocol);
				c->site     = str_enter(c->site);
				c->cclass   = str_enter(c->cclass);
				c->card     = str_enter(c->card);
				c->type     = str_enter(c->type);
				c->args     = str_enter(c->args);
				app (&cf_RP, c);
			} continue;
		case CHAR2 ('L', 'F'):
			if (skipsp (&li)) break; c->card = li;
			if (skipsp (&li)) break;
			if ((c->num = atoi (li)) == 0 && li[0] != '0')
				break;
			if (skipsp (&li)) break;
			if ((c->num2 = atoi (li)) == 0 && li[0] != '0')
				break;
			if (skipsp (&li)) break; c->arg = li;
			if(!skipsp (&li)) break;
			chkone(c);
			do_subclass(c);
			c->card     = str_enter(c->card);
			c->arg      = str_enter(c->arg);
			app (&cf_LF, c);
			continue;
		case CHAR2 ('C', 'M'):
			if (skipsp (&li)) break; c->card = li;
			if (skipsp (&li)) break;
			if ((c->num = atoi (li)) == 0 && li[0] != '0')
				break;
			if (skipsp (&li)) break;
			chkone(c);
			do_subclass(c);
			c->arg = li;
			c->card     = str_enter(c->card);
			app (&cf_CM, c);
			continue;
		case CHAR2 ('C', 'L'):
			if (skipsp (&li)) break; c->protocol = li;
			if (skipsp (&li)) break; c->site = li;
			if (skipsp (&li)) break; c->cclass = li;
			if (skipsp (&li)) break; c->card = li;
			if (skipsp (&li)) break;
			if ((c->num = atoi (li)) == 0 && li[0] != '0')
				break;;
			if (!skipsp (&li)) c->args = li;
			chkone(c);
			do_subclass(c);
			c->protocol = str_enter(c->protocol);
			c->site     = str_enter(c->site);
			c->cclass   = str_enter(c->cclass);
			c->card     = str_enter(c->card);
			c->args     = str_enter(c->args);
			app (&cf_CL, c);
			continue;
		}
		syslog (LOG_ERR, "Bad line %s:%d: %s", errf, errl, (char *) (c + 1));
		free (c);
	}
	return;
}

char **fileargs;

/* Read all the files. */
void
read_args (void *nix)
{
	int nexttime = 0;
#ifdef NEW_TIMEOUT
	static long classtimer;
#endif

	char **arg;
	struct conninfo *conn;
	conngrab cg;
	cf cft;

#define CFREE(what) do { while(what != NULL) { cf cf2 = what->next;free(what);what = cf2; } } while(0)
	CFREE (cf_P);
	CFREE (cf_ML);
	CFREE (cf_MP);
	CFREE (cf_D);
	CFREE (cf_DL);
	CFREE (cf_TM);
	CFREE (cf_DP);
	CFREE (cf_R);
	CFREE (cf_LF);
	CFREE (cf_RP);
	CFREE (cf_C);
	CFREE (cf_CM);
	CFREE (cf_CL);
	seqnum = 0;

	for(conn=isdn4_conn; conn != NULL; conn = conn->next) {
		if((cg = conn->cg) == NULL)
			continue;
		cg->dl = NULL;
		cg->dp = NULL;
		cg->ml = NULL;
		cg->r_ = NULL;
	}
	
	for (arg = fileargs; *arg != NULL; arg++) {
		FILE *f = fopen (*arg, "r");

		if (f == NULL)
			xquit ("Open", *arg);
		read_file (f, *arg);
		fclose (f);
	}
	if(theclass != NULL) {
#ifdef NEW_TIMEOUT
		untimeout(classtimer);
#else
		untimeout(read_args_run,NULL);
#endif
	}
	theclass = "*";
	for(cft = cf_TM; cft != NULL; cft = cft->next) {
		if((nexttime = isintime(cft->arg)) > 0) {
			theclass = cft->arg;
			break;
		}
	}

	if((nexttime == 0) || (nexttime > 32767/HZ/60))
		nexttime = 32767/HZ/60;
#ifdef NEW_TIMEOUT
	classtimer =
#endif
		timeout(read_args_run,NULL,nexttime * 60 * HZ);
}

/* Read all the files and kick off the programs. */
void
read_args_run(void *nix)
{
	read_args(NULL);
	do_run_now++;
	run_now(NULL);
}


