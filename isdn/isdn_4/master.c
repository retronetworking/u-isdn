/*
 *
 * ISDN master program.
 *
 * Copyright (c) 1993-1995 Matthias Urlichs <urlichs@noris.de>.
 */

/*
 * Malloc trace stuff...
 */
#undef DO_DEBUG_MALLOC


#ifndef	_MALLOC_INTERNAL
#define	_MALLOC_INTERNAL
#include <malloc.h>
#endif
#include <stdio.h>

#include <stdlib.h>

static FILE *mallstream;
static char mallenv[]= "MALLOC_TRACE";
static char mallbuf[BUFSIZ];	/* Buffer for the output.  */

/* Old hook values.  */
static void (*tr_old_free_hook) __P ((__ptr_t ptr));
static __ptr_t (*tr_old_malloc_hook) __P ((size_t size));
static __ptr_t (*tr_old_realloc_hook) __P ((__ptr_t ptr, size_t size));

#define BI(x) __builtin_return_address((x))

#ifdef DO_DEBUG_MALLOC

static void tr_freehook __P ((__ptr_t));
static void
tr_freehook (ptr)
     __ptr_t ptr;
{
  fprintf (mallstream, "- %p,%p,%p %p\n", BI(2),BI(3),BI(4), ptr);	/* Be sure to print it first.  */
  __free_hook = tr_old_free_hook;
  free (ptr);
  __free_hook = tr_freehook;
}

static __ptr_t tr_mallochook __P ((size_t));
static __ptr_t
tr_mallochook (size)
     size_t size;
{
  __ptr_t hdr;

  __malloc_hook = tr_old_malloc_hook;
  hdr = (__ptr_t) malloc (size);
  __malloc_hook = tr_mallochook;

  /* We could be printing a NULL here; that's OK.  */
  fprintf (mallstream, "+ %p,%p,%p %p %x\n", BI(2),BI(3),BI(4), hdr, size);

  return hdr;
}

static __ptr_t tr_reallochook __P ((__ptr_t, size_t));
static __ptr_t
tr_reallochook (ptr, size)
     __ptr_t ptr;
     size_t size;
{
  __ptr_t hdr;

  __free_hook = tr_old_free_hook;
  __malloc_hook = tr_old_malloc_hook;
  __realloc_hook = tr_old_realloc_hook;
  hdr = (__ptr_t) realloc (ptr, size);
  __free_hook = tr_freehook;
  __malloc_hook = tr_mallochook;
  __realloc_hook = tr_reallochook;
  if (hdr == NULL)
    /* Failed realloc.  */
    fprintf (mallstream, "! %p,%p,%p %p %x\n", __builtin_return_address(2),BI(3),BI(4), ptr, size);
  else
    fprintf (mallstream, "< %p,%p,%p %p\n> %p,%p,%p %p %x\n", BI(2),BI(3),BI(4), ptr, BI(2),BI(3),BI(4), hdr, size);

  return hdr;
}

static void
mmtrace ()
{
	char *mallfile;

	mallfile = getenv (mallenv);
	if (mallfile != NULL) {
    	mallstream = fopen (mallfile != NULL ? mallfile : "/dev/null", "a");
		if (mallstream != NULL) {
	  		/* Be sure it doesn't malloc its buffer!  */
	  		setbuf (mallstream, mallbuf);
	  		fprintf (mallstream, "= Start\n");
	  		tr_old_free_hook = __free_hook;
	  		__free_hook = tr_freehook;
	  		tr_old_malloc_hook = __malloc_hook;
	  		__malloc_hook = tr_mallochook;
	  		tr_old_realloc_hook = __realloc_hook;
	  		__realloc_hook = tr_reallochook;
		}
    }
}



/* Old hook values.  */
static void (*old_free_hook) __P ((__ptr_t ptr));
static __ptr_t (*old_malloc_hook) __P ((size_t size));
static __ptr_t (*old_realloc_hook) __P ((__ptr_t ptr, size_t size));

/* Function to call when something awful happens.  */
extern void abort __P ((void));
static void (*abortfunc) __P ((void)) = (void (*) __P ((void))) abort;

/* Arbitrary magical numbers.  */
#define MAGICWORD	0xfedabeeb
#define MAGICBYTE	((char) 0xd7)

struct hdr
  {
    size_t size;		/* Exact size requested by user.  */
    unsigned long int magic;	/* Magic number to check header integrity.  */
	struct hdr *prevb;
	struct hdr *nextb;
  } thelist = { 0,0, &thelist, &thelist };

static void checkhdr __P ((__const struct hdr *));
static void
checkhdr (hdr)
     __const struct hdr *hdr;
{
	if (hdr->magic != MAGICWORD || ((char *) &hdr[1])[hdr->size] != MAGICBYTE)
		(*abortfunc) ();
	if(hdr->prevb != &thelist) {
		if (hdr->prevb->magic != MAGICWORD || ((char *) &hdr->prevb[1])[hdr->prevb->size] != MAGICBYTE)
			(*abortfunc) ();
	}
	if(hdr->nextb != &thelist) {
		if (hdr->nextb->magic != MAGICWORD || ((char *) &hdr->nextb[1])[hdr->nextb->size] != MAGICBYTE)
			(*abortfunc) ();
	}
}

int mallcheck(void *foo) {
	int found = 0;
	struct hdr *h = &thelist;
	while((h = h->nextb) != &thelist) {
		checkhdr(h);
		if(h+1 == foo)
			found++;
	}
	return found;
}

static void freehook __P ((__ptr_t));
static void
freehook (ptr)
     __ptr_t ptr;
{
  struct hdr *hdr = ((struct hdr *) ptr) - 1;
  checkhdr (hdr);
  hdr->magic = 0;
  hdr->prevb->nextb = hdr->nextb;
  hdr->nextb->prevb = hdr->prevb;
  __free_hook = old_free_hook;
  free (hdr);
  __free_hook = freehook;
}

static __ptr_t mallochook __P ((size_t));
static __ptr_t
mallochook (size)
     size_t size;
{
  struct hdr *hdr;

  __malloc_hook = old_malloc_hook;
  hdr = (struct hdr *) malloc (sizeof (struct hdr) + size + 1);
  __malloc_hook = mallochook;
  if (hdr == NULL)
    return NULL;

  hdr->size = size;
  hdr->magic = MAGICWORD;
  hdr->prevb = &thelist;
  hdr->nextb = thelist.nextb;
  hdr->prevb->nextb = hdr;
  hdr->nextb->prevb = hdr;

  ((char *) &hdr[1])[size] = MAGICBYTE;
  return (__ptr_t) (hdr + 1);
}

static __ptr_t reallochook __P ((__ptr_t, size_t));
static __ptr_t
reallochook (ptr, size)
     __ptr_t ptr;
     size_t size;
{
  struct hdr *hdr = ((struct hdr *) ptr) - 1;

  checkhdr (hdr);
  hdr->prevb->nextb = hdr->nextb;
  hdr->nextb->prevb = hdr->prevb;

  __free_hook = old_free_hook;
  __malloc_hook = old_malloc_hook;
  __realloc_hook = old_realloc_hook;
  hdr = (struct hdr *) realloc ((__ptr_t) hdr, sizeof (struct hdr) + size + 1);
  __free_hook = freehook;
  __malloc_hook = mallochook;
  __realloc_hook = reallochook;
  if (hdr == NULL)
    return NULL;

  hdr->magic = MAGICWORD;
  hdr->size = size;
  hdr->prevb = &thelist;
  hdr->nextb = thelist.nextb;
  hdr->prevb->nextb = hdr;
  hdr->nextb->prevb = hdr;
  ((char *) &hdr[1])[size] = MAGICBYTE;
  return (__ptr_t) (hdr + 1);
}

void
mcheck (func)
     void (*func) __P ((void));
{
  static int mcheck_used = 0;

  if (func != NULL)
    abortfunc = func;

  /* These hooks may not be safely inserted if malloc is already in use.  */
  if (!__malloc_initialized && !mcheck_used)
    {
      old_free_hook = __free_hook;
      __free_hook = freehook;
      old_malloc_hook = __malloc_hook;
      __malloc_hook = mallochook;
      old_realloc_hook = __realloc_hook;
      __realloc_hook = reallochook;
      mcheck_used = 1;
    }
}

void chkall(void);
void chkone(void *foo);
#else /* not DO_DEBUG_MALLOC */
void chkone(void *foo) { }
void chkall(void) { }
#define chkall() do { } while (0)
#define chkone(f) do { } while (0)
#endif


/*
 * A large heap of include files. Some may no longer be necessary...
 */
#include "primitives.h"
#include "master.h"
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/uio.h>
#include <errno.h>
#include <pwd.h>
#include "f_ioctl.h"
#include "f_termio.h"
#include <sys/sysmacros.h>
#include <sys/stropts.h>
#include "f_signal.h"
#include <sys/wait.h>
#include "f_strings.h"
#include <syslog.h>
#include <sys/time.h>
#include <sys/param.h>
#include "f_user.h"
#include <utmp.h>
#include <ctype.h>
#include <malloc.h>
#include <fcntl.h>
#include "streams.h"
#include "sioctl.h"
#include "streamlib.h"
#include "isdn_34.h"
#include "phone_1TR6.h"
#include "phone_ETSI.h"
#include "isdn_proto.h"
#include "wildmat.h"
#include "vectcmp.h"
#ifdef linux
#include <linux/fs.h>
#endif
#if LEVEL < 4
#include "isdn_23.h"
#endif
#include "isdn_limits.h"
#include "dump.h"
#include "timeout.h"
#include "x75.h"
#include "proto.h"
#ifdef LEONARDO
#include "../cards/leonardo/leo.h"
#endif


int LI = 0;


#define ID_priv_Busy CHAR2('=','B')
#define ID_priv_Print CHAR2('=','P')


#ifndef NCARDS
#define NCARDS 10
#endif

uid_t user[NMINOR];

/* Unique refnum, always incremented by two */
/* The lower level also has one of these, but with the low bit set */

long connrefs = 2;

int fd_mon;			/* the lower/layer connection */
char *progname;
int testonly = 0;	/* set if fd_mon is not actually connected */
int igstdin = 1;	/* Ignore stdin */
int in_boot = 1;	/* We're still coming up */
#ifdef __linux__
int isdnterm = 0;	/* major number of the terminal driver */
int isdnstd = 0;	/* major number of the standard driver */
#endif

char *cardlist[NCARDS];	/* List of available ISDN interfaces */
short cardnrbchan[NCARDS];	/* Nr of B channels */
short cardnum = 0;			/* Number of registered interfaces */
short cardidx = 0;			/* Used to cycle among interfaces */
short numidx = 0;			/* Ditto */

void *locputchar = NULL;	/* Glue */

#if LEVEL < 4				/* More glue */
int xnkernel = 0;
struct user u;
mblk_t *mbfreelist = NULL;
void *dbfreelist[30] = {NULL};
#endif
int connseq = 0;

struct xstream *xs_mon;		/* Pseudo-stream for the L3 engine */

#define DUMPW(s,l) do { printf("HL W "); dumpascii((uchar_t *)(s),(l)); printf("\n"); } while(0)

/* String database. Should probably be an AVL tree or a hash but I'm lazy. */
/** The purpose of this thing is to be able not to bother with keeping track
    of how many references there are. */
/**** Todo: Garbage collection. Sometime in the future... */

struct string {
	struct string *left;
	struct string *right;
	char data[2];
} *stringdb = NULL;

static char *str_enter(char *master)
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

/* List of cards */

typedef struct __card {
	long id;
	long modes;
	uchar_t bchans;
} *_card;
struct __card card[NCARDS];


/* List of connection-related programs, unfinished as of now */

typedef struct proginfo {
	struct proginfo *next;
	struct conninfo *master;
	pid_t pid;
	int die:1;
	int kill:1;
} *_proginfo;


/* States of a connection */

typedef enum connstate {
	c_forceoff,		/* Unknown */
	c_offdown,		/* Being turned off */
	c_off,			/* Turned off, unavailable */
	c_down,			/* Inactive */
	c_going_down,	/* Going inactive */
	c_going_up,		/* Being activated */
	c_up 			/* ONLINE */
} CState;

/* ... translated to human-readable strings */

static char *state2str(CState state) {
	switch(state) {
	case c_up: return "up";
	case c_off: return "off";
	case c_down: return "down";
	case c_offdown: return ">off";
	case c_forceoff: return "OFF";
	case c_going_up: return ">up";
	case c_going_down: return ">down";
	default: return "unknown";
	}
}


/* Info about a connection */

typedef struct conngrab {
	char *errmsg;
	char *site;
	char *protocol;
	char *cclass;
	char *card;
	char *oldnr;
	char *nr;
	char *lnr;
	char *nrsuf;
	char *lnrsuf;
	struct _cf *dl;
	struct _cf *dp;
	struct _cf *ml;
	struct _cf *r_;
	mblk_t *par_in;
	mblk_t *par_out;
int dropline;
	int flags;
	int refs;
	short delay;
} *conngrab;
typedef struct conninfo {
	struct conninfo *next;
	struct proginfo *run;
	struct conngrab *cg;
	long connref;				  /* L3 connection ID, globally unique */
	long charge;
	long ccharge;				  /* cumulative charge, informational msg */
	int chargecount;			/* Debug! */
	pid_t pid;
	uchar_t minor;
	uchar_t fminor;
	CState state;
	long flags;
	int retiming;
	int ctimer;
	int cause;
	char *causeInfo;
	int seqnum;
	short retries;
	unsigned timer_reconn:1;
	unsigned want_reconn:1;
	unsigned retime:1;		/* Timer for off->down change is running */
	unsigned got_hd:2;		/* disconnect... */
	unsigned got_id:2;
	unsigned char locked;
	unsigned ignore:3;
} *conninfo;

/* Special flags. */
#define F_INTERRUPT         01
#define F_PREFOUT           02
#define F_FORCEOUT          04
#define F_IGNORELIMIT      010
#define F_FASTDROP         020
#define F_FASTREDIAL       040
#define F_PERMANENT       0100
#define F_LEASED          0200
#define F_IGNOREBUSY      0400
#define F_NRCOMPLETE     01000
#define F_LNRCOMPLETE    02000
#define F_INCOMING       04000
#define F_OUTGOING      010000
#define F_DIALUP        020000
#define F_IGNORELIMIT2  040000
#define F_OUTCOMPLETE  0100000
#define F_SETINITIAL   0200000
#define F_SETLATER     0400000


char *HdrName (int hdr)
{
	switch(hdr) {
	default: {
	}
	case -1: return "-";
	case HDR_ATCMD: return "AT Cmd";
	case HDR_DATA: return "Data";
	case HDR_XDATA: return "XData";
	case HDR_UIDATA: return "UI Data";
	case HDR_RAWDATA: return "Raw Data";
	case HDR_OPEN: return "Open";
	case HDR_CLOSE: return "Close";
	case HDR_ATTACH: return "Attach";
	case HDR_DETACH: return "Detach";
	case HDR_CARD: return "Card";
	case HDR_NOCARD: return "No Card";
	case HDR_OPENPROT: return "Open Protocol";
	case HDR_CLOSEPROT: return "Close Protocol";
	case HDR_NOTIFY: return "Notify";
	case HDR_INVAL: return "Invalid";
	case HDR_TEI: return "TEI";
	case HDR_PROTOCMD: return "Proto Cmd";
	}
}

