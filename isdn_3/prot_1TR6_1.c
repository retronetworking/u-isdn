#include "primitives.h"
#include "phone.h"
#include "streamlib.h"
#include "phone_1TR6.h"
#include "q_data.h"
#include "isdn_23.h"
#include "isdn3_phone.h"
#include "isdn_34.h"
#include <sys/errno.h>
#include <sys/param.h>
#include "prot_1TR6_1.h"
#include "prot_1TR6_common.h"
#include "sapi.h"

#undef HAS_SUSPEND

#define FAC_PENDING 002
#define SVC_PENDING 004

#define RUN_N1_T308 01
#define RUN_N1_T313 02
#ifdef HAS_SUSPEND
#define RUN_N1_T318 04
#endif
#define RUN_N1_T305 010
#define RUN_N1_T3D1 020
#define RUN_N1_T303 040
#define RUN_N1_T304 0100
#define RUN_N1_T310 0200
#define RUN_N1_T3D2 0400
#ifdef HAS_SUSPEND
#define RUN_N1_T319 01000
#endif
#define RUN_N1_T3AA 02000
#define RUN_N1_TCONN 04000
#define RUN_N1_TALERT 010000
#define REP_T308 020000
#define RUN_N1_TFOO 040000
#define VAL_N1_T308 ( 4 *HZ)
#define VAL_N1_T313 ( 8 *HZ)	  /* Should be 4 */
#ifdef HAS_SUSPEND
#define VAL_N1_T318 ( 4 *HZ)
#endif
#define VAL_N1_T305 ( 4 *HZ)
#define VAL_N1_T3D1 ( 10 *HZ)
#define VAL_N1_T303 ( 10 *HZ)	  /* was 4. L1 startup, TEI, L2 startup ... */
#define VAL_N1_T304 ( 20 *HZ)
#define VAL_N1_T310 ( 120 *HZ)
#define VAL_N1_T3D2 ( 10 *HZ)
#ifdef HAS_SUSPEND
#define VAL_N1_T319 ( 4 *HZ)
#endif
#define VAL_N1_T3AA ( 120 *HZ)
#define VAL_N1_TCONN ( 40 * HZ)	  /* timer for connection establishment */
#define VAL_N1_TALERT ( 1 *HZ)	  /* timer for delaying an ALERT response */
#define VAL_N1_TFOO (4*HZ)

static void N1_T308 (isdn3_conn conn);
static void N1_T313 (isdn3_conn conn);

#ifdef HAS_SUSPEND
static void N1_T318 (isdn3_conn conn);

#endif
static void N1_T305 (isdn3_conn conn);
static void N1_T3D1 (isdn3_conn conn);
static void N1_T303 (isdn3_conn conn);
static void N1_T304 (isdn3_conn conn);
static void N1_T310 (isdn3_conn conn);
static void N1_T3D2 (isdn3_conn conn);

#ifdef HAS_SUSPEND
static void N1_T319 (isdn3_conn conn);
#endif
static void N1_T3AA (isdn3_conn conn);
static void N1_TCONN (isdn3_conn conn);
static void N1_TALERT (isdn3_conn conn);
static void N1_TFOO (isdn3_conn conn);

static int send_disc (isdn3_conn conn, char release, mblk_t * data);
static void report_terminate (isdn3_conn conn, uchar_t * data, int len, ushort_t cause);

struct t_info {
	unsigned short service;
	unsigned char flags;
	unsigned char eaz;
	unsigned char nr[MAXNR];
};

static void
phone_timerup (isdn3_conn conn)
{
	rtimer (N1_T308, conn);
	rtimer (N1_T313, conn);
#ifdef HAS_SUSPEND
	rtimer (N1_T318, conn);
#endif
	rtimer (N1_T305, conn);
	rtimer (N1_T3D1, conn);
	rtimer (N1_T303, conn);
	rtimer (N1_T304, conn);
	rtimer (N1_T310, conn);
	rtimer (N1_T3D2, conn);
#ifdef HAS_SUSPEND
	rtimer (N1_T319, conn);
#endif
	rtimer (N1_T3AA, conn);
	rtimer (N1_TCONN, conn);
	rtimer (N1_TALERT, conn);
	rtimer (N1_TFOO, conn);
}

#define setstate(a,b) Xsetstate((a),(b),__LINE__)
static void
Xsetstate (isdn3_conn conn, uchar_t state, int deb_line)
{
	if(log_34 & 2)printf ("Conn PostN1:%d %ld: State %d --> %d\n", deb_line, conn->call_ref, conn->state, state);
	if (conn->state == state)
		return;
	switch (conn->state) {
	case 1:
		untimer (N1_T303, conn);
		break;
	case 2:
		untimer (N1_T304, conn);
		break;
	case 3:
		untimer (N1_T310, conn);
		break;
	case 8:
		untimer (N1_T313, conn);
		/* FALL THRU */
	case 4:
	case 10:
	case 12:
	case 14:
		untimer (N1_T3D2, conn);
		break;
	case 7:
		untimer (N1_T3D2, conn);
		untimer (N1_TCONN, conn);
		break;
	case 6:
		untimer (N1_TCONN, conn);
		untimer (N1_TALERT, conn);
		break;
	case 11:
		untimer (N1_T305, conn);
		break;
#ifdef HAS_SUSPEND
	case 15:
		untimer (N1_T319, conn);
		break;
	case 17:
		untimer (N1_T318, conn);
		break;
#endif
	case 19:
		untimer (N1_T308, conn);
		break;
	case 21:
	case 20:
		untimer (N1_T3D1, conn);
		break;
	case 99:
		untimer(N1_TFOO, conn);
		break;
	}
	conn->state = state;
	if(state > 10 || state == 0)
		report_terminate (conn, NULL,0,0);
	switch (conn->state) {
	case 1:
		timer (N1_T303, conn);
		break;
	case 2:
		timer (N1_T304, conn);
		break;
	case 3:
		timer (N1_T310, conn);
		break;
	case 6:
		ftimer (N1_TALERT, conn);
		/* FALL THRU */
	case 7:
		ftimer (N1_TCONN, conn);
		break;
	case 8:
		timer (N1_T313, conn);
		break;
	case 11:
		timer (N1_T305, conn);
		break;
#ifdef HAS_SUSPEND
	case 15:
		timer (N1_T319, conn);
		break;
	case 17:
		timer (N1_T318, conn);
		break;
#endif
	case 19:
		timer (N1_T308, conn);
		break;
	case 21:
	case 20:
		timer (N1_T3D1, conn);
		break;
	case 99:
		timer(N1_TFOO, conn);
		break;
	}
}

static int
get_serviceInd (isdn3_conn conn, uchar_t * data, int len)
{
	int qd_len;
	uchar_t *qd_data;

	qd_data = qd_find (data, len, 6, PT_N6_serviceInd, &qd_len);
	if (qd_data == NULL)
		return 0;
	if (qd_len < 1)
		return 0;
	((struct t_info *)conn->p_data)->service = *qd_data++ << 8;
	if (qd_len > 1)
		((struct t_info *)conn->p_data)->service += *qd_data;
	return 1;
}

static int
get_nr (isdn3_conn conn, uchar_t * data, int len, uchar_t what)
{
	int qd_len;
	uchar_t *qd_data;
	uchar_t *nrpos;

	((struct t_info *)conn->p_data)->nr[0] = '\0';
	qd_data = qd_find (data, len, 0, what, &qd_len);
	if (qd_data == NULL)
		return 0;
	while (qd_len-- > 0 && (*qd_data++ & 0x80) == 0) ;
	if (qd_len < 1)
		return 0;
	if (qd_len > MAXNR)
		qd_len = MAXNR;
	nrpos = ((struct t_info *)conn->p_data)->nr;
	while (qd_len-- > 0) {
		*nrpos++ = *qd_data & 0x7F;
		if (*qd_data++ & 0x80)
			break;
	}
	*nrpos = '\0';
	return 1;
}

static int
get_eaz (isdn3_conn conn, uchar_t * data, int len, uchar_t what)
{
	int qd_len;
	uchar_t *qd_data;

	((struct t_info *)conn->p_data)->eaz = 0;
	qd_data = qd_find (data, len, 0, what, &qd_len);
	if (qd_data == NULL)
		return 0;
	while (qd_len-- > 0 && (*qd_data++ & 0x80) == 0) ;
	if (qd_len < 1)
		return 0;
	if (qd_len > MAXNR)
		qd_len = MAXNR;
	while (qd_len-- > 0) {
		((struct t_info *)conn->p_data)->eaz = *qd_data & 0x7F;
		if (*qd_data++ & 0x80)
			break;
	}
	return 1;
}

