#define __PHONE_R
#include "primitives.h"
#include "phone.h"
#include "dump.h"
#include "streamlib.h"
#include "phone_1TR6.h"
#include "phone_ETSI.h"
#include "q_data.h"
#include "isdn_23.h"
#include "isdn3_phone.h"
#include <sys/errno.h>
#include <sys/param.h>
#include "prot_1TR6_0.h"
#include "prot_1TR6_1.h"
#include "prot_ETS.h"
#include "capi.h"
#include "isdn_12.h"

static int
chstate (isdn3_talk talk, uchar_t ind, short add)
{
	printf("CAPI: chstate %d %d\n",ind,add);
										return -ENXIO;
#if 0
	int killit = 0;

	if(0)printf ("PHONE state for card %d says %s:%o\n", talk->card->nr, conv_ind(ind), add);
	switch (ind) {
	case DL_ESTABLISH_IND:
	case DL_ESTABLISH_CONF:{
			isdn3_conn conn, nconn;

			for (conn = talk->conn; conn != NULL; conn = nconn) {
				isdn3_prot prot = isdn3_findprot (talk->card->info, conn->subprotocol);

				nconn = conn->next;
				if (prot != NULL)
					(*prot->chstate) (conn, ind, add);
			}

			talk->state |= PHONE_UP;
			if(0)printf ("PHONE is UP\n");
		} break;
	case PH_DEACTIVATE_IND:
		/* break; */
	case PH_DEACTIVATE_CONF:
	case PH_DISCONNECT_IND:
		killit = 1;
		/* FALL THRU */
	case DL_RELEASE_IND:
	case DL_RELEASE_CONF: {
			isdn3_conn conn, nconn;

			for (conn = talk->conn; conn != NULL; conn = nconn) {
				isdn3_prot prot = isdn3_findprot (talk->card->info, conn->subprotocol);

				nconn = conn->next;
				if (prot != NULL)
					(*prot->chstate) (conn, ind, add);
				else
					isdn3_killconn (conn, 1);
			}

			talk->state &= ~PHONE_UP;
			if(0)printf ("PHONE i4s DOWN\n");
		}
		break;
	}
	return 0;
#endif
}

static int
recv (isdn3_talk talk, char isUI, mblk_t * data)
{
	printf("CAPI: recv %d\n",isUI);
								return -ENXIO;
#if 0
	isdn3_conn conn = NULL;
	uchar_t prot;
	long cref = 0;
	char cref_net = 0;
	uchar_t msgtype;
	uchar_t c;
	uchar_t *mdata;
	ushort_t mlen;

	if (0)
		printf ("Phone_Recv: ");
	if ((data = pullupm (data, 0)) == NULL)
		return 0;
	prot = *data->b_rptr++;
	if ((data = pullupm (data, 0)) == NULL)
		return 0;
	c = *data->b_rptr++;
	if ((data = pullupm (data, 0)) == NULL)
		return 0;
	if (c & 0xF0 || c > 4)
		return 0;
	if ((data = pullupm (data, 0)) == NULL)
		return 0;
	if (c != 0 && *data->b_rptr & 0x80) {
		cref_net = 1;
		*data->b_rptr &= ~0x80;
	}
	cref = 0;
	while (c--) {
		cref = (cref << 8) + (*data->b_rptr++ & 0xFF);
		if ((data = pullupm (data, 0)) == NULL)
			return 0;
	}
	if (!cref_net)
		cref = -(cref+1);
	else
		cref = cref+1;

	msgtype = *data->b_rptr++;
	if ((data = pullupm (data, 0)) == NULL) {
		mdata = NULL;
		mlen = 0;
	} else {
		mblk_t *mb = pullupm (data, -1);

		if (mb == NULL) {
			freemsg (data);
			printf ("No Pullup\n");
			isdn3_killconn (conn, 1);	/* Nothing to be done... */
			return 0;
		}
		data = mb;
		mdata = (uchar_t *) mb->b_rptr;
		mlen = (uchar_t *) mb->b_wptr - mdata;
	}
	conn = isdn3_findconn (talk, prot, cref);
	if (conn == NULL) {
		conn = isdn3_new_conn (talk);
		if (conn == NULL)
			goto out;
		conn->call_ref = cref;
		conn->subprotocol = prot;
		printf (" NewConn");
	} {
		isdn3_prot proto = isdn3_findprot (talk->card->info, conn->subprotocol);

		if (proto != NULL)
			(*proto->recv) (conn, msgtype, isUI, mdata, mlen);
		else
			isdn3_killconn (conn, 1);
	}
  out:
	if (data != NULL)
		freemsg (data);
	return 0;
#endif
}