char *FlagInfo(int flag)
{
	static char fbuf[30];

	fbuf[0]='\0';
	if (flag & F_INTERRUPT)    strcat(fbuf, ":is");
	if (flag & F_PREFOUT)      strcat(fbuf, ":xi");
	if (flag & F_FORCEOUT)     strcat(fbuf, ":yi");
	if (flag & F_IGNORELIMIT)  strcat(fbuf, ":il");
	if (flag & F_FASTDROP)     strcat(fbuf, ":fX");
	if (flag & F_FASTREDIAL)   strcat(fbuf, ":fr");
	if (flag & F_PERMANENT)    strcat(fbuf, ":dP");
	if (flag & F_LEASED)       strcat(fbuf, ":dL");
	if (flag & F_IGNOREBUSY)   strcat(fbuf, ":ib");
	if (flag & F_NRCOMPLETE)   strcat(fbuf, ":nc");
	if (flag & F_LNRCOMPLETE)  strcat(fbuf, ":lc");
	if (flag & F_INCOMING)     strcat(fbuf, ":in");
	if (flag & F_OUTGOING)     strcat(fbuf, ":ou");
	if (flag & F_DIALUP)       strcat(fbuf, ":dD");
	if (flag & F_IGNORELIMIT2) strcat(fbuf, ":iL");
	if (flag & F_OUTCOMPLETE)  strcat(fbuf, ":oc");
	if (flag & F_SETINITIAL)   strcat(fbuf, ":si");
	if (flag & F_SETLATER)     strcat(fbuf, ":sl");

	if(fbuf[0]=='\0')
		strcpy(fbuf,"-");
	return fbuf;
}

/* The List */

struct conninfo *theconn = NULL;


#define newgrab(x) Xnewgrab((x),__LINE__)
conngrab Xnewgrab(conngrab master, int lin)
{
	conngrab slave;

	slave = malloc(sizeof(*slave));
	if(slave == NULL)
		return NULL;
	if(master == NULL)
		bzero(slave,sizeof(*slave));
	else {
		if(master->refs == 0 || master->protocol == (char *)0xdeadbeef)
			panic("FreeGrab");
		*slave = *master;
		if(slave->par_out != NULL)
			slave->par_out = copybufmsg(slave->par_out);
		if(slave->par_in != NULL)
			slave->par_in = dupmsg(slave->par_in);
	}
if(0)printf("\nNG + %p %d\n",slave,lin);
	slave->refs = 1;
	return slave;
}

#define dropgrab(x) Xdropgrab((x),__LINE__)
void Xdropgrab(conngrab cg,int lin)
{
	if(cg == NULL)
		return;
	chkone(cg);
	cg->dropline = lin;

	if(--cg->refs == 0) {
		chkone(cg);
		if(cg->par_out != NULL)
			freemsg(cg->par_out);
		if(cg->par_in != NULL)
			freemsg(cg->par_in);
		cg->par_out = (void *)0xdeadbeef;
		cg->par_in  = (void *)0xdeadbeef;
		cg->site    = (void *)0xdeadbeef;
		cg->protocol= (void *)0xdeadbeef;
		cg->cclass  = (void *)0xdeadbeef;
		cg->card    = (void *)0xdeadbeef;
		chkone(cg);
		free(cg);
if(0)printf("\nNG - %p %d\n",cg,lin);
		return;
	}
if(0)printf("\nNG ? %p %d %d\n",cg,lin,cg->refs);
}

/* Forward declarations */

void kill_progs(struct conninfo *xconn);
void backrun(int fd);
void time_reconn(struct conninfo *conn);
void try_reconn(struct conninfo *);
void retime(struct conninfo *conn);
void run_now(void *nix);
int do_run_now = 0;
int has_progs(void);
void do_quitnow(void *nix);


/* Debug code to show that a connection reference value is changed */

#define setconnref(a,b) Xsetconnref(__LINE__,(a),(b))
void Xsetconnref(unsigned int deb_line, conninfo conn, int connref)
{
	printf("-%d: SetConnRef %d/%d/%ld -> %d\n",deb_line,conn->minor,conn->fminor,conn->connref,connref);
	conn->connref = connref;
}

void connreport(char *foo)
{
	conninfo conn;
	mblk_t xx;
	struct datab db;
	char ans[20];
	xx.b_rptr = ans;
	db.db_base = ans;
	db.db_lim = ans + sizeof (ans);
	xx.b_datap = &db;

	for(conn = theconn; conn != NULL; conn = conn->next) {
		struct iovec io[2];

		chkone(conn);
		if(conn->ignore != 3 || conn->minor == 0)
			continue;

		xx.b_wptr = ans;
		m_putid (&xx, CMD_PROT);
		m_putsx (&xx, ARG_FMINOR);
		m_puti (&xx, conn->minor);
		m_putdelim (&xx);
		m_putid (&xx, PROTO_AT);
		*xx.b_wptr++ = '*';
		io[0].iov_base = xx.b_rptr;
		io[0].iov_len = xx.b_wptr - xx.b_rptr;
		io[1].iov_base = foo;
		io[1].iov_len = strlen(foo);
		DUMPW (xx.b_rptr, io[0].iov_len);
		printf ("+ ");
		DUMPW (foo,strlen(foo));
		(void) strwritev (xs_mon, io, 2, 1);
	}
}


/* Changing a connection status, and things to do */
const char *CauseInfo(int cause, char *pri);

void ReportConn(conninfo conn)
{
	char sp[200], *spf = sp;
	spf += sprintf(spf,"%s%d:%d %s %s %s %d %s/%s %ld %ld %s",
		conn->ignore?"!":"", conn->minor,
		conn->seqnum, conn->cg ? conn->cg->site : "-",
		conn->cg ? conn->cg->protocol : "-", conn->cg ? conn->cg->cclass : "-", conn->pid,
		state2str(conn->state), conn->cg ? conn->cg->card : "-", conn->charge,
		conn->ccharge, FlagInfo(conn->flags));
	if(conn->cg != NULL && (conn->cg->flags ^ conn->flags) != 0) {
		int foo = strlen(FlagInfo(conn->cg->flags ^ conn->flags));
		int bar = strlen(FlagInfo(conn->cg->flags));
		if(foo < bar)
			spf += sprintf(spf, "^%s",FlagInfo(conn->cg->flags ^ conn->flags));
		else
			spf += sprintf(spf, "/%s",FlagInfo(conn->cg->flags));
	}
	if(conn->cg != NULL && conn->cg->nr != NULL)
		spf += sprintf(spf, ",%s",conn->cg->nr);
	else if(conn->cg != NULL && conn->cg->oldnr != NULL)
		spf += sprintf(spf, ",%s",conn->cg->oldnr);
	spf += sprintf(spf," %s", CauseInfo(conn->cause, conn->causeInfo));
	connreport(sp);
}

#define setconnstate(a,b) Xsetconnstate(__FILE__,__LINE__,(a),(b))
void Xsetconnstate(const char *deb_file, unsigned int deb_line,conninfo conn, CState state)
{
	chkone(conn);
	printf("%s:%d: State %d: %s",deb_file,deb_line,conn->minor,state2str(conn->state));
	if(conn->state != state)
		printf(" -> %s\n",state2str(state));
	else
		printf("\n");
	if(conn->timer_reconn && (state == c_offdown || (state >= c_going_up
		&& conn->state < c_going_up))) {
		conn->timer_reconn = 0;
		untimeout(time_reconn,conn);
	} else if(!conn->timer_reconn && state < c_going_up && conn->state >= c_going_up) {
		conn->timer_reconn = 1;
		if(conn->flags & F_FASTREDIAL)
			timeout(time_reconn,conn,HZ);
		else
			timeout(time_reconn,conn,5*HZ);
	}
	if(conn->state <= c_down)
		setconnref(conn,0);
	if(state == c_up)
		conn->cause = 0;
	else if(state == c_going_up)
		conn->cause = 999999;
	if((conn->state < c_going_down && state > c_going_down) || state <= c_off) {
		if(conn->charge > 0) {
			if(conn->cg != NULL)
				syslog(LOG_WARNING,"COST %s:%s %d",conn->cg->site,conn->cg->protocol,conn->charge);
			else
				syslog(LOG_WARNING,"COST ??? %d",conn->charge);
		}
		conn->ccharge += conn->charge;
		conn->charge = 0;
	}
	conn->state=state;
	if(conn->ignore < 2)
		ReportConn(conn);
	if(state <= c_down)
		conn->connref = 0;
	if(conn->ignore)
		return;
	if(state == c_down && conn->want_reconn) {
		conn->want_reconn = 0;
		try_reconn(conn);
	} else if(state == c_up) {
		conn->retries = 0;
		conn->retiming = 0;
		conn->want_reconn = 0;
	}
	if(state >= c_going_up) {
		conn->got_id = 0;
		conn->got_hd = 0;
	}
	if((state == c_off) && !conn->retime && (conn->flags & F_PERMANENT)) {
		conn->retime = 1;
		timeout(retime,conn,((conn->charge != 0) ? 5*60*++conn->retiming : (conn->cause == ID_priv_Busy) ? 5 : (conn->flags & F_FASTREDIAL) ? 2 : 5)*HZ);
	} else if((state != c_off) && conn->retime) {
		conn->retime = 0;
		untimeout(retime,conn);
	}
}


/* Device names. 's'=short, 'm'=medium, ''=fullname; 'i'=non-terminal version */

char *
sdevname (short minor)
{
	static char dev[20];

	sprintf (dev, "i%02d", minor & 0xFF);
	return dev;
}

char *
mdevname (short minor)
{
	static char dev[20];

	sprintf (dev, "tty%s", sdevname (minor));
	return dev;
}

char *
devname (short minor)
{
	static char dev[20];

	sprintf (dev, "/dev/%s", mdevname (minor));
	return dev;
}

char *
isdevname (short minor)
{
	static char dev[20];

	sprintf (dev, "isdn%d", minor & 0xFF);
	return dev;
}

char *
idevname (short minor)
{
	static char dev[20];

	sprintf (dev, "/dev/isdn/%s", isdevname (minor));
	return dev;
}


/* Check a lock file. */

void
checkdev(int dev)
{
	char permtt1[sizeof(LOCKNAME)+15];
	char permtt2[sizeof(LOCKNAME)+15];
	int f, len, pid;
	char sbuf[10];

	sprintf(permtt1,LOCKNAME,mdevname(dev));
	sprintf(permtt2,LOCKNAME,isdevname(dev));

	if((f = open(permtt1,O_RDWR)) < 0)  {
		if(0)syslog(LOG_WARNING,"Checking %s: unopenable, deleted, %m",permtt1);
		unlink(permtt1);
	} else {
		len=read(f,sbuf,sizeof(sbuf)-1);
		if(len<=0) {
			if(0)syslog(LOG_WARNING,"Checking %s: unreadable, deleted, %m",permtt1);
			unlink(permtt1);
		} else {
			if(sbuf[len-1]=='\n')
				sbuf[len-1]='\0';
			else
				sbuf[len]='\0';
			pid = atoi(sbuf);
			if(pid <= 0 || (kill(pid,0) == -1 && errno == ESRCH)) {
				if(0)syslog(LOG_WARNING,"Checking %s: unkillable, pid %d, deleted, %m",permtt1, pid);
				unlink(permtt1);
			}
		}
		close(f);
	}
	if((f = open(permtt2,O_RDWR)) < 0) {
		if(0)syslog(LOG_WARNING,"Checking %s: unopenable, deleted, %m",permtt2);
		unlink(permtt2);
	} else {
		len=read(f,sbuf,sizeof(sbuf)-1);
		if(len<=0) {
			if(0)syslog(LOG_WARNING,"Checking %s: unreadable, deleted, %m",permtt2);
			unlink(permtt2);
		} else {
			if(sbuf[len-1]=='\n')
				sbuf[len-1]='\0';
			else
				sbuf[len]='\0';
			pid = atoi(sbuf);
			if(pid <= 0 || (kill(pid,0) == -1 && errno == ESRCH)) {
				if(0)syslog(LOG_WARNING,"Checking %s: unkillable, pid %d, deleted, %m",permtt2, pid);
				unlink(permtt2);
			}
		}
		close(f);
	}
}


/* Lock a device. One failure, for externally-opened devices (cu), may be tolerated. */

int
lockdev(int dev, char onefailok)
{
	char permtt1[sizeof(LOCKNAME)+15];
	char permtt2[sizeof(LOCKNAME)+15];
	char vartt[sizeof(LOCKNAME)+15];
	char pidnum[7];
	int f, err, len;

	len = sprintf(pidnum,"%d\n",getpid());
	sprintf(vartt,LOCKNAME,pidnum);
	sprintf(permtt1,LOCKNAME,mdevname(dev));
	sprintf(permtt2,LOCKNAME,isdevname(dev));

	unlink(vartt);
	f=open(vartt,O_WRONLY|O_CREAT,0644);
	if(f < 0) {
		syslog(LOG_WARNING,"Locktemp %s: unopenable, %m",vartt);
		return -1;
	}
	if((err = write(f,pidnum,len)) != len) {
		syslog(LOG_WARNING,"Locktemp %s: unwriteable, %m",vartt);
		close(f);
		return -1;
	}
	close(f);
	chmod(vartt,S_IRUSR|S_IRGRP|S_IROTH);

	if((err = link(vartt,permtt1)) < 0) {
		if(onefailok == 0) {
			if(0) syslog(LOG_WARNING,"Lock %s: unlinkable, %m",permtt1);
			checkdev(dev);
			unlink(vartt);
			return -1;
		}
		--onefailok;
		if(0) syslog(LOG_INFO,"Lock %s: unlinkable(ignored), %m",permtt1);
	}
	if((err = link(vartt,permtt2)) < 0) {
		if (onefailok == 0) {
			if(0) syslog(LOG_WARNING,"Lock %s: unlinkable, %m",permtt2);
			checkdev(dev);
			unlink(vartt);
			unlink(permtt1);
			return -1;
		}
		--onefailok;
		if(0) syslog(LOG_INFO,"Lock %s: unlinkable(ignored), %m",permtt2);
	}
	unlink(vartt);
	if(0) syslog(LOG_DEBUG,"Locked %s and %s",permtt1,permtt2);
	return 0;
}


/* ... and unlock it. */

void
unlockdev(int dev)
{
	char permtt1[sizeof(LOCKNAME)+15];
	char permtt2[sizeof(LOCKNAME)+15];

	sprintf(permtt1,LOCKNAME,mdevname(dev));
	sprintf(permtt2,LOCKNAME,isdevname(dev));

	unlink(permtt1);
	unlink(permtt2);
	if(0) syslog(LOG_DEBUG,"Unlocked %s and %s",permtt1,permtt2);
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



/**
 ** Number String Stuff 
 **/


char *match_nr (char *extnr, char *locnr, char *locpref)
/* Vergleicht ankommende Nummern mit dem Eintrag einer D-Zeile. */
/* liefert evtl. ein ungematchtes Suffix zurück */
/* 09119599131,+911-959913/[1-3],=00+0-   ->   /[1-3] */
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
						if(strcmp(extpos,locpos+1) && !wildmat(extpos,locpos+1))
							return NULL;
					}
				} else {
					if(locpos[1] != '\0' && !wildmat(extpos,locpos+1))
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
	return (wildmatch(extsuf,extnr) != NULL);
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
	if(*prefpos == '\0' && islocal) {
		char *xextpos = strchr(extnr,'/');
		char *xlocpos = strchr(locnr,'/');
		if(xextpos != NULL && xlocpos != NULL && 
				(!strcmp(xextpos,xlocpos) || wildmatch(xextpos+1,xlocpos+1)))
			lastprefpos="/";
	}
	
	locpos=strchr(locnr,*lastprefpos);
	extpos=strchr(extnr,*lastprefpos);
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
	if(extpos[1] != '\0' && !wildmatch(extpos,extext))
		return NULL;
	while(extnr != extpos)
		*destpos++ = *extnr++;
	extext++;
	while(*extext != '\0' && !isspace(*extext))
		*destpos++ = *extext++;
	*destpos='\0';
	return str_enter(destnr);
}

char *strip_nr(char *extnr)
/* entfernt die Spezialzeichen aus einer vollständigen Nummer,
   zwecks Dialout; NULL wenn die Nummer unvollständig ist */