static int
get_chanID (isdn3_conn conn, uchar_t * data, int len)
{
	int qd_len;
	uchar_t *qd_data;

	qd_data = qd_find (data, len, 0, PT_N0_chanID, &qd_len);
	if (qd_data == NULL)
		return 0;
	if (qd_len < 1)
		return 0;
	switch (*qd_data & 0xF0) {
	case 0x80:
		conn->bchan = *qd_data & 0x03;
		if (conn->bchan == 3) {
			if (conn->card == NULL)
				conn->bchan = 0;
			else
				conn->bchan = isdn3_free_b (conn->card);
		} else if (conn->bchan != 0) {
			if(!(conn->minorstate & MS_BCHAN)) {
				conn->minorstate |= MS_BCHAN;
				isdn3_setup_conn(conn,EST_SETUP);
			}
		}
		break;
	case 0xA0:
		if ((*qd_data & 0x03) != 1) {
			conn->bchan = 0;
			break;
		}
		if (qd_len < 3)
			return 0;
		conn->bchan = qd_data[2];
		if (conn->bchan != 0) {
			if(!(conn->minorstate & MS_BCHAN)) {
				conn->minorstate |= MS_BCHAN;
				isdn3_setup_conn(conn,EST_SETUP);
			}
		}
		if (conn->card != NULL && conn->card->bchans < conn->bchan)
			return 0;
		break;
	default:
		return 0;
	}
	return 1;
}

static int
report_setup (isdn3_conn conn, uchar_t * data, int len)
	/* Send SETUP up */
{
	int err = 0;

	mblk_t *mb = allocb (256, BPRI_MED);

	if (mb == NULL) {
		setstate (conn, 0);
		return -ENOMEM;
	}
	m_putid (mb, IND_INCOMING);
	conn_info (conn, mb);

	report_addisplay (mb, data, len);

	if ((err = isdn3_at_send (conn, mb, 0)) != 0) {
		freemsg (mb);
		setstate (conn, 99);
		return err;
	}
	return err;
}

static int
report_setup_ack (isdn3_conn conn, uchar_t * data, int len)
{
	int err = 0;

	mblk_t *mb = allocb (256, BPRI_MED);

	if (mb == NULL) {
		setstate (conn, 0);
		return -ENOMEM;
	}
	m_putid (mb, IND_INFO);
	m_putid (mb, ID_N1_SETUP_ACK);
	conn_info (conn, mb);

	report_addisplay (mb, data, len);

	if ((err = isdn3_at_send (conn, mb, 0)) != 0) {
		freemsg (mb);
		setstate (conn, 99);
		return err;
	}
	return err;
}

static int
report_call_sent (isdn3_conn conn, uchar_t * data, int len)
{
	int err = 0;

	mblk_t *mb = allocb (256, BPRI_MED);

	if (mb == NULL) {
		setstate (conn, 0);
		return -ENOMEM;
	}
	m_putid (mb, IND_INFO);
	m_putid (mb, ID_N1_CALL_SENT);
	conn_info (conn, mb);

	report_addisplay (mb, data, len);

	if ((err = isdn3_at_send (conn, mb, 0)) != 0) {
		freemsg (mb);
		setstate (conn, 99);
		return err;
	}
	return err;
}

static int
report_alert (isdn3_conn conn, uchar_t * data, int len)
{
	int err = 0;

	mblk_t *mb = allocb (256, BPRI_MED);

	if (mb == NULL) {
		setstate (conn, 0);
		return -ENOMEM;
	}
	m_putid (mb, IND_INFO);
	m_putid (mb, ID_N1_ALERT);

	conn_info (conn, mb);

	report_addisplay (mb, data, len);
	report_addstatus (mb, data, len);

	if ((err = isdn3_at_send (conn, mb, 0)) != 0) {
		freemsg (mb);
		setstate (conn, 99);
		return err;
	}
	return err;
}

static int
report_user_info (isdn3_conn conn, uchar_t * data, int len)
{
	int err = 0;
	int qd_len;
	uchar_t *qd_data;

	mblk_t *mb = allocb (256, BPRI_MED);

	if (mb == NULL) {
		setstate (conn, 0);
		return -ENOMEM;
	}
	m_putid (mb, IND_INFO);
	m_putid (mb, ID_N1_ALERT);
	conn_info (conn, mb);

	qd_data = qd_find (data, len, 0, PT_N0_userInfo, &qd_len);
	if (qd_data == NULL)
		return 0;
	m_putsx (mb, ID_N0_userInfo);
	m_puts (mb, qd_data, qd_len);

	qd_data = qd_find (data, len, 0, PT_N0_moreData, &qd_len);
	if (qd_data != NULL)
		m_putsx (mb, ID_N0_moreData);

	if ((err = isdn3_at_send (conn, mb, 0)) != 0) {
		freemsg (mb);
		setstate (conn, 99);
		return err;
	}
	return err;
}

static int
report_conn (isdn3_conn conn, uchar_t * data, int len)
{
	int err = 0;

	mblk_t *mb = allocb (256, BPRI_MED);

	if (mb == NULL) {
		setstate (conn, 0);
		return -ENOMEM;
	}
	m_putid (mb, IND_CONN);
	conn_info (conn, mb);
	report_addisplay (mb, data, len);
	report_addnsf (mb, data, len);
	report_addcost (mb, data, len);
	report_adddate (mb, data, len);

	if ((err = isdn3_at_send (conn, mb, 0)) != 0) {
		freemsg (mb);
		setstate (conn, 99);
		return err;
	}
	return err;
}

static int
report_conn_ack (isdn3_conn conn, uchar_t * data, int len)
{
	int err = 0;

	mblk_t *mb = allocb (256, BPRI_MED);

	if (mb == NULL) {
		setstate (conn, 0);
		return -ENOMEM;
	}
	m_putid (mb, IND_INFO);
	m_putid (mb, ID_N1_CONN_ACK);
	conn_info (conn, mb);

	report_addisplay (mb, data, len);
	report_addcost (mb, data, len);
	report_adddate (mb, data, len);

	if ((err = isdn3_at_send (conn, mb, 0)) != 0) {
		freemsg (mb);
		setstate (conn, 99);
		return err;
	}
	return err;
}

static int
report_info (isdn3_conn conn, uchar_t * data, int len)
{
	int err = 0;

	mblk_t *mb = allocb (256, BPRI_MED);

	if (mb == NULL) {
		setstate (conn, 0);
		return -ENOMEM;
	}
	m_putid (mb, IND_INFO);
	m_putid (mb, ID_N1_INFO);
	conn_info (conn, mb);

	report_addisplay (mb, data, len);
	report_addcost (mb, data, len);
	report_addnsf (mb, data, len);

	if ((err = isdn3_at_send (conn, mb, 0)) != 0) {
		freemsg (mb);
		setstate (conn, 99);
		return err;
	}
	return err;
}

static int
report_stat (isdn3_conn conn, uchar_t * data, int len)
{
	int err = 0;
	char cval;

	mblk_t *mb = allocb (256, BPRI_MED);

	if (mb == NULL) {
		setstate (conn, 0);
		return -ENOMEM;
	}
	m_putid (mb, IND_INFO);
	m_putid (mb, ID_N1_STAT);
	conn_info (conn, mb);

	cval = report_addcause (mb, data, len);
	conn->lockit++;
	switch (cval) {
	case N1_RemoteUserSuspend:
		isdn3_setup_conn (conn, EST_DISCONNECT /* was INTERRUPT */ );
		break;
	case N1_RemoteUserResumed:
		switch (conn->state) {
		case 7:
		case 8:
			isdn3_setup_conn (conn, EST_LISTEN);
			break;
		case 14:
			break;
#ifdef HAS_SUSPEND
		case 15:
#endif
		case 10:
			isdn3_setup_conn (conn, EST_CONNECT);
		}
		break;
	}

	if ((err = isdn3_at_send (conn, mb, 0)) != 0) {
		freemsg (mb);
		conn->lockit--;
		setstate (conn, 99);
		return err;
	}
	conn->lockit--;
	return err;
}


static void
report_terminate (isdn3_conn conn, uchar_t * data, int len, ushort_t cause)
{
	int err = 0;

	mblk_t *mb = allocb (256, BPRI_MED);

	if (mb == NULL) {
		setstate (conn, 0);
		return;
	}
	if(conn->state == 0 || conn->state == 99)
		m_putid (mb, IND_DISCONNECT);
	else
		m_putid (mb, IND_DISCONNECTING);
	if(cause != 0) {
		m_putsx(mb,ARG_CAUSE);
		m_putsx2(mb,cause);
	}
	conn_info (conn, mb);
	if (data != NULL) {
		report_addisplay (mb, data, len);
		if(cause == 0)
			report_addcause (mb, data, len);
		report_addcost (mb, data, len);
	}
	if ((err = isdn3_at_send (conn, mb, 0)) != 0) {
		freemsg (mb);
		setstate (conn, 99);
		return;
	}
	return;
}

static void
checkterm (isdn3_conn conn, uchar_t * data, int len)
{
	if (conn->state == 0) {
		report_terminate (conn, data, len,0);
		isdn3_killconn (conn, 1); /* XXX */
	}
}


static void
N1_T313 (isdn3_conn conn)
{
	if(log_34 & 2)printf ("Timer N1_T313\n");
	conn->timerflags &= ~RUN_N1_T313;
	switch (conn->state) {
	case 8:
		phone_sendback (conn, MT_N1_DISC, NULL);
		setstate (conn, 11);
		break;
	}
	checkterm (conn, NULL, 0);
}

