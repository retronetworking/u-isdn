
/* Streams queue monitoring module */

#include "f_module.h"
#include "primitives.h"
#include "kernel.h"
#include "f_signal.h"
#include "f_malloc.h"
#include "streams.h"
#include "stropts.h"
#include "f_user.h"
#include "streamlib.h"
#include "isdn_proto.h"

#define MAXB 10

static struct module_info qinfo_minfo =
{
		0, "qinfo", 0, INFPSZ, 0, 0
};

static qf_open qinfo_open;
static qf_close qinfo_close;
static qf_put qinfo_put;

static struct qinit qinfo_rinit =
{
		qinfo_put, NULL, qinfo_open, qinfo_close, NULL, &qinfo_minfo, NULL
};

static struct qinit qinfo_winit =
{
		qinfo_put, NULL, NULL, NULL, NULL, &qinfo_minfo, NULL
};

struct streamtab qinfoinfo =
{&qinfo_rinit, &qinfo_winit, NULL, NULL};

#include "qinfo.h"
#ifndef linux
#include <sys/var.h>
#endif

#define NQINFO 4
struct _qinfo {
	queue_t *qptr;
	int nr;
#ifdef NEW_TIMEOUT
	int timer;
#endif
	int timeout;
};

static streamchar *
qinf (streamchar *buf, queue_t * q)
{
	struct msgb *m = q->q_first;
	long sum = 0;
	unsigned short fl = q->q_flag;

	if (fl & QENAB)
		*buf++ = 'e';
	if (fl & QWANTR)
		*buf++ = 'r';
	if (fl & QWANTW)
		*buf++ = 'w';
	if (fl & QFULL)
		*buf++ = 'f';
	if (fl & QREADR)
		*buf++ = 'R';
#ifdef QUSE
	if (fl & QUSE)
		*buf++ = 'u';
#endif
#ifdef QRETRY
	if (fl & QRETRY)
		*buf++ = 'a'; /* "a"gain */
#endif
	if (fl & QNOENB)
		*buf++ = 'n';
	*buf++ = ' ';

	while (m != NULL) {
		int siz = dsize (m);

		sum += siz + 1;
		buf += sprintf (buf,"%d", siz);
		m = m->b_next;
		if (m != NULL)
			*buf++ = ',';
	}
	if (sum)
		buf += sprintf (buf, ": %ld", sum);
	if (q->q_count > 0) {
		if ((sum == 0) && (q->q_count > 0)) {
			buf += sprintf (buf, " * %d * ", q->q_count);
			q->q_count = 0;
			q->q_flag &= ~QFULL;
		}
		qenable (q);
		buf += sprintf (buf, " %d/%d/%d", q->q_lowat, q->q_count, q->q_hiwat);
	}
	return buf;
}


static void
qinfo_log (struct _qinfo *qinfo)
{
	mblk_t *mb;

	mb = allocb(1024,BPRI_LO);
	if(mb != NULL) {
		streamchar *bufend;
		queue_t *q = qinfo->qptr;

		m_putid(mb,PROTO_TELLME);
		m_putdelim(mb);
		DATA_TYPE(mb) = MSG_PROTO;
		bufend = mb->b_wptr;

		while (q->q_next != NULL)
			q = q->q_next;
	
		bufend += sprintf (bufend,"*** QInfo %d\n", qinfo->nr);
	
		while (q != NULL) {
			bufend += sprintf (bufend,"%s: ", q->q_qinfo->qi_minfo->mi_idname);
			bufend  = qinf (bufend, q);
			bufend += sprintf (bufend," // ");
			q = WR (q);
			bufend  = qinf (bufend,q);
			bufend += sprintf (bufend,"\n");
	
			q = q->q_next;
			if (q != NULL)
				q = RD (q);
		}
		mb->b_wptr = bufend;
		putnext(WR(qinfo->qptr),mb);
	}
#ifdef NEW_TIMEOUT
	qinfo->timer =
#endif
			timeout ((void *)qinfo_log, qinfo, qinfo->timeout);
	return;
}


