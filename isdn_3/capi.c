#define __PHONE_R
#include "primitives.h"
#include "phone.h"
#include "dump.h"
#include "streamlib.h"
#include "phone_1TR6.h"
#include "q_data.h"
#include "isdn_23.h"
#include "isdn3_phone.h"
#include <sys/errno.h>
#include <sys/param.h>
#include "prot_1TR6_0.h"
#include "prot_1TR6_1.h"
#include "prot_ETS.h"
#include "capi.h"
#include "../cards/capi/capi.h"
#include "isdn_12.h"
#include "asm/byteorder.h"
#include "sapi.h"


#if NICONN <= 14
#error "Need NICONN > 14"
#endif
#if NITALK <= 1
#error "Need NITALK > 1"
#endif
#define message_id talki[0]
#define tstate talki[1]

#define ncci0 conni[0]
#define waitflags conni[1]
#define msgid0 conni[2]
#define WF_CONNECTACTIVE_IND    3
#define WF_CONNECTB3ACTIVE_IND  4
#define WF_DISCONNECTB3_IND     5
#define WF_DISCONNECT_IND       6
#define WF_SELECTB2_CONF        7
#define WF_SELECTB3_CONF        8
#define WF_LISTENB3_CONF        9
#define WF_CONNECTB3_IND       10
#define WF_CONNECTB3_CONF      11
#define WF_DISCONNECTB3_CONF   12
#define WF_DISCONNECT_CONF     13
#define WF_CONNECT_CONF        14

/* Card states */
#define STATE_BOOTING 1
#define STATE_OPENING 2
#define STATE_REGISTER 3

#define STATE_CONF_FIRST 10
#define STATE_CONF_LAST 99
#define STATE_RUNNING 100

#define RUN_CAPI_TALERT 01

#define VAL_CAPI_TALERT ( 2 *HZ)    /* timer for delaying an ALERT response */

static void CAPI_TALERT (isdn3_conn conn);

/* Connection states:
  0: unknown

  1: outgoing, wait for CONNECT_CONF 
  2: outgoing, wait for CONNECTACTIVE_IND SELECTB2_CONF SELECTB3_CONF
  3: outgoing, wait for CONNECTB3_CONF CONNECTB3ACTIVE_IND

  7: incoming, wait for master program
  8: incoming, wait for SELECTB2_CONF SELECTB3_CONF
  9: incoming, wait for LISTENB3_CONF CONNECTACTIVE_IND CONNECTB3_CONF CONNECTB3ACTIVE_IND

 15: CONNECTED

 20: wait for DISCONNECTB3_CONF DISCONNECTB3_IND
 21: wait for DISCONNECT_CONF DISCONNECT_IND
 22: wait for DISCONNECTB3_CONF DISCONNECTB3_IND DISCONNECT_CONF DISCONNECT_IND
*/

struct capi_info {
	unsigned short service;
	unsigned char flags;
	unsigned char eaz;
	unsigned char nr[MAXNR];
};


extern void log_printmsg (void *log, const char *text, mblk_t * mp, const char*);


static void
capi_timerup (isdn3_conn conn)
{
    rtimer (CAPI_TALERT, conn);
}

#define setstate(a,b) Xsetstate(__LINE__,(a),(b))
static void
Xsetstate(unsigned int deb_line, isdn3_conn conn, uchar_t state)
{
printf ("Conn CAPI:%d %08lx: State %d --> %d\n", deb_line, conn->call_ref, conn->state, state);

	if(conn->state == state)
		return;
	switch(conn->state) {
	case 13:
		untimer(CAPI_TALERT, conn);
		break;
	}
	conn->state = state;
	switch(state) {
	case 13:
		timer(CAPI_TALERT,conn);
		break;
	}
}

static ushort_t newmsgid(isdn3_talk talk)
{
	talk->message_id = ((talk->message_id + 1) & 0x3FFF) | 0x4000;

	return talk->message_id;
}

static int
capi_send(isdn3_talk talk, ushort_t appl, ushort_t msgtype, mblk_t *data, ushort_t msgid)
{
	int err;
	mblk_t *mb;
	struct CAPI_every_header *capi;
	/* build a message */

	mb = allocb(sizeof(*capi), BPRI_LO);
	if(mb == NULL)
		return -ENOMEM;
	capi = ((struct CAPI_every_header *)mb->b_wptr)++;
	bzero(capi,sizeof(*capi));
	capi->len = sizeof(*capi) + (data ? msgdsize(data) : 0);
	capi->appl = 0;
	capi->PRIM_type = msgtype;
	capi->messid = msgid;

	if(data != NULL)
		linkb(mb,data);
	if((err = isdn3_send(talk,AS_DATA,mb)) < 0)
		freeb(mb);
	return err;
}

static int
capi_sendctl(isdn3_talk talk, ushort_t appl, ushort_t code, mblk_t *data, ushort_t msgid)
{
	int err;
	mblk_t *mb;
	/* build a message */
	struct CAPI_control_req *capi;

	mb = allocb(sizeof(*capi), BPRI_LO);
	if(mb == NULL)
		return -ENOMEM;
	capi = ((struct CAPI_control_req *)mb->b_wptr)++;
	bzero(capi,sizeof(*capi));
	capi->type = code;
	capi->datalen = msgdsize(data);

	linkb(mb,data);
	if((err = capi_send(talk,appl,CAPI_CONTROL_REQ,mb,msgid)) < 0)
		freeb(mb);
	return err;
}
/* open:
 * 0001 ID LLHH capiLen LLHH appl LLHH type LLHH messid  LLHH control
 * LLHH type CCCC protocol ...
 */


static void
CAPI_TALERT(isdn3_conn conn)
{
	printf("CAPI_TALERT %08lx\n",conn->call_ref);
	if(conn->state != 6)
		return;
	/* Hmmm. Do we really have to do this?  I think not... */
	/* So let's try without. */
	conn->timerflags &= ~RUN_CAPI_TALERT;
}