#ifdef HAS_SUSPEND
static void
N1_T318 (isdn3_conn conn)
{
	if(log_34 & 2)printf ("Timer N1_T318\n");
	conn->timerflags &= ~RUN_N1_T318;
	switch (conn->state) {
	case 17:
		/* Err */
		setstate (conn, 99);
		break;
	}
	checkterm (conn, NULL, 0);
}

#endif

static void
N1_T305 (isdn3_conn conn)
{
	if(log_34 & 2)printf ("Timer N1_T305\n");
	conn->timerflags &= ~RUN_N1_T305;
	switch (conn->state) {
	case 11:
		/* Err */
		phone_sendback (conn, MT_N1_REL, NULL);
		setstate (conn, 19);
		break;
	}
}

static void
N1_T3D1 (isdn3_conn conn)
{
	if(log_34 & 2)printf ("Timer N1_T3D1\n");
	conn->timerflags &= ~RUN_N1_T3D1;
	switch (conn->state) {
	case 20:
	case 21:
		/* Err */
		phone_sendback (conn, MT_N1_REL, NULL);
		setstate (conn, 19);
		break;
	}
	checkterm (conn, NULL, 0);
}

static void
N1_T303 (isdn3_conn conn)
{
	if(log_34 & 2)printf ("Timer N1_T303\n");
	conn->timerflags &= ~RUN_N1_T303;
	switch (conn->state) {
	case 1:
		/* Err */
		send_disc(conn,0,NULL);
		isdn3_setup_conn (conn, EST_DISCONNECT);
		report_terminate (conn, NULL,0,ID_NOREPLY);
		break;
	}
	checkterm (conn, NULL, 0);
}

static void
N1_T304 (isdn3_conn conn)
{
	if(log_34 & 2)printf ("Timer N1_T304\n");
	conn->timerflags &= ~RUN_N1_T304;
	switch (conn->state) {
	case 2:
		/* Err */
		phone_sendback (conn, MT_N1_DISC, NULL);
		setstate (conn, 11);
		break;
	}
	checkterm (conn, NULL, 0);
}

static void
N1_T310 (isdn3_conn conn)
{
	if(log_34 & 2)printf ("Timer N1_T310\n");
	conn->timerflags &= ~RUN_N1_T310;
	switch (conn->state) {
	case 3:
		/* Err */
		phone_sendback (conn, MT_N1_DISC, NULL);
		setstate (conn, 11);
		break;
	}
	checkterm (conn, NULL, 0);
}

static void
N1_T3D2 (isdn3_conn conn)
{
	if(log_34 & 2)printf ("Timer N1_T3D2\n");
	conn->timerflags &= ~RUN_N1_T3D2;
	switch (conn->state) {
	case 4:
	case 7:
	case 8:
	case 10:
	case 14:
	case 12:
		if (((struct t_info *)conn->p_data)->flags & FAC_PENDING) {
			((struct t_info *)conn->p_data)->flags &= ~FAC_PENDING;
			isdn3_setup_conn (conn, EST_NO_CHANGE);
		}
		break;
	}
	checkterm (conn, NULL, 0);
}

static void
N1_T308 (isdn3_conn conn)
{
	if(log_34 & 2)printf ("Timer N1_T308\n");
	conn->timerflags &= ~RUN_N1_T308;
	switch (conn->state) {
	case 19:
		if (conn->timerflags & REP_T308) {
			setstate (conn, 99);
		} else {
			conn->timerflags |= REP_T308;
			phone_sendback (conn, MT_N1_REL, NULL);
			timer (N1_T308, conn);
		}
	}
	checkterm (conn, NULL, 0);
}

#ifdef HAS_SUSPEND
static void
N1_T319 (isdn3_conn conn)
{
	if(log_34 & 2)printf ("Timer N1_T319\n");
	conn->timerflags &= ~RUN_N1_T319;
	switch (conn->state) {
	case 15:
		/* Err */
		setstate (conn, 10);
		break;
	}
	checkterm (conn, NULL, 0);
}

#endif

static void
release_postn1 (isdn3_conn conn, uchar_t minor, char force)
{
	switch (conn->state) {
		case 1:
		untimer (N1_T303, conn);
		if (force) {
			/* technically an error */
		}
		break;
	case 2:
		untimer (N1_T304, conn);
		if (force) {
			/* technically an error */
		}
		break;
	case 3:
		untimer (N1_T310, conn);
		if (force) {
			/* technically an error */
		}
		break;
	case 8:
		untimer (N1_T313, conn);
		/* FALL THRU */
	case 4:
	case 7:
	case 10:
		if (force) {
			/* technically an error */
		}
		/* FALL THRU */
	case 14:
		untimer (N1_T3D2, conn);
		break;
	case 6:
		break;
#ifdef HAS_SUSPEND
	case 15:
		untimer (N1_T319, conn);
		if (force) {
			/* technically an error */
		}
		break;
	case 17:
		untimer (N1_T318, conn);
		if (force) {
			/* technically an error */
		}
		break;
#endif
	case 11:
		untimer (N1_T305, conn);
		if (!force) {
			/* Technically an error */
			force = 1;
		}
	case 12:
		force = 1;
		untimeout (N1_T3D2, conn);
		break;
	case 19:
		/* Technically an error */
		return;
	case 20:
	case 21:
		untimer (N1_T3D1, conn);
		break;
	case 99:
		return;
	default:
		setstate (conn, 99);
		return;
	}
	if (force) {
		phone_sendback (conn, MT_N1_REL, NULL);
		switch (conn->state) {
#ifdef HAS_SUSPEND
		case 15:
#endif
		case 10:
		case 4:
		case 3:
		case 2:
		case 11:
			isdn3_setup_conn (conn, EST_DISCONNECT);
		}
		conn->bchan = 0;
		conn->minorstate &=~ MS_BCHAN;
		setstate (conn, 19);
	} else {
		phone_sendback (conn, MT_N1_DISC, NULL);
		setstate (conn, 11);
	}
}


static void
N1_T3AA (isdn3_conn conn)
{
	if(log_34 & 2)printf ("Timer N1_T3AA\n");
	conn->timerflags &= ~RUN_N1_T3AA;
	switch (conn->state) {
	case 0:
		/* Err */
		break;
	default:
		if (conn->bchan != 0)
			release_postn1 (conn, 0, 0);
	}
	checkterm (conn, NULL, 0);
}

static void N1_TFOO(isdn3_conn conn)
{
	setstate(conn, 0);
	checkterm (conn, NULL, 0);
}

static void
N1_TCONN (isdn3_conn conn)
{
	if(log_34 & 2)printf ("Timer N1_TCONN\n");
	conn->timerflags &= ~RUN_N1_TCONN;
	conn->lockit++;
	switch (conn->state) {
	case 7: {
		mblk_t *mb = NULL;
		int qd_len = 0;
		streamchar *qd_d;
		if((mb = allocb(20,BPRI_LO)) != NULL) {
			
            if ((qd_d = qd_insert ((uchar_t *) mb->b_rptr, &qd_len, 0, PT_N0_cause, 4, 1)) == NULL) {                                   
                freeb(mb);
                mb = NULL;
            } else {
				if(!(conn->minorstate & MS_BCHAN))
					*qd_d = N1_UserBusy;
				else if(!(conn->minorstate & MS_SETUP_MASK))
					*qd_d = N1_NumberChanged; /* XXX ;-) */
				else
					*qd_d = N1_OutOfOrder;
				*qd_d |= 0x80;
				mb->b_wptr = mb->b_rptr + qd_len;
			}
		}
		if(send_disc (conn, 0, mb) != 0 && mb != NULL)
			freemsg(mb);
		goto term;
		}
	case 6:
		setstate (conn, 99);
		break;
	}
  term:
	conn->lockit--;
	checkterm (conn, NULL, 0);
}

static void
N1_TALERT (isdn3_conn conn)
{
	if(log_34 & 2)printf ("Timer N1_TALERT\n");
	conn->timerflags &= ~RUN_N1_TALERT;
	conn->lockit++;
	switch (conn->state) {
	case 6:
		{
			mblk_t *asn = NULL;
			int qd_len = 0;    
			uchar_t *qd_d;
																	
			if ((((struct t_info *)conn->p_data)->flags & SVC_PENDING) && (asn = allocb (32, BPRI_MED)) != NULL) {
				if ((qd_d = qd_insert ((uchar_t *) asn->b_rptr, &qd_len, 0, PT_N0_netSpecFac, 4, 1)) == NULL) {                                   
					freeb(asn);
					asn = NULL;
				} else {
					*(uchar_t *) qd_d++ = 0;                            
					*(uchar_t *) qd_d++ = N1_FAC_SVC;
					*(ushort_t *) qd_d = 0;                     
					asn->b_wptr = asn->b_rptr + qd_len;
				}
			}                                                       

			if(phone_sendback (conn, MT_N1_ALERT, asn) != 0 && asn != NULL)
				freemsg(asn);

			setstate (conn, 7);
		}
		break;
	}
	conn->lockit--;
	checkterm (conn, NULL, 0);
}

