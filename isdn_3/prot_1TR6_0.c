#include "primitives.h"
#include "phone.h"
#include "streamlib.h"
#include "phone_1TR6.h"
#include "q_data.h"
#include "isdn_23.h"
#include "isdn3_phone.h"
#include <sys/errno.h>
#include <sys/param.h>
#include "prot_1TR6_0.h"
#include "prot_1TR6_common.h"

#define RUN_N0_T308 01
#define RUN_N0_T3D1 02
#define RUN_N0_SENDCLOSE 04
#define VAL_N0_T308 ( 4 *HZ)
#define VAL_N0_T3D1 ( 10 *HZ)
static void N0_T308 (isdn3_conn conn);
static void N0_T3D1 (isdn3_conn conn);

static void
phone_timerup (isdn3_conn conn)
{
	rtimer (N0_T308, conn);
	rtimer (N0_T3D1, conn);
}


static void
pr_setstate (isdn3_conn conn, uchar_t state)
{
	printf ("Conn PostN0 %ld: State %d --> %d\n", conn->call_ref, conn->state, state);
	conn->state = state;
}

static void
checkterm (isdn3_conn conn, uchar_t * data, int len)
{
	if (conn->state == 0) {
		isdn3_killconn (conn, 1);
	}
}


static void
N0_T308 (isdn3_conn conn)
{
	printf ("Timer N0_T308\n");
	conn->timerflags &= ~RUN_N0_T308;
	switch (conn->state) {
	case 10:
		if (conn->timerflags & RUN_N0_SENDCLOSE) {
			/* Err */
			pr_setstate (conn, 0);
		} else {
			conn->timerflags |= RUN_N0_SENDCLOSE;
			timer (N0_T308, conn);
		}
		break;
	}
	checkterm (conn, NULL, 0);
}

static void
N0_T3D1 (isdn3_conn conn)
{
	printf ("Timer N0_T3D1\n");
	conn->timerflags &= ~RUN_N0_T3D1;
	switch (conn->state) {
	case 20:
	case 21:
		/* Err */
		phone_sendback (conn, MT_N0_CLOSE, NULL);
		pr_setstate (conn, 0);
		break;
	}
	checkterm (conn, NULL, 0);
}

static int
recv (isdn3_conn conn, uchar_t msgtype, char isUI, uchar_t * data, ushort_t len)
{
#if 0
	QD_INIT (data, len) {
		pr_setstate (conn, 0);
		return ENOMEM;
	}
#endif
	switch (msgtype) {
	case MT_N0_REG_IND:
		break;
	case MT_N0_CANC_IND:
		break;
	default:
		switch (conn->state) {
		case 0:
			switch (msgtype) {
			case MT_N0_CLOSE:
				phone_sendback (conn, MT_N0_CLO_ACK, NULL);
			case MT_N0_CLO_ACK:
			case MT_N0_STA_ACK:
			case MT_N0_STA_REJ:
			case MT_N0_INF_ACK:
			case MT_N0_INF_REJ:
				break;
			default:
				phone_sendback (conn, MT_N0_CLOSE, NULL);
				timer (N0_T308, conn);
				pr_setstate (conn, 10);
				break;
			}
			break;
		case 1:
			switch (msgtype) {
			case MT_N0_CLOSE:
				phone_sendback (conn, MT_N0_CLO_ACK, NULL);
				pr_setstate (conn, 0);
				break;
			case MT_N0_CLO_ACK:
				phone_sendback (conn, MT_N0_CLOSE, NULL);
				timer (N0_T308, conn);
				pr_setstate (conn, 10);
				break;
			default:
				break;
			}
			break;
		case 10:
			switch (msgtype) {
			case MT_N0_CLOSE:
			case MT_N0_CLO_ACK:
				untimer (N0_T308, conn);
				pr_setstate (conn, 0);
				break;
			case MT_N0_STA_ACK:
			case MT_N0_STA_REJ:
			case MT_N0_INF_ACK:
			case MT_N0_INF_REJ:
				break;
			default:
				break;
			}
			break;
		case 20:
			switch (msgtype) {
			case MT_N0_CLOSE:
				phone_sendback (conn, MT_N0_CLO_ACK, NULL);
				pr_setstate (conn, 0);
				break;
			case MT_N0_STA_ACK:
			case MT_N0_STA_REJ:
				pr_setstate (conn, 0);
				break;
			default:
				break;
			}
			break;
		case 21:
			switch (msgtype) {
			case MT_N0_CLOSE:
				phone_sendback (conn, MT_N0_CLO_ACK, NULL);
				pr_setstate (conn, 0);
				break;
			case MT_N0_INF_ACK:
			case MT_N0_INF_REJ:
				pr_setstate (conn, 0);
				break;
			default:
				break;
			}
			break;
		default:
			pr_setstate (conn, 0);
			break;
		}
	}

	return 0;
}

static int
chstate (isdn3_conn conn, uchar_t ind, short add)
{
	switch (ind) {
		case DL_ESTABLISH_IND:
		case DL_ESTABLISH_CONF:
		if (conn->talk->state & PHONE_UP) {		/* Reestablishment */
			switch (conn->state) {
			}
			checkterm (conn, NULL, 0);
		} else {
			phone_timerup (conn);
		}
		break;
	case PH_DEACTIVATE_IND:
		break;
	case DL_RELEASE_IND:
	case DL_RELEASE_CONF:
	case PH_DEACTIVATE_CONF:
	case PH_DISCONNECT_IND:
		pr_setstate (conn, 0);
		checkterm (conn, NULL, 0);
		break;
	}
	return 0;
}

static int
sendcmd (isdn3_conn conn, ushort_t id, mblk_t * data)
{
	pr_setstate (conn, 0);
	return 0;
}

static void
killconn (isdn3_conn conn, char force)
{
	untimer (N0_T308, conn);
	untimer (N0_T3D1, conn);
}

struct _isdn3_prot prot_1TR6_0 =
{
		NULL, PD_N0,
		NULL, &chstate, NULL, &recv, NULL, &sendcmd, &killconn, NULL,
};
