#ifndef __MASTER
#define __MASTER

#undef DO_DEBUG_MALLOC

/*
 * A large heap of include files. Some may no longer be necessary...
 */
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "f_signal.h"
#include <sys/wait.h>
#include <strings.h>
#include <syslog.h>
#include "streams.h"
#include "sioctl.h"
#include "primitives.h"
#include "isdn_34.h"
#include "dump.h"
#include "timeout.h"
#include "primitives.h"
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


#ifdef MASTER_MAIN
#define EXTERN
#define INIT(a) = a
#else
#define EXTERN extern
#define INIT(a)
#endif

EXTERN int fd_mon;
EXTERN char *progname;

void xquit (const char *s, const char *t);

#ifdef DO_DEBUG_MALLOC

#ifndef	_MALLOC_INTERNAL
#define	_MALLOC_INTERNAL
#include <malloc.h>
#endif

EXTERN FILE *mallstream;
EXTERN char mallenv[]= "MALLOC_TRACE";
EXTERN char mallbuf[BUFSIZ];	/* Buffer for the output.  */

/* Old hook values.  */
EXTERN void (*tr_old_free_hook) __P ((__ptr_t ptr));
EXTERN __ptr_t (*tr_old_malloc_hook) __P ((size_t size));
EXTERN __ptr_t (*tr_old_realloc_hook) __P ((__ptr_t ptr, size_t size));

#define BI(x) __builtin_return_address((x))

void tr_freehook __P ((__ptr_t));
__ptr_t tr_mallochook __P ((size_t));
__ptr_t tr_reallochook __P ((__ptr_t, size_t));

void mmtrace ();

/* Old hook values.  */
EXTERN void (*old_free_hook) __P ((__ptr_t ptr));
EXTERN __ptr_t (*old_malloc_hook) __P ((size_t size));
EXTERN __ptr_t (*old_realloc_hook) __P ((__ptr_t ptr, size_t size));

/* Function to call when something awful happens.  */
void abort __P ((void));
EXTERN void (*abortfunc) __P ((void)) INIT((void (*) __P ((void))) abort);

/* Arbitrary magical numbers.  */
#define MAGICWORD	0xfedabeeb
#define MAGICBYTE	((char) 0xd7)

EXTERN struct hdr
  {
    size_t size;		/* Exact size requested by user.  */
    unsigned long int magic;	/* Magic number to check header integrity.  */
	struct hdr *prevb;
	struct hdr *nextb;
  } thelist INIT({ 0,0, &thelist, &thelist });

void checkhdr __P ((__const struct hdr *));
int mallcheck(void *foo);

void freehook __P ((__ptr_t));
__ptr_t mallochook __P ((size_t));
__ptr_t reallochook __P ((__ptr_t, size_t));

void mcheck (void (*func) __P ((void)));

#else /* not DO_DEBUG_MALLOC */
#define chkall() do { } while (0)
#define chkone(f) do { } while (0)
#endif


EXTERN int LI INIT( 0);


#define ID_priv_Busy CHAR2('=','B')
#define ID_priv_Print CHAR2('=','P')


#ifndef NCARDS
#define NCARDS 10
#endif
#ifndef MAXLINE
#define MAXLINE 4096
#endif

EXTERN uid_t user[NMINOR];

/* Unique refnum, always incremented by two */
/* The lower level also has one of these, but with the low bit set */

EXTERN long connrefs INIT(2);

EXTERN int fd_mon;			/* the lower/layer connection */
EXTERN char *progname;
EXTERN int testonly INIT(0);	/* set if fd_mon is not actually connected */
EXTERN int igstdin INIT(1);	/* Ignore stdin */
EXTERN int in_boot INIT(1);	/* We're still coming up */
#ifdef __linux__
EXTERN int isdnterm INIT(0);	/* major number of the terminal driver */
EXTERN int isdnstd INIT(0);	/* major number of the standard driver */
#endif

EXTERN short cardidx INIT(0);			/* Used to cycle among interfaces */
EXTERN short numidx INIT(0);			/* Used to cycle among numbers */

EXTERN void *locputchar INIT(NULL);	/* Glue */

#if LEVEL < 4				/* More glue */
EXTERN int xnkernel INIT(0);
EXTERN struct user u;
EXTERN mblk_t *mbfreelist INIT(NULL);
EXTERN void *dbfreelist[30] INIT({NULL});
#endif
EXTERN int connseq INIT(0);

EXTERN struct xstream *xs_mon;		/* Pseudo-stream for the L3 engine */

#define DUMPW(s,l) do { printf("HL W "); dumpascii((uchar_t *)(s),(l)); printf("\n"); } while(0)