static int
qinfo_open (queue_t * q, dev_t dev, int flag, int sflag ERR_DECL)
{
	struct _qinfo *qinfo;
	static int nr = 1;

	if (q->q_ptr) 
		return 0;
	qinfo = malloc(sizeof(*qinfo));
	if (qinfo == NULL)
		ERR_RETURN(-ENOMEM);
	memset(qinfo,0,sizeof(*qinfo));
	WR (q)->q_ptr = (char *) qinfo;
	q->q_ptr = (char *) qinfo;

	qinfo->qptr = q;
	qinfo->nr = nr++;
	printf ("QInfo driver %d opened.\n", qinfo->nr);
	qinfo->timeout = 60*HZ;
#ifdef NEW_TIMEOUT
	qinfo->timer =
#endif
			timeout ((void *)qinfo_log, qinfo, qinfo->timeout);

	MORE_USE;
	return 0;
}

static void
qinfo_prot (queue_t * q, mblk_t * mp)
{
	struct _qinfo *qinfo = (struct _qinfo *) q->q_ptr;
	ushort_t id;
	streamchar *origmp = mp->b_rptr;
	int error = 0;

    if (m_getid (mp, &id) != 0) {
        mp->b_rptr = origmp;
        m_reply(q,mp,ENXIO);
        return;
    }
	switch (id) {
	default:
        mp->b_rptr = origmp;
		break;
	case PROTO_TELLME:
#ifdef NEW_TIMEOUT
		untimeout (qinfo->timer);
#else
		untimeout ((void *)qinfo_log, qinfo);
#endif
		qinfo_log(qinfo);
		break;
	case PROTO_MODULE:
		if (strnamecmp (q, mp)) {	/* Config information for me. */
			long z;

			while ((mp != NULL) && ((error = m_getsx (mp, &id)) == 0)) {
				switch (id) {
				case PROTO_MODULE:
					break;
				default:
					goto err;
				case QINFO_TIMER:
					if ((error = m_geti (mp, &z)) != 0)
						goto err;
					if (z < 1 || z >= 3600) {
						goto err;
					}
					qinfo->timeout = z;
					break;
				}
			}
			mp->b_rptr = origmp;
			m_reply (q,mp,0);
			mp = NULL;
			/* printf("\n"); */
		} else {
			mp->b_rptr = origmp;
			putnext(q,mp);
			mp = NULL;
		}
		break;
	}
	if (mp != NULL) {
		if(origmp != NULL)
			mp->b_rptr = origmp;
		putnext(q,mp);
	}
	return;
  err:
	mp->b_rptr = origmp;
	m_reply(q,mp,error ? error : -EINVAL);
}
static void
qinfo_put (queue_t * q, mblk_t * mp)
{
#if 0 /* def CONFIG_DEBUG_STREAMS */
	qcheck (q->q_next, 1);
#endif
	if(DATA_TYPE(mp) == MSG_PROTO)
		qinfo_prot(q,mp);
	else
		putnext (q, mp);
#if 0 /* def CONFIG_DEBUG_STREAMS */
	qcheck (q->q_next, 2);
#endif
	return;
}



static void
qinfo_close (queue_t * q, int dummy)
{
	struct _qinfo *qinfo = (struct _qinfo *) q->q_ptr;

#ifdef NEW_TIMEOUT
	untimeout (qinfo->timer);
#else
	untimeout ((void *)qinfo_log, qinfo);
#endif

	flushq (q, FLUSHALL);
	flushq (WR (q), FLUSHALL);
	printf ("QInfo driver %d closed.\n", qinfo->nr);
	free(qinfo);
	LESS_USE;

	return;
}


#ifdef MODULE
static int do_init_module(void)
{
	return register_strmod(&qinfoinfo);
}

static int do_exit_module(void)
{
	return unregister_strmod(&qinfoinfo);
}
#endif
