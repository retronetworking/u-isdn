/* Streams logging module, used here to find out what's going on */

#include "primitives.h"
#include "f_ip.h"
#include <sys/time.h>
#include "f_signal.h"
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

#define MAXB 100

static struct module_info str_if_minfo =
{
		0, "str_if", 0, INFPSZ, 0, 0
};

static qf_open str_if_open;
static qf_close str_if_close;
static qf_put str_if_rput, str_if_wput;

static struct qinit str_if_rinit =
{
		str_if_rput, NULL, str_if_open, str_if_close, NULL, &str_if_minfo, NULL
};

static struct qinit str_if_winit =
{
		str_if_wput, NULL, NULL, NULL, NULL, &str_if_minfo, NULL
};

struct streamtab str_if_info =
{&str_if_rinit, &str_if_winit, NULL, NULL};

#include "../support/log.h"
#include "isdn_limits.h"

#define NLOG NCONN
static struct _log {
	char flags;
	char nr;
} str_if_log[NLOG];

static int str_if_cnt = NLOG;


static int
str_if_open (queue_t * q, dev_t dev, int flag, int sflag ERR_DECL)
{
	struct _log *slog;

	if (q->q_ptr) {
		if (0)
			printf ("Log: already open?\n");
		return 0;
	}
	for (dev = 0; dev < str_if_cnt; dev++) {
		if (!(str_if_log[dev].flags & LOG_INUSE))
			break;
	}

	if (dev >= str_if_cnt) {
		printf ("slog: all %d devices allocated\n", str_if_cnt);
		ERR_RETURN(-ENOSPC);
	}
	slog = &str_if_log[dev];
	WR (q)->q_ptr = (char *) slog;
	q->q_ptr = (char *) slog;

	slog->flags = LOG_INUSE | LOG_READ | LOG_WRITE;
	slog->nr = minor (dev);
	if (1)
		printf ("Log driver %d opened.\n", dev);

	return 0;
}


#if 0
void
str_if_printtcp (struct _log *slog, mblk_t * mp, char wr)
{
	struct ip *ip;
	uint_t hlen;
	struct tcphdr *oth;
	struct tcphdr *th;

	mblk_t *mq = dupmsg (mp);

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
str_if_printmsg (struct _log *slog, const char *text, mblk_t * mp)
{
	int ms = splstr ();

	if (slog != NULL) {
#if 0
		if (slog - str_if_log != 0 && mp->b_datap->db_type == M_DATA)
			return;
#endif
		printf ("Log %d: ", slog - str_if_log);
	}
	printf ("%s", text);

	{
		mblk_t *mp1;
		char *name;
		int nblocks = 0;
		static mblk_t *blocks[MAXB];

		for (mp1 = mp; mp1 != NULL; mp1 = mp1->b_cont) {
			blocks[nblocks++] = mp1;
			switch (mp1->b_datap->db_type) {
			case CASE_DATA:
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
			printf ("; %s: %x.%x.%d", name, mp1, mp1->b_datap, mp1->b_wptr - mp1->b_rptr);
			if (nblocks == MAXB) {
				printf ("\n*** Block 0x%x pointed to 0x%x", mp1, mp1->b_cont);
				mp1->b_cont = NULL;
			} else {
				int j;

				for (j = 0; j < nblocks; j++) {
					if (mp1->b_cont == blocks[j]) {
						printf ("\n*** Block 0x%x circled to 0x%x (%d)", mp1, mp1->b_cont, j);
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
			uchar_t x;

			for (i = 0, dp = (uchar_t *) mp1->b_rptr; dp < (uchar_t *) mp1->b_wptr; dp += BLOCKSIZE) {
				int k;
				int l = (uchar_t *) mp1->b_wptr - dp;

				printf ("    ");
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
	splx (ms);
}

static void
str_if_wput (queue_t * q, mblk_t * mp)
{
	struct _log *slog;

	slog = (struct _log *) q->q_ptr;

	if (slog->flags & LOG_WRITE) {
#if 0
		if (*mp->b_rptr == 0x45)
			str_if_printtcp (slog, mp, 1);
		else
#endif
			str_if_printmsg (slog, "write", mp);
		switch (mp->b_datap->db_type) {
		default:{
				break;
			}
		}
	}
	putnext (q, mp);
	return;
}

static void
str_if_rput (queue_t * q, mblk_t * mp)
{
	struct _log *slog;

	slog = (struct _log *) q->q_ptr;

	if (slog->flags & LOG_READ) {
#if 0
		if (*mp->b_rptr == 0x45)
			str_if_printtcp (slog, mp, 0);
		else
#endif
			str_if_printmsg (slog, "read", mp);
	}
	putnext (q, mp);
	return;
}


static void
str_if_close (queue_t * q, int dummy)
{
	struct _log *slog;

	slog = (struct _log *) q->q_ptr;

	flushq (q, FLUSHALL);
	flushq (WR (q), FLUSHALL);
	if (1)
		printf ("Log driver %d closed.\n", slog->nr);
	slog->flags = 0;
	return;
}
