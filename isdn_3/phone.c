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
#include "sapi.h"


static isdn3_prot isdn3_findprot (mblk_t *info, uchar_t protocol);

int
phone_sendback (isdn3_conn conn, uchar_t msgtype, mblk_t * data)
{
	mblk_t *mb;
	long cref = conn->call_ref;
	uchar_t creflen = 0;
	int err;

	if (cref > 0)
		cref = cref-1;
	else
		cref = -1-cref;
	cref <<= 1;
	do {
		cref >>= 8;
		creflen++;
	} while (cref != 0);
	if ((mb = allocb (creflen + 3, BPRI_MED)) == NULL)
		return -ENOMEM;
	if (data != NULL)
		linkb (mb, data);

	if (conn->subprotocol == SAPI_PHONE_DSS1) {
		if(conn->card == NULL || conn->card->bchans <= 2) {
			if (creflen < 1)
				creflen = 1;
		} else {
			if (creflen < 2)
				creflen = 2;
		}
	}

	*mb->b_wptr++ = conn->subprotocol;
	*mb->b_wptr++ = creflen;

	if (creflen > 0) {
		int clen = creflen;

		cref = conn->call_ref;
		if (cref > 0)
			cref = cref-1;
		else
			cref = -1-cref;
		while (clen--) {
			mb->b_wptr[clen] = cref & 0xFF;
			cref >>= 8;
		}
		if (conn->call_ref < 0)
			*mb->b_wptr |= 0x80;
	}
	mb->b_wptr += creflen;
	*mb->b_wptr++ = msgtype;
	if (!(conn->talk->state & PHONE_UP)) {
		if(0)printf("UpPhone\n");
		err = isdn3_chstate (conn->talk, DL_ESTABLISH_REQ, 0, CH_OPENPROT);
		if (err != 0) {
			freeb (mb);
			return err;
		}
	}
	if ((err = isdn3_send (conn->talk, AS_DATA, mb)) != 0)
		freeb (mb);
	return err;
}



static int
chstate (isdn3_talk talk, uchar_t ind, short add)
{
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
		goto release_common;
	case DL_RELEASE_CONF:
		if(!(talk->state & PHONE_UP))
			break;
		/* ELSE FALL THRU */
	case DL_RELEASE_IND:
	  release_common:
		{
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
}

static int
recv (isdn3_talk talk, char isUI, mblk_t * data)
{
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
}

static int
send (isdn3_conn conn, mblk_t * data)
{
	isdn3_prot prot = isdn3_findprot (conn->card->info, conn->subprotocol);

	if (prot != NULL && prot->send != NULL)
		return (*prot->send) (conn, data);
	else {
		isdn3_killconn (conn, 1);
		return -EINVAL;
	}
}

static int
sendcmd (isdn3_conn conn, ushort_t id, mblk_t * data)
{
	isdn3_prot prot = isdn3_findprot (conn->card->info, conn->subprotocol);

	if (prot != NULL)
		return (*prot->sendcmd) (conn, id, data);
	else {
		if(log_34 & 2)printf("\n !*!*! ProtNull / %ld::%s !*!*!\n",conn->subprotocol,
				conn->card->info ? (char *)conn->card->info->b_rptr : "none");
		isdn3_killconn (conn, 1);
		return -EINVAL;
	}
}

static void
report (isdn3_conn conn, mblk_t * data)
{
	isdn3_prot prot = isdn3_findprot (conn->card->info, conn->subprotocol);

	if (prot != NULL && prot->report != NULL)
		(*prot->report) (conn, data);
}

static void
ckill (isdn3_talk talk, char force)
{
	long flags = isdn3_flags(talk->card->info, SAPI_PHONE,-1);
	if (force) {
		if (talk->state & PHONE_UP) {
			talk->state &= ~PHONE_UP;
			if(0)printf("Phone is aDown\n");
			isdn3_chstate (talk, DL_RELEASE_REQ, 0, CH_CLOSEPROT);
		}
	}
	else if(!(flags & FL_L2KEEP) && 
			(talk != NULL && talk->conn == NULL && (talk->state & PHONE_UP))) {
		/* Last talker got closed. Shutdown for level 2. */
		talk->state &= ~PHONE_UP;
		if(0)printf("Phone is bDown\n");
		(void) isdn3_chstate (talk, DL_RELEASE_REQ, 0, CH_CLOSEPROT);
	}
}

static void
killconn (isdn3_conn conn, char force)
{
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
}

static void
hook (isdn3_conn conn)
{
	isdn3_prot prot = isdn3_findprot (conn->card->info, conn->subprotocol);

	if (prot != NULL && prot->hook != NULL)
		(*prot->hook) (conn);
}


static void
newcard (isdn3_card card)
{
	(void) isdn3_findtalk (card, &PHONE_hndl, card->info, 1);

	/* That's enough. */
}

static struct _isdn3_prot *isdn_prot;

static void
init (void)
{
	isdn_prot = NULL;
#if 0
	isdn3_attach_prot (&prot_1TR6_0);
#endif
#ifdef _german_
	isdn3_attach_prot (&prot_1TR6_1);
#endif
#ifdef _euro_
	isdn3_attach_prot (&prot_ETSI);
#endif
}

int
isdn3_attach_prot (isdn3_prot prot)
{
	isdn3_prot nprot;
	int ms = splstr ();

	for (nprot = isdn_prot; nprot != NULL; nprot = nprot->next) {
		if (nprot->protocol == prot->protocol) {
			splx (ms);
			return -EEXIST;
		}
	}

	prot->next = isdn_prot;
	isdn_prot = prot;

	if (prot->init != NULL)
		(*prot->init) ();
	splx (ms);
	return 0;
}

static isdn3_prot
isdn3_findprot (mblk_t *info, uchar_t protocol)
{
	isdn3_prot prot;
	int skip = 1;

	if(info != NULL) {
		streamchar *sta = info->b_rptr;
		ushort_t idx;

		while(m_getsx(info,&idx) == 0) {
			long sap;
			switch(idx) {
			case ARG_PROTOCOL:
				if (m_geti(info,&sap) == 0) {
					skip = (sap != SAPI_PHONE);
				}
				break;
			case ARG_SUBPROT:
				if (m_geti(info,&sap) == 0 && !skip && sap == protocol) {
					for (prot = isdn_prot; prot != NULL; prot = prot->next) {
						if (prot->protocol == protocol) {
							info->b_rptr = sta;
							return prot;
						}
					}
					info->b_rptr = sta;
					return NULL;
				}
			}
		}
		info->b_rptr = sta;
	} else {
		for (prot = isdn_prot; prot != NULL; prot = prot->next) {
			if (prot->protocol == protocol)
				return prot;
		}
	}
	return NULL;
}

struct _isdn3_hndl PHONE_hndl =
{
		NULL, SAPI_PHONE,0,
		&init, &newcard, &chstate, &report, &recv, &send,
		&sendcmd, &ckill, &killconn, &hook, NULL,
};