/* String database. Should probably be an AVL tree or a hash but I'm lazy. */
/** The purpose of this thing is to be able not to bother with keeping track
    of how many references there are. */
/**** Todo: Garbage collection. Sometime in the future... */

EXTERN struct string {
	struct string *left;
	struct string *right;
	char data[2];
} *stringdb INIT(NULL);

char *str_enter(char *master);
char *wildmatch(char *a, char *b);
char *classmatch(char *a, char *b);
ulong_t maskmatch(ulong_t a, ulong_t b);
char *strippat(char *a);


/* List of cards */

typedef struct __card {
	long id;
	long modes;
	uchar_t bchans;
} *_card;

EXTERN struct __card card[NCARDS];


/* List of connection-related programs */

typedef struct proginfo {
	struct proginfo *next;
	struct conninfo *master;
	char *site;
	char *protocol;
	char *cclass;
	char *card;
	long id;
	ulong_t mask;
	char *type;
	pid_t pid;
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

char *state2str(CState state);

/* Info about a connection */

typedef struct conngrab {
	char *errmsg;
	char *site;
	char *protocol;
	char *cclass;
	char *card;
	ulong_t mask;
	char *oldnr;
	char *oldlnr;
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
	long id;
	uchar_t minor;
	uchar_t fminor;
	CState state;
	long flags;
	int retiming;
	int ctimer;
	int cause;
	char *causeInfo, *cardname, *classname, *lastMsg;
	int seqnum;
	short retries;
	unsigned timer_reconn:1;
	unsigned want_fast_reconn:1;
	unsigned want_reconn:3;
#define MAX_RECONN 7
	unsigned retime:1;		/* Timer for off->down change is running */
	unsigned got_hd:2;		/* disconnect... */
	unsigned got_id:2;
	unsigned char locked;
	unsigned ignore:3; /* 0: normal; 1: did drop it; 2: kill it; 3: reporter */
} *conninfo;

/* Special flags. */
#define F_INTERRUPT         01 /* interrupt, don't disconnect */
#define F_PREFOUT           02 /* drop incoming connection on call collision */
#define F_FORCEOUT          04 /* always drop incoming connection */
#define F_IGNORELIMIT      010 /* override card limit */
#define F_FASTDROP         020 /* immediate connection reject */
#define F_FASTREDIAL       040 /* don't delay as much when a dialup attempt fails */
#define F_PERMANENT       0100 /* connection doesn't really die */
#define F_LEASED          0200 /* leased line */
#define F_CHANBUSY        0400 /* busy if no free channel */
#define F_NRCOMPLETE     01000 /* remote number is complete */
#define F_LNRCOMPLETE    02000 /* local number is complete */
#define F_INCOMING       04000 /* incoming call */
#define F_OUTGOING      010000 /* outgoing call */
#define F_DIALUP        020000 /* dialup line */
#define F_IGNORELIMIT2  040000 /* override card limit */
#define F_OUTCOMPLETE  0100000 /* outgoing call info complete */
#define F_SETINITIAL   0200000 /* initial connection setup */
#define F_SETLATER     0400000 /* later re-setup */
#define F_NOREJECT    01000000 /* don't cause "temp unavailable" messages */
#define F_BACKCALL    02000000 /* callback on B if incoming call on A busy */

#define F_MOVEFLAGS (F_IGNORELIMIT|F_IGNORELIMIT2)
	/* FLags we set on start of a connection from the conngrab */

/*
 * translate a header ID to a name
 */
char *HdrName (int hdr);

/*
 * Translate flag bits to a string
 */
char *FlagInfo(int flag);

/* The List */

EXTERN struct conninfo *isdn4_conn INIT(NULL);


#define newgrab(x) Xnewgrab((x),__LINE__)
conngrab Xnewgrab(conngrab master, int lin);

#define dropgrab(x) Xdropgrab((x),__LINE__)
void Xdropgrab(conngrab cg,int lin);

/* Forward declarations */

int backrun(int fd, int timeout);
void try_reconn(struct conninfo *);
void run_now(void *nix);
EXTERN int do_run_now INIT(0);
int has_progs(void);
void do_quitnow(void *nix);


/* Debug code to show that a connection reference value is changed */

#define setconnref(a,b) Xsetconnref(__FILE__,__LINE__,(a),(b))
void Xsetconnref(const char *deb_file,unsigned int deb_line, conninfo conn, int connref);
void connreport(char *foo, char *card); 
/* Changing a connection status, and things to do */
const char *CauseInfo(int cause, char *pri);

void ReportConn(conninfo conn);

#define setconnstate(a,b) Xsetconnstate(__FILE__,__LINE__,(a),(b))
void Xsetconnstate(const char *deb_file, unsigned int deb_line,conninfo conn, CState state);

/* Device names. 's'=short, 'm'=medium, ''=fullname; 'i'=non-terminal version */

char * sdevname (short minor);
char * mdevname (short minor);
char * devname (short minor);

char * isdevname (short minor);
char * idevname (short minor);

/* Check a lock file. */

void checkdev(int dev);

/* Lock a device. One failure, for externally-opened devices (cu), may be tolerated. */

int lockdev(int dev, char onefailok);

/* ... and unlock it. */

void unlockdev(int dev);

/* Put something into the environment. */

void putenv2 (const char *key, const char *val);

/**
 ** Number String Stuff 
 **/

char *match_nr (char *extnr, char *locnr, char *locpref);
int match_suffix(char *extsuf, char *extnr);
char *build_nr (char *extnr, char *locnr, char *locpref, int islocal);
char *append_nr(char *extnr, char *extext);
char *strip_nr(char *extnr);


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
	ulong_t mask;
	char got_err;
} *cf;

