
/* Streams logging module */

#include "f_module.h"
#include "primitives.h"
#include "kernel.h"
#include "f_signal.h"
#include "f_malloc.h"
#include "stropts.h"
#ifdef DO_ADDUSER
#include "f_user.h"
#endif
#include "streams.h"
#include "streamlib.h"

#ifdef AUX
#include <sys/protosw.h>
#endif
#ifndef linux
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#ifdef KERNEL
#include <linux/sched.h>
#endif
#endif
#ifndef __KERNEL__
#include <netinet/in.h>
#include <netinet/tcp.h>
#endif

#define MAXB 100

static struct module_info log_minfo =
{
		0, "strlog", 0, INFPSZ, 0, 0
};
static struct module_info xlog_minfo =
{
		0, "xstrlog", 0, INFPSZ, 0, 0
};

static qf_open log_open;
static qf_close log_close;
static qf_put log_wput,log_rput;
static qf_put xlog_wput,xlog_rput;

static struct qinit log_rinit =
{ log_rput, NULL, log_open, log_close, NULL, &log_minfo, NULL };
static struct qinit log_winit =
{ log_wput, NULL, NULL, NULL, NULL, &log_minfo, NULL };
static struct qinit xlog_rinit =
{ xlog_rput, NULL, log_open, log_close, NULL, &xlog_minfo, NULL };
static struct qinit xlog_winit =
{ xlog_wput, NULL, NULL, NULL, NULL, &xlog_minfo, NULL };

struct streamtab strloginfo =
{&log_rinit, &log_winit, NULL, NULL};
struct streamtab xstrloginfo =
{&xlog_rinit, &xlog_winit, NULL, NULL};

#include "log.h"
#include "isdn_limits.h"

struct _log {
	int nr;
	char flags;
};

static int
log_open (queue_t * q, dev_t dev, int flag, int sflag ERR_DECL)
{
	struct _log *log;
	static int nr = 1;

	if (q->q_ptr) 
		return 0;

	log = malloc(sizeof(*log));
	if(log == NULL)
		ERR_RETURN(-ENOMEM);
	memset(log,0,sizeof *log);
	WR (q)->q_ptr = (char *) log;
	q->q_ptr = (char *) log;

	log->flags = LOG_INUSE | LOG_READ | LOG_WRITE;
	log->nr = nr++;
	if (1)
		printf ("%sLog driver %d opened.\n", KERN_DEBUG,log->nr);
	MORE_USE;

	return 0;
}