/* 123.45 -> 12345 */
/* 123.[45] -> NULL */
/* 123. -> NULL */
{
	char destnr[40];
	char *destpos = destnr;
	int lastspc=1;


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


/**
 ** Configuration section.
 **/

typedef struct _cf {
	struct _cf *next;
	char *protocol;
	char *site;
	char *cclass;
	char *card;
	char *type;
	char *arg;
	char *args;
	int num, num2;
	char got_err;
} *cf;

cf cf_P = NULL;
cf cf_ML = NULL;
cf cf_MP = NULL;
cf cf_D = NULL;
cf cf_DL = NULL;
cf cf_DP = NULL;
cf cf_R = NULL;
cf cf_RP = NULL;
cf cf_C = NULL;
cf cf_CM = NULL;
cf cf_CL = NULL;

#define chkfree(x) do { } while(0)
#ifdef DO_DEBUG_MALLOC

void chkcf(cf conf)
{
	for(;conf != NULL; conf = conf->next) 
		chkone(conf);
}

void chkone(void *foo)
{
	if(foo == NULL) return;
	checkhdr(((struct hdr *)foo)-1);
}


void chkall(void)
{
	struct conninfo *conn;
	for(conn = theconn; conn != NULL; conn = conn->next)  {
		chkone(conn); chkone(conn->cg);
	}
#if 0 /* takes much too long */
	chkcf(cf_P);
	chkcf(cf_ML);
	chkcf(cf_MP);
	chkcf(cf_D);
	chkcf(cf_DL);
	chkcf(cf_DP);
	chkcf(cf_R);
	chkcf(cf_RP);
	chkcf(cf_C);
	chkcf(cf_CM);
	chkcf(cf_CL);
	mallcheck(NULL);
#endif
}
#endif /* DO_DEBUG_MALLOC */

static struct _cf *
read_line (FILE * ffile, int *theLine)
{
#define MAXLINE 4096
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
		if (*line == '#') {
			sofar = line;
			*line = '\0';
			remain = MAXLINE;
		}
	} while (sofar == line && now > 0);
	if (sofar == line || remain <= 3 || now == 0)
		return NULL;
	*sofar = '\0';
	out = (struct _cf *)malloc (sizeof (struct _cf) + (now = sofar - line + 1));

	chkone(out);
	bcopy (line, (char *) (out + 1), now);
	chkone(out);
	bzero ((char *) out, sizeof (struct _cf));
	chkone(out);
	return out;
}

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

static void
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
			if (skipsp (&li)) break; c->protocol = li;
			if (skipsp (&li)) break; c->site = li;
			if (skipsp (&li)) break; c->cclass = li;
			if (skipsp (&li)) break; c->card = li;
			if (skipsp (&li)) break; c->type = li;
			if (skipsp (&li)) break; c->args = li;
			chkone(c);
			c->protocol = str_enter(c->protocol);
			c->site     = str_enter(c->site);
			c->cclass   = str_enter(c->cclass);
			c->card     = str_enter(c->card);
			c->type     = str_enter(c->type);
			c->args     = str_enter(c->args);
			app (&cf_P, c);
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
			/* D <Art> <Partner> <Key> <Karte> <Mod> <Nr> */
			if (skipsp (&li)) break; c->protocol = li;
			if (skipsp (&li)) break; c->site = li;
			if (skipsp (&li)) break; c->cclass = li;
			if (skipsp (&li)) break; c->card = li;
			if (skipsp (&li)) break; c->type = li;
			if (!skipsp (&li)) c->arg = li;
			chkone(c);
			c->protocol = str_enter(c->protocol);
			c->site     = str_enter(c->site);
			c->cclass   = str_enter(c->cclass);
			c->card     = str_enter(c->card);
			c->type     = str_enter(c->type);
			c->arg      = str_enter(c->arg);
			app (&cf_D, c);
			continue;
		case CHAR2 ('D', 'L'):
			/* DL <Karte> <Nummer> <Protokolle> */
			if (skipsp (&li)) break; c->card = li;
			if (skipsp (&li)) break; c->arg = li;
			if (skipsp (&li)) break; c->args = li;
			chkone(c);
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
				if (skipsp (&li)) break; username = li;
				if (skipsp (&li)) break; c->type = li;
				if (skipsp (&li)) break; c->args = li;
				if ((pw = getpwnam (username)) == NULL)
					break;
				chkone(c);
				c->num = pw->pw_uid;
				c->num2 = pw->pw_gid;
				c->protocol = str_enter(c->protocol);
				c->site     = str_enter(c->site);
				c->cclass   = str_enter(c->cclass);
				c->type     = str_enter(c->type);
				c->args     = str_enter(c->args);
				app (&cf_RP, c);
			} continue;
		case CHAR2 ('C', 'M'):
			if (skipsp (&li)) break; c->card = li;
			if (skipsp (&li)) break;
			if ((c->num = atoi (li)) == 0 && li[0] != '0')
				break;
			if (skipsp (&li)) break;
			chkone(c);
			c->arg = li;
			c->card     = str_enter(c->card);
			app (&cf_CM, c);
			continue;
		case CHAR2 ('C', 'L'):
			if (skipsp (&li)) break; c->card = li;
			if (skipsp (&li)) break;
			if ((c->num = atoi (li)) == 0 && li[0] != '0')
				break;;
			if (!skipsp (&li)) c->args = li;
			chkone(c);
			c->site     = str_enter(c->site);
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

void
read_args (void *nix)
{
	char **arg;
	struct conninfo *conn;
	conngrab cg;

#define CFREE(what) do { while(what != NULL) { cf cf2 = what->next;free(what);what = cf2; } } while(0)
	CFREE (cf_P);
	CFREE (cf_ML);
	CFREE (cf_MP);
	CFREE (cf_D);
	CFREE (cf_DL);
	CFREE (cf_DP);
	CFREE (cf_R);
	CFREE (cf_RP);
	CFREE (cf_C);
	CFREE (cf_CM);
	CFREE (cf_CL);

	for(conn=theconn; conn != NULL; conn = conn->next) {
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
}

void
read_args_run(void *nix)
{
	read_args(NULL);
	do_run_now++;
	run_now(NULL);
}


const char *CauseInfo(int cause, char *pri)
{
	if (cause == 999999) return "-";
	switch(cause) {
	case 0: return "OK";
	case ID_priv_Busy:					return "Local Busy";
	case ID_priv_Print:					if(isdigit(*pri)) return pri+1; else return pri;
	case ID_ET_AccessInfoDiscard:		return "Access info discarded";
	case ID_N1_BearerNotImpl:			return "Bearer Service not implemented";
	case ID_N1_CIDinUse:				return "CID in use";
	case ID_N1_CIDunknown:				return "CID unknown";
	case ID_ET_CallAwarded:				return "Call Awarded";
	case ID_ET_CIDcleared:				return "Call ID cleared";
	case ID_ET_CIDinUse:				return "Call ID in use";
/*	case ID_ET_CallRejected:			return "Call rejected"; */
	case ID_N1_CallRejected:			return "Call rejected";
	case ID_ET_CapNotAuth:				return "Capability not authorized";
	case ID_ET_CapNotAvail:				return "Capability not available";
	case ID_ET_CapNotImpl:				return "Capability not implemented";
	case ID_ET_ChanNotExist:			return "Channel does not exist";
	case ID_ET_ChanNotAvail:			return "Channel not available";
	case ID_ET_ChanTypeNotImpl:			return "Channel type not implemented";
	case ID_ET_DestIncompat:			return "Destination incompatible";
	case ID_N1_DestNotObtain:			return "Destination not obtainable";
/*	case ID_ET_FacNotImpl:				return "Facility not implemented"; */
	case ID_N1_FacNotImpl:				return "Facility not implemented";
/*	case ID_ET_FacNotSubscr:			return "Facility not subscribed"; */
	case ID_N1_FacNotSubscr:			return "Facility not subscribed";
	case ID_ET_FacRejected:				return "Facility rejected";
	case ID_N1_IncomingBarred:			return "Incoming calls barred";
	case ID_ET_InfoElemMissing:			return "Info element missing";
	case ID_ET_InfoInvalid:				return "Info invalid";
	case ID_ET_InfoNotCompatible:		return "Info not compatible";
	case ID_ET_InterworkingUnspec:		return "Interworking unspecified";
	case ID_N1_InvCRef:					return "Invalid Call Reference";
/*	case ID_ET_InvCRef:					return "Invalid call reference"; */
	case ID_ET_InvChan:					return "Invalid channel";
	case ID_ET_InvNumberFormat:			return "Invalid number format";
	case ID_ET_InvTransitNet:			return "Invalid transit network";
	case ID_ET_InvalUnspec:				return "Invalid unspecified";
	case ID_N1_LocalProcErr:			return "Local procedure error";
	case ID_ET_MandatoryMissing:		return "Mandatory missing";
	case ID_ET_MandatoryNotCompatible:	return "Mandatory not compatible";
	case ID_ET_MandatoryNotImpl:		return "Mandatory not implemented";
	case ID_N1_NegativeGBG:				return "Negative GBG";
	case ID_N1_NetworkCongestion:		return "Network congestion";
	case ID_ET_NetworkDown:				return "Network down";
	case ID_N1_NoSPVknown:				return "No SPV known";
	case ID_ET_NoAnswer:				return "No answer";
	case ID_ET_NoCallSusp:				return "No call suspended";
	case ID_ET_NoChanAvail:				return "No channel available";
/*	case ID_N1_NoChans:					return "No channels"; */
	case ID_ET_NoRouteDest:				return "No route to destination";
	case ID_ET_NoRouteTransit:			return "No route to transit network";
	case ID_ET_NoUserResponding:		return "No user responding";
/*	case ID_N1_NoUserResponse:			return "No user response"; */
	case ID_ET_NormalClear:				return "Normal Clear";
	case ID_ET_NormalUnspec:			return "Normal unspecified";
	case ID_ET_NonSelected:				return "Not selected";
	case ID_ET_NumberChanged:			return "Number changed";
/*	case ID_N1_NumberChanged:			return "Number changed"; */
	case ID_ET_NumberUnassigned:		return "Number unassigned";
	case ID_ET_OutOfOrder:				return "Out of order";
/*	case ID_N1_OutOfOrder:				return "Out of order"; */
	case ID_N1_OutgoingBarred:			return "Outgoing calls barred";
	case ID_ET_ProtocolUnspec:			return "Protocol unspecified";
	case ID_ET_QualityUnavail:			return "Quality unavailable";
	case ID_N1_RemoteProcErr:			return "Remote procedure error";
	case ID_N1_RemoteUserResumed:		return "Remote user resumed";
	case ID_N1_RemoteUserSuspend:		return "Remote user suspend";
	case ID_N1_RemoteUser:				return "Remote user";
	case ID_ET_ResourceUnavail:			return "Resource unavailable";
	case ID_ET_RestSTATUS:				return "Restart STATUS";
	case ID_ET_RestrictedInfoAvail:		return "Restricted info available";
	case ID_ET_SwitchCongest:			return "Switch congestion";
	case ID_ET_TempFailure:				return "Temporary failure";
	case ID_ET_TimerRecovery:			return "Timer recovery";
	case ID_ET_UnavailUnspec:			return "Unavailable unspecified";
	case ID_ET_UnimplUnspec:			return "Unimplemented unspecified";
	case ID_N1_UnknownGBG:				return "Unknown GBG";
	case ID_N1_UserAssessBusy:			return "User assess busy";
	case ID_ET_UserBusy:				return "User busy";
/*	case ID_N1_UserBusy:				return "User busy"; */
	case ID_N1_UserInfoDiscarded:		return "User info discarded";
	case ID_ET_WrongCallID:				return "Wrong call ID";
	default: {
		static char buf[10];
		sprintf(buf,"0x%02x",cause);
		return buf;
		}
	}
}

void
Xdropconn (struct conninfo *conn, int deb_line);
#define dropconn(x) Xdropconn((x),__LINE__)

void rdropconn (struct conninfo *conn, int deb_line) {
	conn->ignore=2; dropconn(conn); }

void
Xdropconn (struct conninfo *conn, int deb_line)
{
	chkone(conn);
	if(conn->locked) {
		printf ("DropConn %d: LOCK %d/%d/%ld\n", deb_line, conn->minor, conn->fminor, conn->connref);
		return;
	}
	printf ("DropConn %d: %d/%d/%ld\n", deb_line, conn->minor, conn->fminor, conn->connref);
	if(!conn->ignore) {
		conn->ignore=1;
		setconnstate(conn,c_forceoff);
#if 0
		if(conn->state > c_off)
			setconnstate(conn, c_off);
		else
			ReportConn(conn);
#endif
		timeout(rdropconn,conn,HZ*60*5);
		return;
	} else if(conn->ignore == 1) {
		setconnstate(conn,c_forceoff);
		return;
	} else
		setconnstate(conn,c_forceoff);
	if (theconn == conn)
		theconn = conn->next;
	else {
		struct conninfo *sconn;

		for (sconn = theconn; sconn != NULL; sconn = sconn->next) {
			if (sconn->next == conn) {
				sconn->next = conn->next;
				break;
			}
		}
	}
	{
		struct proginfo *run, *frun;

		for (run = conn->run; run != NULL; run = frun) {
			frun = run->next;
			free (run);
		}
	}
	{
		char xs[10];
		sprintf(xs,"-%d",conn->seqnum);
		connreport(xs);
	}
	dropgrab(conn->cg);
	free(conn);
}

void
deadkid (void)
{
	int pid, val = 0, has_dead = 0;
	struct conninfo *conn;

	while((pid = wait4 (-1,&val,WNOHANG,NULL)) > 0) {
		printf ("\n* PID %d died, %x\n", pid, val);

		chkall();
		for (conn = theconn; conn != NULL; conn = conn ? conn->next : NULL) {
			if(conn->ignore)
				continue;
			if (conn->pid == pid) {
				conn->pid = 0;
				if(conn->flags & F_PERMANENT)
					has_dead = 1;
				if (conn->minor == 0)
					dropconn (conn);
				else
					ReportConn(conn);
				break;
			} else {
				struct proginfo **prog;

				for (prog = &conn->run; prog != NULL && *prog != NULL; prog = &(*prog)->next) {
					if ((*prog)->pid == pid) {
						struct proginfo *fprog = *prog;
						struct proginfo *sprog;

						*prog = fprog->next;
						fprog->pid = 0;
						if (fprog->die) {
							for (sprog = conn->run; sprog != NULL; sprog = sprog->next) {
								if (sprog->die)
									break;
							}
#if 0
							if (sprog == NULL)
								kill (SIGTERM, conn->pid);
#endif
						}
						free (fprog);
						conn = NULL;
						break;
					}
				}
			}
		}
	}
	if(has_dead) {
		in_boot=1;
		do_run_now++;
		timeout(run_now,NULL,3*HZ);
	}
	signal (SIGCHLD, (sigfunc__t) deadkid);
}


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

cf
getcards(conngrab cg, cf list)
{
	if(cg->card == NULL)
		return NULL;
	if(cg->site == NULL)
		return NULL;
	if(cg->protocol == NULL)
		return NULL;
	if(cg->cclass == NULL)
		return NULL;
	while(list != NULL) {
		if(!wildmatch(list->site,cg->site))
			continue;
		if(!wildmatch(list->protocol,cg->protocol))
			continue;
		if(!wildmatch(list->card,cg->card))
			continue;
		if(!classmatch(list->cclass,cg->cclass))
			continue;
		if(!matchflag(cg->flags,list->type))
			continue;
		return list;
	}
	return NULL;
}


void Xbreak(void) { }

static char *
pmatch1 (cf prot, conngrab *cgm)
{
	char *sit, *pro, *cla, *car;
	char first = 1;
	conngrab cg = *cgm;

	chkone(prot); chkone(*cgm);
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
			switch (id) {
			case ARG_INNUMS  : nrt=ARG_IN;  break;
			case ARG_OUTNUMS : nrt=ARG_OUT; break;
			case ARG_BOTHNUMS: nrt=ARG_IN|ARG_OUT; break;
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
						if(!match_suffix(cgc->lnrsuf,yy)) { if(cgc->flags & F_OUTGOING) { printf("  SuffixBadness3  "); Xbreak(); } else DG("2WrongLNrSuffix 2") }
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
#define CHKI(_what,_t)														\
				{ long xx,yy; ushort_t id2;									\
					yy = 0;													\
					if(m_get##_what(cand,(_t *)&yy) != 0) break;			\
					if(cgc->par_in != NULL) {								\
						while(m_getsx(cgc->par_in,&id2) == 0) {				\
							if(id != id2) continue;							\
							xx = 0;											\
							if(m_get##_what (cgc->par_in,(_t *)&xx) != 0) continue;	\
							if(xx == yy) goto found;						\
							if(!strchr(prot->type,'F') && !first) break;	\
							cgc->par_in->b_rptr = mbs_in;					\
							freeb(cand);									\
							dropgrab(cgc); dropgrab(cg); return "3ERR FIND Match Arg";		\
						}													\
					}														\
				} 															/**/
#define CHKO(_what,_t)														\
				{ long xx,yy; ushort_t id2;									\
					yy = 0;													\
					if(m_get##_what(cand,(_t *)&yy) != 0) break;			\
					if(cgc->par_out != NULL) {								\
						while(m_getsx(cgc->par_out,&id2) == 0) {			\
							if(id != id2) continue;							\
							xx = 0;											\
							if(m_get##_what (cgc->par_out,(_t *)&xx) != 0) continue;	\
							if(xx == yy) goto found;						\
							if(!strchr(prot->type,'F') && !first) break;	\
							cgc->par_out->b_rptr = mbs_out;					\
							freeb(cand);									\
							dropgrab(cgc); dropgrab(cg); return "3ERR FIND Match Arg";		\
						}													\
						if(!(cgc->flags & F_OUTCOMPLETE)) {					\
							m_putsx(cgc->par_out,id);						\
							m_put##_what(cgc->par_out,*(_t *)&yy);			\
						}													\
					}														\
				} 															/**/
#define CHK(_what,_t) { \
			if((nrt & ARG_IN) && (cgc->par_in != NULL)) CHKI(_what,_t) \
			if((nrt & ARG_OUT)&& (cgc->par_out!= NULL)) CHKO(_what,_t) } break

#define CHKVI()																\
	{ int xx,yy,xm; streamchar *vx,*vy,*vm; ushort_t id2;					\
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
					{ free(vx); free(vy); if(xm>0)free(vm); goto found; }	\
				if(!strchr(prot->type,'F') && !first) continue;				\
				cgc->par_in->b_rptr = mbs_in;								\
				freeb(cand);												\
				free(vx); free(vy); if(xm>0)free(vm);						\
				dropgrab(cgc); dropgrab(cg); return "3ERR FIND Match Arg";					\
			}																\
			free(vy); if(xm>0)free(vm);										\
		}																	\
	}				 														/**/
#define CHKVO()																\
	{ int xx,yy,xm; streamchar *vx,*vy,*vm; ushort_t id2;					\
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
					{ free(vx); free(vy); if(xm>0)free(vm); goto found; }	\
				if(!strchr(prot->type,'F') && !first) continue;				\
				cgc->par_out->b_rptr = mbs_out;								\
				freeb(cand);												\
				free(vx); free(vy); if(xm>0)free(vm);						\
				dropgrab(cgc); dropgrab(cg); return "3ERR FIND Match Arg";					\
			}																\
			if(!(cgc->flags & F_OUTCOMPLETE)) {								\
				m_putsx(cgc->par_out,id);									\
				m_puthex(cgc->par_out,vy,yy);								\
			}																\
			free(vy); if(xm>0)free(vm);										\
		}																	\
	} 				 														/**/

#define CHKV() { \
				if((nrt & ARG_IN) && (cgc->par_in != NULL)) CHKVI() \
				if((nrt & ARG_OUT)&& (cgc->par_out!= NULL)) CHKVO() } break
#define CHKX()																\
				{ ushort_t id2;												\
					if(cgc->par_out != NULL) {								\
						while(m_getsx(cgc->par_out,&id2) == 0) {			\
							if(id != id2) continue;							\
							goto found;										\
						}													\
						if(!(cgc->flags & F_OUTCOMPLETE))					\
							m_putsx(cgc->par_out,id);						\
					}														\
				} break			  											/**/
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
			case ARG_PREFOUT:    cgc->flags |= F_PREFOUT;    goto argdup;
			case ARG_INT:        cgc->flags |= F_INTERRUPT;  break;
			case ARG_SPV:
			case ARG_FORCETALK:
			  argdup:
				CHKX();
			}
		  found:
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