static int
recv (isdn3_conn conn, uchar_t msgtype, char isUI, uchar_t * data, ushort_t len)
{
	if (0)
		printf (" N1: Recv %x in state %d\n", msgtype, conn->state);
	if(conn->p_data == NULL) {
		if((conn->p_data = malloc(sizeof(struct t_info))) == NULL) {
			return -ENOMEM;
		}
		bzero(conn->p_data,sizeof(struct t_info));
	}
	switch (conn->state) {
	case 0:
		switch (msgtype) {
		case MT_N1_SETUP:
			conn->minorstate |= MS_INCOMING;
			setstate (conn, 6);

			(void) get_chanID (conn, data, len);
			if (!get_serviceInd (conn, data, len)) {
				setstate(conn,99);
				goto pack_err;
			}
			if (isdn3_setup_conn (conn, EST_LISTEN) != 0)
				goto pack_err;
			get_nr (conn, data, len, PT_N0_origAddr);
			get_eaz (conn, data, len, PT_N0_destAddr);

			/*
			 * Check if transferred call. If we are forwarding this B channel
			 * to another device, ignore the call.
			 */
			{
				isdn3_conn nconn;

				QD_INIT (data, len)
						break;

				((struct t_info *)conn->p_data)->flags &=~ SVC_PENDING;
				QD {
				  QD_CASE (0, PT_N0_netSpecFac):
					if (qd_len < 4)
						continue;
					switch (qd_data[1]) {
					case N1_FAC_SVC:
						((struct t_info *)conn->p_data)->flags |= SVC_PENDING;
						break;
					case N1_FAC_DisplayUebergeben:
						if (!(conn->minorstate & MS_BCHAN))
							break;
						for (nconn = conn->talk->conn; nconn != NULL; nconn = nconn->next)
							if ((nconn->minorstate & MS_FORWARDING) && (nconn->minorstate & MS_BCHAN) && (nconn->bchan == conn->bchan))
								goto out;
					}
				}
				QD_EXIT;
			}
			{
				long flags = isdn3_flags(conn->card->info,-1,-1);
				if(flags & FL_ANS_IMMED) {
					untimer(N1_TALERT,conn);
					N1_TALERT(conn);
				}
			}
			report_setup (conn, data, len);
			break;
		case MT_N1_CANC_ACK:
		case MT_N1_CANC_REJ:
		case MT_N1_REG_ACK:
		case MT_N1_REG_REJ:
		case MT_N1_SUSP_ACK:
		case MT_N1_RES_REJ:
			break;
		case MT_N1_REL:
			phone_sendback (conn, MT_N1_REL_ACK, NULL);
		case MT_N1_REL_ACK:
			break;
		default:
			phone_sendback (conn, MT_N1_REL, NULL);
			setstate (conn, 19);
			break;
		}
		break;
	case 1:
		switch (msgtype) {
		case MT_N1_SETUP_ACK:
			(void) get_chanID (conn, data, len);
			if (isdn3_setup_conn (conn, EST_LISTEN) != 0)
				goto pack_err;
			report_setup_ack (conn, data, len);
			setstate (conn, 2);
			break;
		case MT_N1_CALL_SENT:
			(void) get_chanID (conn, data, len);
			if (isdn3_setup_conn (conn, EST_LISTEN) != 0)
				goto pack_err;
			report_call_sent (conn, data, len);
			setstate (conn, 3);
			break;
		case MT_N1_REL:
			/* send REL up -- done when dropping out below */
			phone_sendback (conn, MT_N1_REL_ACK, NULL);
			setstate (conn, 0);
			report_terminate (conn, data, len,0);
			break;
		case MT_N1_DISC:
			isdn3_setup_conn (conn, EST_DISCONNECT);
			(void)send_disc (conn, 1, NULL);
			report_terminate (conn, data, len,0);
			break;
		case MT_N1_REL_ACK:
			isdn3_setup_conn (conn, EST_DISCONNECT);
			phone_sendback (conn, MT_N1_REL, NULL);
			setstate (conn, 19);
			report_terminate (conn, data, len,0);
			break;
		case MT_N1_SUSP_ACK:
		case MT_N1_RES_REJ:
		case MT_N1_REG_ACK:
		case MT_N1_REG_REJ:
		case MT_N1_CANC_ACK:
		case MT_N1_CANC_REJ:
			/* Fehlermeldung */
			break;
		default:
			break;
		}
		break;
	case 2:
		switch (msgtype) {
		case MT_N1_CALL_SENT:
			(void) get_chanID (conn, data, len);
			if (isdn3_setup_conn (conn, EST_LISTEN) != 0)
				goto pack_err;
			report_call_sent (conn, data, len);
			setstate (conn, 3);
			break;
		case MT_N1_ALERT:
			report_alert (conn, data, len);
			setstate (conn, 4);
			break;
		case MT_N1_CONN:
			(void) get_chanID (conn, data, len);
			if (!(conn->minorstate & MS_BCHAN))
				goto pack_err;
			get_serviceInd (conn, data, len);
			if (isdn3_setup_conn (conn, EST_CONNECT) != 0)
				goto pack_err;
			report_conn (conn, data, len);
			phone_sendback (conn, MT_N1_CONN_ACK, NULL);
			setstate (conn, 10);
			break;
		case MT_N1_INFO:
			{
				QD_INIT (data, len) break;
				report_info (conn, data, len);
				QD {
				  QD_CASE (0, PT_N0_netSpecFac):
					{
						char nlen;
						uchar_t facility;

						if (qd_len == 0)
							break;
						nlen = *qd_data++;
						if (qd_len < nlen + 1)
							break;
						qd_data += nlen;
						facility = *qd_data++;
						if (facility != N1_FAC_Forward1 && facility != N1_FAC_Forward2) {
							setstate (conn, 3);
						}
					}
					break;
				}
			}
			break;
		case MT_N1_REL:
			/* send REL up */
			phone_sendback (conn, MT_N1_REL, NULL);
			setstate (conn, 99);
			break;
		case MT_N1_DISC:
			isdn3_setup_conn (conn, EST_DISCONNECT);
            (void)send_disc (conn, 1, NULL);
			report_terminate (conn, data, len,0);
			break;
		case MT_N1_REL_ACK:
			/* send REL_ACK up */
			phone_sendback (conn, MT_N1_REL, NULL);
			setstate (conn, 19);
			break;
		case MT_N1_SUSP_ACK:
		case MT_N1_RES_REJ:
		case MT_N1_REG_ACK:
		case MT_N1_REG_REJ:
		case MT_N1_CANC_ACK:
		case MT_N1_CANC_REJ:
			/* Fehlermeldung */
			break;
		default:
			break;
		}
		break;
	case 3:
		switch (msgtype) {
		case MT_N1_ALERT:
			report_alert (conn, data, len);
			setstate (conn, 4);
			break;
		case MT_N1_CONN:
			(void) get_chanID (conn, data, len);
			if (!(conn->minorstate & MS_BCHAN))
				goto pack_err;
			if (isdn3_setup_conn (conn, EST_CONNECT) != 0)
				goto pack_err;
			report_conn (conn, data, len);
			phone_sendback (conn, MT_N1_CONN_ACK, NULL);
			setstate (conn, 10);
			break;
		case MT_N1_INFO:
			report_info (conn, data, len);
			setstate (conn, 3);
			break;
		case MT_N1_DISC:
			isdn3_setup_conn (conn, EST_DISCONNECT);
            (void)send_disc (conn, 1, NULL);
			report_terminate (conn, data, len,0);
			break;
		case MT_N1_REL:
			/* send REL up */
			phone_sendback (conn, MT_N1_REL_ACK, NULL);
			setstate (conn, 0);
			break;
		case MT_N1_REL_ACK:
			/* send REL_ACK up */
			untimer (N1_T310, conn);
			phone_sendback (conn, MT_N1_REL, NULL);
			setstate (conn, 19);
			break;
		case MT_N1_SUSP_ACK:
		case MT_N1_RES_REJ:
		case MT_N1_REG_ACK:
		case MT_N1_REG_REJ:
		case MT_N1_CANC_ACK:
		case MT_N1_CANC_REJ:
			/* Err */
			break;
		default:
			break;
		}
		break;
	case 4:
		switch (msgtype) {
		case MT_N1_CONN:
			(void) get_chanID (conn, data, len);
			if (!(conn->minorstate & MS_BCHAN))
				goto pack_err;
			if (isdn3_setup_conn (conn, EST_CONNECT) != 0)
				goto pack_err;
			report_conn (conn, data, len);
			phone_sendback (conn, MT_N1_CONN_ACK, NULL);
			setstate (conn, 10);
			break;
		case MT_N1_FAC_ACK:
			/* send FAC_ACK up */
			untimer (N1_T3D2, conn);
			break;
		case MT_N1_FAC_REJ:
			/* send FAC_REJ up */
			untimer (N1_T3D2, conn);
			break;
		case MT_N1_ALERT:
			report_alert (conn, data, len);
			break;
		case MT_N1_INFO:
			report_info (conn, data, len);
			break;
		case MT_N1_REL:
			/* send REL up */
			phone_sendback (conn, MT_N1_REL_ACK, NULL);
			setstate (conn, 0);
			break;
		case MT_N1_DISC:
			isdn3_setup_conn (conn, EST_DISCONNECT);
            (void)send_disc (conn, 1, NULL);
			report_terminate (conn, data, len,0);
			break;
		case MT_N1_REL_ACK:
			untimer (N1_T3D2, conn);
			phone_sendback (conn, MT_N1_REL, NULL);
			setstate (conn, 19);
			break;
		case MT_N1_SUSP_ACK:
		case MT_N1_RES_REJ:
		case MT_N1_REG_ACK:
		case MT_N1_REG_REJ:
		case MT_N1_CANC_ACK:
		case MT_N1_CANC_REJ:
			/* Fehlermeldung */
			break;
		default:
			break;
		}
		break;
	case 6:
		switch (msgtype) {
		case MT_N1_SETUP:
			break;
		case MT_N1_REL:
			phone_sendback (conn, MT_N1_REL_ACK, NULL);
			setstate (conn, 99);
			break;
		case MT_N1_DISC:
			isdn3_setup_conn (conn, EST_DISCONNECT);
            (void)send_disc (conn, 1, NULL);
			report_terminate (conn, data, len,0);
			break;
		case MT_N1_REL_ACK:
			phone_sendback (conn, MT_N1_REL, NULL);
			setstate (conn, 19);
			break;
		case MT_N1_SUSP_ACK:
		case MT_N1_RES_REJ:
		case MT_N1_REG_ACK:
		case MT_N1_REG_REJ:
		case MT_N1_CANC_ACK:
		case MT_N1_CANC_REJ:
			/* Fehlermeldung */
			break;
		default:
			break;
		}
		break;
	case 7:
		switch (msgtype) {
		case MT_N1_SETUP:
			(void) get_chanID (conn, data, len);
			if (isdn3_setup_conn (conn, EST_LISTEN) != 0)
				goto pack_err;
			break;
		case MT_N1_FAC_ACK:
			/* send FAC_ACK up */
			untimer (N1_T3D2, conn);
			break;
		case MT_N1_FAC_REJ:
			/* send FAC_REJ up */
			untimer (N1_T3D2, conn);
			break;
		case MT_N1_INFO:
			report_info (conn, data, len);
			break;
		case MT_N1_STAT:
			report_stat (conn, data, len);
			break;
		case MT_N1_REL:
			/* send REL up */
			phone_sendback (conn, MT_N1_REL_ACK, NULL);
			setstate (conn, 99);
			break;
		case MT_N1_DISC:
			isdn3_setup_conn (conn, EST_DISCONNECT);
            (void)send_disc (conn, 1, NULL);
			report_terminate (conn, data, len,0);
			break;
		case MT_N1_REL_ACK:
			/* send REL_ACK up */
			untimer (N1_T3D2, conn);
			phone_sendback (conn, MT_N1_REL, NULL);
			setstate (conn, 19);
			break;
		case MT_N1_SUSP_ACK:
		case MT_N1_RES_REJ:
		case MT_N1_REG_ACK:
		case MT_N1_REG_REJ:
		case MT_N1_CANC_ACK:
		case MT_N1_CANC_REJ:
			/* Fehlermeldung */
			break;
		default:
			break;
		}
		break;
	case 8:
		switch (msgtype) {
		case MT_N1_CONN_ACK:
			(void) get_chanID (conn, data, len);
			if (!(conn->minorstate & MS_BCHAN))
				goto pack_err;
			if (isdn3_setup_conn (conn, EST_CONNECT) != 0)
				goto pack_err;
			report_conn_ack (conn, data, len);
			setstate (conn, 10);
			break;
		case MT_N1_SETUP:
			break;
		case MT_N1_FAC_ACK:
			/* send FAC_ACK up */
			untimer (N1_T3D2, conn);
			break;
		case MT_N1_FAC_REJ:
			/* send FAC_REJ up */
			untimer (N1_T3D2, conn);
			break;
		case MT_N1_INFO:
			report_info (conn, data, len);
			break;
		case MT_N1_STAT:
			report_stat (conn, data, len);
			break;
		case MT_N1_REL:
			/* send REL up */
			phone_sendback (conn, MT_N1_REL_ACK, NULL);
			setstate (conn, 0);
			break;
		case MT_N1_DISC:
			isdn3_setup_conn (conn, EST_DISCONNECT);
            (void)send_disc (conn, 1, NULL);
			report_terminate (conn, data, len,0);
			break;
		case MT_N1_REL_ACK:
			/* send REL_ACK up */
			phone_sendback (conn, MT_N1_REL, NULL);
			setstate (conn, 19);
			break;
		case MT_N1_SUSP_ACK:
		case MT_N1_RES_REJ:
		case MT_N1_REG_ACK:
		case MT_N1_REG_REJ:
		case MT_N1_CANC_ACK:
		case MT_N1_CANC_REJ:
			/* Fehlermeldung */
			break;
		default:
			break;
		}
		break;
	case 10:
		switch (msgtype) {
		case MT_N1_SETUP:
			break;
		case MT_N1_FAC:
			/* send FAC up */
			break;
		case MT_N1_FAC_ACK:
			/* send FAC_ACK up */
			untimer (N1_T3D2, conn);
			break;
		case MT_N1_FAC_REJ:
			/* send FAC_REJ up */
			untimer (N1_T3D2, conn);
			break;
		case MT_N1_INFO:
			report_info (conn, data, len);
			break;
		case MT_N1_STAT:
			report_stat (conn, data, len);
			break;
		case MT_N1_USER_INFO:
			report_user_info (conn, data, len);
			break;
		case MT_N1_CON_CON:
			/* send CON_CON up */
			break;
		case MT_N1_REL:
			/* send REL up */
			phone_sendback (conn, MT_N1_REL_ACK, NULL);
			setstate (conn, 0);
			break;
		case MT_N1_DISC:
			setstate (conn, 12);
			isdn3_setup_conn (conn, EST_DISCONNECT);
            (void)send_disc (conn, 1, NULL);
			report_terminate (conn, data, len,0);
			break;
		case MT_N1_REL_ACK:
			/* send REL_ACK up */
			phone_sendback (conn, MT_N1_REL, NULL);
			setstate (conn, 19);
			break;
		case MT_N1_SUSP_ACK:
			/* Fehler */
			break;
		case MT_N1_RES_REJ:
		case MT_N1_REG_ACK:
		case MT_N1_REG_REJ:
		case MT_N1_CANC_ACK:
		case MT_N1_CANC_REJ:
			/* Fehlermeldung */
			break;
		default:
			break;
		}
		break;
	case 14:
		switch (msgtype) {
		case MT_N1_SETUP:
			break;
		case MT_N1_FAC_ACK:
			/* send FAC_ACK up */
			untimer (N1_T3D2, conn);
			break;
		case MT_N1_FAC_REJ:
			/* send FAC_REJ up */
			untimer (N1_T3D2, conn);
			break;
		case MT_N1_INFO:
			report_info (conn, data, len);
			break;
		case MT_N1_STAT:
			report_stat (conn, data, len);
			break;
		case MT_N1_CONN_ACK:
			(void) get_chanID (conn, data, len);
			if (!(conn->minorstate & MS_BCHAN))
				goto pack_err;
			if (isdn3_setup_conn (conn, EST_CONNECT) != 0)
				goto pack_err;
			report_conn_ack (conn, data, len);
			setstate (conn, 10);
			break;
		case MT_N1_REL:
			/* send REL up */
			phone_sendback (conn, MT_N1_REL_ACK, NULL);
			setstate (conn, 0);
			break;
		case MT_N1_DISC:
			isdn3_setup_conn (conn, EST_DISCONNECT);
            (void)send_disc (conn, 1, NULL);
			report_terminate (conn, data, len,0);
			break;
		case MT_N1_REL_ACK:
			phone_sendback (conn, MT_N1_REL, NULL);
			setstate (conn, 19);
			break;
		case MT_N1_SUSP_ACK:
		case MT_N1_RES_REJ:
		case MT_N1_REG_ACK:
		case MT_N1_REG_REJ:
		case MT_N1_CANC_ACK:
		case MT_N1_CANC_REJ:
			/* Fehlermeldung */
			break;
		default:
			break;
		}
		break;
#ifdef HAS_SUSPEND
	case 15:
		switch (msgtype) {
		case MT_N1_SETUP:
			break;
		case MT_N1_SUSP_ACK:
			/* send SUSP_ACK up */
			setstate (conn, 99);
			break;
		case MT_N1_SUSP_REJ:
			/* send SUSP_REJ up */
			setstate (conn, 10);
			break;
		case MT_N1_INFO:
			report_info (conn, data, len);
			break;
		case MT_N1_STAT:
			report_stat (conn, data, len);
			break;
		case MT_N1_REL:
			/* send REL up */
			phone_sendback (conn, MT_N1_REL_ACK, NULL);
			setstate (conn, 0);
			break;
		case MT_N1_DISC:
			isdn3_setup_conn (conn, EST_DISCONNECT);
            (void)send_disc (conn, 1, NULL);
			report_terminate (conn, data, len,0);
			break;
		case MT_N1_REL_ACK:
			/* send REL_ACK up */
			phone_sendback (conn, MT_N1_REL, NULL);
			setstate (conn, 19);
			break;
		case MT_N1_RES_REJ:
		case MT_N1_REG_ACK:
		case MT_N1_REG_REJ:
		case MT_N1_CANC_ACK:
		case MT_N1_CANC_REJ:
			/* Fehlermeldung */
			break;
		default:
			break;
		}
		break;
	case 17:
		switch (msgtype) {
		case MT_N1_RES_ACK:
			/* send RES_ACK up */
			(void) get_chanID (conn, data, len);
			if (isdn3_setup_conn (conn, EST_CONNECT) != 0)
				goto pack_err;
			setstate (conn, 10);
			break;
		case MT_N1_RES_REJ:
			/* send RES_REJ up */
			setstate (conn, 99);
			break;
		case MT_N1_REL:
			/* send REL up */
			phone_sendback (conn, MT_N1_REL_ACK, NULL);
			setstate (conn, 0);
			break;
		case MT_N1_REL_ACK:
			/* send REL_ACK up */
			goto common_17_REL;
		case MT_N1_DISC:
			isdn3_setup_conn (conn, EST_DISCONNECT);
		  common_17_REL:
			phone_sendback (conn, MT_N1_REL, NULL);
			setstate (conn, 19);
			report_terminate (conn, data, len,0);
			break;
		case MT_N1_SUSP_ACK:
		case MT_N1_REG_ACK:
		case MT_N1_REG_REJ:
		case MT_N1_CANC_ACK:
		case MT_N1_CANC_REJ:
			/* Fehlermeldung */
			setstate (conn, 4);
			break;
		default:
			break;
		}
		break;
#endif
	case 11:
		switch (msgtype) {
		case MT_N1_SETUP:
			break;
		case MT_N1_REL:
			/* send REL up */
			phone_sendback (conn, MT_N1_REL_ACK, NULL);
			setstate (conn, 0);
			break;
		case MT_N1_SUSP_ACK:
			/* xxx */
			break;
		case MT_N1_FAC_ACK:
			/* send FAC_ACK up */
			break;
		case MT_N1_FAC_REJ:
			/* send FAC_REJ up */
			break;
		case MT_N1_INFO:
			report_info (conn, data, len);
			break;
		case MT_N1_STAT:
			report_stat (conn, data, len);
			break;
		case MT_N1_REL_ACK:
			/* send REL_ACK up */
			/* Release B chan */
			phone_sendback (conn, MT_N1_REL, NULL);
			setstate (conn, 19);
			break;
		default:
			break;
		}
		break;
	case 12:
		switch (msgtype) {
		case MT_N1_SETUP:
			break;
		case MT_N1_REL:
			/* send REL up */
			phone_sendback (conn, MT_N1_REL_ACK, NULL);
			setstate (conn, 0);
			break;
		case MT_N1_FAC_ACK:
			/* send FAC_ACK up */
			untimer (N1_T3D2, conn);
			break;
		case MT_N1_FAC_REJ:
			/* send FAC_REJ up */
			untimer (N1_T3D2, conn);
			break;
		case MT_N1_INFO:
			report_info (conn, data, len);
			break;
		case MT_N1_STAT:
			report_stat (conn, data, len);
			break;
		case MT_N1_REL_ACK:
			/* send REL_ACK up */
			phone_sendback (conn, MT_N1_REL, NULL);
			setstate (conn, 19);
			break;
		case MT_N1_SUSP_ACK:
		case MT_N1_RES_REJ:
		case MT_N1_REG_ACK:
		case MT_N1_REG_REJ:
		case MT_N1_CANC_ACK:
		case MT_N1_CANC_REJ:
			/* Fehlermeldung */
			break;
		default:
			break;
		}
		break;
	case 19:
		switch (msgtype) {
		case MT_N1_SETUP:
			break;
		case MT_N1_REL:
			/* send REL up */
			setstate (conn, 99);
			break;
		case MT_N1_REL_ACK:
			/* send REL_ACK up */
			setstate (conn, 0);
			break;
		case MT_N1_FAC_ACK:
			/* send FAC_ACK up */
			break;
		case MT_N1_FAC_REJ:
			/* send FAC_REJ up */
			break;
		case MT_N1_INFO:
			report_info (conn, data, len);
			break;
		case MT_N1_STAT:
			report_stat (conn, data, len);
			break;
		default:
			break;
		}
		break;
	case 20:
		switch (msgtype) {
		case MT_N1_REL:
			/* send REL up */
			phone_sendback (conn, MT_N1_REL_ACK, NULL);
			setstate (conn, 0);
			break;
		case MT_N1_REG_ACK:
			/* send REG_ACK up */
			setstate (conn, 99);
			break;
		case MT_N1_REG_REJ:
			/* send REG_REJ up */
			setstate (conn, 99);
			break;
		case MT_N1_DISC:
			isdn3_setup_conn (conn, EST_DISCONNECT);
			goto common_20_REL_ACK;
		case MT_N1_REL_ACK:
			/* send REL_ACK up */
		  common_20_REL_ACK:
			phone_sendback (conn, MT_N1_REL, NULL);
			setstate (conn, 19);
			report_terminate (conn, data, len,0);
			break;
		case MT_N1_SUSP_ACK:
		case MT_N1_RES_REJ:
		case MT_N1_CANC_ACK:
		case MT_N1_CANC_REJ:
			/* Fehlermeldung */
			break;
		default:
			break;
		}
		break;
	case 21:
		switch (msgtype) {
		case MT_N1_REL:
			/* send REL up */
			phone_sendback (conn, MT_N1_REL_ACK, NULL);
			setstate (conn, 0);
			break;
		case MT_N1_CANC_ACK:
			/* send CANC_ACK up */
			setstate (conn, 99);
			break;
		case MT_N1_CANC_REJ:
			/* send CANC_REJ up */
			setstate (conn, 99);
			break;
		case MT_N1_DISC:
			isdn3_setup_conn (conn, EST_DISCONNECT);
			goto common_21_REL_ACK;
		case MT_N1_REL_ACK:
			/* send REL_ACK up */
		  common_21_REL_ACK:
			phone_sendback (conn, MT_N1_REL, NULL);
			setstate (conn, 19);
			report_terminate (conn, data, len,0);
			break;
		case MT_N1_SUSP_ACK:
		case MT_N1_RES_REJ:
		case MT_N1_REG_ACK:
		case MT_N1_REG_REJ:
			/* Fehlermeldung */
			break;
		default:
			break;
		}
		break;
	case 99:
		break;
	default:
		setstate (conn, 99);
		break;
	}
  out:
	checkterm (conn, data, len);
	return 0;
  pack_err:
	release_postn1 (conn, 0, 1);
	goto out;
}

