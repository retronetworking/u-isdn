/*
 * Testing driver which registers a device for loopback, and so on.
 *
 */

#define UAREA

#include "f_module.h"
#include "primitives.h"
#include <sys/time.h>
#include "f_signal.h"
#include "f_malloc.h"
#include <sys/sysmacros.h>
#include "streams.h"
#include <sys/stropts.h>
/* #ifdef DONT_ADDERROR */
#include "f_user.h"
/* #endif */
#include <sys/errno.h>
#include <sys/file.h>
#include <fcntl.h>
#include <stddef.h>
#include "streamlib.h"
#include "lap.h"
#include <sys/termios.h>

#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/tqueue.h>
#include <asm/io.h>

extern void log_printmsg (void *log, const char *text, mblk_t * mp, const char*);
extern void logh_printmsg (void *log, const char *text, mblk_t * mp);

#define ddprintf(xx) printf(xx),({int x;for(x=0;x<1000;x++) udelay(1000);})
#define ldprintf if(0)printf
#if 1
#if 1
#define dprintf printf
#else
#define dprintf ({int x;for(x=0;x<300;x++) udelay(1000);}),printf
#endif
#else
#define dprintf if(0)printf
#endif

/*
 * Standard Streams driver information.
 */
static struct module_info itest_minfo =
{
	0, "itest", 0, INFPSZ, 8000, 3000
};

static struct module_info itest_mtinfo =
{
	0, "titest", 0, INFPSZ, 8000, 3000
};

static qf_open itest_open;
static qf_close itest_close;
static qf_srv itest_wsrv, itest_rsrv;
static qf_put itest_wput;

static struct qinit itest_rinit =
{
		putq, itest_rsrv, itest_open, itest_close, NULL, &itest_minfo, NULL
};

static struct qinit itest_winit =
{
		itest_wput, itest_wsrv, NULL, NULL, NULL, &itest_minfo, NULL
};

static struct qinit itest_rtinit =
{
		putq, itest_rsrv, itest_open, itest_close, NULL, &itest_mtinfo, NULL
};

static struct qinit itest_wtinit =
{
		itest_wput, itest_wsrv, NULL, NULL, NULL, &itest_mtinfo, NULL
};

struct streamtab itest_info =
{&itest_rinit, &itest_winit, NULL, NULL};

struct streamtab itest_tinfo =
{&itest_rtinit, &itest_wtinit, NULL, NULL};

#define NCARDS 2
#define NBCHAN 3

struct testdata {
	struct _isdn1_card *card;
	struct {
		queue_t *q;
	} card[NBCHAN+1];
} data[NCARDS];


/* Streams code to open the driver. */
static int
itest_open (queue_t * q, dev_t dev, int flag, int sflag
#ifdef DO_ADDERROR
		,int *err
#define U_ERROR *err
#else
#define U_ERROR u.u_error
#endif
)
{
	struct itest_info *arn = itest_list;
	struct itest_port *arp;
	int mdev = itest_ncards;
	int cdev;
	int err;
ldprintf(".%d.",__LINE__);

	dev = minor (dev);
	cdev = dev & 0x0F;
	while(mdev > 0 && cdev >= arn->nports) {
		cdev -= arn->nports;
		arn++;
		mdev --;
	}
	if(mdev == 0) {
		U_ERROR = ENXIO;
		return OPENFAIL;
	}
	arn->usage++;
	arp = &arn->port[cdev];
	if(arp->q != NULL) {
		printf("itest dev 0x%x: already open, %p\n",dev,arp->q);
		arn->usage--;
		return 0;
	}
	
	arp->q = q;
	arp->mode = dev >> 4;

	MORE_USE;
	WR (q)->q_ptr = (caddr_t) arp;
	q->q_ptr = (caddr_t) arp;

	if((err = itest_mode(arp,arp->mode)) < 0) {
		itest_dmaexit(arp);
		LESS_USE;
		arn->usage--;
		q->q_ptr = NULL;
		WR(q)->q_ptr = NULL;
		arp->q = NULL;
		U_ERROR = -err;
		return OPENFAIL;
	}
	arn->usage = 0;
	return dev;
}