static char *
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


static char *
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
				errstr = "4CARD UNKNOWN";
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
							errstr = "4NrLocMatch";
						}
						continue;
					}
				} else if(0) { /* Hmmm... */
					cg->lnr = build_nr(dl->arg,dl->arg,((cg->flags&F_INCOMING) && (dp->args != NULL)) ? dp->args : dp->arg, 0);
					if(0)printf("BuildL %s,%s,%s,%d -> %s\n",dl->arg,dl->arg,((cg->flags&F_INCOMING) && (dp->args != NULL)) ? dp->args : dp->arg, 0, cg->lnr);
					if(cg->lnr == NULL) {
						if(*errstr > '4') {
							dropgrab(errcg); errcg = cg; cg->refs++;
							errstr="4LocNrMatch";
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

static char *
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
				if((card = wildmatch(st,cg->card)) == NULL)
					return "CARD MISMATCH";
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


#if 0
static mblk_t *
getprot (char *protocol, char *site, char *cclass, char *suffix)
{
	cf prot;
	mblk_t *mi;

	for (prot = cf_P; prot != NULL; prot = prot->next) {
		if (site != NULL && !wildmatch (site, prot->site))
			continue;
		else if (site == NULL)
			site = prot->site;
		if (protocol != NULL && !wildmatch (protocol, prot->protocol))
			continue;
		else if (*protocol == NULL)
			protocol = prot->protocol;
		if (cclass != NULL && !classmatch (cclass, prot->cclass))
			continue;
		else if (cclass == NULL)
			cclass = prot->cclass;
		break;
	}
	if (prot == NULL)
		return NULL;

	if ((mi = allocb (256, BPRI_MED)) == NULL)
		return NULL;
	for (; prot != NULL; prot = prot->next) {
		ushort_t id;
		char *newlim = NULL;
		mblk_t *mz = NULL;

		if (site != prot->site && (site = wildmatch (site, prot->site)) == NULL) 
			continue;
		if (protocol != prot->protocol && (protocol = wildmatch (protocol, prot->protocol)) == NULL) 
			continue;
		if (cclass != prot->cclass && (cclass = classmatch (protocol, prot->protocol))) 
			continue;
		mz = allocsb (strlen (prot->args), (streamchar *)prot->args);

		newlim = mz->b_wptr;
		while (m_getsx (mz, &id) == 0) {
			switch (id) {
			case ARG_NUMBER:
				if (*suffix != 0)
					m_getstr (mz, suffix, MAXNR);
				break;
			default:
				{
					ushort_t id2;
					char *news = NULL;
					char *olds = mi->b_rptr;

					while (m_getsx (mi, &id2) == 0) {
						if (id != id2)
							continue;
						mi->b_rptr = olds;
						goto skip;
					}
					mi->b_rptr = olds;
					m_putsx (mi, id);
					olds = mi->b_wptr;
					*olds++ = ' ';
					m_getskip (mz);
					news = mz->b_rptr;
					while (news < newlim && olds < mi->b_datap->db_lim
							&& *news != ':')
						*olds++ = *news++;
					mi->b_wptr = olds;
					mz->b_rptr = news;
				} break;
			}
		  skip:;
		}
		freeb (mz);
		if (strchr(prot->type,'X'))
			break;
	}
	return mi;
}
#endif

static int
pushprot (conngrab cg, int minor, char update)
{
	cf prot;
	char *mods = NULL;

	for (prot = cf_ML; prot != NULL; prot = prot->next) {
		if(!matchflag(cg->flags,prot->type)) continue;
		if (!wildmatch (cg->site, prot->site)) continue;
		if (!wildmatch (cg->protocol, prot->protocol)) continue;
		if (!wildmatch (cg->card, prot->card)) continue;
		if (!classmatch (cg->cclass, prot->cclass)) continue;
		break;
	}
	if (prot == NULL)
		return ENOENT;
	if(update) 
		cg->flags = (cg->flags & ~F_SETINITIAL) | F_SETLATER;
	else
		cg->flags = (cg->flags & ~F_SETLATER) | F_SETINITIAL;
	if (minor != 0) {
		char *sp1, *sp2;
		char *sx;

		if(!update) {
			mblk_t *mj = allocb (40 + strlen (prot->args), BPRI_LO);
			int len;
			mods = prot->args;

			m_putid (mj, CMD_PROT);
			m_putsx (mj, ARG_MINOR);
			m_puti (mj, minor);
			m_putdelim (mj);
			m_putid (mj, PROTO_MODLIST);
			m_putdelim (mj);
			m_putsz(mj, mods);
			len = mj->b_wptr - mj->b_rptr;
			DUMPW (mj->b_rptr, len);
			(void) strwrite (xs_mon, (uchar_t *) mj->b_rptr, &len, 1);
			freeb (mj);
		}
		sx = (char *)malloc (strlen (prot->args) + 5 + strlen (PROTO_NAME));
		if (sx == NULL)
			return ENOMEM;
		sprintf (sx, " %s %s", prot->args, PROTO_NAME);
		sp1 = sx;
		while (*sp1 != '\0' && !isspace (*sp1))
			sp1++;
		while (*sp1 != '\0' && isspace (*sp1))
			sp1++;
		for (sp2 = sp1; *sp1 != '\0'; sp1 = sp2) {
			cf cm;
			mblk_t *mi;

			while (*sp2 != '\0' && !isspace (*sp2))
				sp2++;
			if(*sp2 != '\0')
				*sp2++ = '\0';
			if ((mi = allocb (256, BPRI_MED)) == NULL) {
				free (sx);
				return ENOMEM;
			}
			for (cm = cf_MP; cm != NULL; cm = cm->next) {
				ushort_t id;
				streamchar *newlim = NULL;
				mblk_t *mz = NULL;

				if(!matchflag(cg->flags,cm->type)) continue;
				if (!wildmatch (cg->site, cm->site)) continue;
				if (!wildmatch (cg->protocol, cm->protocol)) continue;
				if (!wildmatch (cg->card, cm->card)) continue;
				if (!classmatch (cg->cclass, cm->cclass)) continue;
				if (!wildmatch (sp1, cm->arg)) continue;

				mz = allocsb (strlen (cm->args), (streamchar *)cm->args);

				newlim = mz->b_wptr;
				while (m_getsx (mz, &id) == 0) {
					switch (id) {
					default:
						{
							ushort_t id2;
							streamchar *news = NULL;
							streamchar *olds = mi->b_rptr;

							while (m_getsx (mi, &id2) == 0) {
								if (id != id2)
									continue;
								mi->b_rptr = olds;
								goto skip;
							}
							mi->b_rptr = olds;
							m_putsx (mi, id);
							olds = mi->b_wptr;
							*olds++ = ' ';
							m_getskip (mz);
							news = mz->b_rptr;
							while (news < newlim && olds < mi->b_datap->db_lim
									&& *news != ':')
								*olds++ = *news++;
							mi->b_wptr = olds;
							mz->b_rptr = news;
						} break;
					}
				  skip:;
				}
				freeb (mz);
				if (strchr(cm->type,'X'))
					break;
			}
			if (mi->b_rptr < mi->b_wptr) {
				struct iovec io[2];
				mblk_t *mj = allocb (50, BPRI_LO);

				m_putid (mj, CMD_PROT);
				m_putsx (mj, ARG_MINOR);
				m_puti (mj, minor);
				m_putdelim (mj);
				m_putid (mj, PROTO_MODULE);
				m_putsx (mj, PROTO_MODULE);
				m_putsz (mj, (uchar_t *) sp1);	/* Delimiter to mi pushed by
												 * m_putsx */
				io[0].iov_base = mj->b_rptr;
				io[0].iov_len = mj->b_wptr - mj->b_rptr;
				io[1].iov_base = mi->b_rptr;
				io[1].iov_len = mi->b_wptr - mi->b_rptr;
				DUMPW (mj->b_rptr, io[0].iov_len);
				printf ("+ ");
				DUMPW (mi->b_rptr, io[1].iov_len);
				(void) strwritev (xs_mon, io, 2, 1);
				freeb (mj);
			}
			freeb (mi);
		}
		free (sx);
		if(!update) {
			mblk_t *mj = allocb (32, BPRI_LO);
			int len;

			if (mj == NULL)
				return ENOMEM;
			m_putid (mj, CMD_PROT);
			m_putsx (mj, ARG_MINOR);
			m_puti (mj, minor);
			m_putdelim (mj);
			m_putc (mj, PROTO_MODE);
			len = mj->b_wptr - mj->b_rptr;
			DUMPW (mj->b_rptr, len);
			(void) strwrite (xs_mon, (uchar_t *) mj->b_rptr, &len, 1);
			freeb (mj);
		}

	}
	return 0;
}


static int
pushcardprot (conngrab cg, int minor)
{
	cf prot;
	cf cmod = NULL;

	for (prot = cf_ML; prot != NULL; prot = prot->next) {
		if(!matchflag(cg->flags,prot->type)) continue;
		if (!wildmatch (cg->site, prot->site)) continue;
		if (!wildmatch (cg->protocol, prot->protocol)) continue;
		if (!wildmatch (cg->card, prot->card)) continue;
		if (!classmatch (cg->cclass, prot->cclass)) continue;

		for (cmod = cf_CM; cmod != NULL; cmod = cmod->next) {
			if (!wildmatch (cg->card, cmod->card)) continue;
			if (!wildmatch(prot->arg,cmod->arg)) continue;
			break;
		}
		if (cmod != NULL)
			break;
	}
	if (prot == NULL)
		return ENOENT;
	if (minor != 0) {
		mblk_t *mj = allocb (32, BPRI_LO);
		int len;

		if (mj == NULL)
			return ENOMEM;
		m_putid (mj, CMD_CARDSETUP);
		m_putsx (mj, ARG_MINOR);
		m_puti (mj, minor);
		m_putdelim (mj);
		m_putc (mj, PROTO_MODE);
		m_puti (mj, cmod->num);
		m_puti (mj, prot->num);
		len = mj->b_wptr - mj->b_rptr;
		DUMPW (mj->b_rptr, len);
		(void) strwrite (xs_mon, (uchar_t *) mj->b_rptr, &len, 1);
		freeb (mj);
	}
	return 0;
}


void
xquit (const char *s, const char *t)
{
	if (s != NULL)
		syslog (LOG_WARNING, "%s %s: %m", s, t ? t : "");
	exit (4);
}

int quitnow = 0;

void panic(const char *x, ...)
{
	*((char *)0xdeadbeef) = 0; /* Crash */
}


struct conninfo *
startconn(conngrab cg, int fminor, int connref, char **ret)
{
	struct iovec io[3];
	int iovlen = 0;
	streamchar data[MAXLINE];
	mblk_t *xx, yy;
	struct datab db;
	struct conninfo *conn;
	char *str;

	yy.b_rptr = data;
	yy.b_wptr = data;
	db.db_base = data;
	db.db_lim = data + sizeof (data);
	yy.b_datap = &db;

	if(ret == NULL)
		ret = &str;
	*ret = NULL;
	chkall();
	cg->refs++;
	for(conn = theconn; conn != NULL; conn = conn->next) {
		if(conn->ignore)
			continue;
		if(conn->minor == 0)
			continue;
		if(conn->cg == cg)
			break;
	}
	if(conn == NULL) {
		for(conn = theconn; conn != NULL; conn = conn->next) {
			char *sit,*pro,*car,*cla;

			if(conn->ignore)
				continue;
			if(conn->pid == 0 || conn->minor == 0)
				continue;
			if(!(conn->flags & F_PERMANENT))
				continue;
			if(conn->cg == NULL)
				continue;
			if((sit = wildmatch(conn->cg->site,cg->site)) == NULL) continue;
			if((pro = wildmatch(conn->cg->protocol,cg->protocol)) == NULL) continue;
			if((car = wildmatch(conn->cg->card,cg->card)) == NULL) continue;
			if((cla = classmatch(conn->cg->cclass,cg->cclass)) == NULL) continue;
			cg->site = sit; cg->protocol = pro; cg->card = car; cg->cclass = cla;
			break;
		}
	}
	if(conn == NULL) {
		for(conn = theconn; conn != NULL; conn = conn->next) {
			char *sit,*pro;

			if(conn->ignore)
				continue;
			if(conn->pid == 0 || conn->minor == 0)
				continue;
			if(!(conn->flags & F_PERMANENT))
				continue;
			if(conn->cg == NULL)
				continue;
			if((sit = wildmatch(conn->cg->site,cg->site)) == NULL) continue;
			if((pro = wildmatch(conn->cg->protocol,cg->protocol)) == NULL) continue;
			cg->site = sit; cg->protocol = pro;
			break;
		}
	}
	if(conn == NULL) {
		dropgrab(cg);
		return NULL;
	}

	/* Returning "+" int he first position means keep the new connection. */ 

	if(conn->state == c_forceoff) {
		dropgrab(cg);
		*ret = "-COLLISION 1a";
		return conn;
	}
	if(conn->state == c_going_down) {
		dropgrab(cg);
		*ret = "-COLLISION 1b";
		return conn;
	}
	if(conn->state > c_going_down) {
		*ret = "+COLLISION 1b";
		if((conn->state == c_going_up) && (cg->flags & F_PREFOUT))
			**ret = '-';
		if((conn->state == c_up) && (cg->flags & (F_PREFOUT | F_FORCEOUT)))
			**ret = '-';
		dropgrab(cg);
		return conn;
	}

printf("Start: %s:%s #%s...",cg->site,cg->protocol,cg->nr);
	if(((*ret) = findit (&cg)) != NULL) {
		dropgrab(cg);
		chkall();
		return NULL;
	}
	chkall();
	dropgrab(conn->cg);
	conn->cg = cg;
	chkone(cg);

	if (cg->flags & F_INCOMING) {
		m_putid (&yy, CMD_ANSWER);
		xx = cg->par_in;
	} else if(cg->flags & F_OUTGOING) {
		m_putid (&yy, CMD_DIAL);
		xx = cg->par_out;
	} else {
		*ret = "NEITHER IN NOR OUT";
		return NULL;
	}
	m_putsx (&yy, ARG_DELAY);
	m_puti (&yy, cg->delay);
	if(cg->flags & F_OUTGOING) {
		m_putsx(&yy,ARG_NOCONN);
		setconnref(conn,connrefs);
		connrefs += 2;
	} else if(connref != 0) {
		if(conn->connref != 0 && conn->state == c_up) {
			*ret = "COLLISION 2";
			return conn;
		}
		/* setconnref(conn,connref); */
	}
	
	m_putsx (&yy, ARG_MINOR);
	m_puti (&yy, conn->minor);
	if(fminor != 0)
		conn->fminor = fminor;
#if 1
	if (conn->fminor != 0) {
		m_putsx (&yy, ARG_FMINOR);
		m_puti (&yy, conn->fminor);
	}
#endif
	chkone(cg);
	if (cg->flags & F_INTERRUPT)
		m_putsx (&yy, ARG_INT);

	if (connref != 0 || conn->connref != 0) {
		m_putsx (&yy, ARG_CONNREF);
		m_puti (&yy, connref ? connref : conn->connref);
	}

	if (cg->lnrsuf != '\0') {
		char *s = cg->lnrsuf;
		m_putsx (&yy, ARG_LNUMBER);
		m_putsz (&yy, s);
	}
	if (cg->nr != '\0') {
		char *s = strip_nr(cg->nr);
		printf("Strip3 %s -> %s\n",cg->nr,s);
		if(s == NULL) {
			s = append_nr(cg->nr,cg->nrsuf);
			printf("Append3 %s,%s -> %s\n",cg->nr,cg->nrsuf,s);
		}
		if(s != NULL) {
			m_putsx (&yy, ARG_NUMBER);
			m_putsz (&yy, s);
		}
	}
	if (cg->protocol != NULL
			&& strchr (cg->protocol, '*') == NULL) {
		m_putsx (&yy, ARG_STACK);
		m_putsz (&yy, cg->protocol);
	}
	m_putsx (&yy, ARG_CARD);
	m_putsz (&yy, cg->card);
#if 0
	if (strchr (type, 'H') != NULL)
		m_putsx (&yy, ARG_SUPPRESS);
#endif

	io[0].iov_base = yy.b_rptr;
	io[0].iov_len = yy.b_wptr - yy.b_rptr;
	if (xx != NULL) {
		io[1].iov_base = xx->b_rptr;
		io[1].iov_len = xx->b_wptr - xx->b_rptr;
		iovlen = 2;
	} else
		iovlen = 1;
#if 0
	if(cg->par_out != NULL) {
		io[iovlen].iov_base = cg->par_out->b_rptr;
		io[iovlen].iov_len = cg->par_out->b_wptr - cg->par_out->b_rptr;
		iovlen++;
	}
#endif
	DUMPW (yy.b_rptr, io[0].iov_len);
	if (iovlen > 1) {
		printf ("+ ");
		DUMPW (xx->b_rptr, io[1].iov_len);
		if(iovlen > 2) {
			printf ("+ ");
			DUMPW (cg->par_out->b_rptr, io[2].iov_len);
		}
	}
	setconnstate(conn,c_going_up);
	(void) strwritev (xs_mon, io, iovlen, 1);
	chkone(conn);
	*ret = NULL;
	return conn;
}

static struct conninfo *zzconn = NULL;
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

char *
runprog (cf cfr, struct conninfo *conn, conngrab *foo)
{
	int pid = 0;
	int pip[2];
	int dev2;
	int xlen;

	streamchar data[MAXLINE];
	mblk_t yy;
	struct datab db;
	struct proginfo *prog = NULL;
	int dev = 0;
	conngrab cg;

	yy.b_rptr = data;
	yy.b_wptr = data;
	db.db_base = data;
	db.db_lim = data + sizeof (data);
	yy.b_datap = &db;

	chkall();
	cg = *foo;
	if (cfr != NULL) {
		static int sdev = 0;
		if(sdev == 0) sdev = getpid()%(NPORT/2)+(NPORT/4);
		for(; sdev<NPORT*2; sdev++) {
			if((sdev % NPORT) == 0)
				continue;
			if(lockdev(sdev % NPORT,0)<0)
				continue;
			/* We have a device */
			break;
		}
		if (sdev == NPORT*2) {
			dev = sdev = sdev % NPORT;
			syslog(LOG_ERR,"No free devices for ISDN (no spool dir?)");
			return "NO FREE DEVICES";
		}
		dev = sdev = sdev % NPORT;
		sdev++;
	}
	if (cfr != NULL && cg != NULL) {		  /* We're launching a master program */
		if (pipe (pip) == -1) {
			syslog(LOG_CRIT,"Pipe: %m");
			return "NO PIPE";
		}

		{
			char *err;

			if((err = findit (foo)) != NULL) {
				if(conn != NULL)
					free(conn);
				return err;
			}
			cg = *foo;

			if(conn == NULL) {
				conn = malloc(sizeof(*conn));
				if(conn == NULL) {
					return "NO MEMORY.1";
				}

				bzero(conn,sizeof(*conn));
				conn->seqnum = ++connseq;
				conn->state = c_down;
				conn->cause = 999999;
			}
			cg->refs++;
			dropgrab(conn->cg);
			conn->cg = cg;
			conn->flags |= conn->cg->flags;
			conn->pid = (pid_t)~0;
			ReportConn(conn);
		}
	} else {
		prog = (struct proginfo *) malloc (sizeof (struct proginfo));

		if (prog == NULL)
			return "NO MEMORY.2";
		bzero (prog, sizeof (prog));
		prog->master = conn;
	}
	callout_sync ();
	if (conn != NULL) {
		cg->refs++;
		dropgrab(conn->cg);
		conn->cg = cg;
		conn->locked = 1;
		conn->minor = dev;
		conn->next = theconn;
		theconn = conn;
	}
	backrun(-1); backrun(-1);
    signal (SIGCHLD, SIG_DFL);
	switch ((pid = fork ())) {
	case -1:
		if(0)callout_async ();
		return "CANT FORK";
	case 0:
		{					  /* child. Go to some lengths to detach us. */
			char *ap;
			int arc;
			char *arg[99];
			int devfd;

			zzconn = conn;
			alarm(15);
			signal(SIGALRM,(void *)dropdead);
			if (cfr != NULL && cg != NULL) {
				close (pip[0]);
				fcntl (pip[1], F_SETFD, 1);		/* close-on-exec */
			}
			close (fd_mon);
			close (0);
			close (1);
			if(strchr(cfr->type,'S') != NULL)
				close (2);
			if (strchr (cfr->type, 'Q') != NULL) {	/* Trace */
				if(fork() == 0) {
					char *svarg[10], **sarg = svarg; int sarc = 0;
					char pid[10], pfile[15];

					alarm(0);
					sprintf(pid,"%d",getppid());
					sprintf(pfile,"/tmp/follow.%d",getppid());

					if (fork() != 0) _exit(0);
					sleep(1);

					sarg[sarc++] = "/bin/strace";
					sarg[sarc++] = "-s";
					sarg[sarc++] = "240";
					sarg[sarc++] = "-f";
					sarg[sarc++] = "-p";
					sarg[sarc++] = pid;
					sarg[sarc] = NULL;
					if(strchr(cfr->type,'S') == NULL)
						close(2);
					open("/dev/null",O_RDWR);
					open(pfile,O_WRONLY|O_CREAT,0600);
					dup(1);
					execv("/bin/strace",sarg);
				}
				sleep(3);
			}
#ifdef HAVE_SETPGRP_2
			setpgrp (0, conn->minor ? conn->pid : getpid ());
#endif
#ifdef HAVE_SETPGRP_0
			setpgrp();
#endif
#ifdef DO_HAVE_SETSID
			setsid();
#endif
#ifdef TIOCNOTTY
			{
				int a = open ("/dev/tty", O_RDWR);

				if (a >= 0) {
					(void) ioctl (a, TIOCNOTTY, NULL);
					(void) close (a);
				}
			}
#endif
			if (cfr != NULL && cg != NULL) {
				if (strchr(cfr->type,'T') != NULL) {
					mknod (devname (dev), S_IFCHR | S_IRUSR | S_IWUSR, MKDEV(isdnterm,dev));
					chown (devname (dev), cfr->num, cfr->num2);
					chmod (devname (dev), S_IRUSR | S_IWUSR | S_IWGRP);
					devfd = open(devname(dev), O_RDWR | O_EXCL
#ifdef O_GETCTTY
										| O_GETCTTY
#endif
						);
				} else {
					devfd = open(idevname(dev), O_RDWR | O_EXCL
#ifdef O_NOCTTY
										| O_NOCTTY
#endif
						);
				}
				if (devfd < 0) {
					syslog(LOG_CRIT,"unable to open %s: %m",(strchr(cfr->type,'T') != NULL)?devname(dev):idevname(dev));
					unlockdev(dev);
					_exit (1);
				}
				if (strchr (cfr->type, 'C') != NULL) {	/* Open a second device
														 * for talking. Pass
														 * minor of first
														 * device. */
					int dev2;
					for(dev2=1; dev2<NPORT; dev2++) {
						if(lockdev(dev2,0)<0)
							continue;
						/* We have a device */
						break;
					}
					if (dev2 == NPORT) {
						syslog(LOG_ERR,"No free devices2 for ISDN");
						_exit(1);
					}
					write (pip[1], &dev, sizeof (dev));
					if ((devfd = open (devname(dev2), O_RDWR
#ifdef O_NOCTTY
											| O_NOCTTY
#endif
									))!= 1) {
						syslog(LOG_CRIT,"unable to open %s: %m",devname(dev2));
						_exit (1);
					}
				} else
					dup2 (devfd, 1);
			} else {
				if ((devfd = open ("/dev/null", O_RDWR)) != 0) {
					syslog(LOG_CRIT,"Unable to open /dev/null: %m");
					_exit (1);
				}
				dup2 (devfd, 1);
			}
			if (strchr (cfr->type, 'S') != NULL) 
				dup2 (devfd, 2);
			if (cfr != NULL && cg != NULL) {
				struct termio t;
				if (strchr (cfr->type, 'T') != NULL) {
					struct termio tt = {IGNBRK | ICRNL, OPOST | ONLCR, B38400 | CS8 | CREAD | HUPCL, ICANON | ECHO | ECHOE | ECHOK | ISIG, 0, {0x03, 0x02, 0x7F, 0x15, 0x04, 0, 0, 0}};
					t = tt;

#ifndef linux
					if(ioctl (0, I_PUSH, "line") != 0)
						syslog(LOG_ERR,"Unable to push line discipline! %m");
#endif
				} else {
					struct termio tt =
					{IGNBRK, 0, B38400 | CS8 | CREAD | HUPCL, 0, 0, { 0, }};
					t = tt;
				}
				(void) ioctl (0, TCSETA, &t);

				if (strchr (cfr->type, 'U') != NULL) {
					struct utmp ut;

					bzero (&ut, sizeof (ut));
					strncpy (ut.ut_id, sdevname (dev), sizeof (ut.ut_id));
					strncpy (ut.ut_line, mdevname (dev), sizeof (ut.ut_line));
#ifndef M_UNIX
					strncpy (ut.ut_host, cfr->protocol, sizeof (ut.ut_host));
#endif
					ut.ut_pid = getpid ();
					ut.ut_type = LOGIN_PROCESS;
					ut.ut_time = time(NULL);
					getutline (&ut);
					pututline (&ut);
					endutent ();
					{
						int wf = open ("/etc/wtmp", O_WRONLY | O_APPEND);

						if (wf >= 0) {
							(void) write (wf, &ut, sizeof (ut));
							close (wf);
						}
					}
				}
				write (pip[1], &dev, sizeof (dev));
			}
			setregid (cfr->num2, cfr->num2);
			setreuid (cfr->num, cfr->num);
			if(conn != NULL && conn->cg != NULL) {
				if(conn->cg->site != NULL)
					putenv2 ("SITE", conn->cg->site);
				if(conn->cg->protocol != NULL)
					putenv2 ("PROTOCOL", conn->cg->protocol);
				if(conn->cg->cclass != NULL)
					putenv2 ("CLASS", conn->cg->cclass);
				if(conn->cg->nr != NULL)
					putenv2 ("PHONE", conn->cg->nr);
				putenv2 ("DIRECTION", (conn->flags & F_INCOMING) ? "IN" : "OUT");
				if(conn->minor != 0)
					putenv2 ("DEVICE", devname (conn->minor));
			}

			arc = 0;
			ap = cfr->args;
			if (strchr (cfr->type, '$') == NULL) {
				while (*ap != '\0') {
					arg[arc++] = ap;
					while (*ap != '\0' && !isspace (*ap))
						ap++;
					if (*ap == '\0')
						break;
					*ap++ = '\0';
					while (*ap != '\0' && isspace (*ap))
						ap++;
				}
			} else {
				arg[arc++] = "/bin/sh";
				arg[arc++] = "-c";
				arg[arc++] = ap;
			}
			arg[arc] = NULL;
			ap = *arg;
			if ((*arg = strrchr (ap, '/')) == NULL)
				*arg = ap;
			else
				(*arg)++;
			alarm(0);
			execv (ap, arg);
			syslog (LOG_ERR, "Could not execute %s for %s/%s: %m", *arg,cg->site,cg->protocol);
			if (cfr != NULL && cg != NULL)
				write (pip[1], "EXEC", 4);
			_exit (2);
		}
	default:;
	}

	if(cfr != NULL) {
#ifdef HAVE_SETPGRP_2
		setpgrp (pid, conn->minor ? conn->pid : pid);
#endif
#ifdef HAVE_SETPGRP_0
		setpgrp();
#endif
		conn->minor = dev;
		conn->pid = pid;
	}
	backrun(-1); backrun(-1);
	if (cfr != NULL) {
		int ac, devin;

		close (pip[1]);
		backrun(pip[0]);
		if ((strchr (cfr->type, 'C') != NULL) && ((ac=read (pip[0], &dev2, sizeof (dev2))) != sizeof (dev2))) {
			syslog (LOG_ERR, "%s: not opened 1: %d %m", cfr->args, ac);
			if(0)callout_async ();
			conn->minor = 0;
			if (conn->pid == 0)
				dropconn (conn);
			deadkid(); return "OPEN ERR";
		}
		backrun(pip[0]);
		if ((ac=read (pip[0], &devin, sizeof (devin))) != sizeof (dev)) {
			if(ac<0)
				syslog (LOG_ERR, "%s: not opened 2: %d %m", cfr->args,ac);
			else
				syslog (LOG_ERR, "%s: not opened 2: %d", cfr->args,ac);
			if(0)callout_async ();
			conn->minor = 0;
			if (conn->pid == 0)
				dropconn (conn);
			deadkid();
			return "OPEN ERR";
		}
		if(devin != dev)
			syslog (LOG_ERR, "%s: device %d != %d", cfr->args,devin,dev);
		if (strchr (cfr->type, 'C') == NULL)
			dev2 = dev;
#if 0
		if ((ylen = read (pip[0], msg, sizeof (msg) - 1)) > 0) {
			msg[ylen] = '\0';
			close (pip[0]);
			syslog (LOG_ERR, "%s: not executed", cfr->args);
			if(0)callout_async ();
			deadkid(); return msg;
		}
#endif
		close (pip[0]);
	}
	syslog (LOG_INFO, "exec %x:%x %d %s/%s %s", dev, dev2, pid, cg->site,cg->protocol, cfr->args);
	printf ("* PID %d\n", pid);

	if (conn != NULL) {
		conn->locked--;
		if(conn->state > c_off)
			setconnstate(conn, c_down);
		if(conn->minor == 0 && conn->pid == 0) 
			dropconn(conn);
		else if (conn->minor == 0 || conn->pid == 0)
			conn = NULL;
	}

	if (conn != NULL) {
		if (cfr != NULL && conn->pid != pid && conn->pid != (pid_t)~0) {
			prog->pid = pid;
			prog->next = conn->run;
			conn->run = prog;
		} else {
			if(cfr != NULL) {
				conn->fminor = (dev == dev2) ? 0 : dev2;
			}

			if(cfr != NULL) {
				m_putid (&yy, CMD_FAKEOPEN);
				m_putsx (&yy, ARG_MINOR);
				m_puti (&yy, conn->minor);
				xlen = yy.b_wptr - yy.b_rptr;
				DUMPW (yy.b_rptr, xlen);
				(void) strwrite (xs_mon, (uchar_t *) yy.b_rptr, &xlen, 1);
				yy.b_wptr = yy.b_rptr;

				if (conn->fminor != 0 && conn->fminor != conn->minor) {
					m_putid (&yy, CMD_FAKEOPEN);
					m_putsx (&yy, ARG_MINOR);
					m_puti (&yy, conn->fminor);
					xlen = yy.b_wptr - yy.b_rptr;
					DUMPW (yy.b_rptr, xlen);
					(void) strwrite (xs_mon, (uchar_t *) yy.b_rptr, &xlen, 1);
					yy.b_wptr = yy.b_rptr;
				}
				m_putid (&yy, CMD_PROT);
				m_putsx (&yy, ARG_FMINOR);
				m_puti (&yy, conn->minor);
				m_putdelim (&yy);
				m_putid (&yy, PROTO_MODULE);
				m_putsx (&yy, PROTO_MODULE);
				m_putsz (&yy, (uchar_t *) "proto");
				m_putsx (&yy, PROTO_ONLINE);
				m_putsx (&yy, PROTO_CARRIER);
				m_puti (&yy, (cfr != NULL && strchr(cfr->type,'B')) ? 0 : 1);
				m_putsx (&yy, PROTO_BREAK);
				m_puti (&yy, 0);

				xlen = yy.b_wptr - yy.b_rptr;
				DUMPW (yy.b_rptr, xlen);
				(void) strwrite (xs_mon, (uchar_t *) yy.b_rptr, &xlen, 1);
				yy.b_wptr = yy.b_rptr;

				if(conn->fminor != 0 && conn->fminor != conn->minor) {
					m_putid (&yy, CMD_PROT);
					m_putsx (&yy, ARG_FMINOR);
					m_puti (&yy, conn->fminor);
					m_putdelim (&yy);
					m_putid (&yy, PROTO_MODULE);
					m_putsx (&yy, PROTO_MODULE);
					m_putsz (&yy, (uchar_t *) "proto");
					m_putsx (&yy, PROTO_ONLINE);
					xlen = yy.b_wptr - yy.b_rptr;
					DUMPW (yy.b_rptr, xlen);
					(void) strwrite (xs_mon, (uchar_t *) yy.b_rptr, &xlen, 1);
					yy.b_wptr = yy.b_rptr;
				}

				if(cg != NULL && (cg->flags & (F_INCOMING|F_OUTGOING))) {
					cg->refs++;
					dropgrab(conn->cg);
					conn->cg = cg;
					{
						char *xd;

						if ((xd = strchr (cfr->type, '.')) != NULL && (conn->flags & F_INCOMING))
							cg->delay = atoi (xd + 1);
						else
							cg->delay = 0;
					}
					if(startconn(conn->cg,0,0, NULL) == conn) 
						setconnstate(conn,c_going_up);
					else {
						syslog(LOG_CRIT,"Bug in runprog->startconn for %s:%s",cg->site,cg->protocol);
						dropgrab(conn->cg);
						conn->cg = NULL;
						chkone(conn);
					}
				} else {
					int err = pushprot (conn->cg, conn->minor, 0);
					if(err != 0) {
printf("NoProtoEnable NotPushprot\n");
						m_putid (&yy, CMD_CLOSE);
						m_putsx (&yy, ARG_MINOR);
						m_puti (&yy, conn->minor);
						xlen = yy.b_wptr - yy.b_rptr;
						DUMPW (yy.b_rptr, xlen);
						(void) strwrite (xs_mon, (uchar_t *) yy.b_rptr, &xlen, 1);
						conn->minor = 0;
						if(conn->pid == 0)
							dropconn(conn);
						else
							kill(conn->pid,SIGHUP);
						return "CANT PUSHPROT";
					}
					if(conn->flags & F_PERMANENT) {
						m_putid (&yy, CMD_PROT);
						m_putsx (&yy, ARG_MINOR);
						m_puti (&yy, conn->minor);
						m_putdelim (&yy);
						m_putid(&yy,PROTO_ENABLE);
						xlen = yy.b_wptr - yy.b_rptr;
						DUMPW (yy.b_rptr, xlen);
						(void) strwrite (xs_mon, (uchar_t *) yy.b_rptr, &xlen, 1);
					}
else printf("NoProtoEnable NotPermanent\n");
					cg->card = str_enter("*"); /* cosmetic */
					ReportConn(conn); /* even more cosmetic... */
				}
			}
		}
	}
	deadkid();
	if(0)callout_async ();
	return NULL;
}

void retime(struct conninfo *conn)
{
	if(conn->retime) {
		int xlen;

		if (conn->flags & F_PERMANENT) {
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

		conn->retime = 0;
		setconnstate(conn,c_down);

	}
}

void time_reconn(struct conninfo *conn)
{
	if(conn->timer_reconn) {
		conn->timer_reconn = 0;
		if(conn->want_reconn) 
			try_reconn(conn);
	}
}

void try_reconn(struct conninfo *conn)
{
	mblk_t *md;
	int xlen;

	chkone(conn);
	if(conn == NULL || conn->state <= c_off)
		return;
	if(conn->state != c_down && conn->state != c_up) {
		conn->want_reconn = 1;
		return;
	}
	if(conn->timer_reconn) {
		conn->want_reconn = 1;
		return;
	}

	if(conn->state != c_down)
		return;

	md = allocb(256,BPRI_LO);
	if(md != NULL) {
		conngrab cg = conn->cg;
		struct conninfo *xconn;
		char *ret = NULL;

		if(cg == NULL)
			return;

		chkone(cg);
		cg->refs++;

		cg->nr = NULL; cg->nrsuf = NULL;
		cg->lnr = NULL; cg->lnrsuf = NULL;
		cg->card = str_enter("*");;
		cg->cclass = str_enter("*");;
		cg->flags &=~(F_INCOMING|F_OUTCOMPLETE|F_NRCOMPLETE|F_LNRCOMPLETE);
		cg->flags |= F_OUTGOING;
		if((cg->flags & (F_PERMANENT|F_LEASED)) == F_PERMANENT)
			cg->flags |= F_DIALUP;
		if(cg->par_out != NULL)
			freemsg(cg->par_out);
		if((cg->par_out = allocb(256,BPRI_LO)) == NULL) {
			dropgrab(cg);
			freeb(md);
			return;
		}
		if(cg->par_in != NULL) {
			freemsg(cg->par_in);
			cg->par_in = NULL;
		}

		/* anything else is added by startconn */

		if((xconn = startconn(cg,0,0, &ret)) == conn) {
			dropgrab(cg);
			freeb(md);
			return;
		}
		dropgrab(cg);
		if(ret != NULL) {
			setconnstate(conn,c_going_up);
			if(!strcmp(ret,"0BUSY")) {
				conn->cause = ID_priv_Busy;
				if ((conn->flags & F_PERMANENT) && (conn->minor != 0)) {
					mblk_t *mb = allocb(30,BPRI_MED);

					setconnstate(conn, c_down);
					m_putid (mb, CMD_PROT);
					m_putsx (mb, ARG_MINOR);
					m_puti (mb, conn->minor);
					m_putdelim (mb);
					m_putid (mb, PROTO_DISABLE);
					xlen = mb->b_wptr - mb->b_rptr;
					DUMPW (mb->b_rptr, xlen);
					(void) strwrite (xs_mon, (uchar_t *) mb->b_rptr, &xlen, 1);
					freeb(mb);
				}
			} else {
				conn->cause = ID_priv_Print;
				conn->causeInfo = ret;
			}
printf("DropThis, %s\n",ret);
			setconnstate(conn,c_off);
			return;
		}

		md->b_rptr = md->b_wptr = md->b_datap->db_base;
		m_putid(md,CMD_PROT);
		m_putsx(md,ARG_MINOR);
		m_puti(md,conn->minor);
		m_putdelim (md);
		m_putid (md, PROTO_DISABLE);
		setconnstate(conn,c_off);

		xlen=md->b_wptr-md->b_rptr;
		DUMPW (md->b_rptr, xlen);
		(void) strwrite (xs_mon, md->b_rptr, &xlen, 1);
		freeb(md);
	} else
		setconnstate(conn,c_off);
}



void
do_info (streamchar * data, int len)
{
	mblk_t xx;
	struct datab db;
	ushort_t id;
	ushort_t ind;
	streamchar ans[MAXLINE];
	char *msgbuf = NULL;
	char *resp = NULL;
	long fminor = 0;
	long minor = 0;
	long callref = 0;
	struct conninfo *conn = NULL;
	char crd[5] = "";
	char prot[20] = "*";
	char nr[MAXNR + 2] = "";
	char lnr[MAXNR + 2] = "";
	long uid = -1;
	long connref = 0;
	char dialin = -1;
	long charge = 0;
	ushort_t cause = 0;
	int xlen;
	int has_force = 0;
	int bchan = -1;
	int hdrval = -1;
	char no_error=0;

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
		return;
	(char *) data = (char *) xx.b_rptr;
	while (m_getsx (&xx, &id) == 0) {
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
						goto err;
					}
				}
			} break;
		case ARG_LNUMBER:
			{
				int err2;

				if ((err2 = m_getstr (&xx, lnr, MAXNR)) != 0) {
					if (err2 != ENOENT && err2 != ESRCH) {
						printf (" XErr 02\n");
						goto err;
					}
				}
			} break;
		case ARG_CARD:
			if (m_getstr (&xx, crd, 4) != 0) {
				printf (" XErr 1\n");
				goto err;
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
				goto err;
			}
			if (minor < 0 || minor >= NMINOR) {
				printf (" XErr 4a\n");
				goto err;
			}
			break;
		case ARG_UID:
			if (m_geti (&xx, &uid) != 0) {
				printf (" XErr u\n");
				goto err;
			}
			break;
		case ARG_ERRHDR:
			if (m_geti (&xx, &hdrval) != 0) {
				printf (" XErr 34\n");
				if(0)goto err;
			}
			break;
		case ARG_FMINOR:
			{
				long fmi;
				if (m_geti (&xx, &fmi) != 0) {
					printf (" XErr 3\n");
					goto err;
				}
				if (fminor != 0 && fminor != fmi) {
					resp = "ILLEGAL FMINOR";
					goto print;
				}
				fminor = fmi;
				if (fminor < 0 || fminor >= NMINOR) {
					printf (" XErr 4b\n");
					goto err;
				}
			} break;
		case ARG_CALLREF:
			if (m_geti (&xx, &callref) != 0) {
				printf (" XErr 5x\n");
				goto err;
			}
			break;
		case ARG_CONNREF:
			if (m_geti (&xx, &connref) != 0) {
				printf (" XErr 5\n");
				goto err;
			}
			break;
		case ARG_CHANNEL:
			if (m_geti (&xx, &bchan) != 0) {
				printf (" XErr 9a\n");
				goto err;
			}
			break;
		case ARG_STACK:
			if (m_getstr (&xx, prot, sizeof(prot)-1) != 0) {
				printf (" XErr 9\n");
				goto err;
			}
			break;
		}
	}
	/* if(ind != IND_OPEN) */ {
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
	(char *) xx.b_rptr = (char *) data;
  redo:
  	if (conn != NULL && conn->cg != NULL) {
		if (conn->cg->nr != NULL && nr[0] == '\0')
			strcpy(nr, conn->cg->nr);
	}
	chkall();
	switch (ind) {
#ifdef DO_BOOT
	case IND_BOOT:
		{
			syslog (LOG_WARNING, "*** Rebooting because of a serious problem ***");
			system ("/etc/telinit 5");
			if (fork ()== 0) {
				sleep (120);
				system ("/etc/down1");
				sleep (120);
				system ("/etc/down2");
				sleep (60);
				system ("/etc/down3");
				sleep (30);
				system ("/etc/reboot");
			}
		}
#endif
	case IND_CARD:
		{
			short cpos;
			cf dl;
			long nbchan;

			if (m_getstr (&xx, crd, 4) != 0)
				goto err;
			if (m_geti (&xx, &nbchan) != 0)
				goto err;
			for (cpos = 0; cpos < cardnum; cpos++) {
				if (!strcmp(cardlist[cpos], crd))
					return;
			}
			if (cardnum >= NCARDS)
				goto err;
			cardlist[cardnum] = str_enter(crd);
			cardnrbchan[cardnum] = nbchan;
			cardnum++;
			for(dl = cf_DL; dl != NULL; dl = dl->next) {
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
		} break;
	case IND_NOCARD:
		{
			short cpos;

			if (m_getstr(&xx, crd, 4) != 0)
				goto err;
			for (cpos = 0; cpos < cardnum; cpos++) {
				if (!strcmp(cardlist[cpos], crd)) {
					--cardnum;
					cardlist[cpos] = cardlist[cardnum];
					cardnrbchan[cpos] = cardnrbchan[cardnum];
					if(cardidx >= cardnum)
						cardidx = 0;
					return;
				}
			}
			goto err;
		} break;
	case IND_CARDPROTO:
		{
			if (crd[0] == '\0' || connref == 0 || minor == 0) {
				printf ("\n*** NoProto: Card %p, callref %ld, minor %ld\n", crd, callref, minor);
				goto err;
			}
			if (conn == NULL) {
				printf ("\n*** Warn NoConnProto: Card %p, callref %ld, minor %ld\n", crd, callref, minor);
			}
			{
				conngrab cg = newgrab(conn ? conn->cg : NULL);
				if(cg == NULL) {
					resp = "OutOfMem";
					goto print;
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

					syslog (LOG_ERR, "ISDN NoProtocol1 %d %s", minor, data);
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

					goto err;
				}
				if (pushcardprot (cg, minor) == 0) {
					dropgrab(cg);
					/* Success */
					return;
				} else {
					dropgrab(cg);
					syslog (LOG_ERR, "ISDN NoProtocol2 %d %s", minor, data);
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
					goto err;
				}
			}
		} break;
	case IND_PROTO:
	case IND_PROTO_AGAIN:
		{
			if (connref == 0 || minor == 0) {
				printf ("\n*** NoProto: Card %p, callref %ld, minor %ld\n", crd, callref, minor);
				goto err;
			}
			if (conn == NULL) {
				printf ("\n*** Warn NoConnProto: Card %p, callref %ld, minor %ld\n", crd, callref, minor);
			}
			{
				conngrab cg = newgrab(conn ? conn->cg : NULL);
				if(cg == NULL) {
					resp = "NoMemErrErr";
					goto print;
				}
				cg->protocol = str_enter(prot);
				if(cg->par_in != NULL)
					freemsg(cg->par_in);
				cg->par_in = copymsg(&xx);

				if (crd[0] != '\0')
					cg->card = str_enter(crd);

				if ((resp = findsite (&cg)) != NULL) {
					dropgrab(cg);
					syslog (LOG_ERR, "ISDN NoProtocol3 %d %s", minor, data);

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
					goto err;
				}
				if (pushprot (cg, minor, ind == IND_PROTO_AGAIN) == 0) {
					/* Success */
					dropgrab(cg);
					return;
				} else {
					dropgrab(cg);
					syslog (LOG_ERR, "ISDN NoProtocol4 %d %s", minor, data);

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
					goto err;
				}
			}
		} break;
	case IND_INCOMING:
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

			syslog (LOG_INFO, "ISDN In %d %s", minor, data);
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
					goto cont;
				}

				printf("\n*** ConnRef Clash! old is %d, new %d\n",conn->connref,connref);

				mz = allocb(40,BPRI_HI); if(mz == NULL) goto cont;

				if(*resp != '+') {
printf("Dis1 ");
					m_putid (mz, CMD_OFF);
					m_putsx (mz, ARG_NODISC);
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
					resp = NULL;

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
				} else {
					dropother:
printf("Dis2 ");
					m_putid (mz, CMD_OFF);
					m_putsx (mz, ARG_NODISC);
					if(conn->connref != 0) {
						m_putsx (mz, ARG_CONNREF);
						m_puti (mz, conn->connref);
					}
					m_putsx (mz, ID_N0_cause);
					m_putsx2(mz, ID_N1_CallRejected);
					if(crd[0] != '\0') {
						m_putsx(mz,ARG_CARD);
						m_putsz(mz,crd);
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
			conn->state = c_down;
			ReportConn(conn);
			resp = runprog (cfr, conn, &cg);
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
				   many out there */
				m_putsx (&xx, ARG_CAUSE);
				if((bchan < 0) || !strcmp(resp,"0BUSY"))
					m_putsx2 (&xx, ID_N1_UserBusy);
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
					dropconn(conn);
				}
			}
			dropgrab(cg);
		}
		break;
	case IND_CONN:
		{
			if (conn != NULL)
				setconnstate(conn, c_going_up);
			if(conn == NULL || (conn->flags & F_OUTGOING)) {
				syslog (LOG_INFO, "ISDN Out %d %s", minor, data);

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
		}
		break;
	case PROTO_HAS_ENABLE:
		if(conn != NULL) {
			if(conn->state == c_off)
				setconnstate(conn, c_down);
			else if(conn->state == c_offdown)
				setconnstate(conn, c_going_down);
		}
		break;
	case PROTO_HAS_DISABLE:
		if(conn != NULL) {
			if(conn->state == c_down)
				setconnstate(conn, c_off);
			else if(conn->state == c_going_down)
				setconnstate(conn, c_offdown);
		}
		break;
#if 0
	case ID_N1_REL:
		if(conn != NULL) {
			conn->got_id = 2;
			goto after_ind;
		}
		break;
#endif
	case ID_N1_REL:
	case IND_DISC:
		{
			if (conn != NULL) {
				conn->got_id = 1;
				conn->cg->oldnr = conn->cg->nr;
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
					  conn_off: {
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
						if(conn->flags & F_PERMANENT) {
							mblk_t *mb = allocb(30,BPRI_MED);

							if(mb != NULL) {
								m_putid (mb, CMD_PROT);
								m_putsx (mb, ARG_MINOR);
								m_puti (mb, minor ? minor : fminor);
								m_putdelim (mb);
								m_putid (mb, PROTO_ENABLE);
								xlen = mb->b_wptr - mb->b_rptr;
								DUMPW (mb->b_rptr, xlen);
								(void) strwrite (xs_mon, (uchar_t *) mb->b_rptr, &xlen, 1);
								freemsg(mb);
							}
							if(conn->want_reconn)
								try_reconn(conn);
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
						syslog(LOG_ERR,"fMinor does not match -- closing (%d/%d)",minor,fminor);
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
					syslog(LOG_WARNING,"COST ??:?? %d",charge);
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
			if(ind == IND_DISC) {
				resp = "NO CARRIER";
				goto print;
			}
#endif
		} break;
#if 0
	case IND_PACKET:
		{
		} break;
#endif
	case IND_CLOSE: {
		if (minor == 0)
			goto err;

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
printf("ConnForgotten: %d/%d/%d\n",conn->minor,conn->fminor,conn->seqnum);
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
		}
		break;
	case IND_OPEN:
		{
			if (minor == 0)
				goto err;

			user[fminor] = uid;
			chmod (idevname (minor), S_IRUSR | S_IWUSR);
			chown (idevname (minor), uid,-1);

			if(conn != NULL)
				break;

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
		}
		break;
	case PROTO_HAS_CONNECTED:
		{
			if (conn != NULL)
				setconnstate(conn,c_up);
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

		} return;
	case PROTO_DISCONNECT:
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
		} break;
	case PROTO_HAS_DISCONNECT:
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
						{
							setconnstate(conn,c_down);
							if(conn->want_reconn)
								try_reconn(conn);
						}
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
			goto print;
#endif
		} break;
	case PROTO_WANT_CONNECTED:
		{
			if (!quitnow && conn != NULL && (conn->state > c_off || (conn->state == c_off && has_force))) {
				if (conn->state == c_off) {
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
		} break;
	case PROTO_AT:
		{
			for (;;) {
				while (xx.b_rptr < xx.b_wptr && *xx.b_rptr != 'a' && *xx.b_rptr != 'A')
					xx.b_rptr++;
				if (xx.b_rptr == xx.b_wptr)
					return;
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
					goto print;
				printf ("AT %c\n", *xx.b_rptr);
				switch (*xx.b_rptr++) {
				case '/':
					m_getskip (&xx);
					switch (*xx.b_rptr++) {
					default:
						goto error;
					case 'k':
					case 'K': /* Kill running programs. Restart in half a minute. */
						if(user[fminor] != 0) {
							resp = "NO PERMISSION";
							goto print;
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
							goto print;
						}
						break;
					case 'r':
					case 'R': /* Reload database. */
						if(user[fminor] != 0) {
							resp = "NO PERMISSION";
							goto print;
						}
						read_args(NULL);
						do_run_now++;
    					run_now(NULL);
						break;
					case 'q':
					case 'Q': /* Shutdown. */
						if(user[fminor] != 0) {
							resp = "NO PERMISSION";
							goto print;
						}
						do_quitnow(NULL);
						break;
					case 'b':
					case 'B': { /* Reenable a connection. */
						if(m_geti(&xx,&minor) != 0) {
							resp = "ERROR";
							goto print;
						}
						for(conn = theconn; conn != NULL; conn = conn->next) {
							if(conn->ignore)
								continue;
							if(conn->minor == minor)
								break;
						}
						if(conn != NULL) {
							if(conn->state == c_off) {
								if (conn->flags & F_PERMANENT) {
									mblk_t *mb = allocb(30,BPRI_MED);

									setconnstate(conn, c_down);
									m_putid (mb, CMD_PROT);
									m_putsx (mb, ARG_MINOR);
									m_puti (mb, minor);
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
								goto print;
							}
						}
						resp = "NOT FOUND";
						goto print;
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
								goto print;
							}
							if(m_geti(&xx,&minor) != 0) {
								resp = "ERROR";
								goto print;
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
									goto print;
								}
							}
							resp = "NOT FOUND";
							goto print;
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
								goto print;
							}
							sp = resp = msgbuf;
							sp += sprintf(sp,"#:ref id site protocol class pid state/card cost total flags,remotenr cause\r\n");
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

							goto print;
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
								goto print;
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

							goto print;
						}
						break;

					case 'm':
					case 'M':
						{
							m_getskip (&xx);
							if (xx.b_rptr == xx.b_wptr)
								goto error;
							xlen = xx.b_wptr - xx.b_rptr;
							DUMPW (xx.b_rptr, xlen);
							(void) strwrite (xs_mon, xx.b_wptr, &xlen, 1);
							return;
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
							goto print;
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
					return;
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
							goto print;
						}
						if(quitnow) {
							freeb(md);
							dropgrab(cg);
							resp = "SHUTTING DOWN";
							goto print;
						}
						if(in_boot) {
							freemsg(md);
							dropgrab(cg);
							resp = "STARTING UP";
							goto print;
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
							goto error;
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
							goto print;
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
						goto print;
					}
					break;
				case 'A':
				case 'a':
					goto error;
				default:
					goto error;
				}
			} /* end while */
		} /* end AT */
		break;
	case ID_N1_INFO:
		if(conn != NULL && conn-charge != 0 && (conn->charge % 10) == 0)
			syslog(LOG_INFO,"Cost %s:%s %d",conn->cg->site,conn->cg->protocol,conn->charge);
		break;
	case IND_INFO:
		if (m_getid (&xx, &ind) != 0)
			goto err;
		goto redo;
	case IND_ERR:
printf("GotAnError: Minor %d, connref %d, hdr %s\n",minor,connref,HdrName(hdrval));
		if(hdrval == HDR_CLOSE) /* Ignore? */
			break;
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
		resp = "ERROR";
		goto print;
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
	}
}

void
read_info (void)
{
	streamchar x[MAXLINE];
	int len;

	if ((len = read (fileno (stdin), x, sizeof (x) - 1)) <= 0)
		xquit ("Read", "HL");
	x[len] = '\0';
	do_info (x, len);
}

void
read_data ()
{
	struct _isdn23_hdr hdr;
	int err;
	uchar_t dbuf[MAXLINE];
	int len = 0;
	int iovlen = 1;
	struct iovec io[2];

#define xREAD(what) 														\
	do { 																\
		int xlen = hdr.hdr_##what.len; 									\
		uchar_t *dbufp = dbuf; 											\
		if (xlen == 0) 													\
			break; 														\
		if (xlen >= sizeof(dbuf)) { 									\
			syslog(LOG_ERR,"Header %d: Len %d",hdr.key,hdr.hdr_##what.len);	\
			dumpaschex((u_char *)&hdr,sizeof(hdr));								\
			xquit(NULL,NULL); 											\
		} 																\
		while((err = read(fd_mon,dbufp,xlen)) > 0) { 					\
			len += err; 												\
			dbufp += err;												\
			xlen -= err; 												\
			if (xlen <= 0) 												\
				break; 													\
		} 																\
		if (err < 0) 													\
			xquit("Err Header Read",NULL); 								\
		else if (err == 0) 												\
			xquit("EOF Header Read",NULL); 								\
	} while(0);

	if ((err = read (fd_mon, &hdr, sizeof (hdr))) != sizeof (hdr)) {
		if (err < 0)
			xquit ("Read Header", NULL);
		syslog (LOG_ERR, "Header: len is %d, got %d", sizeof (hdr), err);
		xquit (NULL, NULL);
	}
	switch (hdr.key) {
	default:
		syslog (LOG_ERR, "Unknown header ID %d", hdr.key);
		xquit (NULL, NULL);
	case HDR_ATCMD:
		xREAD (atcmd);
		break;
	case HDR_PROTOCMD:
		xREAD (protocmd);
		break;
	case HDR_XDATA:
		xREAD (xdata);
		break;
	case HDR_DATA:
		xREAD (data);
		break;
	case HDR_UIDATA:
		xREAD (uidata);
		break;
	case HDR_RAWDATA:
		xREAD (rawdata);
		break;
	case HDR_OPEN:
		break;
	case HDR_CLOSE:
		break;
	case HDR_ATTACH:
		break;
	case HDR_DETACH:
		break;
	case HDR_CARD:
		break;
	case HDR_NOCARD:
		break;
	case HDR_OPENPROT:
		break;
	case HDR_CLOSEPROT:
		break;
	case HDR_NOTIFY:
		break;
	case HDR_TEI:
		break;
	case HDR_INVAL:
		if ((err = read (fd_mon, dbuf, sizeof (hdr))) != sizeof (hdr)) {
			if (err < 0)
				xquit ("Read InvHeader", NULL);
			syslog (LOG_ERR, "InvHeader: len is %d, got %d", sizeof (hdr), err);
			xquit (NULL, NULL);
		}
		len = sizeof (hdr);
		break;
	}

	/* dump_hdr(&hdr,"Read 2->3",dbuf); */

	io[0].iov_base = (caddr_t) & hdr;
	io[0].iov_len = sizeof (struct _isdn23_hdr);

	if (len > 0) {
		io[1].iov_base = (caddr_t) dbuf;
		io[1].iov_len = len;
		iovlen = 2;
	}
	strwritev (xs_mon, io, iovlen, 0);
}

fd_set rd;

void
backrun (int fd)
{
	int err;
	fd_set rdx;

	if(!testonly && !FD_ISSET(fd_mon,&rd)) 
		xquit("RD fd_set cleared",NULL);
	do {
		struct timeval now = {0, 0};
		rdx = rd;

		runqueues (); runqueues ();
		if(fd >= 0) FD_SET(fd,&rdx);
		err = select (FD_SETSIZE, &rdx, NULL, NULL, &now);
		if (err == 0 || (err < 0 && errno == EINTR)) {
			rdx = rd;
			if(fd >= 0) FD_SET(fd,&rdx);

			if(fd == -1) return;
			if(fd < 0) callout_adjtimeout ();
			err = select (FD_SETSIZE, &rdx, NULL, NULL, (fd < 0 && 
				callout_hastimer ())? &callout_time : NULL);
		}
		runqueues(); runqueues();
		if (err <= 0) {
			if (err < 0 && errno != EINTR)
				xquit ("Select", NULL);
			callout_alarm ();
			continue;
		}
		if (!testonly && FD_ISSET (fd_mon, &rdx)) 
			read_data ();
		if (FD_ISSET (fileno (stdin), &rdx)) 
			read_info ();
		runqueues(); runqueues();
	} while((fd >= 0) ? !FD_ISSET(fd,&rdx) : has_progs());
	runqueues(); runqueues();
}


void
syspoll (void)
{
	FD_ZERO (&rd);
	if (!testonly)
		FD_SET (fd_mon, &rd);
	if (!igstdin)
		FD_SET (fileno (stdin), &rd);
	while (has_progs())
		backrun(-2);
}


void
do_h (queue_t * q)
{
	uchar_t data[MAXLINE];
	int len = sizeof (data) - 1;
	int err;

	while ((err = strread (xs_mon, (streamchar *) data, &len, 1)) == 0 && len > 0) {
		do_info (data, len);
		len = sizeof (data) - 1;
	}
	if (err != 0) {
		errno = err;
		syslog (LOG_ERR, "Read H: %m");
	} else
		q->q_flag |= QWANTR;
}

void
do_l (queue_t * q)
{
	struct _isdn23_hdr h;
	int err;
	uchar_t data[MAXLINE];

	while (1) {
		struct iovec io[3];
		int iovlen = 1;
		int len = sizeof (struct _isdn23_hdr);

		if ((err = strread (xs_mon, (streamchar *) &h, &len, 0)) == 0 && len == sizeof (struct _isdn23_hdr)) ;

		else
			break;

		io[0].iov_base = (caddr_t) & h;
		io[0].iov_len = sizeof (struct _isdn23_hdr);

		switch (h.key) {
		default:
			break;
		case HDR_ATCMD:
		case HDR_XDATA:
		case HDR_RAWDATA:
		case HDR_DATA:
		case HDR_UIDATA:
		case HDR_PROTOCMD:
			io[iovlen].iov_base = (caddr_t) data;
			io[iovlen].iov_len = len = h.hdr_atcmd.len;
			if ((err = strread (xs_mon, (streamchar *) data, &len, 0)) != 0 || len != h.hdr_atcmd.len) {
				syslog (LOG_ERR, "do_l: Fault, %d, %m", len);
				return;
			}
			io[iovlen].iov_base = (caddr_t) data;
			io[iovlen].iov_len = len;
			iovlen++;
			break;
		case HDR_INVAL:
			io[iovlen].iov_base = (caddr_t) data;
			io[iovlen].iov_len = len = sizeof (struct _isdn23_hdr);
			if ((err = strread (xs_mon, (streamchar *) data, &len, 0)) != 0 || len != sizeof (struct _isdn23_hdr)) {
				syslog (LOG_ERR, "do_l: Fault, %d, %m", len);
				return;
			}
			iovlen++;
			break;
		}

		/*
		 * dump_hdr(&h,testonly ? "Write <-3" : "Write 2<-3",(uchar_t
		 * *)io[1].iov_base);
		 */
		if (testonly)
			err = 0;
		else {
			if (((char *)(io[0].iov_base))[0] == 0x3F)
				abort ();
			err = writev (fd_mon, io, iovlen);
		}
	}
	if (err != 0) {
		errno = err;
		syslog (LOG_ERR, "Read L: %m");
	} else
		q->q_flag |= QWANTR;
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
run_now(void *nix)
{
	cf what;
	static int npos = 0;
	int spos = 0;

	if(do_run_now > 1) {
		do_run_now--;
		return;
	}
	if(signal(SIGHUP,SIG_IGN) != SIG_IGN)
		signal (SIGHUP, SIG_DFL);
	if(quitnow)
		return;

	for(what = cf_R; what != NULL; what = what->next) {
		if(spos++ < npos) {
printf("Skip #%d; ",spos);
			continue;
		}
		npos++;
printf("Do #%d...",spos);
		if(what->got_err) {
printf("StoredErr; ",spos);
			continue;
		}
		if(strchr(what->type,'B') != NULL || strchr(what->type,'p') != NULL) {
			struct conninfo *conn;

			for(conn = theconn; conn != NULL; conn = conn->next) {
				if(conn->ignore || (conn->cg == NULL))
					continue;
				if(strcmp(conn->cg->site,what->site))
					continue;
				if(strcmp(conn->cg->protocol,what->protocol))
					continue;
				if(!classmatch(conn->cg->cclass,what->cclass))
					continue;
				break;
			}
			if(conn == NULL) {
				conngrab cg = newgrab(NULL);
				char *err;
printf("run %s:%s; ",what->site,what->protocol);
				cg->site = what->site; cg->protocol = what->protocol;
				cg->card = what->card; cg->cclass = what->cclass;
				if (strchr(what->type,'p') != NULL)
					cg->flags |= F_PERMANENT;
				if (strchr(what->type,'f') != NULL)
					cg->flags |= F_LEASED;
				if (strchr(what->type,'B') != NULL)
					cg->flags |= F_OUTGOING;
				err = runprog(what,NULL,&cg);
				chkone(cg);
				dropgrab(cg);
				if(err == NULL || !strncasecmp(err,"no free dev",11)) {
					if(err != NULL) {
						spos--;
						printf("Try again: %s",err);
					}
printf("\n");
					timeout(run_now,(void *)spos,2*HZ);
					return;
				} else {
printf("FAIL %s\n",err);
				}
			} else {
printf("exist %s:%s\n",conn->cg->site,conn->cg->protocol);
				if(conn->cg != NULL && conn->minor != 0 && conn->pid != 0)
					pushprot(conn->cg,conn->minor,1);
			}
		}
	}
printf("\nBoot Finished\n");
	in_boot = 0;
	if(signal(SIGHUP,SIG_IGN) != SIG_IGN)
    	signal (SIGHUP, (sigfunc__t) read_args_run);
	npos = 0;
	do_run_now = 0;
}

int
has_progs(void)
{
	struct conninfo *conn;
	if(!quitnow)
		return 1;
	for(conn = theconn; conn != NULL; conn = conn->next) {
		if(conn->ignore)
			continue;
		if(conn->pid != 0)
			return 1;
		if(conn->minor != 0)
			return 1;
	}
	return 0;
}

void
kill_progs(struct conninfo *xconn)
{
	struct conninfo *conn, *nconn;
	if(!quitnow)
		in_boot = 1;
	for(conn = theconn; conn != NULL; conn = nconn) {
		nconn = conn->next;
		if(conn->ignore)
			continue;
		if(conn->pid != 0 && conn->minor != 0 && (xconn == NULL || conn == xconn)) {
			mblk_t *mb = allocb(30,BPRI_LO);
			int xlen;
			if(mb == NULL)
				continue;
			if(conn->cg != NULL)
				syslog (LOG_INFO, "ISDN drop %d %d %s:%s:%s", conn->pid, conn->minor, conn->cg->site,conn->cg->protocol,conn->cg->cclass);
			else
				syslog (LOG_INFO, "ISDN drop %d %d <unknown>", conn->pid, conn->minor);
			
			m_putid (mb, CMD_CLOSE);
			m_putsx (mb, ARG_MINOR);
			m_puti (mb, conn->minor);

			xlen = mb->b_wptr - mb->b_rptr;
			DUMPW (mb->b_rptr, xlen);
			(void) strwrite (xs_mon, mb->b_rptr, &xlen, 1);
			freemsg(mb);
			conn->minor = 0;
			if(xconn != NULL)
				break;
		}
	}
	if(!quitnow) {
		do_run_now++;
		timeout(run_now,NULL,5*HZ);
	}
}

void
do_quitnow(void *nix)
{
	quitnow = 1;
	kill_progs(NULL);
}

int
main (int argc, char *argv[])
{
	extern char *optarg;
	extern int optind;

	char *devnam = "/dev/isdnmon";
	char *execf = NULL;
	int pushlog = 0;
	int debug = 0;
	int x;

#ifdef linux
	reboot(0xfee1dead,0x17392634,1);		/* Magic to make me nonswappable */
#endif
#ifdef DO_DEBUG_MALLOC
	mcheck(NULL);
	mmtrace();
#endif
	chkall();
	setlinebuf(stdout); setlinebuf(stderr);

	progname = strrchr (*argv, '/');	/* basename */
	if (progname == NULL)
		progname = *argv;
	else
		progname++;

	while ((x = getopt (argc, argv, "iItf:x:dlLwWqQ"))!= EOF) {
		switch (x) {
		case 'l':
			pushlog |= 1;
			break;
		case 'L':
			pushlog |= 2;
			break;
		case 'w':
			pushlog |= 4;
			break;
		case 'W':
			pushlog |= 8;
			break;
		case 'q':
			pushlog |= 16;
			break;
		case 'Q':
			pushlog |= 32;
			break;
		case 'd':
			debug = 1;
			break;
		case 't':
			testonly = 1;
			break;
		case 'x':
			if (execf != NULL)
				goto usage;
			execf = optarg;
			break;
		case 'I':
			igstdin = 0;
			break;
		case 'i':
			igstdin = 1;
			break;
		case 'f':
			if (getuid ()!= 0)
				goto usage;
			devnam = optarg;
			break;
		default:
		  usage:
			fprintf (stderr, "Usage: %s -t[est] -x Data -l[ogging] -F epromfile -f devfile\n"
					"\t-M load+direct+Leo -m direct+Leo \n", progname);
			exit (1);
		}
	}
	openlog (progname, debug ? LOG_PERROR : 0, LOG_LOCAL7);

	fileargs = &argv[optind];
	read_args (NULL);

	seteuid (getuid ());

	if (!debug) {
		switch (fork ()){
		case -1:
			xquit ("fork", NULL);
		case 0:
			break;
		default:
			exit (0);
		}

#ifdef TIOCNOTTY
		{
			int a = open ("/dev/tty", O_RDWR);

			if (a >= 0) {
				(void) ioctl (a, TIOCNOTTY, NULL);
				close (a);
			}
		}
#endif
		{
			int i;
			for (i=getdtablesize()-1;i>=0;i--)
				(void) close(i);
		}
		igstdin=1;
		open("/dev/null",O_RDWR);
		dup(0); dup(0);
#ifdef HAVE_SETPGRP_2
		setpgrp (0, getpid ());
#endif
#ifdef HAVE_SETPGRP_0
		setpgrp();
#endif
	}

#ifdef linux
	{
		FILE * fd;
		if((fd = fopen("/proc/devices","r")) == NULL)
			syslog(LOG_ERR,"Reading device numbers: %m");
		else {
			char xx[80];
			int len;

			while(fgets(xx,sizeof(xx)-1,fd) != NULL) {
				char *x = xx;
				len = strlen(xx);
				if(len>0 && xx[len-1]=='\n')
					xx[len-1]='\0';
				while(*x != '\0' && isspace(*x))
					x++;
				if(isdigit(*x)) {
					int devnum = atoi(x);
					while(*x != '\0' && isdigit(*x)) x++;
					while(*x != '\0' && isspace(*x)) x++;
					if(strcmp(x,"tisdn") == 0) 
						isdnterm = devnum;
					else if(strcmp(x,"isdn") == 0) 
						isdnstd = devnum;
				}
			}
			fclose(fd);
			if(isdnstd == 0)
				syslog(LOG_CRIT, "No ISDN driver found!");
			else {
				int i;

				system("rm -rf /dev/isdn /dev/isdnmon");
				mkdir("/dev/isdn",0755);
				mknod ("/dev/isdnmon", S_IFCHR | S_IRUSR | S_IWUSR, MKDEV(isdnstd,0));

				for(i=1;i<NPORT;i++) {
					mknod (idevname (i), S_IFCHR | S_IRUSR | S_IWUSR, MKDEV(isdnstd,i));
					chmod (idevname (i), S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
				}
				syslog(LOG_DEBUG,"ISDN: isdn/XX: major number %d",isdnstd);
			}
				
			if(isdnterm == 0) 
				syslog(LOG_CRIT,"No ISDN terminal device found!");
			else {
				int i;
				for(i=1;i<NPORT;i++)
					unlink(devname(i));
				syslog(LOG_DEBUG,"ISDN: ttyiXX: major number %d",isdnterm);
			}
		}
	}
#endif

	signal (SIGALRM, (sigfunc__t) alarmsig);
	signal (SIGPIPE, SIG_IGN);
	if(signal(SIGHUP,SIG_IGN) != SIG_IGN)
		signal (SIGHUP, SIG_DFL);
	if(signal(SIGINT,SIG_IGN) != SIG_IGN)
		signal (SIGINT, do_quitnow);
	signal (SIGQUIT, (sigfunc__t) do_quitnow);
	signal (SIGUSR1, (sigfunc__t) kill_progs);
	signal (SIGCHLD, (sigfunc__t) deadkid);

	chkall();
	xs_mon = stropen (0);
	if (xs_mon == NULL)
		xquit ("xs_mon = NULL", "");

	if (!testonly) {
		fd_mon = open (devnam, O_RDWR);
		if (fd_mon < 0)
			xquit ("Open Dev", devnam);
		if (ioctl (fd_mon, I_SRDOPT, RMSGN) < 0)
			if (errno != ENOTTY)
				xquit ("SetStrOpt", "");

		if (pushlog & 2)
			if (ioctl (fd_mon, I_PUSH, "strlog") < 0)
				if (errno != ENOTTY)
					xquit ("Push", "strlog 1");
		if (pushlog & 8)
			if (ioctl (fd_mon, I_PUSH, "logh") < 0)
				if (errno != ENOTTY)
					xquit ("Push", "strlog 1");
		if (pushlog & 32)
			if (ioctl (fd_mon, I_PUSH, "qinfo") < 0)
				if (errno != ENOTTY)
					xquit ("Push", "strlog 1");
#if LEVEL < 4
		if (xnkernel) {
			if (ioctl (fd_mon, I_PUSH, "isdn_3") < 0) {
				syslog (LOG_ERR, "No kernel ISDN module: %m");
				xnkernel = 0;
			} else {
				if (pushlog & 1)
					if (ioctl (fd_mon, I_PUSH, "strlog") < 0)
						xquit ("Push", "strlog 1");
				if (pushlog & 4)
					if (ioctl (fd_mon, I_PUSH, "logh") < 0)
						xquit ("Push", "strlog 1");
				if (pushlog & 16)
					if (ioctl (fd_mon, I_PUSH, "qinfo") < 0)
						xquit ("Push", "strlog 1");
			}
		}
#endif
	} {
		typedef int (*F) ();
		extern void isdn3_init (void);
		extern void isdn_init (void);
		extern qf_put strrput;
		extern qf_srv strwsrv;
		extern struct streamtab strloginfo, loghinfo;
#if LEVEL < 4
		extern struct streamtab isdn3_info;
#endif
		extern struct qinit stwhdata, stwldata;
		extern struct module_info strhm_info;
		static struct qinit xhr =
		{strrput, do_h, NULL, NULL, NULL, &strhm_info, NULL};
		static struct qinit xlr =
		{strrput, do_l, NULL, NULL, NULL, &strhm_info, NULL};

#if LEVEL < 4
		isdn3_init ();
#endif
		setq (&xs_mon->rh, &xhr, &stwhdata);
		setq (&xs_mon->wl, &stwldata, &xlr);
#if LEVEL < 4
		register_strmod (&isdn3_info);
#endif
		register_strmod (&strloginfo);
		register_strmod (&loghinfo);
#if LEVEL < 4
		if (!xnkernel || testonly) {
			if (pushlog & 1)
				strioctl (xs_mon, I_PUSH, (long) "log");
			if (pushlog & 4)
				strioctl (xs_mon, I_PUSH, (long) "logh");
			if (pushlog & 16)
				strioctl (xs_mon, I_PUSH, (long) "qinfo");
			strioctl (xs_mon, I_PUSH, (long) "isdn_3");
		}
#endif
	}

	if(0)callout_async ();
	timeout (log_idle, NULL, 5 * HZ);
	timeout (queue_idle, NULL, HZ);
	if (execf != NULL) {
		FILE *efile = fopen (execf, "r");

		if (efile == NULL)
			xquit ("Exec File", execf);
	}
	syspoll ();

	strclose (xs_mon, 0);
	exit (0);
	return 0;
}