static int
send_setup(isdn3_conn conn)
{
	int err;
	struct CAPI_selectb2_req *c2;
	struct CAPI_selectb3_req *c3;
	struct dlpd *dl;
	mblk_t *m2,*m3;

	m2 = allocb(sizeof(*c2)+sizeof(*dl),BPRI_MED);
	m3 = allocb(sizeof(*c3),BPRI_MED);
	if(m2 == NULL || m3 == NULL) {
		if(m2 != NULL)
			freemsg(m2);
		if(m3 != NULL)
			freemsg(m3);
		return -ENOMEM;
	}
	c2 = ((typeof(c2))m2->b_wptr)++;
	dl = ((typeof(dl))m2->b_wptr)++;
	c3 = ((typeof(c3))m3->b_wptr)++;

	bzero(c2,sizeof(*c2));
	bzero(dl,sizeof(*dl));
	bzero(c3,sizeof(*c3));

	c2->plci = conn->call_ref & 0xFFFF;
	c2->B2_proto = 0x02; /* transparent HDLC */
	c2->dlpdlen = sizeof(*dl);
	dl->data_length = 4096;
	c3->plci = conn->call_ref & 0xFFFF;
	c3->B3_proto = 0x04; /* transparent */
	err = capi_send(conn->talk,conn->call_ref >> 16, CAPI_SELECTB2_REQ, m2, conn->conni[WF_SELECTB2_CONF] = newmsgid(conn->talk));
	if(err < 0) {
		freemsg(m2);
		freemsg(m3);
	} else {
		err = capi_send(conn->talk,conn->call_ref >> 16, CAPI_SELECTB3_REQ, m3, conn->conni[WF_SELECTB3_CONF] = newmsgid(conn->talk));
		if(err < 0)
			freemsg(m3);
	}
	return err;
}

static int
chstate (isdn3_talk talk, uchar_t ind, short add)
{
	printf("CAPI: chstate %d %d, in state %ld\n",ind,add,talk->tstate);
	switch (ind) {
	case PH_ACTIVATE_NOTE:
	case DL_ESTABLISH_IND:
	case DL_ESTABLISH_CONF:
		{
			{
				isdn3_conn conn, nconn;
				for(conn = talk->conn; conn != NULL; conn = nconn) {
					nconn = conn->next;
					capi_timerup(conn);
				}
			}
			if(talk->tstate != STATE_BOOTING) {
				/* TODO: reset / restart / XXX the card? */
				break;
			}
			{
				struct apiopen *capi;
				int err;
				mblk_t *data;
#define DEFPROFILE "u_dss1_pmp"
				char profile[32] = DEFPROFILE;
				{	/* Find correct driver name */
					int err; char skip = 0;
					mblk_t *info = talk->card->info;
					if(info != NULL) {
						streamchar *sta = info->b_rptr;
						ushort_t idx;

						while(m_getid(info,&idx) == 0) {
							long sap;
							switch(idx) {
							case ARG_PROTOCOL:
								if (m_geti(info,&sap) == 0) {
										skip = (sap != SAPI_CAPI);
								}
								break;
							case ARG_SUBPROT:
								if (m_geti(info,&sap) == 0 && !skip) {
									switch(sap) {
									case SAPI_CAPI_BINTEC:
										skip=0;
										break;
									default:
										/* Wrong card. TODO: Do something! */
										info->b_rptr = sta;
										return -ENXIO;
									}
								}
								break;
							case ARG_STACK:
								if(skip)
									break;
								if((err = m_getstr(info,profile,sizeof(profile)-1)) < 0)
									strcpy(profile,DEFPROFILE);
								break;
							}
						}
						info->b_rptr = sta;
					}
				}

				data = allocb(sizeof(*capi), BPRI_MED);
				if(data == NULL) /* XXX error processing */
					return -ENOMEM;
				capi = ((struct apiopen *)data->b_wptr)++;
				bzero(capi,sizeof(*capi));
				strcpy(capi->protocol,"u_dss1_pmp");
				capi->teid = ~0;
				capi->t3id = ~0;
				capi->contrl = 0;
				capi->time = time(NULL);
				if((err = capi_sendctl(talk,0,CONTROL_API_OPEN,data,newmsgid(talk))) < 0) {
					freemsg(data);
					break;
				}
				talk->tstate = STATE_OPENING;
			}
		}
	}
	return 0;

#if 0 /* to end of function */
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

			talk->tstate |= PHONE_UP;
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

			talk->tstate &= ~PHONE_UP;
			if(0)printf ("PHONE i4s DOWN\n");
		}
		break;
	}
	return 0;
#endif
}

static inline isdn3_conn
capi_findconn(isdn3_talk talk, ushort_t appl, ushort_t plci)
{
	isdn3_conn conn;
	ulong_t cref = (appl << 16) | plci;

	for(conn = talk->conn; conn != NULL; conn = conn->next) {
		if(conn->call_ref == cref) {
			printf(" (conn %08lx) ",conn->call_ref);
			return conn;
		}
	}
	return NULL;
}

static inline isdn3_conn
capi_findconn3(isdn3_talk talk, ushort_t appl, ushort_t ncci)
{
	isdn3_conn conn;

	for(conn = talk->conn; conn != NULL; conn = conn->next) {
		if((conn->call_ref >> 16) == appl && conn->ncci0 == ncci) {
			printf(" (conn %08lx) ",conn->call_ref);
			return conn;
		}
	}
	return NULL;
}

static inline isdn3_conn
capi_findconnm(isdn3_talk talk, ushort_t appl, ushort_t msgid)
{
	isdn3_conn conn;

	for(conn = talk->conn; conn != NULL; conn = conn->next) {
		if((conn->call_ref >> 16) == appl && conn->msgid0 == msgid) {
			printf(" (conn %08lx) ",conn->call_ref);
			return conn;
		}
	}
	return NULL;
}

static int
report_incoming (isdn3_conn conn)
{
    int err = 0;

    mblk_t *mb = allocb (256, BPRI_MED);

    if (mb == NULL) {
        setstate (conn, 0);
        return -ENOMEM;
    }
    m_putid (mb, IND_CONN);
    conn_info (conn, mb);

    if ((err = isdn3_at_send (conn, mb, 0)) != 0) {
        freemsg (mb);
        setstate (conn, 0);
        return err;
    }
    return err;
}

static void
report_terminate (isdn3_conn conn)
{
    int err = 0;

    mblk_t *mb = allocb (256, BPRI_MED);

    if (conn->minorstate & MS_TERM_SENT) {
        m_putid (mb, IND_INFO);
        m_putid (mb, ID_N1_REL);
    } else {
        conn->minorstate |= MS_TERM_SENT;
        m_putid (mb, IND_DISC);
    }
    conn_info (conn, mb);
    if ((err = isdn3_at_send (conn, mb, 0)) != 0) {
        freemsg (mb);
        setstate (conn, 0);
        return;
    }
    return;
}