static int
chstate (isdn3_conn conn, uchar_t ind, short add)
{
	if(0)printf ("PHONE state for card %d says %d:%o\n", conn->card->nr, ind, add);
	if(conn->p_data == NULL) {
		if((conn->p_data = malloc(sizeof(struct t_info))) == NULL) {
			return -ENOMEM;
		}
		bzero(conn->p_data,sizeof(struct t_info));
	}
	switch (ind) {
	case DL_ESTABLISH_IND:
	case DL_ESTABLISH_CONF:
		if (conn->talk->state & PHONE_UP) {		/* Reestablishment */
			switch (conn->state) {
			case 1:
			case 3:
			case 4:
			case 8:
			case 10:
#ifdef HAS_SUSPEND
			case 15:
			case 17:
#endif
			case 12:
			case 20:
			case 21:
				/* Error */
				/* FALL THRU */
			case 11:
			case 19:
				break;
			case 2:
				/* Error */
				phone_sendback (conn, MT_N1_DISC, NULL);
				setstate (conn, 11);
				break;
			case 7:
				if (0) {		  /* Last MSG war ALERT */
			case 14:
					untimer (N1_T3D2, conn);
					phone_sendback (conn, MT_N1_DISC, NULL);
					timer (N1_T305, conn);
				} else {
					/* Error */
				}
				break;
			}
			checkterm (conn, NULL, 0);
		} else
			phone_timerup (conn);
		break;
	case MDL_ERROR_IND:
		if(!(add & ERR_G))
			break;
		/* FALL THRU */
	case DL_RELEASE_IND:
	case DL_RELEASE_CONF:
	case PH_DEACTIVATE_CONF:
	case PH_DEACTIVATE_IND:
	case PH_DISCONNECT_IND:
		setstate (conn, 99);
		checkterm (conn, NULL, 0);
		break;
	}
	return 0;
}

