
/* Streams PR_ON module */

#include "f_module.h"
#include "primitives.h"
#include <sys/types.h>
#include <sys/time.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include "streams.h"
#include "stropts.h"
#include "f_user.h"
#include "f_malloc.h"
#include <sys/errno.h>
#include "streamlib.h"
#include "isdn_proto.h"
#include "msgtype.h"
#include "port_m.h"

#define NPR_ON 8

static struct module_info pr_on_minfo =
{
		0, "pr_on", 0, INFPSZ, 0, 0
};

static qf_open pr_on_open;
static qf_close pr_on_close;
static qf_put pr_on_rput,pr_on_wput;

static struct qinit pr_on_rinit =
{
		pr_on_rput, NULL, pr_on_open, pr_on_close, NULL, &pr_on_minfo, NULL
};

static struct qinit pr_on_winit =
{
		pr_on_wput, NULL, NULL, NULL, NULL, &pr_on_minfo, NULL
};

struct streamtab pr_oninfo =
{&pr_on_rinit, &pr_on_winit, NULL, NULL};

struct _pr_on {
	queue_t *q;
	short flags;
#define PR_ON_TIMER 01
#ifdef NEW_TIMEOUT
	long timer;
#endif
};

static void
pr_on_send (struct _pr_on *pr_on)
{
	if (!(pr_on->flags & PR_ON_TIMER))
		return;
	pr_on->flags &= ~PR_ON_TIMER;

	{
		mblk_t *mb = allocb (3, BPRI_MED);

		if (mb != NULL) {
			m_putid (mb, PROTO_CONNECTED);
			DATA_TYPE(mb) = MSG_PROTO;

			putnext (WR (pr_on->q), mb);
		}
	}
#if 0
	{
		mblk_t *mb = allocb (3, BPRI_MED);

		if (mb != NULL) {
			*mb->b_wptr++ = PROTO_SYS;
			m_putid (mb, PROTO_CONNECTED);
			DATA_TYPE(mb) = MSG_PROTO;

			putnext (WR (pr_on->q), mb);
		}
	}
#endif
}

static int
pr_on_open (queue_t * q, dev_t dev, int flag, int sflag ERR_DECL)
{
	struct _pr_on *pr_on;

	if (q->q_ptr) {
		return 0;
	}
	pr_on = malloc(sizeof(*pr_on));
	if(pr_on == NULL)
		ERR_RETURN(-ENOMEM);
	memset(pr_on,0,sizeof(*pr_on));
	WR (q)->q_ptr = (char *) pr_on;
	q->q_ptr = (char *) pr_on;
	pr_on->q = q;

	pr_on->flags = PR_ON_TIMER;

#ifdef NEW_TIMEOUT
	pr_on->timer =
#endif
	timeout ((void *)pr_on_send, pr_on, HZ);

	MORE_USE;
	return 0;
}


static void
pr_on_wput (queue_t * q, mblk_t * mp)
{
	if (q->q_next != NULL)
		putnext (q, mp);
	else
		freemsg (mp);
	return;
}


static void
pr_on_close (queue_t * q, int dummy)
{
	struct _pr_on *pr_on = (struct _pr_on *) q->q_ptr;

	if(pr_on->flags & PR_ON_TIMER) {
		pr_on->flags &=~ PR_ON_TIMER;
#ifdef NEW_TIMEOUT
		untimeout(pr_on->timer);
#else
		untimeout (pr_on_send, pr_on);
#endif
	}

	flushq (q, FLUSHALL);
	flushq (WR (q), FLUSHALL);
	free(pr_on);
	return;
}

static void
pr_on_rput (queue_t * q, mblk_t * mp)
{
	if (q->q_next != NULL)
		putnext (q, mp);		  /* send it along too */
	else
		freemsg (mp);
	return;
}



#ifdef MODULE
static int do_init_module(void)
{
	return register_strmod(&pr_oninfo);
}

static int do_exit_module(void)
{
	return unregister_strmod(&pr_oninfo);
}
#endif