static void
checkterm (isdn3_conn conn)
{
    if (conn->state == 0) {
        report_terminate (conn);
        isdn3_killconn (conn, 1); /* XXX */
    }
}

static int
send_disconnect(isdn3_conn conn, char do_L3)
{
	int err;

	if(conn->state >= 21)
		return 0;
	if(conn->state == 0)
		return 0;
	conn->waitflags = 0;
	report_terminate(conn);
	if(do_L3 && (conn->state < 20)) {
		struct CAPI_disconnectb3_req *c3;
		mblk_t *m3 = allocb(sizeof(*c3),BPRI_MED);
		if(m3 == NULL)
			return -ENOMEM;

		c3 = ((typeof(c3))m3->b_wptr)++;
		bzero(c3,sizeof(*c3));
		c3->ncci = conn->ncci0;
		if((err = capi_send(conn->talk,conn->call_ref >> 16,CAPI_DISCONNECTB3_REQ,m3,
				conn->conni[WF_DISCONNECTB3_CONF] = newmsgid(conn->talk))) < 0) {
			freemsg(m3);
		} else {
			conn->waitflags |= 1 << WF_DISCONNECTB3_CONF;
			conn->waitflags |= 1 << WF_DISCONNECTB3_IND;
			setstate(conn,20);
		}
	}
	{
		struct CAPI_disconnect_req *c3;
		mblk_t *m3 = allocb(sizeof(*c3),BPRI_MED);
		if(m3 == NULL)
			return -ENOMEM;

		c3 = ((typeof(c3))m3->b_wptr)++;
		bzero(c3,sizeof(*c3));
		c3->plci = conn->call_ref;
		if((err = capi_send(conn->talk,conn->call_ref >> 16,CAPI_DISCONNECT_REQ,m3,
				conn->conni[WF_DISCONNECT_CONF] = newmsgid(conn->talk))) < 0) {
			freemsg(m3);
		} else {
			conn->waitflags |= 1 << WF_DISCONNECT_CONF;
			conn->waitflags |= 1 << WF_DISCONNECT_IND;
			if(conn->state < 20)
				setstate(conn,21);
			else
				setstate(conn,22);
		}
	}
	return err;	
}

static int
after_active(isdn3_conn conn)
{
	int err;
	if(conn->waitflags)
		return 0; /* not yet */
	switch(conn->state) {
	default:
		err = -EIO;
		if(send_disconnect(conn,0) < 0)
			setstate(conn,0);
		break;
	case 3:
	case 9:
		setstate(conn,15);
		isdn3_setup_conn (conn, EST_CONNECT);
		err = 0;
		break;
	}
	return err;
}

static int
after_selectb(isdn3_conn conn)
{
	int err;
	if(conn->waitflags)
		return 0; /* not yet */
	switch(conn->state) {
	case 2: /* active */
		{
			struct CAPI_connect_resp *c3;
			mblk_t *m3 = allocb(sizeof(*c3),BPRI_MED);
			if(m3 == NULL) {
				err = -ENOMEM;
				break;
			}
			c3 = ((typeof(c3))m3->b_wptr)++;
			bzero(c3,sizeof(*c3));
			c3->plci = conn->call_ref;
			if((err = capi_send(conn->talk,conn->call_ref>>16,CAPI_CONNECT_RESP,m3,conn->msgid0)) < 0)
				freemsg(m3);
			else {
				conn->waitflags  = 1<<WF_CONNECTB3_CONF;
				conn->waitflags |= 1<<WF_CONNECTB3ACTIVE_IND;
				setstate(conn,3);
			}
		}
		break;
	case 8:  /* passive */
		{
			struct CAPI_listenb3_req *c2;
			struct CAPI_connect_resp *c3;
			mblk_t *m2 = allocb(sizeof(*c2),BPRI_MED);
			mblk_t *m3 = allocb(sizeof(*c3),BPRI_MED);
			if(m2 == NULL || m3 == NULL) {
				if(m2 != NULL)
					freemsg(m2);
				if(m3 != NULL)
					freemsg(m3);
				err = -ENOMEM;
				break;
			}
			c2 = ((typeof(c2))m2->b_wptr)++;
			c3 = ((typeof(c3))m3->b_wptr)++;
			bzero(c2,sizeof(*c2));
			bzero(c3,sizeof(*c3));
			c2->plci = conn->call_ref;
			c3->plci = conn->call_ref;
			if((err = capi_send(conn->talk,conn->call_ref>>16,CAPI_LISTENB3_REQ,m3,conn->conni[WF_LISTENB3_CONF] = newmsgid(conn->talk))) < 0) {
				freemsg(m2);
				freemsg(m3);
			} else if((err = capi_send(conn->talk,conn->call_ref>>16,CAPI_CONNECT_RESP,m3,conn->msgid0)) < 0)
				freemsg(m3);
			else {
				conn->waitflags  = 1<<WF_LISTENB3_CONF;
				conn->waitflags |= 1<<WF_CONNECTACTIVE_IND;
				conn->waitflags |= 1<<WF_CONNECTB3_IND;
				conn->waitflags |= 1<<WF_CONNECTB3ACTIVE_IND;
				setstate(conn,9);
			}
		}
		break;
	default:
		printf("CAPI error: wrong state %d in after_select\n",conn->state);
		err = send_disconnect(conn,0);
		if(err < 0)
			setstate(conn,0);
	}
	if(err < 0)
		send_disconnect(conn,0);
	return err;
}