static int
send_disc (isdn3_conn conn, char release, mblk_t * data)
{
	int err = 0, err2;

	switch (conn->state) {
	case 0:
		break;
#ifdef HAS_SUSPEND
	case 17:
#endif
	case 14:
	case 20:
	case 21:
	case 8:
	  common_off2:
		if ((err = phone_sendback (conn, MT_N1_DISC, data)) == 0)
			data = NULL;
		if (release) {
	case 6:
	case 7:
#if 1
			if (release > 1) {
				release = 0;
				goto common_off2;
			}
#endif
			if ((err2 = phone_sendback (conn, MT_N1_REL, data)) != 0)
				err = err2;
			setstate (conn, 19);
		} else {
			setstate (conn, 11);
		}
		break;
	case 11:
		if (release) {
			goto common_off_noconn;
		} else
			return -EBUSY;
	case 1:
	case 2:
	case 3:
#ifdef HAS_SUSPEND
	case 15:
#endif
	case 4:
	case 10:
	   /* common_off: */
		if ((err = phone_sendback (conn, MT_N1_DISC, data)) == 0)
			data = NULL;
		if (!release)
			setstate (conn, 11);
		else
			setstate (conn, 12);
		break; /* NO FALL THRU */
	case 12:
	  common_off_noconn:
		/* B-Kanal trennen */
		if ((err2 = phone_sendback (conn, MT_N1_REL, data)) != 0)
			err = err2;
		setstate (conn, 19);
		break;
	case 19:
		break;
	}
	return err;
}

