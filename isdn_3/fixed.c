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

static int
fixed_chstate (isdn3_talk talk, uchar_t ind, short add)
{
	printf ("FIXED state for card %d says %s:%o ", talk->card->nr, conv_ind(ind), add);

	switch (ind) {
	case PH_ACTIVATE_IND:
	case PH_ACTIVATE_CONF:
	case PH_ACTIVATE_NOTE:
	case DL_ESTABLISH_IND:
	case DL_ESTABLISH_CONF:
		{
			isdn3_conn conn, nconn;

			if((talk->state & ST_up))
				break;

			for (conn = talk->conn; conn != NULL; conn = nconn) {
				int err;
				mblk_t *mb = allocb (16, BPRI_MED);

				if(conn->state == 3)
					conn->state = 1;
				nconn = conn->next;

				if (mb != NULL) {
					m_putid (mb, PROTO_WANT_CONNECTED);
					m_putsx (mb, ARG_FORCE);
					if ((err = isdn3_at_send (conn, mb, 0)) != 0) 
						freemsg (mb);
				}
			}

			talk->state |= ST_up;
		} break;
	case PH_DEACTIVATE_IND:
	case PH_DEACTIVATE_CONF:
	case PH_DISCONNECT_IND:
	case DL_RELEASE_IND:
	case DL_RELEASE_CONF:
		{
			isdn3_conn conn, nconn;

			if(!(talk->state & ST_up))
				break;

			for (conn = talk->conn; conn != NULL; conn = nconn) {
				nconn = conn->next;
				conn->state = 2;
				isdn3_killconn (conn, 0);
			}

			talk->state &= ~ST_up;
		}
		break;
	}
	return 0;
}

static int
send_fixed_disc (isdn3_conn conn, char release, mblk_t * data)
{
	int err = 0;

	switch (conn->state) {
	case 0:
		break;
	case 1:
	case 3:
		/* B-Kanal trennen */
		conn->state = 0;
		isdn3_setup_conn (conn, EST_DISCONNECT);
		break;
	}
	return err;
}
static int
fixed_sendcmd (isdn3_conn conn, ushort_t id, mblk_t * data)
{
	streamchar *oldpos = data->b_rptr;
	int err = 0;
	ushort_t typ;

	switch (id) {
	case CMD_DIAL:
	case CMD_ANSWER:
		{
			if (data == NULL) {
				printf("DataInvalA ");
				return -EINVAL;
			}
			while ((err = m_getsx (data, &typ)) == 0) {
				switch (typ) {
				default:;
				}
			}
		  /* end_arg_dial: */
			conn->minorstate |= MS_OUTGOING | MS_WANTCONN;
			conn->lockit++;
			isdn3_setup_conn (conn, EST_NO_CHANGE);
			conn->lockit--;

			if ((conn->minorstate & (MS_PROTO|MS_INITPROTO)) != (MS_PROTO|MS_INITPROTO)) {
				data->b_rptr = oldpos;
				isdn3_repeat (conn, id, data);
				return 0;
			}
			switch (conn->state) {
			case 0:
			case 2:
				if (conn->minor == 0)
					return -ENOENT;
				if (conn->mode == 0)
					err = -ENOEXEC;
				conn->state++;
				{
					int err = 0;

					mblk_t *mb = allocb (128, BPRI_MED);

					if (mb == NULL) {
						conn->state = 0;
						return -ENOMEM;
					}
					m_putid (mb, IND_CONN);
					conn_info (conn, mb);

					if ((err = isdn3_at_send (conn, mb, 0)) != 0) {
						freemsg (mb);
						conn->state = 0;
						return err;
					}
					isdn3_setup_conn (conn, EST_CONNECT);
					return 0;
				}
				break;
			default:
				return -EBUSY;
			}
		}
		break;
	case CMD_OFF:
		{
			long error = -1;
			char forceit = 0;
			mblk_t *mb = NULL;

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
			if (send_fixed_disc (conn, 1 + forceit, mb) != 0 && mb != NULL)
				freemsg (mb);
		}
		break;
	default:
		err = -EINVAL;
		break;
	}
	if (data != NULL && err == 0)
		freemsg (data);

	if(conn->state == 0)
		isdn3_killconn(conn,1);
	return err;
}

static void
fixed_killconn (isdn3_conn conn, char force)
{
	int err;
	mblk_t *mb = allocb (16, BPRI_MED);

	if (mb == NULL) {
		conn->state = 0;
		return;
	}
	m_putid (mb, IND_DISC);
	m_putsx (mb, ARG_FORCE);
	if ((err = isdn3_at_send (conn, mb, 0)) != 0) {
		freemsg (mb);
		conn->state = 0;
	}
	return;
}


static void
fixed_newcard (isdn3_card card)
{
	(void) isdn3_findtalk (card, &FIXED_hndl, card->info, 1);
}

struct _isdn3_hndl FIXED_hndl =
{
		NULL, SAPI_FIXED,1,
		NULL, &fixed_newcard, NULL, &fixed_chstate, NULL, NULL,
		NULL, &fixed_sendcmd, NULL, &fixed_killconn, NULL, NULL,
};