static int
recv (isdn3_talk talk, char isUI, mblk_t * data)
{
	struct CAPI_every_header *capi;
	streamchar *origmb;
	int err = 0;
	isdn3_conn conn = 0;

	printf("CAPI: recv %d, in state %ld\n",isUI,talk->tstate);
	origmb = data->b_rptr;
	if(data->b_wptr-data->b_rptr < sizeof(*capi)) 
		goto less_room;

	capi = ((typeof(capi))data->b_rptr)++;

	switch(capi->PRIM_type) {
	default:
	  	printf("CAPI: Unknown primary type 0x%04x\n",capi->PRIM_type);
		err = -ENXIO;
		goto printit;
	
	case CAPI_DISCONNECTB3_CONF:
		{
			struct CAPI_disconnectb3_conf *c2;
			if(data->b_wptr-data->b_rptr < sizeof(*c2))
				goto less_room;
			c2 = ((typeof(c2))data->b_rptr)++;
			if((conn = capi_findconn3(talk,capi->appl,c2->ncci)) == NULL) {
				printf("CAPI error: DISCONNECTB3_CONF has unknown cref3 %04x%04x\n",capi->appl,c2->ncci);
				err = -ENXIO;
				break;
			}
			if(c2->info != 0) {
				printf("CAPI error: DISCONNECTB3_CONF returns %04x\n",c2->info);
				send_disconnect(conn,0);
				break;
			}
			if(conn->waitflags & (1<<WF_DISCONNECTB3_CONF)) {
				conn->waitflags &=~ (1<<WF_DISCONNECTB3_CONF);
				if(conn->waitflags == 0) {
					setstate(conn,0);
					break;
				}

			} else {
				printf("CAPI error: DISCONNECTB3_CONF in wrong state %d\n",conn->state);
				err = -EIO;
				break;
			}
		}
		break;
	case CAPI_DISCONNECTB3_IND:
		{
			struct CAPI_disconnectb3_ind *c2;
			if(data->b_wptr-data->b_rptr < sizeof(*c2))
				goto less_room;
			c2 = ((typeof(c2))data->b_rptr)++;
			if((conn = capi_findconn3(talk,capi->appl,c2->ncci)) == NULL) {
				printf("CAPI error: DISCONNECTB3_IND has unknown cref3 %04x%04x\n",capi->appl,c2->ncci);
				err = -ENXIO;
				break;
			}
			if((c2->info == 0) && (conn->waitflags & (1<<WF_DISCONNECTB3_IND))) {
				conn->waitflags &=~ (1<<WF_DISCONNECTB3_IND);
				if(conn->waitflags == 0) {
					report_terminate(conn);
					if(conn->state == 20) {
						err = send_disconnect(conn,0);
						if(err < 0)
							setstate(conn,0);
					} else 
						setstate(conn,0);
				}
			} else if((c2->info != 0) || (conn->state >= 20)) {
				printf("CAPI error: DISCONNECTB3_IND in wrong state %d, info %04x\n",conn->state,c2->info);
				err = -EIO;
			} else {
				isdn3_setup_conn (conn, EST_DISCONNECT);
				send_disconnect(conn,0);
				report_terminate(conn);
			}
			{
				int err3 = 0;
				struct CAPI_disconnectb3_resp *c3;
				mblk_t *m3 = allocb(sizeof(*c3),BPRI_MED);
				if(m3 == NULL) 
					err3 = -ENOMEM;
				if(err3 == 0) {
					c3 = ((typeof(c3))data->b_wptr)++;
					bzero(c3,sizeof(*c3));
					c3->ncci = c2->ncci;
					if((err3 = capi_send(talk,capi->appl,CAPI_DISCONNECTB3_RESP,m3,capi->messid)) < 0)
						freemsg(m3);
				}
				if(err == 0)
					err = send_disconnect(conn,0);
				if(err == 0)
					err = err3;
			}
		}
		break;
	case CAPI_DISCONNECT_IND:
		{
			struct CAPI_disconnect_ind *c2;
			if(data->b_wptr-data->b_rptr < sizeof(*c2))
				goto less_room;
			c2 = ((typeof(c2))data->b_rptr)++;
			if((conn = capi_findconn(talk,capi->appl,c2->plci)) == NULL) {
				printf("CAPI error: DISCONNECT_IND has unknown cref %04x%04x\n",capi->appl,c2->plci);
				err = -ENXIO;
				break;
			}
			if((c2->info == 0) && (conn->waitflags & (1<<WF_DISCONNECT_IND))) {
				conn->waitflags &=~ (1<<WF_DISCONNECT_IND);
				if(conn->waitflags == 0) {
					report_terminate(conn);
					setstate(conn,0);
				}
			} else if((c2->info != 0) || (conn->state >= 21)) {
				printf("CAPI error: DISCONNECT_IND in wrong state %d, info %04x\n",conn->state,c2->info);
				err = -EIO;
			} else {
				isdn3_setup_conn (conn, EST_DISCONNECT);
				send_disconnect(conn,0);
				report_terminate(conn);
			}
			{
				int err3 = 0;
				struct CAPI_disconnect_resp *c3;
				mblk_t *m3 = allocb(sizeof(*c3),BPRI_MED);
				if(m3 == NULL) 
					err3 = -ENOMEM;
				if(err3 == 0) {
					c3 = ((typeof(c3))data->b_wptr)++;
					bzero(c3,sizeof(*c3));
					c3->plci = c2->plci;
					if((err3 = capi_send(talk,capi->appl,CAPI_DISCONNECT_RESP,m3,capi->messid)) < 0)
						freemsg(m3);
				}
				if(err == 0)
					err = send_disconnect(conn,0);
				if(err == 0)
					err = err3;
			}
		}
		break;

	case CAPI_DISCONNECT_CONF:
		{
			struct CAPI_disconnect_conf *c2;
			if(data->b_wptr-data->b_rptr < sizeof(*c2))
				goto less_room;
			c2 = ((typeof(c2))data->b_rptr)++;
			if((conn = capi_findconn(talk,capi->appl,c2->plci)) == NULL) {
				printf("CAPI error: DISCONNECT_CONF has unknown cref %04x%04x\n",capi->appl,c2->plci);
				err = -ENXIO;
				break;
			}
			if((c2->info == 0) || (conn->waitflags & (1<<WF_DISCONNECT_CONF))) {
				conn->waitflags &=~ (1<<WF_DISCONNECT_CONF);
				if(conn->waitflags == 0) {
					report_terminate(conn);
					setstate(conn,0);
				}
			} else {
				printf("CAPI error: DISCONNECT_CONF in wrong state %d, info %04x\n",conn->state,c2->info);
				isdn3_setup_conn (conn, EST_DISCONNECT);
				setstate(conn,0);
				report_terminate(conn);
			}
		}
		break;

	case CAPI_CONNECT_CONF:
		{
			struct CAPI_connect_conf *c2;
			if(data->b_wptr-data->b_rptr < sizeof(*c2))
				goto less_room;
			c2 = ((typeof(c2))data->b_rptr)++;
			if((conn = capi_findconn(talk,capi->appl,c2->plci)) != NULL) {
				printf("CAPI error: CONNECT_CONF has known cref %04x%04x\n",capi->appl,c2->plci);
				setstate(conn,0);
				err = -ENXIO;
				break;
			}
			if((conn = capi_findconnm(talk,capi->appl,capi->messid)) == NULL) {
				printf("CAPI error: CONNECT_CONF has unknown msgid %04x.%04x\n",capi->appl,capi->messid);
				setstate(conn,0);
				err = -ENXIO;
				break;
			}
			conn->call_ref = (capi->appl << 16) | c2->plci;
			if((c2->info == 0) && (conn->waitflags & (1<<WF_CONNECT_CONF))) {
				conn->waitflags &=~ (1<<WF_CONNECT_CONF);
				if(conn->waitflags == 0) {
					if((err = send_setup(conn)) < 0) {
						if(send_disconnect(conn,0) < 0) {
							setstate(conn,0);
						}
					} else {
						conn->waitflags |= 1<<WF_CONNECTACTIVE_IND;
						setstate(conn,2);
					}

				}
			} else {
				printf("CAPI error: CONNECT_CONF in wrong state %d, info %04x\n",conn->state,c2->info);
				isdn3_setup_conn (conn, EST_DISCONNECT);
				report_terminate(conn);
				if(send_disconnect(conn,0) < 0)
					setstate(conn,0);
			}
		}
		break;

	case CAPI_SELECTB2_CONF:
		{
			struct CAPI_selectb2_conf *c2;
			if(data->b_wptr-data->b_rptr < sizeof(*c2))
				goto less_room;
			c2 = ((typeof(c2))data->b_rptr)++;
			if((conn = capi_findconn(talk,capi->appl,c2->plci)) == NULL) {
				printf("CAPI error: SELECTB2_CONF has unknown cref %04x%04x\n",capi->appl,c2->plci);
				err = -ENXIO;
				break;
			}
			if((c2->info == 0) && (conn->waitflags & (1<<WF_SELECTB2_CONF))) {
				conn->waitflags &=~ (1<<WF_SELECTB2_CONF);
				err = after_selectb(conn);
			} else {
				printf("CAPI error: SELECTB2_CONF in wrong state %d, info %04x\n",conn->state,c2->info);
				report_terminate(conn);
				if(send_disconnect(conn,0) < 0) 
					setstate(conn,0);
			}
		}
		break;
	case CAPI_SELECTB3_CONF:
		{
			struct CAPI_selectb3_conf *c2;
			if(data->b_wptr-data->b_rptr < sizeof(*c2))
				goto less_room;
			c2 = ((typeof(c2))data->b_rptr)++;
			if((conn = capi_findconn(talk,capi->appl,c2->plci)) == NULL) {
				printf("CAPI error: SELECTB3_CONF has unknown cref %04x%04x\n",capi->appl,c2->plci);
				err = -ENXIO;
				break;
			}
			if((c2->info == 0) && (conn->waitflags & (1<<WF_SELECTB3_CONF))) {
				conn->waitflags &=~ (1<<WF_SELECTB3_CONF);
				err = after_selectb(conn);
			} else  {
				printf("CAPI error: SELECTB3_CONF in wrong state %d, info %04x\n",conn->state,c2->info);
				report_terminate(conn);
				if(send_disconnect(conn,0) < 0) 
					setstate(conn,0);
			}
		}
		break;
	case CAPI_CONNECTACTIVE_IND:
		{
			struct CAPI_connectactive_ind *c2;
			if(data->b_wptr-data->b_rptr < sizeof(*c2))
				goto less_room;
			c2 = ((typeof(c2))data->b_rptr)++;
			if((conn = capi_findconn(talk,capi->appl,c2->plci)) == NULL) {
				printf("CAPI error: CONNECTACTIVE_IND has unknown cref %04x%04x\n",capi->appl,c2->plci);
				err = -ENXIO;
				break;
			}
			if(conn->waitflags & (1<<WF_CONNECTACTIVE_IND)) {
				conn->waitflags &=~ (1<<WF_CONNECTACTIVE_IND);
				err = after_active(conn);
			} else  {
				printf("CAPI error: CONNECTACTIVE_IND in wrong state %d\n",conn->state);
				if(send_disconnect(conn,0) < 0) 
					setstate(conn,0);
			}
			{
				int err3 = 0;
				struct CAPI_connectactive_resp *c3;
				mblk_t *m3 = allocb(sizeof(*c3),BPRI_MED);
				if(m3 == NULL) 
					err3 = -ENOMEM;
				if(err3 == 0) {
					c3 = ((typeof(c3))data->b_wptr)++;
					bzero(c3,sizeof(*c3));
					c3->plci = c2->plci;
					if((err3 = capi_send(talk,capi->appl,CAPI_CONNECTACTIVE_RESP,m3,capi->messid)) < 0)
						freemsg(m3);
				}
				if(err == 0)
					err = err3;
			}
		}
		break;
	case CAPI_LISTENB3_CONF:
		{
			struct CAPI_listenb3_conf *c2;
			if(data->b_wptr-data->b_rptr < sizeof(*c2))
				goto less_room;
			c2 = ((typeof(c2))data->b_rptr)++;
			if((conn = capi_findconn(talk,capi->appl,c2->plci)) == NULL) {
				printf("CAPI error: LISTENB3_CONF has unknown cref %04x%04x\n",capi->appl,c2->plci);
				err = -ENXIO;
				break;
			}
			if((c2->info == 0) && (conn->waitflags & (1<<WF_LISTENB3_CONF))) {
				conn->waitflags &=~ (1<<WF_LISTENB3_CONF);
				err = after_active(conn);
			} else  {
				err = -EIO;
				printf("CAPI error: LISTENB3_CONF in wrong state %d, info %04x\n",conn->state,c2->info);
				if(send_disconnect(conn,0) < 0) 
					setstate(conn,0);
			}
		}
		break;
	case CAPI_CONNECTB3_CONF:
		{
			struct CAPI_connectb3_conf *c2;
			if(data->b_wptr-data->b_rptr < sizeof(*c2))
				goto less_room;
			c2 = ((typeof(c2))data->b_rptr)++;
			if((conn = capi_findconn(talk,capi->appl,c2->plci)) == NULL) {
				printf("CAPI error: CONNECTB3_CONF has unknown cref %04x%04x\n",capi->appl,c2->plci);
				err = -ENXIO;
				break;
			}
			if((c2->info == 0) && (conn->waitflags & (1<<WF_CONNECTB3_CONF))) {
				conn->waitflags &=~ (1<<WF_CONNECTB3_CONF);
				{
					mblk_t *mz = allocb(64,BPRI_MED);
					if(mz == NULL) {
						err = -ENOMEM;
						goto exSendD;
					}
					m_putid(mz,CMD_CARDPROT);
					m_putsx(mz,ARG_ASSOC);
					m_puti(mz,capi->appl);
					m_puti(mz,c2->plci);
					m_puti(mz,c2->ncci);
					err = isdn3_send(conn->talk,AS_PROTO,mz);
					if(err < 0) {
						freemsg(mz);
						goto exSendD;
					}
					conn->ncci0 = c2->ncci;
				}
				err = after_active(conn);
			} else  {
				err = -EIO;
				printf("CAPI error: CONNECTB3_CONF in wrong state %d, info %04x\n",conn->state,c2->info);
			  exSendD:
				if(send_disconnect(conn,0) < 0) 
					setstate(conn,0);
			}
		}
		break;

	case CAPI_CONNECTB3ACTIVE_IND:
		{
			struct CAPI_connectb3active_ind *c2;
			if(data->b_wptr-data->b_rptr < sizeof(*c2))
				goto less_room;
			c2 = ((typeof(c2))data->b_rptr)++;
			if((conn = capi_findconn3(talk,capi->appl,c2->ncci)) == NULL) {
				printf("CAPI error: CONNECTB3ACTIVE_IND has unknown cref %04x%04x\n",capi->appl,c2->ncci);
				err = -ENXIO;
				break;
			}
			if(conn->waitflags & (1<<WF_CONNECTB3ACTIVE_IND)) {
				conn->waitflags &=~ (1<<WF_CONNECTB3ACTIVE_IND);
				err = after_active(conn);
			} else  {
				printf("CAPI error: CONNECTB3ACTIVE_IND in wrong state %d\n",conn->state);
				if(send_disconnect(conn,0) < 0) 
					setstate(conn,0);
			}
			{
				int err3 = 0;
				struct CAPI_connectb3active_resp *c3;
				mblk_t *m3 = allocb(sizeof(*c3),BPRI_MED);
				if(m3 == NULL) 
					err3 = -ENOMEM;
				if(err3 == 0) {
					c3 = ((typeof(c3))data->b_wptr)++;
					bzero(c3,sizeof(*c3));
					c3->ncci = c2->ncci;
					if((err3 = capi_send(talk,capi->appl,CAPI_CONNECTB3ACTIVE_RESP,m3,capi->messid)) < 0)
						freemsg(m3);
				}
				if(err == 0)
					err = err3;
			}
		}
		break;

	case CAPI_CONNECT_IND:
		{
			struct CAPI_connect_ind *c2;

			if(data->b_wptr-data->b_rptr < sizeof(*c2))
				goto less_room;
			c2 = ((typeof(c2))data->b_rptr)++;
			if(data->b_wptr-data->b_rptr < c2->telnolen)
				goto less_room;
			if((conn = capi_findconn(talk,capi->appl,c2->plci)) != NULL) { /* Duplicate. Hmmm. */
				printf("CAPI error: Incoming call has dup cref %04x%04x for conn %d\n",capi->appl, c2->plci,conn->conn_id);
				err = 0;
				break;
			} else {
				conn = isdn3_new_conn (talk);
				if (conn == NULL) 
					err = -ENOMEM;
				else {
					if((conn->p_data = malloc(sizeof(struct capi_info))) == NULL) {
						err = -ENOMEM;
					} else {
    					struct capi_info *info = conn->p_data;
						info->eaz = c2->DST_eaz;
						info->service = (c2->DST_service << 8) | c2->DST_addinfo;
						bcopy(data->b_rptr,info->nr,c2->telnolen);

						conn->call_ref = (capi->appl << 16) | c2->plci;
						conn->msgid0 = capi->messid;

						err = report_incoming(conn);
						if(err == 0) {
							setstate(conn,7);
							break;
						}
					}
				}
			}
			
			/* Reject. */
			{
				struct CAPI_connect_resp *c3;
				mblk_t *mp = allocb(sizeof(*c3),BPRI_MED);
				if(mp != NULL) {
					c3 = ((struct CAPI_connect_resp *)mp->b_wptr)++;
					c3->plci = c2->plci;
					c3->reject = N1_OutOfOrder;
					if((err = capi_send(talk,capi->appl,CAPI_CONNECT_RESP,mp,capi->messid)) < 0)
						freemsg(mp);
				}
			}
		}
		break;
	
	/* Non-connection-related messages */
	case CAPI_REGISTER_CONF:
		talk->tstate = STATE_CONF_LAST;
		/* TODO: get parameters (EAZMAP, et al.) and send them */
		{
			mblk_t *info = talk->card->info;
        	if(info != NULL) {
                streamchar *sta = info->b_rptr;
                ushort_t idx;
				char skip = 0;

				while(m_getid(info,&idx) == 0) {
					long sap;
					switch(idx) {
					case ARG_PROTOCOL:
						if (m_geti(info,&sap) == 0) {
								skip = (sap != SAPI_CAPI);
						}
						break;
					case ARG_SUBPROT:
						if (m_geti(info,&sap) == 0 && !skip) {
							switch(sap) {
							case SAPI_CAPI_BINTEC:
								skip=0;
								break;
							default:
								/* Wrong card. Do something! */
                				info->b_rptr = sta;
								return -ENXIO;
							}
						}
						break;
					case ARG_EAZ:
						if(skip)
							break;
						{
							char eaz;
							struct eazmapping *ce;
							mblk_t *mp;
							int len;

							if((err = m_getc(info,&eaz)) < 0)
								break;
							if((len = m_getstrlen(info)) < 0)
								break;
							mp = allocb(sizeof(*ce)+len, BPRI_MED);
							if(mp == NULL)
								break;
							ce = ((struct eazmapping *)mp->b_wptr)++;
							bzero(ce,sizeof(*ce));
							ce->eaz = eaz;
							ce->telnolen = len;
							if((err = m_getstr(info,mp->b_wptr,len)) < 0) {
								freemsg(mp);
								break;
							}
							mp->b_wptr += len;
							if((err = capi_sendctl(talk,0,CONTROL_EAZMAPPING,mp,newmsgid(talk))) < 0) {
								freemsg(mp);
								break;
							}
						}
						break; /* ARG_EAZ */
					}
				}
                info->b_rptr = sta;
        	}
		}

		{	/* Tell the board to listen to everything */
			struct CAPI_listen_req *c2;
			mblk_t *mp = allocb(sizeof(*c2),BPRI_MED);

			if(mp == NULL)
				return -ENOMEM;
			c2 = ((struct CAPI_listen_req *)mp->b_wptr)++;
			bzero(c2,sizeof(*c2));
			c2->info_mask = 0x003F;
			c2->eaz_mask = 0x03FF;
			c2->service_mask = 0xFFFF;
			{	/* Find correct driver name */
				int err; char skip = 0;
				mblk_t *info = talk->card->info;
				if(info != NULL) {
					streamchar *sta = info->b_rptr;
					ushort_t idx;

					while(m_getid(info,&idx) == 0) {
						long sap;
						switch(idx) {
						case ARG_PROTOCOL:
							if (m_geti(info,&sap) == 0) {
									skip = (sap != SAPI_CAPI);
							}
							break;
						case ARG_SUBPROT:
							if (m_geti(info,&sap) == 0 && !skip) {
								switch(sap) {
								case SAPI_CAPI_BINTEC:
									skip=0;
									break;
								default:
									/* Wrong card. TODO: Do something! */
									info->b_rptr = sta;
									return -ENXIO;
								}
							}
							break;
						case ARG_LISTEN:
							{
								long x;
								if(skip)
									break;
								if((err = m_getx(info,&x)) >= 0) {
									c2->eaz_mask = x;
									if((err = m_getx(info,&x)) >= 0) {
										c2->service_mask = x;
										if((err = m_getx(info,&x)) >= 0) {
											c2->info_mask = x;
										}
									}
								}
							}
							break;
						}
					}
					info->b_rptr = sta;
				}
			}
			if((err = capi_send(talk,0,CAPI_LISTEN_REQ,mp,newmsgid(talk))) < 0) {
				freemsg(mp);
				return err;
			}
			talk->tstate--;
		}
		break;
	case CAPI_LISTEN_CONF:
		{
			struct CAPI_listen_conf *c2;
			c2 = ((typeof(c2))data->b_rptr)++;
			if(c2->info != 0) {
				printf("CAPI error: LISTEN failed %04x\n",c2->info);
				return -EIO;
			}
			if(talk->tstate < STATE_CONF_FIRST || talk->tstate > STATE_CONF_LAST) {
				printf("CAPI error: LISTEN return in bad state\n");
				return -EIO;
			}
			if(++talk->tstate == STATE_CONF_LAST)
				talk->tstate = STATE_RUNNING;
		}
		break;
	case CAPI_ALIVE_IND:
		if(talk->tstate < STATE_CONF_FIRST) {
			printf("CAPI: ALIVE_REQ in state %ld\n",talk->tstate);
			goto printit;
		}
		err = capi_send(talk,0,CAPI_ALIVE_RESP,NULL,capi->messid);
		break;
	case CAPI_CONTROL_CONF:
		{
			struct CAPI_control_conf *c2;
			if(data->b_wptr-data->b_rptr < sizeof(*c2))
				goto less_room;
			c2 = ((struct CAPI_control_conf *)data->b_rptr)++;
			switch(c2->type) {
			default:
			    printf("Unknown control_conf type 0x%02x\n",c2->type);
				goto printit;
			case CONTROL_API_OPEN: /* Boot: Step One */
				if(talk->tstate != STATE_OPENING) {
					printf("CAPI error: API_OPEN reply for bad state\n");
					return -EINVAL;
				}
				if(c2->info <= 0) {
					printf("CAPI error: open failed!\n");
					return -EIO;
				}
				if(c2->info > 1)
					printf("CAPI: %d D channels -- using only the first, for now\n",c2->info);
				{
					struct CAPI_register_req *c3;

					mblk_t *mp = allocb(sizeof(*c3), BPRI_MED);
					if(mp == NULL) /* XXX error processing */
						return -ENOMEM;
					c3 = ((struct CAPI_register_req *)mp->b_wptr)++;
					bzero(c3,sizeof(*c3));
					c3->nmess = 10;
					c3->nconn = 2 /* TODO: 30 */ ;
					c3->ndblock = c3->nconn*10;
					c3->dblocksiz = 4096;
					if((err = capi_send(talk,0,CAPI_REGISTER_REQ,mp,newmsgid(talk))) < 0) {
						freemsg(mp);
						break;
					}
					talk->tstate = STATE_REGISTER;
				}
				break;
			}
		}
	}
	if(err >= 0)
		freemsg(data);
	return err;

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
  less_room:
  	printf("CAPI error: too few data bytes\n");
  printit:
  	data->b_rptr = origmb;
	log_printmsg(NULL,"CAPI recvErr",data,"=");
	if(conn != NULL)
		checkterm(conn);
	return err ? err : -EINVAL;
}