static int
sendcmd (isdn3_conn conn, ushort_t id, mblk_t * data)
{
	streamchar *oldpos = data->b_rptr;
	ulong_t service = ~0;
	int err = 0;
	ushort_t typ;
	char suppress = 0;
	char svc = 0;
	char doforce = 0;
	char donodisc = 0;
	long cause = -1;

	if(conn->p_data == NULL) {
		if((conn->p_data = malloc(sizeof(struct t_info))) == NULL) {
			return -ENOMEM;
		}
		bzero(conn->p_data,sizeof(struct t_info));
	}
	if(data != NULL) {
		while (m_getsx (data, &typ) == 0) {
			switch (typ) {
			case ARG_FASTDROP:
				if (conn->state == 6 || conn->state == 7 ||
					conn->state == 8)
					doforce = 1;
				break;
			case ARG_CAUSE:
				{
					ushort_t causeid;

					if (m_getid (data, &causeid) != 0)
						break;
					cause = n1_idtocause(causeid);
				}
				break;
			case ARG_FORCE:
				doforce = 1;
				break;
			case ARG_NODISC:
				donodisc = 1;
				break;
			case ARG_SUPPRESS:
				suppress = 1;
				break;
			case ARG_SPV:
				svc = 1;
				break;
			case ARG_SERVICE:
				if ((err = m_getx (data, &service)) != 0) {
					data->b_rptr = oldpos;
					return err;
				}
				break;
			case ARG_NUMBER:
				m_getskip (data);
				if ((err = m_getstr (data, (char *) ((struct t_info *)conn->p_data)->nr, MAXNR)) != 0) {
					printf("GetStr Number: ");
					return err;
				}
				break;
			case ARG_LNUMBER:
				{
					char nbuf[MAXNR];
					m_getskip (data);
					if ((err = m_getstr (data, nbuf, MAXNR)) != 0) {
						printf("GetX EAZans: ");
						return err;
					}
					((struct t_info *)conn->p_data)->eaz = nbuf[strlen(nbuf)-1];
				}
				break;
			}
		}
	}
	conn->lockit++;
	switch (id) {
	case CMD_DIAL:
		{
			if (data == NULL) {
				printf("DataNull: ");
				conn->lockit--;
				return -EINVAL;
			}
		  /* end_arg_dial: */
			if (service == ~0) {
				if(log_34 & 2)printf("No Service: ");
				conn->lockit--;
				return -EINVAL;
			}

			conn->minorstate |= MS_OUTGOING | MS_WANTCONN;

			conn->lockit++;
			isdn3_setup_conn (conn, EST_SETUP);
			conn->lockit--;

			switch (conn->state) {
			case 0:
				if (conn->minor == 0) {
					printf("ConnMinorZero: ");
					conn->lockit--;
					return -ENOENT;
				}
				{
					mblk_t *asn = allocb (256, BPRI_MED);
					int qd_len = 0;
					uchar_t *qd_d;

					if (asn == NULL) {
						conn->lockit--;
						return -ENOMEM;
					}

					if (suppress) {
						if ((qd_d = qd_insert ((uchar_t *) asn->b_rptr, &qd_len, 0, PT_N0_netSpecFac, 4, 1)) == NULL) {
							conn->lockit--;
							return -EIO;
						}
						*(uchar_t *) qd_d++ = 0;
						*(uchar_t *) qd_d++ = N1_FAC_Unterdruecke;
						*(ushort_t *) qd_d = 0;
					}
					if (svc) {
						if ((qd_d = qd_insert ((uchar_t *) asn->b_rptr, &qd_len, 0, PT_N0_netSpecFac, 4, 1)) == NULL) {
							conn->lockit--;
							return -EIO;
						}
						*(uchar_t *) qd_d++ = 0;
						*(uchar_t *) qd_d++ = N1_FAC_SVC;
						*(ushort_t *) qd_d = 0;
						if ((qd_d = qd_insert ((uchar_t *) asn->b_rptr, &qd_len, 0, PT_N0_netSpecFac, 4, 1)) == NULL) {
							conn->lockit--;
							return -EIO;
						}
						*(uchar_t *) qd_d++ = 0;
						*(uchar_t *) qd_d++ = N1_FAC_Activate;
						*(ushort_t *) qd_d = 0;
					}
					if ((qd_d = qd_insert ((uchar_t *) asn->b_rptr, &qd_len, 6, PT_N6_serviceInd, 2, 0)) == NULL) {
						conn->lockit--;
						return -EIO;
					}
					*((uchar_t *) qd_d) = service >> 8;
					*(((uchar_t *) qd_d)+1) = service & 0xFF;
					if (((struct t_info *)conn->p_data)->eaz != 0) {
						if ((qd_d = qd_insert ((uchar_t *) asn->b_rptr, &qd_len, 0, PT_N0_origAddr, 2, 0)) == NULL) {
							conn->lockit--;
							return -EIO;
						}
						*qd_d++ = 0x81;
						*qd_d++ = ((struct t_info *)conn->p_data)->eaz;
					}
					if (((struct t_info *)conn->p_data)->nr[0] != '\0') {
						int i;

						for (i = 0; i < MAXNR; i++)
							if (((struct t_info *)conn->p_data)->nr[i] == '\0')
								break;
						if ((qd_d = qd_insert ((uchar_t *) asn->b_rptr, &qd_len, 0, PT_N0_destAddr, i + 1, 0)) == NULL) {
							conn->lockit--;
							return -EIO;
						}
						*qd_d++ = 0x81;
						while (i--)
							qd_d[i] = ((struct t_info *)conn->p_data)->nr[i];
					}
					if (conn->bchan != 0) {
						if ((qd_d = qd_insert ((uchar_t *) asn->b_rptr, &qd_len, 0, PT_N0_chanID, (conn->bchan <= 2) ? 1 : 3, 0)) == NULL) {
							conn->lockit--;
							return -EIO;
						}
						if (conn->bchan <= 2) {
							*qd_d = 0x80 | conn->bchan;
						} else {
							*qd_d++ = 0xA1;
							*qd_d++ = 0xC3;
							*qd_d++ = conn->bchan;
						}
						conn->bchan = 0;		/* Network will tell us which
												 * to use */
					}
					/* NSF: SemiPerm etc. */
					asn->b_wptr = asn->b_rptr + qd_len;
					if ((err = phone_sendback (conn, MT_N1_SETUP, asn)) != 0) {
						freeb (asn);
						printf("SendBack: ");
						conn->lockit--;
						return err;
					}
					setstate (conn, 1);
				}
				break;
			default:
				printf("Default %d: ", conn->state);
				conn->lockit--;
				return -EBUSY;
			}
		}
		break;
	case CMD_PREPANSWER:
		switch (conn->state) {
		case 6: {
	 		mblk_t *asn = NULL;
        	int qd_len = 0;    
        	uchar_t *qd_d;
			setstate (conn, 7);
                                                                  
        	if ((((struct t_info *)conn->p_data)->flags & SVC_PENDING) && (asn = allocb (32, BPRI_MED)) != NULL) {
            	if ((qd_d = qd_insert ((uchar_t *) asn->b_rptr, &qd_len, 0, PT_N0_netSpecFac, 4, 1)) == NULL) {                                   
                	freeb(asn);
                	asn = NULL;
            	} else {
            		*(uchar_t *) qd_d++ = 0;                            
            		*(uchar_t *) qd_d++ = N1_FAC_SVC;
            		*(ushort_t *) qd_d = 0;                     
					asn->b_wptr = asn->b_rptr + qd_len;
				}
        	}                                                       

			if(phone_sendback (conn, MT_N1_ALERT, asn) != 0 && asn != NULL)
				freemsg(asn);

			} break;
		default:
			printf("BadState1: ");
			err = -EINVAL;
		}
		break;
	case CMD_ANSWER:
		{
			mblk_t *asn = NULL;

			if (data != NULL) {
				int qd_len = 0;
				uchar_t *qd_d;

				if ((asn = allocb (32, BPRI_MED)) == NULL) {
					conn->lockit--;
					return -ENOMEM;
				}

				if (((struct t_info *)conn->p_data)->flags & SVC_PENDING) {
					if ((qd_d = qd_insert ((uchar_t *) asn->b_rptr, &qd_len, 0, PT_N0_netSpecFac, 4, 1)) == NULL) {
						freeb(asn);
						conn->lockit--;
						return -EIO;
					}
					*(uchar_t *) qd_d++ = 0;
					*(uchar_t *) qd_d++ = N1_FAC_SVC;
					*(ushort_t *) qd_d = 0;
					if ((qd_d = qd_insert ((uchar_t *) asn->b_rptr, &qd_len, 0, PT_N0_netSpecFac, 4, 1)) == NULL) {
						freeb(asn);
						conn->lockit--;
						return -EIO;
					}
					*(uchar_t *) qd_d++ = 0;
					*(uchar_t *) qd_d++ = N1_FAC_Activate;
					*(ushort_t *) qd_d = 0;
				}

				if ((qd_d = qd_insert ((uchar_t *) asn->b_rptr, &qd_len, 6, PT_N6_serviceInd, 2, 0)) == NULL) {
					freeb (asn);
					conn->lockit--;
					return -EIO;
				}
				*((uchar_t *) qd_d) = service >> 8;
				*(((uchar_t *) qd_d)+1) = service & 0xFF;
				if (((struct t_info *)conn->p_data)->eaz != 0) {
					if ((qd_d = qd_insert ((uchar_t *) asn->b_rptr, &qd_len, 0, PT_N0_origAddr, 2, 0)) == NULL) {
						conn->lockit--;
						return -EIO;
					}
					*qd_d++ = 0x81;
					*qd_d++ = ((struct t_info *)conn->p_data)->eaz;
				}
				asn->b_wptr = asn->b_rptr + qd_len;
			}
			switch (conn->state) {
			case 6:
			case 7:
				break;
			default:
				printf("BadState2 ");
				err = -EINVAL;
				if(asn != NULL)
					freemsg(asn);
				goto ex;
			}
			conn->minorstate |= MS_WANTCONN;

			isdn3_setup_conn (conn, EST_SETUP);

			if (((conn->delay > 0) && (conn->minorstate & MS_DELAYING))
				 || !(conn->minorstate & MS_INITPROTO)
				 /* || !(conn->minorstate & MS_BCHAN) */ ) {
				data->b_rptr = oldpos;
				isdn3_repeat (conn, id, data);
				if (asn != NULL)
					freemsg (asn);
				conn->lockit--;
				return 0;
			}
			setstate (conn, 8);
			if ((err = phone_sendback (conn, MT_N1_CONN, asn)) == 0)
				asn = NULL;
			isdn3_setup_conn (conn, EST_LISTEN);
			if (asn != NULL)
				freemsg (asn);
		}
	  ex:
		break;
#if 0
	case CMD_FORWARD:
		{
			mblk_t *asn = NULL;
			char donum = 0;
			char gotservice = 0;
			char eaz = 0;
			char eaz2 = 0;

			service = ((struct t_info *)conn->p_data)->service;

			if (data == NULL) {
				conn->lockit--;
				return -ENOENT;
			}
			isdn3_setup_conn (conn, EST_NO_CHANGE);

			if (((conn->delay > 0) && (conn->minorstate & MS_DELAYING))
			     || !(conn->minorstate & MS_INITPROTO)
				 || !(conn->minorstate & MS_BCHAN)
				 || (((struct t_info *)conn->p_data)->flags & FAC_PENDING)) {
				data->b_rptr = oldpos;
				isdn3_repeat (conn, id, data);
				conn->lockit--;
				return 0;
			}
			if ((conn->minorstate & MS_CONN_MASK) == MS_CONN_NONE) {
				printf("NoConnThere ");
				conn->lockit--;
				return -EINVAL;
			}
			if ((conn->minorstate & MS_CONN_MASK) != MS_CONN_INTERRUPT) {
				isdn3_setup_conn (conn, EST_WILL_INTERRUPT);
				data->b_rptr = oldpos;
				isdn3_repeat (conn, id, data);
				conn->lockit--;
				return 0;
			}
			{
				int qd_len = 0;
				uchar_t *qd_d;

				if ((asn = allocb (32, BPRI_MED)) == NULL) {
					conn->lockit--;
					return -ENOMEM;
				}

				if ((qd_d = qd_insert ((uchar_t *) asn->b_rptr, &qd_len, 0, PT_N0_netSpecFac, (gotservice || eaz2 != 0) ? ((eaz != 0 || eaz2 != 0) ? 6 : 4) : (eaz != 0) ? 5 : 4, 0)) == NULL) {
					freeb (asn);
					conn->lockit--;
					return -EIO;
				}
				qd_d[0] = 0;
				qd_d[1] = (eaz2 > 0) ? N1_FAC_Dienstwechsel2 : N1_FAC_Dienstwechsel1;
				qd_d[2] = service >> 8;
				qd_d[3] = service & 0xFF;
				if ((gotservice && eaz != 0) || eaz2 != 0) {
					qd_d[4] = (eaz != 0) ? eaz : '0';
					qd_d[5] = (eaz2 != 0) ? eaz2 : '0';
				} else if (eaz != 0)
					qd_d[4] = eaz;
				asn->b_wptr = asn->b_rptr + qd_len;
			}

			switch (conn->state) {
			case 4:
			case 7:
			case 8:
				if (!doforce) {
					data->b_rptr = oldpos;
					isdn3_repeat (conn, id, data);
					if (asn != NULL)
						freemsg (asn);
					conn->lockit--;
					return 0;
				}
			case 10:
				if (!donum)
					conn->minorstate |= MS_FORWARDING;
				((struct t_info *)conn->p_data)->flags |= FAC_PENDING;

				if ((err = phone_sendback (conn, MT_N1_FAC, asn)) == 0)
					asn = NULL;
				isdn3_setup_conn (conn, EST_LISTEN);
				setstate (conn, 8);
				break;
			default:
				printf("BadState4 ");
				err = -EINVAL;
				break;
			}
			if (asn != NULL)
				freemsg (asn);
		}
		break;
#endif
	case CMD_OFF:
		{
			mblk_t *mb = NULL;

			conn->minorstate &= ~MS_WANTCONN;
			if(cause != -1) {
				int len;
				uchar_t *dp;

				if ((mb = allocb (16, BPRI_LO)) == NULL)
					break;

				len = mb->b_wptr - mb->b_rptr;
				dp = qd_insert ((uchar_t *) mb->b_rptr, &len, 0, PT_N0_cause, 1, 0);
				if (dp != NULL) {
					mb->b_wptr = mb->b_rptr + len;
					*dp = cause | 0x80;
				}
			}
			/* set Data */
			if (conn->state == 6 && cause == -1) {
				setstate(conn,99);
				if(mb != NULL)
					freemsg(mb);
			} else {
				if(doforce && donodisc) {
					doforce = 0;
					/* Hmmm */
				}
				if (send_disc (conn, 1 + doforce, mb) != 0 && mb != NULL)
					freemsg (mb);
			}

			isdn3_setup_conn (conn, EST_DISCONNECT);
			err = 0;
		}
		break;
	default:
		printf("UnknownCmd ");
		err = -EINVAL;
		break;
	}
	if (data != NULL && err == 0)
		freemsg (data);

	checkterm (conn, NULL, 0);
	conn->lockit--;
	return err;
}