static int
send (isdn3_conn conn, mblk_t * data)
{
	printf("CAPI: send\n");
	return -ENXIO;
#if 0
	isdn3_prot prot = isdn3_findprot (conn->card->info, conn->subprotocol);

	if (prot != NULL && prot->send != NULL)
		return (*prot->send) (conn, data);
	else {
		isdn3_killconn (conn, 1);
		return EINVAL;
	}
#endif
}

static int
sendcmd (isdn3_conn conn, ushort_t id, mblk_t * data)
{
	printf("CAPI: sendcmd %x\n",id);
	return -ENXIO;
#if 0
	isdn3_prot prot = isdn3_findprot (conn->card->info, conn->subprotocol);

	if (prot != NULL)
		return (*prot->sendcmd) (conn, id, data);
	else {
		printf("\n !*!*! ProtNull / %ld::%s !*!*!\n",conn->subprotocol,
				conn->card->info ? (char *)conn->card->info->b_rptr : "none");
		isdn3_killconn (conn, 1);
		return EINVAL;
	}
#endif
}

static void
report (isdn3_conn conn, mblk_t * data)
{
	printf("CAPI: report\n");
#if 0
	isdn3_prot prot = isdn3_findprot (conn->card->info, conn->subprotocol);

	if (prot != NULL && prot->report != NULL)
		(*prot->report) (conn, data);
#endif
}

static void
ckill (isdn3_talk talk, char force)
{
	printf("CAPI: ckill %d\n",force);
#if 0
	if (force) {
		if (talk->state & PHONE_UP) {
			talk->state &= ~PHONE_UP;
			if(0)printf("Phone is aDown\n");
			isdn3_chstate (talk, DL_RELEASE_REQ, 0, CH_CLOSEPROT);
		}
	}
#endif
}

static void
killconn (isdn3_conn conn, char force)
{
	printf("CAPI: killconn %d\n",force);
#if 0
	isdn3_talk talk = conn->talk;
	isdn3_prot prot = isdn3_findprot (conn->card->info, conn->subprotocol);

	if (prot != NULL)
		(*prot->killconn) (conn, force);

	if (talk != NULL && talk->conn == NULL && (talk->state & PHONE_UP)) {
		/* Last talker got closed. Shutdown for level 2. */
		talk->state &= ~PHONE_UP;
		if(0)printf ("PHONE i2s DOWN\n");
		(void) isdn3_chstate (talk, DL_RELEASE_REQ, 0, CH_CLOSEPROT);
	} else {
#if 0
		printf ("Killconn: Phone s1tays up");
		if (talk == NULL)
			printf (": Talk NULL");
		else if (talk->conn != NULL)
			printf (": talk->conn %x (ref %d, state %d)", talk->conn, talk->conn->call_ref, talk->conn->state);
		if (!(talk->state & PHONE_UP))
			if(0)printf (": PHONE i3s DOWN\n");
		printf ("\n");
#endif
	}
#endif
}

static void
hook (isdn3_conn conn)
{
	printf("CAPI: hook\n");
#if 0
	isdn3_prot prot = isdn3_findprot (conn->card->info, conn->subprotocol);

	if (prot != NULL && prot->hook != NULL)
		(*prot->hook) (conn);
#endif
}


static void
newcard (isdn3_card card)
{
	printf("CAPI: newcard\n");
	(void) isdn3_findtalk (card, &CAPI_hndl, card->info, 1);
	
	/* That's enough. */
}

static ulong_t
modeflags (long protocol)
{
	printf("CAPI: modeflags\n");
	return 1 << M_HDLC || 1 << CHM_INTELLIGENT;
}


static void
init (void)
{
	printf("CAPI: init\n");
}

struct _isdn3_hndl CAPI_hndl =
{
		NULL, /* SAPI */ 65,0,
		&init, &newcard, &modeflags, &chstate, &report, &recv, &send,
		&sendcmd, &ckill, &killconn, &hook,
};