/* Streams code to close the driver. */
static void
itest_close (queue_t *q, int dummy)
{
	struct itest_port *arp = (struct itest_port *) q->q_ptr;
	struct itest_info *arn = arp->inf;
ldprintf(".%d.",__LINE__);

	arn->usage++;
	if(arp->readmulti != NULL) {
		freemsg(arp->readmulti);
		arp->readmulti = NULL;
	}
	if(arp->q == NULL)
		return;
	itest_mode(arp,-1);
	itest_dmaexit(arp);
	arp->q = NULL;
	arn->usage--;

	LESS_USE;
	return;
}


/* Streams code to write data. */
static void
itest_wput (queue_t *q, mblk_t *mp)
{
	struct itest_port *arp = (struct itest_port *) q->q_ptr;
	struct itest_info *arn = arp->inf;
ldprintf(".%d.",__LINE__);

#ifdef CONFIG_DEBUG_STREAMS
	if(msgdsize(mp) < 0)
		return;
#endif
	if(arp->q == NULL)  {
		freemsg(mp);
		return;
	}
	switch (mp->b_datap->db_type) {
	case M_IOCTL:
		mp->b_datap->db_type = M_IOCNAK;
		((struct iocblk *)mp->b_rptr)->ioc_error = EINVAL;
		qreply (q, mp);
		break;
	CASE_DATA
		putq (q, mp);
		break;
	case M_FLUSH:
		if (*mp->b_rptr & FLUSHW) 
			flushq (q, 0);

		if (*mp->b_rptr & FLUSHR) {
			flushq (RD (q), 0);
			*mp->b_rptr &= ~FLUSHW;
			qreply (q, mp);
		} else
			freemsg (mp);
		break;
	default:
		log_printmsg (NULL, "Strange itest", mp, KERN_WARNING);
		/* putctl1(RD(q)->b_next, M_ERROR, ENXIO); */
		freemsg (mp);
		break;
	}
	return;
}

/* Streams code to scan the write queue. */
static void
itest_wsrv (queue_t *q)
{
	struct itest_port *arp = (struct itest_port *) q->q_ptr;
	struct itest_info *arn;
	mblk_t *mp;
	int moredata = 1;
ldprintf(".%d.",__LINE__);

	if(arp == NULL || arp->q == NULL) {
		flushq(q,FLUSHALL);
		return;
	}
	arn = arp->inf;
	if(arn->usage > 0) {
		dprintf("  * itest Write: Stop; try again *  ");
		q->q_flag |= QWANTR;
		return;
	}
	arn->usage++;
	while (moredata && ((mp = getq (q)) != NULL)) {
		int err = itest_writebuf(arp,&mp, moredata = (q->q_first != NULL));
		if(err < 0) {
			dprintf("itest %d: error %d\n",arp->portnr,err);
			putbq(q,mp);
			goto Out;
		} else if(mp != NULL) {
			dprintf("itest %d: buffer full\n",arp->portnr);
			putbq(q,mp);
			goto Out;
		}
	}
  Out:
	dprintf("EndWrite, %d\n",arn->usage);
	arn->usage--;
	return;
}



static void
itest_rsrv (queue_t * q)
{
	struct itest_port *arp = (struct itest_port *) q->q_ptr;
	struct itest_info *arn = arp->inf;
	mblk_t *mp;
ldprintf(".%d.",__LINE__);

	if(arp->q == NULL) {
		flushq(q,FLUSHALL);
		return;
	}
	while (itest_readbuf(arp,&mp) == 0) {
		if(mp != NULL)
			putq(q,mp);
	}
	while ((mp = getq (q)) != NULL) {
		if (q->q_next == NULL) {
			freemsg (mp);
			continue;
		}
		if (mp->b_datap->db_type >= QPCTL || canput (q->q_next)) {
			putnext (q, mp);
			continue;
		} else {
			putbq (q, mp);
			break;
		}
	}
	return;
}



#ifdef MODULE
static int devmajor1 = 0;
static int devmajor2 = 0;

static int do_init_module(void)
{
	int err;
ldprintf(".%d.",__LINE__);

	err = register_strdev(0,&itest_info,0);
	if(err < 0) {
		itest_exit();
		return err;
	}
	devmajor1 = err;
	return 0;
}

static int do_exit_module(void)
{
	int err1;
	int i;

	err1 = unregister_strdev(devmajor1,&itest_info,0);
	isdn2_unregister(&dumb->card);
ldprintf(".%d.",__LINE__);
	itest_exit();
	return err1;
}
#endif