static int
send (isdn3_conn conn, mblk_t * data)
{
	printf("CAPI: send %08lx\n",conn->call_ref);
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
	streamchar *oldpos = NULL;
	int err = 0;
	ushort_t typ;
	/* uchar_t suppress = 0; */
	/* uchar_t svc = 0; */
	struct capi_info *info;
	int force = 0;

	printf("CAPI: sendcmd %08lx %04x: ",conn->call_ref,id);
	{
		mblk_t *mb = data;
		if(mb == NULL) printf("NULL"); else
		while(mb != NULL) {
			dumpascii(mb->b_rptr,mb->b_wptr-mb->b_rptr);
			mb = mb->b_cont;
		}
	}
	printf("\n");
	if(conn->p_data == NULL) {
		if((conn->p_data = malloc(sizeof(struct capi_info))) == NULL) {
			return -ENOMEM;
		}
		bzero(conn->p_data,sizeof(struct capi_info));
	}
	conn->lockit++;
	info = conn->p_data;
    if (data != NULL) {
		unsigned long x;

        oldpos = data->b_rptr;
        while ((err = m_getsx (data, &typ)) == 0) {
            switch (typ) {
            case ARG_SERVICE:
                if ((err = m_getx (data, &x)) != 0) {
                    data->b_rptr = oldpos;
                    printf("GetX Service: ");
                    conn->lockit--;
                    return err;
                }
				info->service = x;
                break;
            case ARG_EAZ:
                if ((err = m_getc (data, &info->eaz)) != 0) {
                    data->b_rptr = oldpos;
                    printf("GetX EAZ: ");
                    conn->lockit--;
                    return err;
                }
                break;
            case ARG_NUMBER:
                m_getskip (data);
                if ((err = m_getstr (data, (char *) info->nr, MAXNR)) != 0) {
                    data->b_rptr = oldpos;
                    printf("GetStr Number: ");
                    conn->lockit--;
                    return err;
                }
                break;
			case ARG_FORCE:
				force = 1;
				break;
            default:;
            }
        }
        data->b_rptr = oldpos;
    }
    err = 0;
    switch (id) {
	case CMD_ANSWER:
		{
            if (data == NULL) {
                printf("DataNull: ");
                conn->lockit--;
                return -EINVAL;
            }
			if(conn->state != 7) {
				printf("CAPI error: ANSWER in bad state!\n");
				conn->lockit--;
				return -EINVAL;
			}
			err = send_setup(conn);
			if(err == 0) {
				freemsg(data);
				setstate(conn,8);
				conn->waitflags  = 1 << WF_SELECTB2_CONF;
				conn->waitflags |= 1 << WF_SELECTB3_CONF;
			} else
				send_disconnect(conn,0);
		}
    case CMD_DIAL:
        {
            if (data == NULL) {
                printf("DataNull: ");
                conn->lockit--;
                return -EINVAL;
            }
			if(conn->state != 0) {
				printf("CAPI error: DIAL in bad state!\n");
				conn->lockit--;
				return -EINVAL;
			}
			{
				struct CAPI_connect_req *c2;
				mblk_t *m2 = allocb(sizeof(*c2)+strlen(info->nr),BPRI_MED);

				if(m2 == NULL) 
					return -ENOMEM;
				c2 = ((typeof(c2))m2->b_wptr++);
				bzero(c2,sizeof(*c2));
				c2->infomask = ~0;
				c2->DST_service = info->service >> 8;
				c2->DST_addinfo = info->service;
				c2->SRC_eaz = info->eaz;
				c2->telnolen = strlen(info->nr);
				strcpy(m2->b_wptr,info->nr);
				m2->b_wptr += strlen(info->nr);
				if((err = capi_send(conn->talk,0,CAPI_CONNECT_REQ,m2,conn->conni[WF_CONNECT_CONF]=newmsgid(conn->talk))) < 0) 
					freemsg(m2);
				else {
					conn->waitflags = 1<<WF_CONNECT_CONF;
					setstate(conn,1);
				}
			}
		}
		break;
	case CMD_OFF:
		{
			/* TODO: set cause */
			err = send_disconnect(conn,!force);
			if(err < 0)
				setstate(conn,0);
		}
		break;
	default:
		err = -ENXIO;
		break;
	}

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
	checkterm(conn);
	return err;
}

