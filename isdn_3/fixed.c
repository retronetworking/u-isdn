#include "primitives.h"
#include "fixed.h"
#include "streamlib.h"
#include "isdn_23.h"
#include "isdn_34.h"
#include "isdn_proto.h"
#include "lap.h"
#include "dump.h"
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/param.h>
#include "sapi.h"

#define ST_up 01
#define ST_sent 02

#define STATE_WAIT 1
#define STATE_RUN 2
#define STATE_DOWN 3

static int
chstate (isdn3_talk talk, uchar_t ind, short add)
{
	if(log_34 & 2)
		printf ("FIXED state for card %d says %s:%o ", talk->card->nr, conv_ind(ind), add);

	switch (ind) {
	case PH_DEACTIVATE_IND:
	case PH_DEACTIVATE_CONF:
	case PH_DISCONNECT_IND:
	case DL_RELEASE_IND:
	case DL_RELEASE_CONF:
		{
			isdn3_conn conn, nconn;

			for (conn = talk->conn; conn != NULL; conn = nconn) {
				nconn = conn->next;
				if(conn->state == STATE_RUN) 
					conn->state = STATE_DOWN;
				conn->minorstate &= ~MS_WANTCONN;
				conn->id = 0;
				isdn3_killconn (conn, 0);
			}

			if((ind != PH_DEACTIVATE_IND) || !(talk->state & ST_sent)) 
				talk->state &= ~(ST_up|ST_sent);
		}
		break;
	case PH_ACTIVATE_NOTE:
	case PH_ACTIVATE_IND:
	case PH_ACTIVATE_CONF:
	case DL_ESTABLISH_IND:
	case DL_ESTABLISH_CONF:
		{
			isdn3_conn conn, nconn;

			if(talk->state & ST_up)
				break;

			for (conn = talk->conn; conn != NULL; conn = nconn) {
				int err;
				mblk_t *mb;

				nconn = conn->next;

				if(conn->state == STATE_DOWN)
					conn->state = STATE_RUN;
				else if(conn->state == STATE_WAIT) {
					conn->minorstate |= MS_WANTCONN;
					isdn3_setup_conn (conn, EST_NO_CHANGE);
					continue;
				}

				mb = allocb (16, BPRI_MED);
				if (mb != NULL) {
					m_putid (mb, PROTO_WANT_CONNECTED);
					m_putsx (mb, ARG_FORCE);
					m_putsx (mb, ARG_MINOR);
					m_puti (mb,conn->minor);
					if ((err = isdn3_at_send (conn, mb, 0)) != 0) 
						freemsg (mb);
				}
			}
			if(ind != PH_ACTIVATE_NOTE)
				talk->state = (talk->state | ST_up) & ~ST_sent;
		}
		break;
	}
	return 0;
}

static int
send_disc (isdn3_conn conn, char release, mblk_t * data)
{
	int err = 0;

	switch (conn->state) {
	case 0:
		break;
	case STATE_RUN:
	case STATE_DOWN:
		/* B-Kanal trennen */
		conn->state = 0;
		isdn3_setup_conn (conn, EST_DISCONNECT);
		break;
	}
	return err;
}
static int
sendcmd (isdn3_conn conn, ushort_t id, mblk_t * data)
{
	streamchar *oldpos = data->b_rptr;
	int err = 0;
	ushort_t typ;

	conn->lockit++;
	switch (id) {
	case CMD_DIAL:
		{
			if (data == NULL) {
				printf("DataInvalA ");
				err = -EINVAL;
				break;
			}
#if 0
			while (m_getsx (data, &typ) == 0) {
				switch (typ) {
				default:;
				}
			}
#endif
			conn->minorstate |= MS_OUTGOING;
			if(conn->talk->state & ST_up)
				conn->minorstate |= MS_WANTCONN;
			isdn3_setup_conn (conn, EST_NO_CHANGE);

			switch (conn->state) {
			case 0:
			case STATE_WAIT:
				if (conn->minor == 0) {
					err = -ENOENT;
					break;
				}
				if (!(conn->talk->state & ST_up) || !(conn->minorstate & MS_INITPROTO)) {
					if((conn->talk->state & (ST_sent|ST_up)) == 0) {
						conn->talk->state = ST_sent;
						isdn3_chstate(conn->talk,PH_ACTIVATE_REQ,0,CH_OPENPROT);
					}
					isdn3_setup_conn (conn, EST_SETUP);
					conn->state = STATE_WAIT;
					data->b_rptr = oldpos;
					isdn3_repeat (conn, id, data);
					data = NULL;
					break;
				}
				conn->state = STATE_RUN;
				{
					mblk_t *mb = allocb (128, BPRI_MED);

					if (mb == NULL) {
						conn->state = 0;
						conn->lockit--;
						return -ENOMEM;
					}
					m_putid (mb, IND_CONN);
					conn_info (conn, mb);

					if ((err = isdn3_at_send (conn, mb, 0)) != 0) {
						freemsg (mb);
						conn->state = 0;
						break;
					}
					isdn3_setup_conn (conn, EST_CONNECT);
					err = 0;
					break;
				}
				break;
			default:
				err = -EBUSY;
				break;
			}
		}
		break;
	case CMD_OFF:
		{
			long error = -1;
			char forceit = 0;

			if (data != NULL)
				while (m_getsx (data, &typ) == 0)
					switch (typ) {
					case ARG_FORCE:
						forceit = 1;
						break;
					case ARG_ERRNO:
						if (m_geti (data, &error) != 0)
							break;
						break;
					}
			conn->minorstate &= ~MS_WANTCONN;

			/* set Data */
			send_disc (conn, 1 + forceit, NULL);
		}
		break;
	default:
		err = -EINVAL;
		break;
	}
	if ((data != NULL) && (err == 0))
		freemsg (data);

	conn->lockit--;
	if(conn->state == 0)
		isdn3_killconn(conn,1);
	return err;
}

static void
killconn (isdn3_conn conn, char force)
{
	int err;
	mblk_t *mb = allocb (16, BPRI_MED);

	if (mb == NULL) {
		conn->state = 0;
		return;
	}
	m_putid (mb, IND_DISCONNECT);
	m_putsx (mb, ARG_FORCE);
	if ((err = isdn3_at_send (conn, mb, 0)) != 0) {
		freemsg (mb);
		conn->state = 0;
	}
	return;
}


static void
newcard (isdn3_card card)
{
	(void) isdn3_findtalk (card, &FIXED_hndl, card->info, 1);
}

struct _isdn3_hndl FIXED_hndl =
{
		NULL, SAPI_FIXED,1,
		NULL, &newcard, &chstate, NULL, NULL,
		NULL, &sendcmd, NULL, &killconn, NULL, NULL,
};

