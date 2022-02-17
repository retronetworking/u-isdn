
/* Streams loopback driver, as of Sun manual */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/signal.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/user.h>
#include <sys/errno.h>

static struct module_info loop_minfo =
{
		0, "loop", 0, INFPSZ, 10240, 512
};

static int loop_open (queue_t *,dev_t,int,int);
static void  loop_close (queue_t *, int), loop_wput (queue_t *, mblk_t *);
static void loop_wsrv (queue_t *), loop_rsrv (queue_t *);

static struct qinit loop_rinit =
{
		NULL, loop_rsrv, loop_open, loop_close, NULL, &loop_minfo, NULL
};

static struct qinit loop_winit =
{
		loop_wput, loop_wsrv, NULL, NULL, NULL, &loop_minfo, NULL
};

struct streamtab loop_info =
{&loop_rinit, &loop_winit, NULL, NULL};

struct loop {
	queue_t *qptr;
	queue_t *oqptr;
};

#include "loop.h"

#define NLOOP 8
static struct loop loop_loop[NLOOP];
static int loop_cnt = NLOOP;

static int
loop_open (queue_t *q, dev_t dev, int flag, int sflag)
{
	struct loop *loop;

	if (sflag == CLONEOPEN) {
		for (dev = 0; dev < loop_cnt; dev++) {
			if (loop_loop[dev].qptr == NULL)
				break;
		}
	} else
		dev = minor (dev);

	if (dev >= loop_cnt)
		return OPENFAIL;

	if (q->q_ptr)
		return dev;

	loop = &loop_loop[dev];
	WR (q)->q_ptr = (char *) loop;
	q->q_ptr = (char *) loop;
	loop->qptr = WR (q);
#ifdef STREAMS_DEBUG
	printf("Opened %x\n",dev);
#endif

	return dev;
}

static void
loop_wput (queue_t *q, mblk_t *mp)
{
	struct loop *loop;

	loop = (struct loop *) q->q_ptr;

	switch (mp->b_datap->db_type) {
	case M_IOCTL:{
			struct iocblk *iocb;
			int error;

			iocb = (struct iocblk *) mp->b_rptr;

			switch (iocb->ioc_cmd) {
			case LOOP_SET:{
					int to;

					if (iocb->ioc_count != sizeof (int)) {
						printf ("Expected ioctl len %d, got %d\n", sizeof (int), iocb->ioc_count);

						error = EINVAL;
						goto iocnak;
					}
					to = *(int *) mp->b_cont->b_rptr;

					if (to >= loop_cnt || to < 0 ||
							!loop_loop[to].qptr) {
						error = ENXIO;
						goto iocnak;
					}
					if (loop->oqptr || loop_loop[to].oqptr) {
						error = EBUSY;
						goto iocnak;
					}
					loop->oqptr = RD (loop_loop[to].qptr);
					loop_loop[to].oqptr = RD (q);

					mp->b_datap->db_type = M_IOCACK;
					iocb->ioc_count = 0;
					qreply (q, mp);
					break;
				}
			default:
				printf ("Expected ioctl %x, got %x\n", LOOP_SET, iocb->ioc_cmd);
				error = EINVAL;
			  iocnak:{
					mp->b_datap->db_type = M_IOCNAK;
					iocb->ioc_error = error;
					qreply (q, mp);
				}
			}
			break;
		}
	case M_FLUSH:{
			if (*mp->b_rptr & FLUSHW)
				flushq (q, FLUSHDATA);
			if (*mp->b_rptr & FLUSHR) {
				flushq (RD (q), FLUSHDATA);
				*mp->b_rptr &= ~FLUSHW;
				qreply (q, mp);
			} else
				freemsg (mp);
			break;
		}
	default:{
			if (loop->oqptr == NULL) {
				putctl1 (RD (q)->q_next, M_ERROR, ENXIO);
				freemsg (mp);
				break;
			}
			putq (q, mp);
			break;
		}
	}
}

static void
loop_wsrv (queue_t *q)
{
	mblk_t *mp;
	register struct loop *loop;

	loop = (struct loop *) q->q_ptr;

	while ((mp = getq (q)) != NULL) {
		if (mp->b_datap->db_type <= QPCTL &&
				!canput (loop->oqptr->q_next)) {
			putbq (q, mp);
			break;
		}
		putnext (loop->oqptr, mp);
	}
}

static void
loop_rsrv (queue_t *q)
{
	struct loop *loop;

	loop = (struct loop *) q->q_ptr;
	if (loop->oqptr == NULL)
		return;

	qenable (WR (loop->oqptr));
}

static void
loop_close (queue_t *q, int dummy)
{
	struct loop *loop;

	loop = (struct loop *) q->q_ptr;
	loop->qptr = NULL;

	if (loop->oqptr) {
		((struct loop *) loop->oqptr->q_ptr)->qptr = NULL;
		((struct loop *) loop->oqptr->q_ptr)->oqptr = NULL;
		putctl (loop->oqptr->q_next, M_HANGUP);
		loop->oqptr = NULL;
	}
	flushq (q, FLUSHALL);
	flushq (WR (q), FLUSHALL);
}