static void
report (isdn3_conn conn, mblk_t * data)
{
    struct capi_info *info;
	printf("CAPI: report %08lx: ",conn->call_ref);
	{
		mblk_t *mb = data;
		if(mb == NULL) printf("NULL"); else
		while(mb != NULL) {
			dumpascii(mb->b_rptr,mb->b_wptr-mb->b_rptr);
			mb = mb->b_cont;
		}
	}
	printf("\n");
    info = conn->p_data;
    if (info == NULL)
        return;
	if (info->nr[0] != 0) {
		m_putsx (data, ARG_NUMBER);
		m_putsz (data, info->nr);
	}
	if (info->eaz != 0) {
		m_putsx (data, ARG_EAZ);
		*data->b_wptr++=' ';
		*data->b_wptr++=info->eaz;
	}
	if (info->service != 0) {
		m_putsx (data, ARG_SERVICE);
		m_putx (data, info->service);
	}
}

static void
ckill (isdn3_talk talk, char force)
{
	printf("CAPI: ckill %d\n",force);
#if 0
	if (force) {
		if (talk->tstate & PHONE_UP) {
			talk->tstate &= ~PHONE_UP;
			if(0)printf("Phone is aDown\n");
			isdn3_chstate (talk, DL_RELEASE_REQ, 0, CH_CLOSEPROT);
		}
	}
#endif
}