void
log_printmsg (void *xlog, const char *text, mblk_t * mp, const char *prefix)
{
	struct _log *log = (struct _log *)xlog;
	/* int ms = splstr (); */
	static char pprefix[100];
	
	if(prefix != NULL) {
		strcpy(pprefix,prefix);
		printf("%s",pprefix);
	} else 
		*pprefix = '\0';
	if (log != NULL) 
		printf ("Log %d: ", log->nr);
#if defined(linux) && defined(KERNEL)
	printf("%ld ",jiffies);
#endif
	if(text != NULL)
		printf ("%s", text);
	{
		mblk_t *mp1;
		char *name;
		int nblocks = 0;
		static mblk_t *blocks[MAXB];
		char nam[20];

		for (mp1 = mp; mp1 != NULL; mp1 = mp1->b_cont) {
			blocks[nblocks++] = mp1;
			switch (DATA_TYPE(mp1)) {
			case M_DATA:
				name = "DATA";
				break;
			case M_PROTO:
				name = "PROTO";
				break;
#ifdef M_SPROTO
			case M_SPROTO:
				name = "SPROTO";
				break;
#endif
			case M_BREAK:
				name = "BREAK";
				break;
			case M_SIG:
				name = "SIG";
				break;
			case M_DELAY:
				name = "DELAY";
				break;
			case M_CTL:
				name = "CTL";
				break;
			case M_IOCTL:
				name = "IOCTL";
				break;
			case M_SETOPTS:
				name = "SETOPTS";
				break;
#ifdef M_ADMIN
			case M_ADMIN:
				name = "ADMIN";
				break;
#endif
#ifdef M_EXPROTO
			case M_EXPROTO:
				name = "EXPROTO";
				break;
#endif
#ifdef M_EXDATA
			case M_EXDATA:
				name = "EXDATA";
				break;
#endif
#ifdef M_EXSPROTO
			case M_EXSPROTO:
				name = "EXSPROTO";
				break;
#endif
#ifdef M_EXSIG
			case M_EXSIG:
				name = "EXSIG";
				break;
#endif
#ifdef M_PCDATA
			case M_PCDATA:
				name = "PCDATA";
				break;
#endif
#ifdef M_PCPROTO
			case M_PCPROTO:
				name = "PCPROTO";
				break;
#endif
#ifdef M_PKT
			case M_PKT:
				name = "PKT";
				break;
#endif
#ifdef M_PKTSTOP
			case M_PKTSTOP:
				name = "PKTSTOP";
				break;
#endif
			case M_IOCACK:
				name = "IOCACK";
				break;
			case M_IOCNAK:
				name = "IOCNAK";
				break;
			case M_PCSIG:
				name = "PCSIG";
				break;
			case M_FLUSH:
				name = "FLUSH";
				break;
			case M_STOP:
				name = "STOP";
				break;
			case M_START:
				name = "START";
				break;
			case M_HANGUP:
				name = "HANGUP";
				break;
			case M_ERROR:
				name = "ERROR";
				break;
#ifdef _MSG_PROTO_OWN
#ifdef MSG_PROTO
			case MSG_PROTO:
				name = "MSGPROTO";
				break;
#endif
#endif
			default:
				/* printf (":%d:", DATA_TYPE(mp1)); */
				sprintf(nam,"unknown [0x%x]",DATA_TYPE(mp1));
				name = nam;
				break;
			}
#if 0
			printf ("; %s: %x.%x.%x.%d", name, mp1, DATA_BLOCK(mp1), mp1->b_rptr, mp1->b_wptr - mp1->b_rptr);
#else
			printf ("; %s: %p.%d", name, mp1, mp1->b_wptr - mp1->b_rptr);
#endif
			if (nblocks == MAXB) {
				printf ("\n%s*** Block %p pointed to %p", KERN_CRIT, mp1, mp1->b_cont);
				mp1->b_cont = NULL;
			} else {
				int j;

				for (j = 0; j < nblocks; j++) {
					if (mp1->b_cont == blocks[j]) {
						printf ("\n%s*** Block %p circled to %p (%d)", KERN_CRIT, mp1, mp1->b_cont, j);
						mp1->b_cont = NULL;
					}
				}
			}
		}
	}
	printf ("\n");
	{
		int j;
		mblk_t *mp1;
		const char ctab[]= "0123456789abcdef";

#define BLOCKSIZE 0x10
		for (j = 0, mp1 = mp; mp1 != NULL; mp1 = mp1->b_cont, j++) {
			int i;
			uchar_t *dp;

			for (i = 0, dp = (uchar_t *) mp1->b_rptr; dp < (uchar_t *) mp1->b_wptr; dp += BLOCKSIZE) {
				int k;
				int l = (uchar_t *) mp1->b_wptr - dp;

				printf ("%s%03x ",pprefix,i);
#if 1
				if(i >= 3*BLOCKSIZE && l >= 4*BLOCKSIZE) { /* Skip the stuff in the middle */
					l -= 2*BLOCKSIZE + (l % BLOCKSIZE);
					printf("[... %d bytes (0x%x) skipped ...]\n",l,l);
					dp += l-BLOCKSIZE; i += l;
					continue;
				}
#endif
				for (k = 0; k < BLOCKSIZE && k < l; k++)
					printf ("%c%c ", ctab[dp[k] >> 4], ctab[dp[k] & 0x0F]);
				i += k;
				for (; k < BLOCKSIZE; k++)
					printf ("   ");
				printf (" : ");
				for (k = 0; k < BLOCKSIZE && k < l; k++)
					if (dp[k] > 31 && dp[k] < 127)
						printf ("%c", dp[k]);
					else
						printf (".");
				if (k < l)
					printf (" +\n");
				else if (mp1->b_cont != NULL) {
					for (; k < BLOCKSIZE; k++)
						printf (" ");
					printf (" -\n");
				} else
					printf ("\n");
			}
		}
	}
	/* splx (ms); */
}

static void
log_wput (queue_t * q, mblk_t * mp)
{
    register struct _log *log = (struct _log *) q->q_ptr;

	log_printmsg (log, "write", mp, KERN_DEBUG);
	putnext (q, mp);
	return;
}

static void
log_rput (queue_t * q, mblk_t * mp)
{
    register struct _log *log = (struct _log *) q->q_ptr;

	log_printmsg (log, "read", mp, KERN_DEBUG);
	putnext (q, mp);
	return;
}

static void
xlog_rput (queue_t * q, mblk_t * mp)
{
    register struct _log *log = (struct _log *) q->q_ptr;

	switch (DATA_TYPE(mp)) {
	default:
		log_printmsg (log, "read", mp, KERN_DEBUG);
		/* FALL THRU */
	case CASE_DATA:
			break;
	}
	putnext (q, mp);
	return;
}

static void
xlog_wput (queue_t * q, mblk_t * mp)
{
    register struct _log *log = (struct _log *) q->q_ptr;

	switch (DATA_TYPE(mp)) {
	default:
		log_printmsg (log, "write", mp, KERN_DEBUG);
		/* FALL THRU */
	case CASE_DATA:
			break;
	}
	putnext (q, mp);
	return;
}


static void
log_close (queue_t * q, int dummy)
{
	struct _log *log;

	log = (struct _log *) q->q_ptr;

	flushq (q, FLUSHALL);
	flushq (WR (q), FLUSHALL);
	if(1)
		printf ("%sLog driver %d closed.\n", KERN_DEBUG,log->nr);
	free(log);
	LESS_USE;
	return;
}


#ifdef MODULE
static int do_init_module(void)
{
	int err;
	
	err = register_strmod(&strloginfo);
	if (err) return err;
	err = register_strmod(&xstrloginfo);
	if (err) {
		unregister_strmod(&strloginfo);
		return err ;
	}
	return 0;
}

static int do_exit_module(void)
{
	int err1 = unregister_strmod(&strloginfo);
	int err2 = unregister_strmod(&xstrloginfo);
	return err1 || err2;
}
#endif
