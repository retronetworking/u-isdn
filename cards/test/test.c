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

static qf_open itest_open;
static qf_close itest_close;
static qf_srv itest_wsrv, itest_rsrv;
static qf_put itest_wput;

static struct qinit itest_rinit =
{
		NULL, NULL, itest_open, itest_close, NULL, &itest_minfo, NULL
};

static struct qinit itest_winit =
{
		itest_wput, itest_wsrv, NULL, NULL, NULL, &itest_minfo, NULL
};

struct streamtab itest_info =
{&itest_rinit, &itest_winit, NULL, NULL};

#define NCARDS 2
#define NBCHAN 3

struct testcard {
	struct _isdn1_card *card;
	struct testchannel {
		struct testcard *card;
		queue_t *q;
	} port[NBCHAN+1];
} test_card[NCARDS];


static int test_poll (struct _isdn1_card * card, short channel)
{
	struct testcard *test_card = (struct testcad *)card;
	struct testchannel *testport;

	if(channel < 0 || channel >= NBCHAN)
		return -EIO;
	testport = &testcard.port[channel];
	if(testport->q == NULL)
		return -ENXIO;
	if(WR(testport->q)->q_first == NULL)
		return -EAGAIN;
	qenable(WR(testport->q));
	return 0;
}

static int test_candata (struct _isdn1_card * card, short channel)
{
	struct testcard *test_card = (struct testcad *)card;
	struct testchannel *testport;

	if(channel < 0 || channel >= NBCHAN)
		return 0;
	testport = &testcard.port[channel];
	if(testport->q == NULL)
		return 0;
	return canput(testport->q->q_next);
}

static int test_data (struct _isdn1_card * card, short channel, mblk_t * data)
{
	struct testcard *test_card = (struct testcad *)card;
	struct testchannel *testport;

	if(channel < 0 || channel >= NBCHAN)
		return -EIO;
	testport = &testcard.port[channel];
	if(testport->q == NULL)
		return -ENXIO;
	if(!canput(testport->q->q_next))
		return -EAGAIN;
	putnext(testport->q,data);
}

data
candata

static int test_flush (struct _isdn1_card * card, short channel)
{
	struct testcard *test_card = (struct testcad *)card;
	struct testchannel *testport;

	if(channel < 0 || channel >= NBCHAN)
		return -EIO;
	testport = &testcard.port[channel];
	if(testport->q == NULL)
		return -ENXIO;
	putctlx1(testport->q,M_FLUSH,FLUSHR|FLUSHW);
}

static int test_prot (struct _isdn1_card * card, short channel, mblk_t * proto)
{
	return EINVAL;
}

static int test_mode (struct _isdn1_card * card, short channel, char mode,
	char listen)
{
	struct testcard *test_card = (struct testcad *)card;
	struct testchannel *testport;

	if(channel < 0 || channel >= NBCHAN)
		return -EIO;
	testport = &testcard.port[channel];
	if(testport->q == NULL)
		return -ENXIO;
	if(mode == MODE_OFF)
		putctlx1(testport->q,M_HANGUP);
}


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
	struct testcard *test_card;
	struct testchannel *testport;
	int mdev = 0;
	int cdev;
	int err;

	dev = minor (dev);
	cdev = dev & 0x0F;
	mdev = cdev / (NBCHAN+1);
	cdev %= (NBCHAN+1);

	if(mdev >= NCARDS) {
		U_ERROR = ENXIO;
		return OPENFAIL;
	}
	testcard = test_card[mdev];
	testport = &testcard->port[cdev];
	if(testport->q != NULL) {
		printf("itest dev 0x%x: already open, %p\n",dev,testport->q);
		return 0;
	}

	MORE_USE;

	if(cdev == 0) {
		int theID = CHAR4('T','s','t','A'+mdev);

		testcard->card.nr_chans = NBCHAN;
		testcard->card.ctl = testcard;
		testcard->card.chan = NULL;
		testcard->card.modes = (1<<M_HDLC)| (1<<M_TRANSPARENT)| (1<<M_TRANS_ALAW)| (1<<M_TRANS_V110)| (1<<M_TRANS_HDLC);
		testcard->card.ch_mode = test_mode;
		testcard->card.ch_prot = test_prot;
		testcard->card.send = test_data;
		testcard->card.flush = test_flush;
		testcard->card.cansend = test_candata;
		testcard->card.poll = test_poll;

		if((err = isdn2_register(&testcard->card, theID)) != 0) {
			U_ERROR = err;
			LESS_USE;
			return OPENFAIL;
		}
	}

	testport->card = testcard;
	testport->q = q;

	WR (q)->q_ptr = (caddr_t) testport;
	q->q_ptr = (caddr_t) testport;

	return dev;
}

/* Streams code to close the driver. */
static void
itest_close (queue_t *q, int dummy)
{
	struct testchannel *testport = (struct testchannel *) q->q_ptr;
	struct testcard *test_card = testport->card;

	if(testport->q == NULL)
		return;
	if(testport == &testcard->port[0])
		isdn2_unregister(&testcard->card);

	testport->q = NULL;

	LESS_USE;
	return;
}


/* Streams code to write data. */
static void
itest_wput (queue_t *q, mblk_t *mp)
{
	struct testchannel *testport = (struct testchannel *) q->q_ptr;
	struct testcard *test_card = testport->card;
ldprintf(".%d.",__LINE__);

#ifdef CONFIG_DEBUG_STREAMS
	if(msgdsize(mp) < 0)
		return;
#endif
	if(testport->q == NULL)  {
		freemsg(mp);
		return;
	}
	switch (DATA_TYPE(mp)) {
	case M_IOCTL:
		DATA_TYPE(mp) = M_IOCNAK;
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
	struct testchannel *testport = (struct testchannel *) q->q_ptr;
	struct testcard *testcard;
	mblk_t *mp;
	int chan;

	if(testport == NULL || testport->q == NULL) {
		flushq(q,FLUSHALL);
		return;
	}
	testcard = testport->card;
	chan = testport - testcard->port;

	while (q->q_first != NULL) {
		int err = isdn2_canrecv(&testcard->card, chan);
		if(err != 0)
			break;
		mp = getq (q)) != NULL);
		if(mp != NULL) {
			err = isdn2_recv(&testcard->card, chan,mp);
			if(err != 0)
				freemsg(mp);
		}
	}
}



#ifdef MODULE
static int devmajor1 = 0;

static int do_init_module(void)
{
	int err;

	bzero(test_card,sizeof(test_card));

	err = register_strdev(0,&itest_info,0);
	if(err < 0) {
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
	return err1;
}
#endif



