
/* Streams logging module */

#include "f_module.h"
#include "primitives.h"
#include <sys/time.h>
#include "f_signal.h"
#include "f_malloc.h"
#include <sys/sysmacros.h>
#include <sys/stropts.h>
#ifdef DO_ADDUSER
#include "f_user.h"
#endif
#include <sys/errno.h>
#include "streams.h"
#include "streamlib.h"

#ifdef AUX
#include <sys/protosw.h>
#endif
#include <netinet/in.h>
#ifndef linux
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#ifdef KERNEL
#include <linux/sched.h>
#endif
#endif
#include <netinet/tcp.h>
#ifndef KERNEL
#include "kernel.h"
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
	char flags;
	char nr;
};

static int
log_open (queue_t * q, dev_t dev, int flag, int sflag
#ifdef DO_ADDERROR
		,int *err
#endif
)
{
	struct _log *log;
	static int nr = 1;

	if (q->q_ptr) 
		return 0;

	log = malloc(sizeof(*log));
	if(log == NULL)
		return OPENFAIL;
	memset(log,0,sizeof *log);
	WR (q)->q_ptr = (char *) log;
	q->q_ptr = (char *) log;

	log->flags = LOG_INUSE | LOG_READ | LOG_WRITE;
	log->nr = nr++;
	if (1)
		printf (KERN_DEBUG "Log driver %d opened.\n", log->nr);
	MORE_USE;

	return 0;
}


#if 0
void
log_printtcp (struct _log *log, mblk_t * mp, char wr)
{
	register struct ip *ip;
	register uint_t hlen;
	register struct tcphdr *oth;
	register struct tcphdr *th;

	register mblk_t *mq = dupmsg (mp);

	if (wr)
		wr = 'w';
	else
		wr = 'r';

	mp = pullupm (mq, 128);
	if (mp == NULL) {
		freemsg (mq);
		printf ("Log%cN %d ", wr);
		return;
	}
	ip = (struct ip *) mp->b_rptr;
	hlen = ip->ip_hl;


	if (ip->ip_p != IPPROTO_TCP
			|| (ip->ip_off & htons (0x3fff)) || mq->b_wptr - mq->b_rptr < 40) {
		freemsg (mp);
		printf ("Log%cT %x", wr, ip->ip_p);
		return;
	}
	th = (struct tcphdr *) & ((uchar_t *) ip)[hlen << 2];
	if (wr == 'w') {
		printf ("TCP %x.%x ", th->th_seq, dsize (mp) - th->th_off);
	} else {
		printf ("TCP %x ", th->th_ack);
	}
	freemsg (mp);
}

#endif

void
log_printmsg (void *xlog, const char *text, mblk_t * mp, const char *prefix)
{
	struct _log *log = (struct _log *)xlog;
	/* int ms = splstr (); */

	if(prefix != NULL)
		printf("%s",prefix);
	if (log != NULL) 
		printf ("Log %d: ", log->nr);
#if defined(linux) && defined(KERNEL)
	printf("%ld ",jiffies);
#endif
	printf ("%s", text);
#ifdef KERNEL
	if(msgdsize(mp) < 0)
		return;
#endif
	{
		mblk_t *mp1;
		char *name;
		int nblocks = 0;
		static mblk_t *blocks[MAXB];

		for (mp1 = mp; mp1 != NULL; mp1 = mp1->b_cont) {
			blocks[nblocks++] = mp1;
			switch (mp1->b_datap->db_type) {
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
				printf (":%d:", mp1->b_datap->db_type);
				name = "unknown"; /* ,mp->b_datap->db_type); */
				break;
			}
#if 0
			printf ("; %s: %x.%x.%x.%d", name, mp1, mp1->b_datap, mp1->b_rptr, mp1->b_wptr - mp1->b_rptr);
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
#ifdef KERNEL
	if(msgdsize(mp) < 0)
		return;
#else
	if(msgdsize(mp) < 100)
#endif
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

				printf ("%s    ",prefix?prefix:"");
				for (k = 0; k < BLOCKSIZE && k < l; k++)
					printf ("%c%c ", ctab[dp[k] >> 4], ctab[dp[k] & 0x0F]);
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

	switch (mp->b_datap->db_type) {
	default:
		log_printmsg (log, "read", mp, KERN_DEBUG);
		/* FALL THRU */
	CASE_DATA
			break;
	}
	putnext (q, mp);
	return;
}

static void
xlog_wput (queue_t * q, mblk_t * mp)
{
    register struct _log *log = (struct _log *) q->q_ptr;

	switch (mp->b_datap->db_type) {
	default:
		log_printmsg (log, "write", mp, KERN_DEBUG);
		/* FALL THRU */
	CASE_DATA
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
		printf (KERN_DEBUG "Log driver %d closed.\n", log->nr);
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