static void
killconn (isdn3_conn conn, char force)
{
	if (force) {
		untimer (N1_T308, conn);
		untimer (N1_T313, conn);
#ifdef HAS_SUSPEND
		untimer (N1_T318, conn);
#endif
		untimer (N1_T305, conn);
		untimer (N1_T3D1, conn);
		untimer (N1_T303, conn);
		untimer (N1_T304, conn);
		untimer (N1_T310, conn);
		untimer (N1_T3D2, conn);
#ifdef HAS_SUSPEND
		untimer (N1_T319, conn);
#endif
		untimer (N1_T3AA, conn);
		untimer (N1_TCONN, conn);
		untimer (N1_TALERT, conn);
		untimer (N1_TFOO, conn);
	}
	if(conn->state != 0) {
		if(conn->state == 99)
			setstate(conn, 0);
		else if(force)
			(void) phone_sendback (conn, MT_N1_REL, NULL);
		else
			(void) send_disc (conn, 1, NULL);
	}
}

static void report (isdn3_conn conn, mblk_t *mb)
{
	struct t_info *info = conn->p_data;
	if (info == NULL)
		return;
	if (info->nr[0] != 0) {
		m_putsx (mb, ARG_NUMBER);
		m_putsz (mb, info->nr);
	}
	if (info->eaz != 0) {
		m_putsx (mb, ARG_LNUMBER);
		*mb->b_wptr++=' ';
		*mb->b_wptr++='/';
		*mb->b_wptr++=info->eaz;
	}
	if (info->service != 0) {
		m_putsx (mb, ARG_SERVICE);
		m_putx (mb, info->service);
	}
}

struct _isdn3_prot prot_1TR6_1 =
{
		NULL, SAPI_PHONE_1TR6_1,
		NULL, &chstate, &report, &recv, NULL, &sendcmd, &killconn, NULL,
};