static void
killconn (isdn3_conn conn, char force)
{
	printf("CAPI: killconn %08lx: %d\n",conn->call_ref,force);
	if (conn->p_data != NULL) {
		free (conn->p_data);
		conn->p_data = NULL;
	}
    if (force) {
        untimer (CAPI_TALERT, conn);
	}
	switch(conn->state) {
	}

#if 0
	isdn3_talk talk = conn->talk;
	isdn3_prot prot = isdn3_findprot (conn->card->info, conn->subprotocol);

	if (prot != NULL)
		(*prot->killconn) (conn, force);

	if (talk != NULL && talk->conn == NULL && (talk->tstate & PHONE_UP)) {
		/* Last talker got closed. Shutdown for level 2. */
		talk->tstate &= ~PHONE_UP;
		if(0)printf ("PHONE i2s DOWN\n");
		(void) isdn3_chstate (talk, DL_RELEASE_REQ, 0, CH_CLOSEPROT);
	} else {
#if 0
		printf ("Killconn: Phone s1tays up");
		if (talk == NULL)
			printf (": Talk NULL");
		else if (talk->conn != NULL)
			printf (": talk->conn %x (ref %08lx, state %d)", talk->conn, talk->conn->call_ref, talk->conn->state);
		if (!(talk->tstate & PHONE_UP))
			if(0)printf (": PHONE i3s DOWN\n");
		printf ("\n");
#endif
	}
#endif
}