EXTERN cf cf_P  INIT(NULL);
EXTERN cf cf_ML INIT(NULL);
EXTERN cf cf_MP INIT(NULL);
EXTERN cf cf_D  INIT(NULL);
EXTERN cf cf_DL INIT(NULL);
EXTERN cf cf_DP INIT(NULL);
EXTERN cf cf_R  INIT(NULL);
EXTERN cf cf_RP INIT(NULL);
EXTERN cf cf_C  INIT(NULL);
EXTERN cf cf_CM INIT(NULL);
EXTERN cf cf_CL INIT(NULL);
EXTERN cf cf_LF INIT(NULL);
EXTERN cf cf_TM INIT(NULL);

#define chkfree(x) do { } while(0)
#ifdef DO_DEBUG_MALLOC
void chkcf(cf conf);
void chkone(void *foo);
void chkall(void);
#endif /* DO_DEBUG_MALLOC */

void read_file (FILE * ffile, char *errf);

EXTERN char **fileargs;

void read_args (void *nix);
void read_args_run(void *nix);

const char *CauseInfo(int cause, char *pri);

void Xdropconn (struct conninfo *conn, const char *deb_file, unsigned int deb_line);
#define dropconn(x) Xdropconn((x),__FILE__,__LINE__)
void rdropconn (struct conninfo *conn, int deb_line);
void deadkid (void);

int matchflag(long flags, char *ts);
cf getcards(conngrab cg, cf list);
void Xbreak(void);

char * pmatch1 (cf prot, conngrab *cgm);
char * pmatch (conngrab *cgm);
char * findsite (conngrab *foo, int nobusy);
char * findit (conngrab *foo, int nobusy);

#if 0
mblk_t * getprot (char *protocol, char *site, char *cclass, char *suffix);
#endif

int pushprot (conngrab cg, int minor, char update);
int pushcardprot (conngrab cg, int minor);

void xquit (const char *s, const char *t);

EXTERN int quitnow INIT(0);

void panic(const char *x, ...);

struct conninfo * startconn(conngrab cg, int fminor, int connref, char **ret);
EXTERN struct conninfo *zzconn INIT(NULL);
void dropdead(void);
char * runprog (cf cfr, struct conninfo **rconn, conngrab *foo);
void run_rp(struct conninfo *conn, char what);
void kill_rp(struct conninfo *conn, char whatnot);
void retime(struct conninfo *conn);
void time_reconn(struct conninfo *conn);

void do_info (streamchar * data, int len);
void read_info (void);
void read_data (void);

EXTERN fd_set rd;

void syspoll (void);

void do_h (queue_t * q);
void do_l (queue_t * q);

void log_idle (void *xxx);
void queue_idle (void *xxx);
void alarmsig(void);

void kill_progs(struct conninfo *xconn);


struct loader {
	char *name;
	struct isdncard *card;
	FILE *file;
	long seqnum; /* position in config file(s) */
	int nrfile; /* loaded to card */
	long foffset; /* position in load file */
	struct loader *next;
	int cardnum;
	int connseq;
	unsigned timer:1;
};
void card_load(struct loader *ld);
void card_load_fail(struct loader *ld, int err); /* error */
EXTERN struct loader *isdn4_loader INIT(NULL);

struct isdncard {
	struct isdncard *next;
	char *name;
	long cap;
	ushort_t nrdchan; /* seqnum... */
	ushort_t nrbchan;
	ulong_t mask;
};
EXTERN struct isdncard *isdn4_card INIT(NULL);

int isintime(char *z);
EXTERN uchar_t *theclass INIT(NULL);

#endif
