
/* Streams logging module */

#include "f_module.h"
#include "primitives.h"
#ifdef __KERNEL__
#include <linux/time.h>
#else
#include <sys/time.h>
#include <sys/sysmacros.h>
#endif
#include "f_signal.h"
#include "f_malloc.h"
#include "streams.h"
#include "stropts.h"
#include "streamlib.h"
#include "isdn_23.h"
#include "kernel.h"

#define MAXB 10

extern void log_printmsg (void *log, const char *, mblk_t *, const char *);
extern void dump_hdr (isdn23_hdr , const char *, uchar_t * );

static struct module_info logh_minfo =
{
		0, "logh", 0, INFPSZ, 0, 0
};

static qf_open logh_open;
static qf_close logh_close;
static qf_put logh_wput, logh_rput;

static struct qinit logh_rinit =
{
		logh_rput, NULL, logh_open, logh_close, NULL, &logh_minfo, NULL
};

static struct qinit logh_winit =
{
		logh_wput, NULL, NULL, NULL, NULL, &logh_minfo, NULL
};

struct streamtab loghinfo =
{&logh_rinit, &logh_winit, NULL, NULL};

#include "log.h"

struct _log {
	char flags;
	char nr;
};


static int
logh_open (queue_t * q, dev_t dev, int flag, int sflag ERR_DECL)
{
	struct _log *log;
	static int nr = 1;

	log = malloc(sizeof(*log));
	if(log == NULL)
		ERR_RETURN(-ENOMEM);
	
	memset(log,0,sizeof *log);
	WR (q)->q_ptr = (char *) log;
	q->q_ptr = (char *) log;

	log->flags = LOG_INUSE | LOG_READ | LOG_WRITE;
	printf ("%sLog driver %d opened.\n",KERN_DEBUG, nr);
	log->nr = nr++;
	MORE_USE;

	return 0;
}

void
logh_printmsg (void *xlog, const char *text, mblk_t * mp)
{
	int ms = splstr ();
	struct _log *log = (struct _log *)xlog;

	if (log != NULL)
		printf ("%s* * ",KERN_DEBUG);
	else
		printf ("%s**  ",KERN_DEBUG);

	if ((DATA_TYPE(mp) == M_DATA)
#ifdef M_EXDATA
			|| (DATA_TYPE(mp) == M_EXDATA)
#endif
				) do {
		mblk_t *mm;
		mblk_t *mn;

		if (mp == NULL) {
			printf ("Null\n");
			break;
		}
		mm = dupmsg (mp);
		if (mm == NULL) {
			printf ("DupNull\n");
			break;
		}
		mn = pullupm (mm, -1);
		if (mn == NULL) {
			printf ("NoPullup\n");
			freemsg (mm);
			break;
		}
		dump_hdr ((isdn23_hdr) mn->b_rptr, text, mn->b_rptr+sizeof(struct _isdn23_hdr));

		freemsg (mn);
	} while(0);
	splx (ms);
}

static void
logh_wput (queue_t * q, mblk_t * mp)
{
	register struct _log *log;

	log = (struct _log *) q->q_ptr;

	if (log->flags & LOG_WRITE) {
		logh_printmsg (log, "Send", mp);
		switch (DATA_TYPE(mp)) {
		default:{
				break;
			}
		}
	}
	putnext (q, mp);
	return;
}

static void
logh_rput (queue_t * q, mblk_t * mp)
{
	register struct _log *log;

	log = (struct _log *) q->q_ptr;

	if (log->flags & LOG_READ) {
		logh_printmsg (log, "Recv", mp);
		switch (DATA_TYPE(mp)) {
		default:{
				break;
			}
		}
	}
	putnext (q, mp);
	return;
}


static void
logh_close (queue_t * q, int dummy)
{
	struct _log *log;

	log = (struct _log *) q->q_ptr;

	flushq (q, FLUSHALL);
	flushq (WR (q), FLUSHALL);
	printf ("%sLogH driver %d closed.\n",KERN_DEBUG, log->nr);
	free(log);
	LESS_USE;
	return;
}


#ifdef MODULE
static int do_init_module(void)
{
	return register_strmod(&loghinfo);
}

static int do_exit_module(void)
{
	return unregister_strmod(&loghinfo);
}
#endif