static void
hook (isdn3_conn conn)
{
	printf("CAPI: hook %08lx\n",conn->call_ref);
#if 0
	isdn3_prot prot = isdn3_findprot (conn->card->info, conn->subprotocol);

	if (prot != NULL && prot->hook != NULL)
		(*prot->hook) (conn);
#endif
}


static void
newcard (isdn3_card card)
{
	isdn3_talk talk;

	printf("CAPI: newcard\n");
	talk = isdn3_findtalk (card, &CAPI_hndl, card->info, 1);
	if(talk == NULL) {
		printf("CAPI: newcard: talker not found ???\n");
		return;
	}
	talk->message_id = 0x4000;
	talk->tstate = STATE_BOOTING;
	if(card->is_up)
		chstate (talk, DL_ESTABLISH_CONF,0);
	
}

static ulong_t
modeflags (long protocol)
{
	printf("CAPI: modeflags %lx\n",protocol);
	return 1 << M_HDLC || 1 << CHM_INTELLIGENT;
}


static int
proto(struct _isdn3_conn * conn, mblk_t **data, char down)
{
	mblk_t *mb = *data;

	printf("CAPI: proto %08lx %s: ",conn->call_ref, down ? "down" : "up");
	if(mb == NULL) printf("NULL"); else
	while(mb != NULL) {
		dumpascii(mb->b_rptr,mb->b_wptr-mb->b_rptr);
		mb = mb->b_cont;
	}
	printf("\n");
	
	return 0;
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
		&sendcmd, &ckill, &killconn, &hook, &proto,
};

