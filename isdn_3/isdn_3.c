/* Show the reasons for not changing connection states */
#define REPORT

/**
 ** ISDN Level 3 control module
 **/

#include "primitives.h"
#include <sys/types.h>
#include <sys/time.h>
#include "f_signal.h"
#include "kernel.h"
#include <sys/param.h>
#include <sys/sysmacros.h>
#include "streams.h"
#include <sys/stropts.h>
#ifdef DONT_ADDERROR
#include "f_user.h"
#endif
#include <sys/errno.h>
#include "streamlib.h"
#include "isdn_23.h"
#include "isdn_3.h"
#include "isdn_34.h"
#include "lap.h"
#ifdef _tei_
#include "tei.h"
#endif
#ifdef _fixed_
#include "fixed.h"
#endif
#ifdef _phone_
#include "phone.h"
#endif
#ifdef _capi_
#include "capi.h"
#endif
#include "isdn_limits.h"
#include "isdn_proto.h"
#ifndef KERNEL
#include <syslog.h>
#endif
#include "sapi.h"

ushort_t hdrseq = 2;

/*
 * Global data. These should be allocated dynamically. They are not because
 * most kernels don't have a way to find out how much kernel memory is left,
 * and they crash when it's full. :-(
 */
extern void log_printmsg (void *log, const char *, mblk_t *, const char*);

/* static */ struct _isdn3_card *isdn_card = NULL;
/* static */ struct _isdn3_hndl *isdn_hndl = NULL;
/* static */ isdn3_conn minor2conn[NMINOR];/* x = Minor2conn[n]-1 points the minor
								   * number to isdn_conn[x]; x=0 means no
								   * connection. */
static ushort_t minorflags[NMINOR];

/* Minorflags flag bits */
#define MINOR_OPEN 01			  /* Somebody's out there. */
#define MINOR_WILL_OPEN 02		  /* Necessary to circumvent possible race
								   * condition -- the "opened" message may
								   * arrive after the first command
								   * refering to it */
#define MINOR_INITPROTO_SENT 04	  /* told L4 to setup stack */

/* Moved from conn->flags */

#define MINOR_STATE_NONE 0000	  /* Nothing or PROTO_DISC has been sent down */
#define MINOR_LISTEN_SENT 0100	  /* PROTO_CONN_LISTEN has been sent down. */
#define MINOR_CONN_SENT 0200		  /* PROTO_CONN has ben sent down. */
#define MINOR_INTERRUPT_SENT 0300  /* PROTO_INTERRUPT has been sent down */
#define MINOR_STATE_MASK 0300

#define MINOR_PROTO 0400

#define MINOR_INT 01000	/* Don't send HANGUP on connection close */

#define MINOR_INITPROTO_SENT2 02000	  /* told L4 to setup stack -- not cleared */
/*
 * Basic module info records
 */

static struct module_info isdn3_minfo =
{
		0, "isdn_3", 0, INFPSZ, 4000, 2000
};

static qf_open isdn3_open;
static qf_close isdn3_close;
static qf_srv isdn3_rsrv, isdn3_wsrv;
static qf_put isdn3_rput, isdn3_wput;

static struct qinit isdn3_rinit =
{
		isdn3_rput, isdn3_rsrv, isdn3_open, isdn3_close, NULL, &isdn3_minfo, NULL
};

static struct qinit isdn3_winit =
{
		isdn3_wput, isdn3_wsrv, NULL, NULL, NULL, &isdn3_minfo, NULL
};

struct streamtab isdn3_info =
{&isdn3_rinit, &isdn3_winit, NULL, NULL};

queue_t *isdn3_q;				  /* There is only one isdn_3 device in the
								   * system */

/* See below */
static int timeout_conn (isdn3_conn conn);
static int delay_conn (isdn3_conn conn);
static void later_conn (isdn3_conn conn);

#if 0 /* def CONFIG_DEBUG_ISDN */
void l3chk(void) { int i; for(i=1;i<NMINOR;i++) if(minor2conn[i]!=NULL)chkfree(minor2conn[i]); }
void chkall(void);
#define chkall();
#else
#define CHK
#endif

#ifndef KERNEL
int l3print(char *buf)
{
	char *obuf = buf;
	int i;

	for(i=1;i<NMINOR;i++) {
		if(minor2conn[i] != NULL) {
			isdn3_conn conn = minor2conn[i];
			char card[5];

			if(conn->card != NULL) {
				*(long *)card = conn->card->id;
				card[4]='0';
			} else {
				card[0] = '?';
				card[1] = '\0';
			}

			buf += sprintf(buf,"%d:(%s %lo;%o) ",i,card,conn->minorstate,minorflags[i]);
		} else if(minorflags[i] != 0) {
			buf += sprintf(buf,"%d:(%o) ",i,minorflags[i]);
		}
	}
	return buf-obuf;
}
#endif

/* --> isdn_3.h */
void
conn_info (isdn3_conn conn, mblk_t * mb)
{
	m_putsx (mb, ARG_CONNREF);
	m_puti (mb, conn->conn_id);
	if (conn->minor != 0) {
		m_putsx (mb, ARG_MINOR);
		m_puti (mb, conn->minor);

		if (conn->fminor != 0) {
			m_putsx (mb, ARG_FMINOR);
			m_puti (mb, conn->fminor);
		}
	}
	if (conn->minorstate & MS_SENTINFO)
		return;
	if (conn->card != NULL) {
		m_putsx (mb, ARG_CARD);
		m_putlx (mb, conn->card->id);
	}
	if (conn->minorstate & (MS_INCOMING | MS_OUTGOING))
		m_putsx (mb, (conn->minorstate & MS_INCOMING) ? PROTO_INCOMING : PROTO_OUTGOING);
	if (conn->stack[0] != 0) {
		m_putsx (mb, ARG_STACK);
		m_putsz (mb, conn->stack);
	}
	if (conn->talk != NULL) {
		m_putsx (mb, ARG_PROTOCOL);
		m_puti (mb, conn->talk->hndl->SAPI);
		m_putsx (mb, ARG_SUBPROT);
		m_puti (mb, conn->subprotocol);
	}
	if (conn->call_ref != 0) {
		m_putsx (mb, ARG_CALLREF);
		m_puti (mb, conn->call_ref);
	}
	if (conn->minorstate & MS_INITPROTO) {
		m_putsx (mb, ARG_MODE);
		m_puti (mb, conn->mode);
	}
	if (conn->minorstate & MS_BCHAN) {
		m_putsx (mb, ARG_CHANNEL);
		m_puti (mb, conn->bchan);
	}
	if(conn->talk != NULL && conn->talk->hndl->report != NULL)
		(*conn->talk->hndl->report) (conn, mb);
}

/*
 * Disconnect timeout procedure. Implements the delay between sending
 * PROTO_DISCONNET to the device and taking down the connection.
 */
static int
timeout_disc (isdn3_conn conn)
{
	if (conn->minorstate & MS_END_TIMER) {
		conn->minorstate &= ~MS_END_TIMER;
		isdn3_killconn (conn, 3);
	}
	return 0;
}

/* --> isdn_3.h */
#ifdef CONFIG_DEBUG_ISDN
void
deb_isdn3_killconn (isdn3_conn conn, char force, const char *deb_file, int deb_line)
#else
void
isdn3_killconn (isdn3_conn conn, char force)
#endif
{
	int ms = splstr ();
	isdn3_talk talk = conn->talk;


	printf ("Conn %ld: Killing %d St %lo: min %d, at %d", conn->call_ref, force, conn->minorstate, conn->minor, conn->fminor);

	if(force)
		conn->id = 0;
	
	if ((conn->delay > 0) && (conn->minorstate & MS_DELAYING)) {
#ifdef NEW_TIMEOUT
		untimeout (conn->later_timer);
#else
		untimeout (later_conn, conn);
#endif
	} else if (conn->delay == 0 && force == 0 && (conn->minorstate & MS_DELAYING)) {
		splx (ms);
		return;
	}
	conn->delay = 0;
	conn->minorstate |= MS_DELAYING;

	if (conn->minorstate & MS_CONNDELAY_TIMER) {
		conn->minorstate &= ~MS_CONNDELAY_TIMER;
#ifdef NEW_TIMEOUT
		untimeout (conn->delay_timer);
#else
		untimeout (delay_conn, conn);
#endif
	}
	if (conn->minorstate & MS_CONN_TIMER) {		/* Kill connection startup
												 * timer if it's running */
		conn->minorstate &= ~MS_CONN_TIMER;
#ifdef NEW_TIMEOUT
		untimeout (conn->start_timer);
#else
		untimeout (timeout_conn, conn);
#endif
	}
	if (conn->hupdelay
			&& ((minorflags[conn->minor] & MINOR_STATE_MASK) == MINOR_CONN_SENT
					|| (minorflags[conn->minor] & MINOR_STATE_MASK) == MINOR_LISTEN_SENT)
			&& (minorflags[conn->minor] & MINOR_PROTO)
			&& (conn->minorstate & MS_INITPROTO)
			&& (force == 0)) {		  /* Connection end delay? */

		if (conn->minorstate & MS_END_TIMER) {	/* Already did that */
			splx (ms);
			return;
		}
		if (isdn3_setup_conn (conn, (force & 2) ? EST_WILL_INTERRUPT : EST_WILL_DISCONNECT) == 0) {
#ifdef NEW_TIMEOUT
			conn->disc_timer =
#endif
					timeout (timeout_disc, conn, HZ * conn->hupdelay);
			conn->minorstate |= MS_END_TIMER;
			if(0)printf (" -- Setting up timer (%d sec)\n", conn->hupdelay);
			conn->hupdelay = 0;
			splx (ms);
			return;
		} else {				  /* Unable. Kill it immediately. */
			force = 1;
		}
	}
	if (conn->minorstate & MS_END_TIMER) {		/* Timer running? */
		if (!(force & 1)) {			  /* Need to kill the connection now? */
			conn->minorstate &= ~MS_END_TIMER;
#ifdef NEW_TIMEOUT
			untimeout (conn->disc_timer);
#else
			untimeout (timeout_disc, conn);
#endif
		} else {
			splx (ms);
			return;
		}
	}
	if (conn->minor != 0 && conn->fminor != 0 && conn->minor != conn->fminor && (minorflags[conn->minor] & (MINOR_OPEN | MINOR_INT) == MINOR_OPEN)) {
		/*
		 * The minor is open and it's not the control channel. Tell the lower
		 * layer to close the channel (send HANGUP).
		 */
		isdn23_hdr hdr;
		mblk_t *mb = allocb (sizeof (struct _isdn23_hdr), BPRI_MED);

		if (mb != NULL) {
			hdr = ((isdn23_hdr) mb->b_wptr)++;
#ifdef __CHECKER__
			memset(hdr,0,sizeof (*hdr));
#endif
			hdr->key = HDR_CLOSE|HDR_NOERROR;
			hdr->seqnum = hdrseq; hdrseq += 2;
			hdr->hdr_close.minor = conn->minor;
			hdr->hdr_close.error = 0;
			if (isdn3_sendhdr (mb) != 0)
				freeb (mb);
		}
	}
	if (conn->minor != 0 && isdn3_findminor (conn->minor) == conn) {
		isdn3_setup_conn (conn, (force & 2) ? EST_INTERRUPT : EST_DISCONNECT);
	}
	if(!(force & 1))
		force = 0;
	if (talk != NULL) {
		if(talk->hndl->killconn != NULL)
			(*talk->hndl->killconn) (conn, force);		/* tell it to take the
													 * connection down. */
		if (force) 
			conn->state = 0;
	} else
		conn->state = 0;

	if(0)printf ("\n\n*** KillConn: State %d, Force %d\n", conn->state, force);
	if (conn->state == 0) {		  /* No more activity. Clear some pointers and
								   * unlink from talk->conn->next chain. */
		if (conn->minor != 0 && isdn3_findminor (conn->minor) == conn) {
			minor2conn[conn->minor] = NULL;
		}
		if(conn->lockit > 0)
			return;
		if (conn->minor != 0 && conn->fminor != 0) {
			if (conn->fminor != conn->minor)
				minor2conn[conn->fminor] = 0;
		}
		if (talk == NULL)
			;
		else if (talk->conn == NULL)
			talk = NULL;
		else if (talk->conn == conn) {
			talk->conn = conn->next;
		} else {
			isdn3_conn nconn;

			for (nconn = talk->conn; nconn != NULL; nconn = nconn->next) {
				if (nconn->next == conn) {
					nconn->next = conn->next;
					break;
				}
			}
			if (nconn == NULL)
				conn = NULL;
		}
		if (conn == NULL && talk != NULL) {
			splx (ms);
			printf ("PANIC Conn not in talk chain\n");
			return;
		} else {
			if (talk != NULL && talk->hndl->kill != NULL)
				(*talk->hndl->kill) (talk, 0);
			if(conn != NULL) {
				int i;
				if(conn->minor != 0 && minor2conn[conn->minor] == conn) {
					minor2conn[conn->minor] = NULL;
					conn->minor = 0;
					conn->fminor = 0;
					/* minorflags[conn->minor] &= MINOR_OPEN; */
				}
				{
					int i;
					for(i=0;i<NMINOR;i++) {
						if(minor2conn[i]==conn) {
							printf("ISDN_3 DeadConn %p, at pos %d, minor %d\n",minor2conn[i],i,conn->minor);
							minor2conn[i]=NULL;
						}
					}
				}
				if(conn->id_msg != NULL)
					freemsg(conn->id_msg);
				for(i=0;i < NSCONN; i++) {
					if(conn->conns[i] != NULL)
						free(conn->conns[i]);
				}
				if(conn->p_data != NULL)
					free(conn->p_data);
				free(conn);
			}
		}
	}
	splx (ms);
}

/*
 * Watchdog timer. If a connection isn't fully established within two minutes,
 * take it down again -- it's not going anywhere and possibly costs money.
 * Probably some upper protocol handler can't establish itself.
 */
static int
timeout_conn (isdn3_conn conn)
{
	if (conn->minorstate & MS_CONN_TIMER) {
		conn->minorstate &= ~MS_CONN_TIMER;
		isdn3_killconn (conn, 0);
	}
	return 0;
}

/*
 * Delay timer. Often, a connection it established in one direction only for
 * the first second or so, which means that data sent in the other direction is
 * lost. We avoid this here by waiting a bit.
 */
static int
delay_conn (isdn3_conn conn)
{
	if (conn->minorstate & MS_CONNDELAY_TIMER) {
		conn->minorstate &= ~MS_CONNDELAY_TIMER;
		isdn3_setup_conn (conn, EST_NO_CHANGE);
	}
	return 0;
}

/*
 * Delay establishing the connection -- wait for someone else to pick it up
 * first. Example: Answering machine, backup server.
 */
static void
later_conn (isdn3_conn conn)
{
	conn->minorstate &= ~MS_DELAYING;
	conn->delay = 0;
	isdn3_setup_conn (conn, EST_NO_CHANGE);
}


/* --> isdn_3.h */
void
isdn3_repeat (isdn3_conn conn, ushort_t id, mblk_t * data)
{
	if (conn->id != 0) {
		mblk_t *mb;

		mb = allocb (32, BPRI_LO);
		if (mb != NULL) {
			m_putid (mb, IND_ERR);
			m_putsx (mb, ARG_ERRNO);
			m_puti (mb, EINTR);
			m_putdelim (mb);
			m_putid (mb, conn->id);
			if (conn->id_msg != NULL)
				linkb (mb, conn->id_msg);
			if (isdn3_at_send (conn, mb, 0) != 0)
				freemsg (mb);
		}
	}
	conn->id = id;
	conn->id_msg = data;
}

/* --> isdn_3.h */
int
Xisdn3_setup_conn (isdn3_conn conn, char established, const char *deb_file, unsigned deb_line)
{
	int err = 0;
	mblk_t *mb;

	/**
     ** State (Flags)
     ** -1 B channel (MS_BCHAN)
     ** -2 Mode setting (MS_INITPROTO)
     ** -3 Minor number (conn->minor)
     ** -4 Protocol stack set up (MINOR_PROTO)
     ** -5 Direction is known (MS_INCOMING/OUTGOING)
     ** -6 Connection state (MS_CONN_LISTEN,MS_CONN,...)
     **
     ** Actions (Prerequisites) Flags
     ** -1 Initiate setup of protocol stack (3) MINOR_INITPROTO_SENT
     ** -2 Send directional info to stack (3,4) MS_DIR_SENT
     ** -3 Set channel mode, attach channel. (2,3,4) MS_SETUP_SENT
     ** -4 Send LISTEN or CONNECTED or DISCONNECTED (1,2,3,4,5/!5) MINOR_LISTEN_SENT, MINOR_CONN_SENT
     **/

	conn->lockit++;
#ifdef REPORT
	printf ("ConnAttach %p Delay%d %d(%d/%d) %lo/%o %d %s:%d: ",
	conn, conn->delay, conn->conn_id, conn->minor, conn->fminor,
	conn->minorstate, (conn->minor > 0) ? minorflags[conn->minor]: 0,
	established, deb_file, deb_line);
#endif
	if((conn->minor > 0) && (minorflags[conn->minor] & MINOR_PROTO)) 
		conn->minorstate |= MS_PROTO;
	else
		conn->minorstate &=~ MS_PROTO;
	/*
	 * Waiting for Godot
	 */
	if (conn->delay > 0) {
		if (conn->minorstate & MS_DELAYING) {
#ifdef NEW_TIMEOUT
			untimeout (conn->later_timer);
#else
			untimeout (later_conn, conn);
#endif
		}
#ifdef NEW_TIMEOUT
		conn->later_timer =
#endif
				timeout (later_conn, conn, conn->delay * HZ);
		conn->minorstate |= MS_DELAYING;
	}
	/*
	 * Initiate setting up the protocol stack. This is done via L4.
	 */
	if ((conn->minor != 0)
			&& (conn->stack[0] != 0)
			&& !(minorflags[conn->minor] & MINOR_INITPROTO_SENT)
			&& !(conn->minorstate & MS_DETACHED)
			&& (minorflags[conn->minor] & MINOR_OPEN)
			&& (conn->minorstate & MS_WANTCONN)
			&& canput (isdn3_q->q_next)) {
		if ((mb = allocb (128, BPRI_HI)) == NULL) {
			err = -ENOMEM;
			goto exitme;
		}
		m_putid (mb, (minorflags[conn->minor] & MINOR_INITPROTO_SENT2) ? IND_PROTO_AGAIN : IND_PROTO);
		conn_info (conn, mb);
		putnext (isdn3_q, mb);
		minorflags[conn->minor] |= MINOR_INITPROTO_SENT | MINOR_INITPROTO_SENT2;
	}
#ifdef REPORT
	else if (!(minorflags[conn->minor] & MINOR_INITPROTO_SENT)) {
		printf ("-InitProto: ");
		if (conn->minor == 0)
			printf ("Minor zero; ");
		else if (!(minorflags[conn->minor] & MINOR_OPEN))
			printf ("Minor not open; ");
		if (!(conn->minorstate & MS_WANTCONN))
			printf ("WantConn not set; ");
		if (conn->stack[0] == 0)
			printf ("Stack zero; ");
		if (conn->minorstate & MS_DETACHED)
			printf ("Detached; ");
	}
#endif

	/*
	 * Initiate setting up the card stack. This is also done via L4.
	 */
	if ((conn->minor != 0)
			&& ((conn->stack[0] != 0) || (minorflags[conn->minor] & MINOR_INITPROTO_SENT))
			&& !(conn->minorstate & (MS_INITPROTO_SENT | MS_DETACHED))
			&& (minorflags[conn->minor] & MINOR_OPEN)
			&& (conn->minorstate & MS_WANTCONN)
			&& canput (isdn3_q->q_next)) {
		if ((mb = allocb (128, BPRI_HI)) == NULL) {
			err = -ENOMEM;
			goto exitme;
		}
		m_putid (mb, IND_CARDPROTO);
		conn_info (conn, mb);
		putnext (isdn3_q, mb);
		conn->minorstate |= MS_INITPROTO_SENT;
	}
#ifdef REPORT
	else if (!(conn->minorstate & MS_INITPROTO_SENT)) {
		printf ("-InitCard: ");
		if (conn->minor == 0)
			printf ("Minor zero; ");
		else if (!(minorflags[conn->minor] & MINOR_OPEN))
			printf ("Minor not open; ");
		if (!(conn->minorstate & MS_WANTCONN))
			printf ("WantConn not set; ");
		if((conn->stack[0] == 0) && !(minorflags[conn->minor] & MINOR_INITPROTO_SENT))
			printf ("Stack zero; ");
		if (conn->minorstate & MS_DETACHED)
			printf ("Detached; ");
		if (conn->minorstate & MS_INITPROTO_SENT)
			printf ("DidInitProto; ");
	}
#endif

	/* If detached and connecting, clear detached flag */
	if (established > EST_NO_CHANGE)
		conn->minorstate &= ~MS_DETACHED;

	/*
	 * Attach device to B channel.
	 */
	if ((conn->minor != 0)
			&& (conn->minorstate & MS_BCHAN)
			&& (minorflags[conn->minor] & MINOR_PROTO)
			&& (conn->minorstate & MS_INITPROTO)
			&& !(conn->minorstate & (MS_NOMINOR | MS_DETACHED))
			&& ( /* (conn->minorstate & MS_OUTGOING) || */ (established >= EST_LISTEN))
			&& (established > EST_NO_CHANGE || !(conn->minorstate & MS_SETUP_SENT))) {
		isdn23_hdr hdr;
		mb = allocb (sizeof (struct _isdn23_hdr), BPRI_MED);

		if (mb == NULL) {
			err = -EAGAIN;
			goto exitme;
		}
		hdr = ((isdn23_hdr) mb->b_wptr)++;
#ifdef __CHECKER__
		memset(hdr,0,sizeof (*hdr));
#endif
		hdr->key = HDR_ATTACH;
		hdr->seqnum = hdrseq; hdrseq += 2;
		hdr->hdr_attach.minor = conn->minor;
		hdr->hdr_attach.card = conn->card->nr;
		hdr->hdr_attach.chan = conn->bchan;
		hdr->hdr_attach.mode = conn->mode;
		hdr->hdr_attach.listen = ((established == EST_LISTEN) | (((conn->minorstate & MS_FORCING) != 0) << 1));
		if ((err = isdn3_sendhdr (mb)) != 0) {
			freeb (mb);
			goto exitme;
		}
		conn->minorstate |= MS_SETUP_SENT;
		conn->minorstate &=~ MS_DETACHED;
	}
#ifdef REPORT
	else if (!(conn->minorstate & MS_SETUP_SENT)) {
		printf ("-SentAttach: ");
		if (conn->minor == 0)
			printf ("Minor zero; ");
#if 0
		if ((conn->minorstate & MS_INCOMING) && (established == EST_LISTEN)) 
			printf ("Incoming Listen; ");
#else
		if ((established == EST_LISTEN)) 
			printf ("Listen; ");
		else if(established < EST_LISTEN)
			printf ("<Listen; ");
#endif
		if (conn->minorstate & MS_DETACHED)
			printf("Detached; ");
		if (conn->minorstate & MS_NOMINOR)
			printf("NoMinor; ");
		if (!(conn->minorstate & MS_BCHAN))
			printf ("No B Channel; ");
		if (!(conn->minorstate & MS_INITPROTO))
			printf ("Card mode not set; ");
		if (!(minorflags[conn->minor] & MINOR_PROTO))
			printf ("Protocol stack not set; ");
	}
#endif

	/*
	 * Send directional information, setup info to protocol stack.
	 */
	if ((conn->minorstate & (MS_INCOMING | MS_OUTGOING))
			&& (minorflags[conn->minor] & MINOR_PROTO)
			&& !(conn->minorstate & MS_DIR_SENT)) {
		mb = allocb (3, BPRI_HI);
		if (mb == NULL) {
			err = -ENOMEM;
			goto exitme;
		}
		m_putid(mb,(conn->minorstate & MS_INCOMING) ? PROTO_INCOMING : PROTO_OUTGOING);
		if ((err = isdn3_send_conn (conn->minor, AS_PROTO, mb)) == 0)
			conn->minorstate |= MS_DIR_SENT;
		else
			return err;
		mb = allocb (10,BPRI_HI);
		if (mb == NULL) {
			err = -ENOMEM;
			goto exitme;
		}
		m_putid(mb,PROTO_OFFSET);
		m_puti(mb,0);
		if ((err = isdn3_send_conn (conn->minor, AS_PROTO, mb)) != 0)
			goto exitme;
	}
#ifdef REPORT
	else if (!(conn->minorstate & MS_DIR_SENT)) {
		printf ("-SentDir: ");
		if (conn->minor == 0)
			printf ("Minor zero; ");
		if (!(conn->minorstate & (MS_INCOMING | MS_OUTGOING)))
			printf ("Unknown; ");
		if (!(minorflags[conn->minor] & MINOR_PROTO))
			printf ("Protocol stack not set; ");
	}
#endif

	/*
	 * Figure out what the intended state is, and setup the timeout.
	 */
	switch (established) {
	case EST_WILL_INTERRUPT:
	case EST_WILL_DISCONNECT:
		if ((conn->minor != 0) && (minorflags[conn->minor] & MINOR_PROTO)) {
			mb = allocb (3, BPRI_HI);
			if (mb == NULL)
				err = -ENOMEM;
			else {
				m_putid (mb, (established == EST_WILL_INTERRUPT) ? PROTO_WILL_INTERRUPT : PROTO_WILL_DISCONNECT);
				if ((err = isdn3_send_conn (conn->minor, AS_PROTO, mb)) != 0)
					freemsg (mb);
			}
			break;
		} else
			established = EST_DISCONNECT;
	case EST_DISCONNECT:
		conn->minorstate &= ~MS_CONN_MASK;
		conn->minorstate |= MS_CONN_NONE;
		if (conn->minorstate & MS_CONN_TIMER) {
			conn->minorstate &= ~MS_CONN_TIMER;
#ifdef NEW_TIMEOUT
			untimeout (conn->start_timer);
#else
			untimeout (timeout_conn, conn);
#endif
		}
		if (conn->minorstate & MS_CONNDELAY_TIMER) {
			conn->minorstate &= ~MS_CONNDELAY_TIMER;
#ifdef NEW_TIMEOUT
			untimeout (conn->delay_timer);
#else
			untimeout (delay_conn, conn);
#endif
		}
		break;
	case EST_INTERRUPT:
		conn->minorstate &= ~MS_CONN_MASK;
		conn->minorstate |= MS_CONN_INTERRUPT;
		if (conn->minorstate & MS_CONN_TIMER) {
			conn->minorstate &= ~MS_CONN_TIMER;
#ifdef NEW_TIMEOUT
			untimeout (conn->start_timer);
#else
			untimeout (timeout_conn, conn);
#endif
		}
		if (conn->minorstate & MS_CONNDELAY_TIMER) {
			conn->minorstate &= ~MS_CONNDELAY_TIMER;
#ifdef NEW_TIMEOUT
			untimeout (conn->delay_timer);
#else
			untimeout (delay_conn, conn);
#endif
		}
		break;
	case EST_LISTEN:
		conn->minorstate &= ~MS_CONN_MASK;
		conn->minorstate |= MS_CONN_LISTEN;
		if (!(conn->minorstate & MS_CONN_TIMER)) {
			conn->minorstate |= MS_CONN_TIMER;
#ifdef NEW_TIMEOUT
			conn->start_timer =
#endif
					timeout (timeout_conn, conn, HZ * 120);
		}
		break;
	case EST_CONNECT:
		{
			long flags = isdn3_flags(conn->card->info,-1,-1);
			if((flags & FL_UPDELAY)
				&& ((conn->minorstate & MS_CONN_MASK) != MS_CONN)
				&& !(conn->minorstate & MS_CONNDELAY_TIMER))
			{
				conn->minorstate |= MS_CONNDELAY_TIMER;
#ifdef NEW_TIMEOUT
				conn->delay_timer =
#endif
						timeout (delay_conn, conn, (HZ * (flags & FL_UPDELAY)) >> FL_UPDELAY_SHIFT);
			}
		}
		conn->minorstate &= ~MS_CONN_MASK;
		conn->minorstate |= MS_CONN;
		if ((minorflags[conn->minor] & MINOR_STATE_MASK) != MINOR_CONN_SENT) {
			if (!(conn->minorstate & MS_CONN_TIMER)) {
				conn->minorstate |= MS_CONN_TIMER;
#ifdef NEW_TIMEOUT
				conn->start_timer =
#endif
						timeout (timeout_conn, conn, HZ * 120);
			}
		}
		break;
	}
	/*
	 * If the protocol stack is set up, tell it about the change.
	 */
	if ((conn->minor != 0) && (minorflags[conn->minor] & MINOR_PROTO)) {
		switch (conn->minorstate & MS_CONN_MASK) {
		case MS_CONN_NONE:
			if ((minorflags[conn->minor] & MINOR_STATE_MASK) != MINOR_STATE_NONE) {
				mb = allocb (3, BPRI_HI);
				if (mb == NULL)
					err = -ENOMEM;
				else {
					m_putid (mb, PROTO_DISCONNECT);
					if ((err = isdn3_send_conn (conn->minor, AS_PROTO, mb)) == 0) {
						minorflags[conn->minor] &= ~MINOR_STATE_MASK;
						minorflags[conn->minor] |= MINOR_STATE_NONE;
					} else
						freeb (mb);
				}
			}
			break;
		case MS_CONN_INTERRUPT:
			if ((minorflags[conn->minor] & MINOR_STATE_MASK) != MINOR_INTERRUPT_SENT) {
				mb = allocb (3, BPRI_HI);
				if (mb == NULL)
					err = -ENOMEM;
				else {
					m_putid (mb, PROTO_INTERRUPT);
					if ((err = isdn3_send_conn (conn->minor, AS_PROTO, mb)) == 0) {
						minorflags[conn->minor] &= ~MINOR_STATE_MASK;
						minorflags[conn->minor] |= MINOR_INTERRUPT_SENT;
					} else
						freeb (mb);
				}
			}
			break;
		case MS_CONN_LISTEN:
			if ((minorflags[conn->minor] & MINOR_STATE_MASK) != MINOR_LISTEN_SENT) {
				mb = allocb (3, BPRI_HI);
				if (mb == NULL)
					err = -ENOMEM;
				else {
					m_putid (mb, PROTO_LISTEN);
					if ((err = isdn3_send_conn (conn->minor, AS_PROTO, mb)) == 0) {
						minorflags[conn->minor] &= ~MINOR_STATE_MASK;
						minorflags[conn->minor] |= MINOR_LISTEN_SENT;
					} else
						freeb (mb);
				}
			}
			break;
		case MS_CONN:
			if (((minorflags[conn->minor] & MINOR_STATE_MASK) != MINOR_CONN_SENT) && !(conn->minorstate & MS_CONNDELAY_TIMER)) {
				mb = allocb (3, BPRI_HI);
				if (mb == NULL)
					err = -ENOMEM;
				else {
					m_putid (mb, PROTO_CONNECTED);
					if ((err = isdn3_send_conn (conn->minor, AS_PROTO, mb)) == 0) {
						minorflags[conn->minor] &= ~MINOR_STATE_MASK;
						minorflags[conn->minor] |= MINOR_CONN_SENT;
						/* Connected. Kill the timer. */
						if (conn->minorstate & MS_CONN_TIMER) {
							conn->minorstate &= ~MS_CONN_TIMER;
#ifdef NEW_TIMEOUT
							untimeout (conn->start_timer);
#else
							untimeout (timeout_conn, conn);
#endif
						}
					} else
						freeb (mb);
				}
			}
			break;
		}
	}
	/*
	 * If taking down the connection, detach it.
	 */
	if ((established < EST_NO_REAL_CHANGE)
			&& ((conn->minorstate & MS_SETUP_SENT)
			 /* || (minorflags[conn->minor] & (MINOR_PROTO | MINOR_INITPROTO_SENT)) */ )) {
		mblk_t *mp;
		isdn23_hdr hdr;

		printf ("Disconnect %d.",established);
		/* The connection isn't in the process of being established any more. */
		if (conn->minorstate & MS_CONNDELAY_TIMER) {
			conn->minorstate &= ~MS_CONNDELAY_TIMER;
#ifdef NEW_TIMEOUT
			untimeout (conn->delay_timer);
#else
			untimeout (delay_conn, conn);
#endif
		}
		if (conn->minorstate & MS_CONN_TIMER) {
			conn->minorstate &= ~MS_CONN_TIMER;
#if NEW_TIMEOUT
			untimeout (conn->start_timer);
#else
			untimeout (timeout_conn, conn);
#endif
		}
		if ((conn->minorstate && MS_SETUP_SENT) && !(conn->minorstate & (MS_NOMINOR | MS_DETACHED))
			   && (minor2conn[conn->minor] == conn || minor2conn[conn->minor] == NULL)
			   && ((mp = allocb (sizeof (struct _isdn23_hdr), BPRI_MED)) != NULL)) {
			hdr = ((isdn23_hdr) mp->b_wptr)++;
#ifdef __CHECKER__
			memset(hdr,0,sizeof (*hdr));
#endif
			hdr->key = HDR_DETACH;
			hdr->seqnum = hdrseq; hdrseq += 2;
			hdr->hdr_detach.minor = conn->minor;
			hdr->hdr_detach.error = 0;
			hdr->hdr_detach.perm = 0;
			if ((err = isdn3_sendhdr (mp)) != 0) {
				freeb (mp);
			}
		}
		conn->minorstate &= ~MS_SETUP_SENT;
		conn->minorstate |= MS_DETACHED;
		if ((established < EST_INTERRUPT) && !(minorflags[conn->minor] & MINOR_INT)) {
			minorflags[conn->minor] &= ~MINOR_INITPROTO_SENT;
		}
		conn->minorstate &=~ MS_INITPROTO;
#ifdef REPORT
		printf (" SentDetach %d ",conn->minor);
#endif
	}
#ifdef REPORT
	else {
		printf (" -Detach: ");
		if (established >= EST_NO_REAL_CHANGE)
			printf ("Establ %d ", established);
		if (minor2conn[conn->minor] != conn && minor2conn[conn->minor] != NULL)
			printf ("conn %d ", conn->minor);
		if (!(conn->minorstate & MS_SETUP_SENT))
			printf ("MS %lx ", conn->minorstate);
		if (!(minorflags[conn->minor] & (MINOR_PROTO | MINOR_INITPROTO_SENT)))
			printf ("MF %x ", minorflags[conn->minor]);
	}
	printf ("\n");
#endif
	if (conn->talk == NULL)
		goto exitme;
	if (conn->id != 0) {
		ushort_t id = conn->id;
		mblk_t *msg = conn->id_msg;

		conn->id = 0;
		conn->id_msg = NULL;
		(*conn->talk->hndl->sendcmd) (conn, id, msg);
	}
	if (conn->talk->hndl->hook != NULL) {
			(*conn->talk->hndl->hook) (conn);
	}
  exitme:
	conn->lockit--;
	if (conn->lockit == 0 && conn->state==0)
		isdn3_killconn(conn,0);
	return err;
}

/* --> isdn_3.h */
isdn3_conn
isdn3_new_conn (isdn3_talk talk)
{
	isdn3_conn conn;
	int ms;
	static int conn_id = 1;

	if (talk == NULL) {
		printf ("PANIC: isdn3_new_conn(talk NULL)\n");
		return NULL;
	}
	conn = malloc(sizeof(*conn));
	if(conn == NULL)
		return NULL;
	memset(conn,0,sizeof (*conn));

	ms = splstr();
	conn->conn_id = conn_id; conn_id += 2;
	conn->card = talk->card;
	conn->next = talk->conn;
	conn->talk = talk;
	talk->conn = conn;
	splx (ms);
	return conn;
}

/* --> isdn_3.h */
int
isdn3_new_callref (isdn3_talk talk)
{
	isdn3_conn conn;
	long cref = 1;

	do {
		cref++;
		for (conn = talk->conn; conn != NULL; conn = conn->next)
			if (cref == conn->call_ref)
				break;
	} while (conn != NULL);
	return cref;
}

/* --> isdn_3.h */
void
isdn3_killtalk (isdn3_talk talk)
{
	int ms = splstr ();
	isdn3_card card = talk->card;
	int i;

	(void) isdn3_chstate (talk, 0, 0, CH_CLOSEPROT);

	/* Unlink from its card's talk chain */
	if (card == NULL || card->talk == NULL)
		talk = NULL;
	else if (card->talk == talk) {
		card->talk = talk->next;
	} else {
		isdn3_talk ntalk;

		for (ntalk = card->talk; ntalk != NULL; ntalk = ntalk->next) {
			if (ntalk->next == talk) {
				ntalk->next = talk->next;
				break;
			}
		}
		if (ntalk == NULL)
			talk = NULL;
	}
	if (talk == NULL) {
		splx (ms);
		printf ("PANIC Talk not in card chain\n");
		return;
	}
	/* Force disconnect and removal of all connections. */
	while (talk->conn != NULL)
		isdn3_killconn (talk->conn, 1);
	if (talk->hndl->kill != NULL)
		(*talk->hndl->kill) (talk, 1);

	for(i=0;i < NSTALK; i++) {
		if(talk->talks[i] != NULL)
			free(talk->talks[i]);
	}
	free(talk);
	splx (ms);
}

/* --> isdn_3.h */
int
isdn3_sendhdr (mblk_t * mb)
{
	if (isdn3_q != NULL) {
		putnext (WR (isdn3_q), mb);
		return 0;
	} else
		return -ENXIO;
}

/* --> isdn_3.h */
int
isdn3_send_conn (SUBDEV minor, char what, mblk_t * data)
{
	mblk_t *mb;
	isdn23_hdr hdr;
	int err = 0;

	if (minor == 0 || minor >= NMINOR) {
printf("ErX l\n");
		return -EINVAL;
	}
	mb = allocb (sizeof (struct _isdn23_hdr), BPRI_MED);

	if (mb == NULL)
		return -ENOMEM;
	hdr = ((isdn23_hdr) mb->b_wptr)++;
#ifdef __CHECKER__
	memset(hdr,0,sizeof (*hdr));
#endif
	switch (what) {
	case AS_XDATA:
		hdr->key = HDR_XDATA;
		hdr->seqnum = hdrseq; hdrseq += 2;
		hdr->hdr_xdata.minor = minor;
		hdr->hdr_xdata.len = dsize (data);
		break;
	case AS_PROTO:
		hdr->key = HDR_PROTOCMD;
		goto cont_proto;
	case AS_PROTO_NOERR:
		hdr->key = HDR_PROTOCMD|HDR_NOERROR;
	  cont_proto:
		hdr->seqnum = hdrseq; hdrseq += 2;
		hdr->hdr_protocmd.minor = minor;
		hdr->hdr_protocmd.len = dsize (data);
		break;
	default:
		freeb (mb);
printf("ErX b\n");
		return -EINVAL;
	}
	linkb (mb, data);
	if ((err = isdn3_sendhdr (mb)) != 0)
		freeb (mb);
	return err;
}

/* --> isdn_3.h */
int
isdn3_send (isdn3_talk talk, char what, mblk_t * data)
{
	mblk_t *mb;
	isdn23_hdr hdr;
	int err = 0;

	mb = allocb (sizeof (struct _isdn23_hdr), BPRI_MED);

	if (mb == NULL)
		return -ENOMEM;
	hdr = ((isdn23_hdr) mb->b_wptr)++;
#ifdef __CHECKER__
	memset(hdr,0,sizeof (*hdr));
#endif
	switch (what) {
	case AS_UIDATA:
	case AS_UIBROADCAST:
		hdr->key = HDR_UIDATA;
		hdr->seqnum = hdrseq; hdrseq += 2;
		hdr->hdr_uidata.card = talk->card->nr;
		hdr->hdr_uidata.SAPI = talk->hndl->SAPI;
		hdr->hdr_uidata.broadcast = (what == AS_UIBROADCAST);
		hdr->hdr_uidata.len = dsize (data);
		break;
	case AS_DATA:
		hdr->key = HDR_DATA;
		hdr->seqnum = hdrseq; hdrseq += 2;
		hdr->hdr_data.card = talk->card->nr;
		hdr->hdr_data.SAPI = talk->hndl->SAPI;
		hdr->hdr_data.len = dsize (data);
		break;
	default:
		freeb (mb);
printf("ErX c\n");
		return -EINVAL;
	}
	linkb (mb, data);
	if ((err = isdn3_sendhdr (mb)) != 0)
		freeb (mb);
	return err;
}

/* --> isdn_3.h */
int
isdn3_at_send (isdn3_conn conn, mblk_t * data, char bypass)
{
	mblk_t *mb;
	isdn23_hdr hdr;
	int err = 0;
	SUBDEV minor = (conn != NULL && conn->minor != 0) ? conn->fminor : 0;

	/*
	 * Currently does not call canput(). I assume that failure of isdn3_at_send
	 * is too disrupting, compared to an occasional overly-full read queue, to
	 * warrant differing action.
	 */

	if (minor == 0) {
		putnext (isdn3_q, data);
		return 0;
	}
	if (!bypass) {
		mblk_t *mm = allocb (32, BPRI_MED);
		ushort_t ind;

		if (mm == NULL)
			return -ENOMEM;
#if 0
		m_putid (mm, IND_ATRESP);
		m_putsx (mm, ARG_FMINOR);
		m_puti (mm, minor);
		m_putdelim (mm);
#else
		m_getid(data,&ind);
		if(ind == IND_INFO) {
			m_putid(mm,ind);
			m_getid(data,&ind);
		}
		m_putid(mm,ind);
		m_putsx (mm, ARG_FMINOR);
		m_puti (mm, minor);
		if(ind == PROTO_AT)
			m_putdelim(mm);
#endif
		linkb (mm, data);
		putnext (isdn3_q, mm);
		return 0;
	}
	mb = allocb (sizeof (struct _isdn23_hdr), BPRI_MED);

	if (mb == NULL)
		return -EAGAIN;
	hdr = ((isdn23_hdr) mb->b_wptr)++;
#ifdef __CHECKER__
	memset(hdr,0,sizeof (*hdr));
#endif
	hdr->key = HDR_ATCMD;
	hdr->seqnum = hdrseq; hdrseq += 2;
	hdr->hdr_atcmd.minor = minor;
	hdr->hdr_atcmd.len = dsize (data);
	linkb (mb, data);
	if ((err = isdn3_sendhdr (mb)) != 0)
		freeb (mb);
	return err;
}

/* --> isdn_3.h */
int
isdn3_chstate (isdn3_talk talk, uchar_t ind, short add, char what)
{
	mblk_t *mb;
	isdn23_hdr hdr;
	int err = 0;

	mb = allocb (sizeof (struct _isdn23_hdr), BPRI_MED);

	if (mb == NULL)
		return -EAGAIN;
	hdr = ((isdn23_hdr) mb->b_wptr)++;
#ifdef __CHECKER__
	memset(hdr,0,sizeof (*hdr));
#endif
	switch (what) {
	case CH_MSG:
		hdr->key = HDR_NOTIFY;
		hdr->seqnum = hdrseq; hdrseq += 2;
		hdr->hdr_notify.card = talk->card->nr;
		hdr->hdr_notify.SAPI = talk->hndl->SAPI;
		hdr->hdr_notify.ind = ind;
		hdr->hdr_notify.add = add;
		break;
	case CH_OPENPROT:
		hdr->key = HDR_OPENPROT;
		hdr->seqnum = hdrseq; hdrseq += 2;
		hdr->hdr_openprot.card = talk->card->nr;
		hdr->hdr_openprot.SAPI = talk->hndl->SAPI;
		hdr->hdr_openprot.ind = ind;
		hdr->hdr_openprot.broadcast = talk->hndl->broadcast;
		break;
	case CH_CLOSEPROT:
		hdr->key = HDR_CLOSEPROT|HDR_NOERROR;
		hdr->seqnum = hdrseq; hdrseq += 2;
		hdr->hdr_closeprot.card = talk->card->nr;
		hdr->hdr_closeprot.SAPI = talk->hndl->SAPI;
		hdr->hdr_closeprot.ind = ind;
		break;
	default:
		freeb (mb);
printf("ErX d\n");
		return -EINVAL;
	}
	if ((err = isdn3_sendhdr (mb)) != 0)
		freeb (mb);
	return err;
}


/* --> isdn_3.h */
uchar_t
isdn3_free_b (isdn3_card card)
{
	isdn3_talk talk;
	isdn3_conn conn;
	uchar_t bc;

	for (bc = 1; bc <= card->bchans; bc++) {
		for (talk = card->talk; talk != NULL; talk = talk->next) {
			for (conn = talk->conn; conn != NULL; conn = conn->next) {
				if (conn->bchan == bc) {
					bc = 0;
					break;
				}
			}
		}
		if (bc > 0)
			return bc;
	}
	return 0;
}

/*
 * Scans and interprets a command string. Unknown commands are redirected to
 * the handler, if one can be inferred from the arguments.
 * 
 * Warning: This procedure is big and very ugly.
 */
static int
scan_at (SUBDEV fminor, mblk_t * mx)
{
	isdn3_conn conn = NULL;
	isdn3_card card = NULL;
	isdn3_talk talk = NULL;
	isdn3_hndl hndl = NULL;
	isdn23_hdr hdr;
	SUBDEV minor = 0;
	signed char chan = -1;
	long subprotocol = -1;
	long call_ref = 0;
	long modes = -1;
	char stack[STACK_LEN] = "";
	long delay = 0;
	char force = 0;
	int conn_id = 0;
	int seqnum = -1;
	int foffset = -1;
	char do_int = 0;
	char noconn = 0;
	char nodisc = 0;
	char do_talk = 0;
	char no_error = 0;

	ushort_t theID, id;
	int err = 0;
	streamchar *oldpos;
	streamchar *origmx;

	mx = pullupm (mx, -1);
	if (mx == NULL)
		return -ENOMEM;			  /* We have not clobbered the original
								   * argument! */

	/*
	 * ... well, we are doing so now. Do not return anything other than zero
	 * after this point, please.
	 */
	origmx = mx->b_rptr;
	while(mx->b_rptr < mx->b_wptr) {
		switch(*mx->b_rptr) {
		case PREF_NOERR:
			no_error = 1;
			mx->b_rptr++;
			break;
		default:
			goto contpref;
		}
	}
  contpref:
	if ((err = m_getid (mx, &theID)) != 0)
		goto err_out;
	oldpos = mx->b_rptr;
	while ((err = m_getsx (mx, &id)) == 0) {
		switch (id) {
#if 0
		case ARG_EXPAND:
			{		  /* Expand something. Punt to L4. */
				mblk_t *mm;

				if (!canput (isdn3_q->q_next))
					goto err_out;
				if ((mm = allocb (32, BPRI_MED)) == NULL)
					goto err_out;
				m_putid (mm, IND_ATCMD);
				m_putsx (mm, ARG_FMINOR);
				m_puti (mm, fminor);
				m_putdelim (mm);
				mx->b_rptr = origmx;
				linkb (mm, mx);
				putnext (isdn3_q, mm);
				mx = NULL;
				goto ok_out;
			}
			break;
#endif
		case ARG_FMINOR:
			{
				long nm;

				if (fminor != 0) {
					err = -EPERM;
					goto err_out;
				}
				if ((err = m_geti (mx, &nm)) != 0)
					goto err_out;
				if (nm <= 0 || nm >= NMINOR) {
					err = -ENXIO;
					goto err_out;
				}
				if (conn != NULL && conn->minor != 0 && conn->fminor != 0 && conn->fminor != nm) {
printf("ErX e\n");
					err = -EINVAL;
					goto err_out;
				}
				fminor = nm;
			}
			break;
		case ARG_CONNREF:
			{
				long nm;

				if ((err = m_geti (mx, &nm)) != 0)
					goto err_out;
				conn_id = nm;
			}
			break;
		case ARG_SEQNUM:
			{
				long nm;

				if ((err = m_geti (mx, &nm)) != 0)
					goto err_out;
				seqnum = nm;
			}
			break;
		case ARG_OFFSET:
			{
				long nm;

				if ((err = m_geti (mx, &nm)) != 0)
					goto err_out;
				foffset = nm;
			}
			break;
		case ARG_CARD:
			{
				ulong_t nm;

				if ((err = m_getlx (mx, &nm)) != 0)
					goto err_out;
				card = isdn3_findcardid(nm);
				if (card == NULL) {
					err = -ENXIO;
					goto err_out;
				}
				if (conn != NULL && conn->card != NULL && conn->card != card) {
printf("ErX f\n");
					err = -EINVAL;
					goto err_out;
				}
				if (card->info != NULL && subprotocol == -1) {
					mblk_t *mi = card->info;
					streamchar *sta = mi->b_rptr;
					ushort_t idx;

					while(m_getid(mi,&idx) == 0) {
						long sap;
						if(idx == ARG_PROTOCOL && m_geti(mi,&sap) == 0) {
							if ((hndl = isdn3_findhndl (sap)) == NULL)
								break;
							if (subprotocol == -1 && m_getid(mi,&idx) == 0
								&& idx == ARG_SUBPROT && m_geti(mi,&sap) == 0) {
								subprotocol = sap;
printf(" Grab SP %ld; ",subprotocol);
								break;
							}
						}
					}
					mi->b_rptr = sta;
				}
			}
			break;
		case ARG_STACK:
			{
				if ((err = m_getstr (mx, stack, sizeof(stack)-1)) != 0)
					goto err_out;
			}
			break;
		case ARG_CHANNEL:
			{
				long nm;

				if ((err = m_geti (mx, &nm)) != 0)
					goto err_out;
				if (nm < 0) {
printf("ErX g\n");
					err = -EINVAL;
					goto err_out;
				}
				if (card != NULL && nm > card->bchans) {
					err = -ENXIO;
					goto err_out;
				}
				chan = nm;
			}
			break;
		case ARG_DELAY:
			if ((err = m_geti (mx, &delay)) != 0)
				goto err_out;
			break;
		case ARG_CALLREF:
			if ((err = m_geti (mx, &call_ref)) != 0)
				goto err_out;
			break;
		case ARG_INT:
			do_int = 1;
			break;
		case ARG_NOINT:
			do_int = -1;
			break;
		case ARG_FORCETALK:
			do_talk = 1;
			break;
		case ARG_NODISC:
			nodisc = 1;
			break;
		case ARG_NOCONN:
			noconn = 1;
			break;
		case ARG_MINOR:
			{
				long nm;

				if ((err = m_geti (mx, &nm)) != 0)
					goto err_out;
				if (nm <= 0 || nm >= NMINOR) {
					err = -ENXIO;
					goto err_out;
				}
				if (conn != NULL && conn->minor != 0 && conn->minor != nm) {
printf("ErX i\n");
					err = -EINVAL;
					goto err_out;
				}
				minor = nm;
			}
			break;
		case ARG_ERRNO:
			{
				long nm;

				if ((err = m_geti (mx, &nm)) != 0)
					goto err_out;
				err = nm;
			}
			break;
#if 0
		case ARG_LISTWHAT:
			{
				long nm;

				if ((err = m_getid (mx, &nm)) != 0)
					goto err_out;
				what = nm;
			}
			break;
#endif
		case ARG_PROTOCOL:
			{
				long nm;

				if ((err = m_geti (mx, &nm)) != 0)
					goto err_out;
				if ((hndl = isdn3_findhndl (nm)) == NULL) {
printf("ErX j\n");
					err = -EINVAL;
					goto err_out;
				}
				if(nm >= 64 && subprotocol == -1) {
					subprotocol = 0;
printf(" Faked SP %ld; ",subprotocol);
				}
			}
			break;
		case ARG_FORCE:
			force = 1;
			break;
		case ARG_SUBPROT:
			if ((err = m_geti (mx, &subprotocol)) != 0)
				goto err_out;
printf(" Got SP %ld; ",subprotocol);
			break;
		default:;
		}
	}

	/*** Inferring missing arguents ***/
	/* mx->b_rptr = oldpos; ** do not do this here */

	/* Find an appropriate card. */
	if (card == NULL && hndl != NULL) {
		if (subprotocol == -1) {
			err = -ENXIO;
			printf ("ErrOut 3\n");
			goto err_out;
		}
		for (card = isdn_card; card != NULL; card = card->next) {
			if ((modes & card->modes) == 0)
				continue;
			if (isdn3_free_b (card) != 0)
				break;
		}
		if (card == NULL) {
			err = -EBUSY;
			printf ("ErrOut 4\n");
			goto err_out;
		}
		if ((talk = isdn3_findtalk (card, hndl, mx, do_talk)) == NULL) {
			err = -ENOMEM;
			printf ("ErrOut 5\n");
			goto err_out;
		}
		modes &= card->modes;
	}

	/* If we have card and handler, get the talker. */
	if (card != NULL && hndl != NULL && talk == NULL)
		talk = isdn3_findtalk (card, hndl, mx, do_talk);

	/* Infer a connection, first try */
	if (conn == NULL && conn_id != 0 && !noconn) {
		if (talk != NULL) {
			for (conn = talk->conn; conn != NULL; conn = conn->next) {
				if (conn->conn_id == conn_id)
					break;
			}
		} else if (card != NULL) {
			for (talk = card->talk; talk != NULL; talk = talk->next) {
				for (conn = talk->conn; conn != NULL; conn = conn->next) {
					if (conn->conn_id == conn_id)
						goto found_cid;
				}
			}
		}
	}
  found_cid:;
	if (conn == NULL && call_ref != 0 && !noconn) {
		if (talk != NULL) {
			for (conn = talk->conn; conn != NULL; conn = conn->next) {
				if (conn->call_ref == call_ref)
					break;
			}
		} else if (card != NULL) {
			for (talk = card->talk; talk != NULL; talk = talk->next) {
				for (conn = talk->conn; conn != NULL; conn = conn->next) {
					if (conn->call_ref == call_ref)
						goto found_ref;
				}
			}
		}
	}
  found_ref:;

	if (conn == NULL && minor != 0 && (minorflags[minor] & (MINOR_OPEN|MINOR_WILL_OPEN)) && !noconn) {
		printf("! Find: minor %d",minor);
		if(minor > 0) 
			printf(": Flags %o, 2conn %p",minorflags[minor], minor2conn[minor]);
		printf("\n");
		conn = minor2conn[minor];
	}
	if (conn == NULL && fminor != 0 && (minorflags[fminor] & (MINOR_OPEN | MINOR_WILL_OPEN)) && !noconn)
		conn = minor2conn[fminor];

	if(fminor == 0)
		fminor = minor;
	if(conn != NULL && minor == 0)
		minor = conn->minor;
	if(minor == 0)
		minor = fminor;

	if (conn != NULL) {
		if (conn->card != NULL)
			card = conn->card;
		if (conn->talk != NULL) {
			talk = conn->talk;
			hndl = talk->hndl;
		}
	}
	/* If we have card and handler, get the talker. */
	if (card != NULL && hndl != NULL && talk == NULL)
		talk = isdn3_findtalk (card, hndl, mx, do_talk);

	/* Setup variables if we have a connection. */
	if (conn != NULL) {
		if (conn->conn_id != 0 && conn_id != 0 && conn->conn_id != conn_id) {
			err = -EINVAL;
			printf ("ErrOut 6\n");
			goto err_out;
		}
		if (conn->call_ref != 0 && call_ref != 0 && conn->call_ref != call_ref) {
			err = -EINVAL;
			printf ("ErrOut 6a\n");
			goto err_out;
		}
		if (chan == -1)
			chan = conn->bchan;
		if (card == NULL)
			card = conn->card;
		if (talk == NULL)
			talk = conn->talk;

		if (fminor != 0 && conn->minor != 0) {
			if (fminor != minor) {
				if (conn->fminor != 0 && conn->fminor != fminor) {
					err = -EPERM;
					printf ("ErrOut 1 at %d; min %d nmin %d\n", 
					conn->fminor, fminor, minor);
					goto err_out;
				} else if (conn->fminor != fminor) {
					conn->fminor = fminor;
					/* minor2conn[fminor] = minor2conn[minor]; */
				}
			}
		} else if (minor == 0 || !(minorflags[minor] & (MINOR_OPEN | MINOR_WILL_OPEN))) {
			if (minor != 0) {
				if (theID == CMD_FAKEOPEN && fminor == 0) {
					goto after_fake;
				}
				err = -EINVAL;
				printf ("ErrOut 2 %d %o\n",minor,minorflags[minor]);
				goto err_out;
			}
			if (fminor != 0 && (minorflags[fminor] & (MINOR_OPEN | MINOR_WILL_OPEN)) && minor2conn[minor] != NULL) {
				conn = minor2conn[fminor];
			}
		}
	}
  after_fake:
	if (conn != NULL) {
		if(delay > 0)
			conn->delay = delay;
		if(do_int != 0) {
			if(do_int > 0)
				minorflags[conn->minor] |= MINOR_INT;
			else
				minorflags[conn->minor] &=~ MINOR_INT;
		}
		if(call_ref != 0)
			conn->call_ref = call_ref;
		if(conn_id != 0 && conn->conn_id != conn_id) {
			if(conn->conn_id == 0) 
				conn->conn_id = conn_id;
			else {
printf ("ConnBadness%p %d(%d/%d) %lo/%o\n", conn,conn->conn_id, conn->minor, 
conn->fminor, conn->minorstate, (conn->minor > 0) ? minorflags[conn->minor]: 0);
				conn = NULL;
			}
		}
	}
	/*
	 * Now find out what to do with all of this.
	 */
	switch (theID) {
	case CMD_FAKEOPEN:
		if ((fminor != 0 && fminor != minor) || minor == 0) {
			err = -EINVAL;
			printf ("ErrOut n2 %d %d\n", fminor, minor);
			goto err_out;
		};
		if (minorflags[minor] & MINOR_OPEN)
			break;
		minorflags[minor] |= MINOR_WILL_OPEN;
		minor2conn[minor] = NULL;
		break;
	case CMD_DOCARD:
		if(card != NULL) {
			int flags = 0;
			m_getskip(mx);
			if(card->info != NULL)
				freemsg(card->info);
			if(mx->b_rptr < mx->b_wptr) 
				card->info = dupmsg(mx);
			else
				card->info = NULL;
			if(card->info != NULL) {
				mblk_t *mi = card->info;
				streamchar *sta = mi->b_rptr;
				ushort_t idx;
				ulong_t sap;
				while(m_getsx(mx,&idx) == 0) {
					if(idx == ARG_DEBUG)
						flags |= HDR_CARD_DEBUG;
					else if(idx == ARG_PROTOCOL && m_geti (mx, &sap) == 0) {
						if((hndl = isdn3_findhndl(sap)) != NULL) {
							streamchar *stb = mi->b_rptr;
							mi->b_rptr = sta;
							(*hndl->newcard) (card);
							mi->b_rptr = stb;
						}
					}
				}
			} else {
				for (hndl = isdn_hndl; hndl != NULL; hndl = hndl->next) {
					if (hndl->newcard != NULL)
						(*hndl->newcard) (card);
				}
			}
			if(flags) {
				mblk_t *mb;
				isdn23_hdr hdr;
				int err = 0;

				mb = allocb (sizeof (struct _isdn23_hdr), BPRI_MED);

				if (mb != NULL) {
					hdr = ((isdn23_hdr) mb->b_wptr)++;
#ifdef __CHECKER__
					memset(hdr,0,sizeof (*hdr));
#endif
					hdr->key = HDR_CARD;
					hdr->seqnum = hdrseq; hdrseq += 2;
					hdr->hdr_card.card = card->nr;
					hdr->hdr_card.flags = flags;
					hdr->hdr_card.id = card->id;
					hdr->hdr_card.dchans = card->dchans;
					hdr->hdr_card.bchans = card->bchans;
					hdr->hdr_card.modes = card->modes;
					if ((err = isdn3_sendhdr (mb)) != 0)
						freeb (mb);
				}
			}
		}
		break;
	case CMD_INFO:
		{
			m_getskip (mx);
			if(conn == NULL) {
printf("ErX k\n");
				err = -EINVAL;
				goto err_out;
			}
			if ((err = isdn3_at_send (conn, mx, 1)) != 0) {
				fminor = 0;
				printf ("ErrOut 7 %d\n", err);
				goto err_out;
			}
			mx = NULL;
		}
		break;
	case CMD_LOADFILE:
		{
			mblk_t *mb;
			if(card == NULL || seqnum == -1 || foffset == -1) {
				err = -EINVAL;
				goto err_out;
			}
			mb = allocb (sizeof (struct _isdn23_hdr), BPRI_MED);

			if (mb == NULL) {
				err = -ENOMEM;
				printf ("ErrOut 9l\n");
				goto err_out;
			}
			hdr = ((isdn23_hdr) mb->b_wptr)++;
#ifdef __CHECKER__
			memset(hdr,0,sizeof (*hdr));
#endif
			hdr->key = HDR_LOAD;
			hdr->seqnum = hdrseq; hdrseq += 2;
			hdr->hdr_load.len = msgdsize(mx);
			hdr->hdr_load.card = card->nr;
			hdr->hdr_load.seqnum = seqnum;
			hdr->hdr_load.foffset = foffset;
			linkb (mb, mx);
			if ((err = isdn3_sendhdr (mb)) == 0)
				mx = NULL;
			else {
				freeb (mb);
				printf ("ErrOut al\n");
				goto err_out;
			}
		}
		break;
#if 0
	case CMD_LIST:
		{
			switch (what) {
			default:
			  err:{
					mblk_t *mb = allocb (32, BPRI_LO);

					if (mb == NULL)
						break;
					m_putid (mb, IND_ERR);
					m_putsx (mb, IND_INFO);
					m_putsx (mb, CMD_LIST);
					*mb->b_wptr++ = ' ';
					m_putid (mb, id);
					if (isdn3_at_send (conn, mb, 1) != 0)
						freemsg (mb);
				}
				break;
			case LISTARG_CONN:
				{
					mblk_t *mb;

					if (conn == NULL) {
						if ((mb = allocb (16, BPRI_MED)) == NULL)
							break;
						m_putid (mb, IND_ERR);
						m_putsx (mb, ARG_CARD);
						m_putsx (mb, IND_INFO);
					} else {
						if ((mb = allocb (128, BPRI_LO)) == NULL)
							break;
						m_putid (mb, IND_INFO);
						conn_info (conn, mb);
					}
					if (isdn3_at_send (conn, mb, 1) != NULL)
						freemsg (mb);
				}
				break;
			case LISTARG_CARD:
				{
					int j = 0;
					mblk_t *mb;

					for (card = isdn3_card; card != NULL, card = card->next)
						if (card->id != 0)
							j++;
					if ((mb = allocb (10 + 5 * j, BPRI_LO)) == NULL)
						break;
					m_putid (mb, IND_INFO);
					for (card = isdn3_card; card != NULL; card = card->next) {
						if (card->id != 0) {
							m_putsx (mb, ARG_CARD);
							m_putlx (mb, card->id);
							m_putsx (mb, ARG_CHANNEL);
							m_puti (mb, card->bchans);
							m_putsx (mb, ARG_MODEMASK);
							m_putx (mb, card->modes);
						}
					}
					if (isdn3_at_send (conn, mb, 1) != 0)
						freemsg (mb);
				}
				break;
			}
		}
		break;
#endif
	case CMD_NOCARD:
		{
			mblk_t *mb;

			if(card == NULL) {
				err = -ESRCH;
				printf("ErrOut NCa");
				goto err_out;
			}
			mb = allocb (sizeof (struct _isdn23_hdr), BPRI_MED);

			if (mb == NULL) {
				err = -ENOMEM;
				printf ("ErrOut 9\n");
				goto err_out;
			}
			hdr = ((isdn23_hdr) mb->b_wptr)++;
#ifdef __CHECKER__
			memset(hdr,0,sizeof (*hdr));
#endif
			hdr->key = HDR_NOCARD;
			if(no_error)
				hdr->key |= HDR_NOERROR;
			hdr->seqnum = hdrseq; hdrseq += 2;
			hdr->hdr_nocard.card = card->nr;
			linkb (mb, mx);
			if ((err = isdn3_sendhdr (mb)) == 0)
				mx = NULL;
			else {
				freeb (mb);
				printf ("ErrOut a\n");
				goto err_out;
			}
		}

		break;
	case CMD_CARDPROT:
		{
			mblk_t *mb;

			if (card == NULL || chan == -1) {
				err = -EINVAL;
				printf ("ErrOut 8\n");
				goto err_out;
			}
			mb = allocb (sizeof (struct _isdn23_hdr), BPRI_MED);

			if (mb == NULL) {
				err = -ENOMEM;
				printf ("ErrOut 9\n");
				goto err_out;
			}
			hdr = ((isdn23_hdr) mb->b_wptr)++;
#ifdef __CHECKER__
			memset(hdr,0,sizeof (*hdr));
#endif
			hdr->key = HDR_PROTOCMD;
			if(no_error)
				hdr->key |= HDR_NOERROR;
			hdr->seqnum = hdrseq; hdrseq += 2;
			hdr->hdr_protocmd.card = card->nr;
			hdr->hdr_protocmd.channel = chan;
			hdr->hdr_protocmd.minor = 0;
			hdr->hdr_protocmd.len = dsize (mx);
			linkb (mb, mx);
			if ((err = isdn3_sendhdr (mb)) == 0)
				mx = NULL;
			else {
				freeb (mb);
				printf ("ErrOut a\n");
				goto err_out;
			}
		}
		break;
	case CMD_CLOSE:
		{
			mblk_t *mb;

			if (fminor == 0 && minor == 0) {
				err = -EINVAL;
				printf ("ErrOut 88\n");
				goto err_out;
			}
			mb = allocb (sizeof (struct _isdn23_hdr), BPRI_MED);

			if (mb == NULL) {
				err = -ENOMEM;
				printf ("ErrOut 99\n");
				goto err_out;
			}
			hdr = ((isdn23_hdr) mb->b_wptr)++;
#ifdef __CHECKER__
			memset(hdr,0,sizeof (*hdr));
#endif
			hdr->key = HDR_CLOSE;
			if(no_error)
				hdr->key |= HDR_NOERROR;
			hdr->seqnum = hdrseq; hdrseq += 2;
			hdr->hdr_close.minor = minor ? minor : fminor;
			hdr->hdr_close.error = (err < 0) ? -err : err;
			if ((err = isdn3_sendhdr (mb)) != 0) {
				freeb (mb);
				printf ("ErrOut a\n");
				goto err_out;
			}
		}
		break;
	case CMD_PROT:
		m_getskip (mx);
		if ((mx->b_rptr < mx->b_wptr) && (*mx->b_rptr == PROTO_MODE)) {
			if (0)
				printf ("Proto SetMode\n");
			/* Protocol setup completed. Or so it seems. */
			if (minorflags[minor] & MINOR_PROTO) {
				err = -EBUSY;
				printf ("ErrOut d\n");
				goto err_out_x;
			}
			mx->b_rptr++;
			m_getskip (mx);
			minorflags[minor] |= MINOR_PROTO|MINOR_INITPROTO_SENT|MINOR_INITPROTO_SENT2;
			if(conn != NULL) {
				conn->lockit++;
				isdn3_setup_conn (conn, EST_NO_CHANGE);
				conn->lockit--;
				if(conn->state == 0) {
					isdn3_killconn(conn,0);
				}
			}
		} else {
			if (minor == 0) {
				if (fminor == 0) {
					err = -EINVAL;
					printf ("ErrOut s2\n");
					goto err_out;
				} else
					minor = fminor;
			}
			if (conn != NULL) {
				switch (*(ushort_t *) mx->b_rptr) {
				case PROTO_DISCONNECT: /* force */
                    minorflags[conn->minor] &= ~MINOR_STATE_MASK;
			        minorflags[conn->minor] |= MINOR_STATE_NONE;
					isdn3_killconn (conn, 0); /* XXX */
					break;
#if 0
				case PROTO_INTERRUPT: /* force */
                    minorflags[conn->minor] &= ~MINOR_STATE_MASK;
			        minorflags[conn->minor] |= MINOR_STATE_NONE;
					err = isdn3_setup_conn (conn, EST_INTERRUPT);
					freemsg(mx); 
					mx = NULL;
					break;
#endif
				}
			}
#if 0
			else if (conn == NULL) {
				err = -EINVAL;
				printf ("ErrOut s3\n");
				goto err_out;
			}
#endif
			if (mx != NULL) {
				if(talk != NULL && conn != NULL && talk->hndl->proto != NULL) {
					mblk_t *mm = mx;
					err = (*talk->hndl->proto)(conn,&mx,1);
					if(mx != mm && mx != NULL)
						origmx = mx->b_rptr;
					if(err < 0) {
						printf ("ErrOut hx %d nm %d\n", err, minor);
						goto err_out;
					}
					if(mx == NULL) /* skip */
						break;
				}
				if ((err = isdn3_send_conn (minor, AS_PROTO, mx)) == 0) {
					/* minorflags[minor] |= MINOR_INITPROTO_SENT;  */
					mx = NULL;
				} else {
					printf ("ErrOut h %d nm %d\n", err, minor);
					goto err_out;
				}
			}
			if (mx == NULL || *(ushort_t *) mx->b_rptr != PROTO_AT)
				conn = NULL;
		}
		break;
	case CMD_CARDSETUP:
		m_getskip (mx);
		if (mx->b_rptr < mx->b_wptr && *mx->b_rptr == PROTO_MODE) {
			long nm;

			if (conn == NULL) {
				err = -EINVAL;
				printf ("ErrOut sdc\n");
				goto err_out;
			}
			if (0)
				printf ("Proto SetMode\n");
			/* Protocol setup completed. Or so it seems. */
			if ((conn->minorstate & MS_INITPROTO) || !(conn->minorstate & MS_INITPROTO_SENT)) {
				err = -EBUSY;
				printf ("ErrOut dc\n");
				goto err_out;
			}
			mx->b_rptr++;
			m_getskip (mx);
			if ((err = m_geti (mx, &nm)) != 0) {
				printf ("ErrOut fc\n");
				goto err_out;
			}
			if (nm < 1 || nm > 31) {
				err = -EINVAL;
				printf ("ErrOut g\n");
				goto err_out;
			}
			conn->minorstate |= MS_INITPROTO;
			conn->mode = nm;
			if ((m_geti (mx, &nm) == 0) && (nm >= 0) && (nm <= 255))
				conn->hupdelay = nm;
			else
				conn->hupdelay = 1;
			conn->lockit++;
			isdn3_setup_conn (conn, EST_NO_CHANGE);
			conn->lockit--;
			if(conn->state == 0) {
				/* XXX */
			}
		} else {
#if 1
			printf("ErrOut 2c");
			err = -ENOENT;
			goto err_out;
#else
			if (minor == 0) {
				if (fminor == 0) {
					err = -EINVAL;
					printf ("ErrOut s2\n");
					goto err_out;
				} else
					minor = fminor;
			}
			if (conn != NULL) {
				switch (*(ushort_t *) mx->b_rptr) {
				case PROTO_DISCONNECT:
					isdn3_killconn (conn, 0); /* XXX */
					break;
				case PROTO_INTERRUPT:
					err = isdn3_setup_conn (conn, EST_INTERRUPT);
					break;
				}
			}
			if (*(ushort_t *) mx->b_rptr != PROTO_AT)
				conn = NULL;
#if 0
			else if (conn == NULL) {
				err = -EINVAL;
				printf ("ErrOut s3\n");
				goto err_out;
			}
#endif
			if (mx != NULL) {
				if ((err = isdn3_send_conn (minor, AS_PROTO, mx)) == 0) {
					if (conn != NULL)
						minorflags[conn->minor] |= MINOR_INITPROTO_SENT;
					mx = NULL;
				} else {
					printf ("ErrOut h %d nm %d\n", err, minor);
					goto err_out;
				}
			}
#endif
		}
		break;
	case CMD_NOPROT:
		{
			mblk_t *mp;
			if(fminor == 0)
				fminor = minor;
			if(fminor == 0 && conn != NULL)
				fminor = conn->minor;
			if (fminor == 0) {
				err = -EINVAL;
				printf ("ErrOut j\n");
				goto err_out;
			}
			if ((mp = allocb (sizeof (struct _isdn23_hdr), BPRI_MED)) != NULL) {
				hdr = ((isdn23_hdr) mp->b_wptr)++;
#ifdef __CHECKER__
				memset(hdr,0,sizeof (*hdr));
#endif
				hdr->key = HDR_DETACH;
				if(no_error)
					hdr->key |= HDR_NOERROR;
				hdr->seqnum = hdrseq; hdrseq += 2;
				hdr->hdr_detach.minor = fminor;
				hdr->hdr_detach.error = 0xFF;
				hdr->hdr_detach.perm = 1;
				if ((err = isdn3_sendhdr (mp)) != 0) {
					freeb (mp);
				}
				if(conn != NULL)
					conn->minorstate |= MS_DETACHED;
			}
			if(conn != NULL) {
				minorflags[conn->minor] &= ~(MINOR_INITPROTO_SENT|MINOR_INITPROTO_SENT2);
				conn->lockit++;
				isdn3_setup_conn (conn, EST_NO_CHANGE);
				conn->lockit--;
				if(conn->state == 0) {
					/* XXX */
				}
			}
		}
		break;
	case CMD_OFF:
		if(nodisc) { /* Prevent disconnect from being sent to the stream */
			if(conn != NULL) {
				if(conn->minor != 0 && minor2conn[conn->minor] == conn) 
					minor2conn[conn->minor] = NULL;
				conn->minor = 0;
				conn->minorstate |= MS_NOMINOR|MS_DETACHED;
			}
			minor = fminor = 0; /* XXX */
		}
#if 0
		if (conn != NULL && !force) {
			isdn3_killconn (conn, 0);
			break;
		}
#endif
		if(minor != 0 && do_int < 0) {
			minorflags[minor] &= ~(MINOR_PROTO|MINOR_INITPROTO_SENT);
		}
		/* FALL THRU */
	default:
		/*
		 * Unknown command. Set pointer back to the beginning and forward to
		 * the appropriate handler, if any.
		 */
		mx->b_rptr = oldpos;
		if(theID == CMD_OFF && noconn)
			break;
		if (conn == NULL && talk == NULL) {
			err = -EINVAL;
			printf ("ErrOut o\n");
			goto err_out;
		}
		if (conn == NULL) {		  /* If no appropriate connection, create one. */
			if (subprotocol == -1) {
				err = -EINVAL;
				printf ("ErrOut i\n");
				goto err_out;
			}
			if (chan < -1 || chan > (char)talk->card->bchans) {
				err = -ENXIO;
				printf ("ErrOut u\n");
				goto err_out;
			}
			if ((conn = isdn3_new_conn (talk)) == NULL) {
				err = -ENXIO;
				printf ("ErrOut z\n");
				goto err_out;
			}
			conn->subprotocol = subprotocol;
			if(chan != -1) {
				conn->minorstate |= MS_BCHAN;
				conn->bchan = chan;
			}
			if (delay > 0)
				conn->delay = delay;
			if (conn_id != 0)
				conn->conn_id = conn_id;
			if(call_ref != 0)
				conn->call_ref = call_ref;
		}
if(theID == CMD_OFF) printf(" D1 ");
		printf ("* Conn%p: min %d at %d; minor %d min %d; callref %ld connref %d \n",
				conn, conn->minor, conn->fminor, 
				minor, fminor, conn->call_ref, conn->conn_id);
		if (conn->minor == 0 && !nodisc && (theID != CMD_OFF)) {
			conn->minor = (minor ? minor : fminor);
			if (conn->minor != 0)
				minor2conn[conn->minor] = conn;
		}
else printf(" D4 ");
		if (do_talk)
			conn->minorstate |= MS_FORCING;
		if (conn->minor != 0) {
if(theID == CMD_OFF) printf(" D5 ");
			if (conn->fminor == 0) 
				conn->fminor = fminor;
			minor2conn[conn->minor] = conn;
		}
		if (conn->call_ref == 0)
			conn->call_ref = ((call_ref != 0) ? call_ref : isdn3_new_callref (talk));
		if (conn->stack[0] == 0)
			memcpy(conn->stack,stack,sizeof(conn->stack));
		if(do_int != 0) {
if(theID == CMD_OFF) printf(" D6 ");
			if(conn->minor == 0) {
				printf("\n*** CM Zero!\n");
			} else if(do_int > 0)
				minorflags[conn->minor] |= MINOR_INT;
			else
				minorflags[conn->minor] &=~ MINOR_INT;
		}
if(theID == CMD_OFF) printf(" D7 ");
		conn->lockit++;
		isdn3_setup_conn (conn, EST_NO_CHANGE);
		conn->lockit--;
if(theID == CMD_OFF) printf(" D8 ");
		if(conn->state == 0) {
			/* XXX */
		}
		if ((err = (*talk->hndl->sendcmd) (conn, theID, mx)) == 0 ||
				err == -EBUSY)
			mx = NULL;
		else {
			printf ("ErrOut t of %d is %d\n", talk->hndl->SAPI, err);
			goto err_out;
		}
if(theID == CMD_OFF) printf(" D9 ");
		break;
	}
  /* ok_out: */
	if (mx != NULL)
		freemsg (mx);
	return 0;
  err_out:
	if (mx != NULL) {
		mblk_t *mb;

		mx->b_rptr = origmx;
		mb = allocb (50, BPRI_LO);
		if (mb != NULL) {
			m_putid (mb, IND_ERR);
			m_putsx (mb, ARG_ERRNO);
			m_puti (mb, err);
			if(conn_id == 0 && conn != NULL)
				conn_id = conn->conn_id;
			if(conn_id != 0) {
				m_putsx (mb, ARG_CONNREF);
				m_puti (mb, conn_id);
			}
			if(minor != 0) {
				m_putsx (mb, ARG_MINOR);
				m_puti (mb, minor);
			}
			if(fminor != 0) {
				m_putsx (mb, ARG_FMINOR);
				m_puti (mb, fminor);
			}
			m_putdelim (mb);
			linkb (mb, mx);
			if (isdn3_at_send (conn, mb, 0) != 0)
				freemsg (mb);
		} else
			freemsg (mx);
	}
	return 0;
  err_out_x:
  	if(mx != NULL)
		freemsg(mx);
  	return 0;
}

/* --> isdn_3.h */
isdn3_conn
isdn3_findatcmd (ushort_t fminor)
{
	isdn3_card card;
	isdn3_talk talk;
	isdn3_conn conn;

	for(card = isdn_card; card != NULL; card = card->next) {
		for (talk = card->talk; talk != NULL; talk = talk->next) {
			for (conn = talk->conn; conn != NULL; conn = conn->next) {
				if (conn->fminor == fminor)
					return conn;
			}
		}
	}
	return NULL;
}

/* --> isdn_3.h */
isdn3_card
isdn3_findcardid (ulong_t cardid)
{
	isdn3_card card;

	for(card = isdn_card; card != NULL; card = card->next) {
		if (card->id == cardid)
			return card;
	}
	return NULL;
}

/* --> isdn_3.h */
isdn3_card
isdn3_findcard (uchar_t cardnr)
{
	isdn3_card card;

	for(card = isdn_card; card != NULL; card = card->next) {
		if (card->nr == cardnr)
			return card;
	}
	return NULL;
}

/* --> isdn_3.h */
isdn3_conn
isdn3_findminor (ushort_t minor)
{
	isdn3_conn conn;

	if (minor <= 0 || minor >= NMINOR)
		return NULL;
	if (!(minorflags[minor] & (MINOR_OPEN | MINOR_WILL_OPEN)))
		return NULL;
	if (minor2conn[minor] == NULL)
		return NULL;
	conn = minor2conn[minor];
	printf (".FindMinor%p %d gets C %d:%d\n", conn, minor, conn->minor, conn->fminor);
	return conn;
}


/* --> isdn_3.h */
isdn3_hndl
isdn3_findhndl (uchar_t SAPI)
{
	isdn3_hndl hndl;

	for (hndl = isdn_hndl; hndl != NULL; hndl = hndl->next) {
		if (hndl->SAPI == SAPI)
			return hndl;
	}
	return NULL;
}

/* --> isdn_3.h */
isdn3_talk
isdn3_findtalk (isdn3_card card, isdn3_hndl hndl, mblk_t *info, int create)
{
	isdn3_talk talk;

	if (card == NULL || hndl == NULL || card->id == 0)
		return NULL;
	for (talk = card->talk; talk != NULL; talk = talk->next)
		if (talk->hndl == hndl)
			return talk;
	if (create) {
		int ms = splstr ();
		int i;
		int err;

char systr[200] = "";
if(card->info != NULL)
 sprintf(systr+strlen(systr),"'%-*s'",card->info->b_wptr-card->info->b_rptr,card->info->b_rptr);
else strcat(systr,"NULL");
strcat(systr," and info ");
if(info != NULL)
 sprintf(systr+strlen(systr),"'%-*s'",info->b_wptr-info->b_rptr,info->b_rptr);
else strcat(systr,"NULL");
syslog(LOG_DEBUG,"====== Create 0x%02x with card %s",hndl->SAPI,systr);
		if(card->info != NULL) {
			mblk_t *mi = card->info;
			streamchar *sta = mi->b_rptr;
			ushort_t idx;
			i = 0;

			while(m_getid(mi,&idx) == 0) {
				long sap;
				i = 1;
				if(idx == ARG_PROTOCOL && m_geti(mi,&sap) == 0) {
					if (sap == hndl->SAPI) {
						i = 2;
						break;
					}
				}
			}
			mi->b_rptr = sta;
			if (i == 1)
				return NULL;
		}
		if (info != NULL) {
			mblk_t *mi = info;
			streamchar *sta = mi->b_rptr;
			ushort_t idx;
			i = 0;

			while(m_getid(mi,&idx) == 0) {
				long sap;
				i = 1;
				if(idx == ARG_PROTOCOL && m_geti(mi,&sap) == 0) {
					if (sap == hndl->SAPI) {
						i = 2;
						break;
					}
				}
			}
			mi->b_rptr = sta;
			if (i == 1)
				return NULL;
		}

		talk = malloc(sizeof(*talk));
		if (talk == NULL) {
			splx (ms);
			return NULL;
		}
		memset(talk,0,sizeof(*talk));
		talk->hndl = hndl;

		talk->card = card;
		talk->next = card->talk;
		card->talk = talk;
		if ((err = isdn3_chstate (talk, 0, 0, CH_OPENPROT)) != 0) {
			card->talk = card->talk->next;
			talk->card = NULL;
			splx (ms);
			return NULL;
		}
		splx (ms);
		return talk;
	}
	return NULL;
}

/* --> isdn_3.h */
long
isdn3_flags(mblk_t *info, uchar_t protocol, uchar_t subprot)
{
	long flags = 2; /* FL_UPDELAY, 1/4th second */
	uchar_t thisprot = (uchar_t)-1;
	int skip = 0;

	if(info != NULL) {
		streamchar *sta = info->b_rptr;
		ushort_t idx;

		while(m_getid(info,&idx) == 0) {
			long sap;
			switch(idx) {
			case ARG_PROTOCOL:
				if (m_geti(info,&sap) == 0) {
					skip = (sap != protocol && protocol != (uchar_t)-1);
					thisprot = protocol;
				}
				break;
			case ARG_SUBPROT:
				if (m_geti(info,&sap) == 0 && thisprot == protocol) {
					skip = (sap != subprot && subprot != (uchar_t)-1);
				}
				break;
			default:
				if(skip)
					break;
				switch(idx) {
				case ARG_UPDELAY:
					if(m_geti(info,&sap) == 0) {
						flags &=~ FL_UPDELAY;
						if(sap > 0) {
							sap = (sap << FL_UPDELAY_SHIFT) / 10;
							if(sap > FL_UPDELAY)
								sap = FL_UPDELAY;
							else if(sap == 0)
								sap = 1;
							flags |= sap;
						}
					}
					break;
				case ARG_POINTOPOINT:
					flags &=~ FL_POINTMASK;
					flags |= FL_POINTOPOINT;
					break;
				case ARG_MULTIPOINT1:
					flags &=~ FL_POINTMASK;
					flags |= FL_MULTIPOINT1;
					break;
				case ARG_MULTIPOINT2:
					flags &=~ FL_POINTMASK;
					flags |= FL_MULTIPOINT2;
					break;
				case ARG_MULTIPOINT3:
					flags &=~ FL_POINTMASK;
					flags |= FL_MULTIPOINT3;
					break;
				case ARG_ANSWER_IMMED:
					flags |= FL_ANS_IMMED;
					break;
				case ARG_ANSWER_DELAY:
					flags &=~ FL_ANS_IMMED;
					break;
				case ARG_TEI_IMMED:
					flags |= FL_TEI_IMMED;
					break;
				case ARG_TEI_DELAY:
					flags &=~ FL_TEI_IMMED;
					break;
				case ARG_L2KEEP:
					flags |= FL_L2KEEP;
					break;
				case ARG_L2CLOSE:
					flags &=~ FL_L2KEEP;
					break;
				default:
					break;
				}
			}
		}
		info->b_rptr = sta;
	}
	return flags;
}


/* --> isdn_3.h */
isdn3_conn
isdn3_findconn (isdn3_talk talk, long protocol, long call_ref)
{
	isdn3_conn conn;

	for (conn = talk->conn; conn != NULL; conn = conn->next)
		if (conn->subprotocol == protocol && conn->call_ref == call_ref
		&& (conn->state != 0)) {
			printf(".FindConn%p, ref %ld:",conn,call_ref);
			return conn;
		}
	return NULL;
}


/* --> isdn_3.h */
int
isdn3_attach_hndl (isdn3_hndl hndl)
{
	isdn3_hndl nhndl;
	int ms = splstr ();

	for (nhndl = isdn_hndl; nhndl != NULL; nhndl = nhndl->next) {
		if (nhndl->SAPI == hndl->SAPI) {
			splx (ms);
			return -EEXIST;
		}
	}

	hndl->next = isdn_hndl;
	isdn_hndl = hndl;

	if (hndl->init != NULL)
		(*hndl->init) ();

	splx (ms);
	return 0;
}



/*
 * Basic module data initialization.
 */
void
isdn3_init (void)
{
	memset(minor2conn,0,sizeof (minor2conn));
	memset(minorflags,0,sizeof (minorflags));

	/* Mark everything as free. */
	isdn_hndl = NULL;

	/* Attach default handlers */
#ifdef _phone_
	isdn3_attach_hndl (&PHONE_hndl);
#endif
#ifdef _fixed_
	isdn3_attach_hndl (&FIXED_hndl);
#endif
#ifdef _capi_
	isdn3_attach_hndl (&CAPI_hndl);
#endif
#ifdef _tei_
	isdn3_attach_hndl (&TEI_hndl);
#endif
}

/* Streams code to open the module. */
static int
isdn3_open (queue_t * q, dev_t dev, int flag, int sflag ERR_DECL)
{
	if (q->q_ptr)				  /* dup(), fork(), ... */
		return 0;

	if (isdn3_q != NULL) {		  /* Another ioctl(I_PUSH). */
		ERR_RETURN(-EALREADY);
	}
	isdn3_q = q;
	WR (q)->q_ptr = (caddr_t) & isdn3_q;
	q->q_ptr = (caddr_t) isdn3_q;

	printf ("ISDN Master driver %d opened.\n", dev);

	return 0;
}

/* Streams code to close the module. */
static void
isdn3_close (queue_t *q, int dummy)
{
	flushq (q, FLUSHALL);
	flushq (WR (q), FLUSHALL);
	printf ("ISDN Master driver closed.\n");
	isdn3_q = NULL;
	return;
}


/* Streams code to write data. */
static void
isdn3_wput (queue_t * q, mblk_t * mp)
{
	switch (DATA_TYPE(mp)) {
	case CASE_DATA:
		putq (q, mp);
		break;
	case M_FLUSH:
		if (*mp->b_rptr & FLUSHW)
			flushq (q, FLUSHDATA);
		putnext (q, mp);
		break;
	default:{
			putnext (q, mp);
			break;
		}
	}
	return;
}

/* Streams code to scan the write queue. */
static void
isdn3_wsrv (queue_t * q)
{
	mblk_t *mp;

	while ((mp = getq (q)) != NULL) {
		switch (DATA_TYPE(mp)) {
		case CASE_DATA:
			if (scan_at (0, mp) != 0)
				freemsg (mp);
			break;
		case M_FLUSH:
			if (*mp->b_rptr & FLUSHW)
				flushq (q, FLUSHDATA);
			/* FALL THRU */
		default:
			putnext (q, mp);
			continue;
		}
	}
	return;
}

/* Streams code to read data. */
static void
isdn3_rput (queue_t * q, mblk_t * mp)
{
	switch (DATA_TYPE(mp)) {

	case M_FLUSH:
		if (*mp->b_rptr & FLUSHR) {
			flushq (q, FLUSHDATA);
		}
		putnext (q, mp);		  /* send it along too */
		break;
	case CASE_DATA:
		putq (q, mp);			  /* queue it for my service routine */
		break;

	default:
		putnext (q, mp);
	}
	return;
}

/* Streams code to scan the read queue. */
static void
isdn3_rsrv (queue_t * q)
{
	mblk_t *mp;

	while ((mp = getq (q)) != NULL) {
		switch (DATA_TYPE(mp)) {
		case CASE_DATA:
			{
				struct _isdn23_hdr hdr;
				if (mp->b_wptr - mp->b_rptr < sizeof (struct _isdn23_hdr)) {
					printf ("isdn3_rsrv: small msg recv\n");
					break;
				}
				hdr = *((isdn23_hdr) mp->b_rptr)++;
				mp = pullupm (mp, 1);

				switch (hdr.key & ~HDR_FLAGS) {
				case HDR_ATCMD:
					{
						mblk_t *mx;

						if ((hdr.hdr_atcmd.minor > 0) && (hdr.hdr_atcmd.minor < NMINOR) && !(minorflags[hdr.hdr_atcmd.minor])) {
							if (scan_at (hdr.hdr_atcmd.minor, mp) == 0)
								mp = NULL;
							break;
						}
						if ((mx = allocb (16, BPRI_MED)) == NULL)
							break;
						m_putid (mx, IND_ATCMD);
						m_putsx (mx, ARG_FMINOR);
						m_puti (mx, hdr.hdr_atcmd.minor);
						m_putdelim (mx);
						linkb (mx, mp);
						mp = NULL;
						putnext (isdn3_q, mx);
					}
					break;
				case HDR_PROTOCMD:
					{
						mblk_t *mx;
						int err;
						isdn3_conn conn = isdn3_findminor (hdr.hdr_protocmd.minor);

						if(conn != NULL && conn->talk != NULL && conn->talk->hndl->proto != NULL) {
							err = (*conn->talk->hndl->proto)(conn,&mp,0);
							if((mp == NULL) || (err < 0))
								break;
						}

						m_getskip (mp);
						if (conn != NULL) {
							switch (*(ushort_t *) mp->b_rptr) {
							case PROTO_DISCONNECT:
								if ((minorflags[conn->minor] & MINOR_STATE_MASK) == MINOR_STATE_NONE)
									goto sdrop;
								isdn3_setup_conn (conn, EST_DISCONNECT);
								break;
							case PROTO_INTERRUPT:
								if ((minorflags[conn->minor] & MINOR_STATE_MASK) == MINOR_INTERRUPT_SENT)
									goto sdrop;
								isdn3_setup_conn (conn, EST_INTERRUPT);
								break;
#if 0
							case PROTO_LISTEN:
								if ((minorflags[conn->minor] & MINOR_STATE_MASK) == MINOR_LISTEN_SENT)
									goto sdrop;
								minorflags[conn->minor] &= ~MINOR_STATE_MASK;
								minorflags[conn->minor] |= MINOR_LISTEN_SENT;
								break;
							case PROTO_CONNECTED:
								if ((minorflags[conn->minor] & MINOR_STATE_MASK) == MINOR_CONN_SENT)
									goto sdrop;
								minorflags[conn->minor] &= ~MINOR_STATE_MASK;
								minorflags[conn->minor] |= MINOR_CONN_SENT;
								break;
#endif
							}
						}
						if ((mx = allocb (64, BPRI_MED)) == NULL)
							break;
						{
							ushort_t ind;

							m_getid(mp,&ind);
							m_putid(mx,ind);

						}
						if (conn != NULL && conn->fminor != 0) {
							m_putsx (mx, ARG_FMINOR);
							m_puti (mx, conn->fminor);
						}
						if(hdr.hdr_protocmd.minor != 0) {
							m_putsx (mx, ARG_MINOR);
							m_puti (mx, hdr.hdr_protocmd.minor);
						}
						if(conn != NULL && conn->conn_id != 0) {
							m_putsx (mx, ARG_CONNREF);
							m_puti (mx, conn->conn_id);
						}

						m_getskip(mp);
						*mx->b_wptr++ = ' ';

						mp = pullupm(mp,0);
						if(mp != NULL) {
							linkb (mx, mp);
							mp = NULL;
						}
						putnext (isdn3_q, mx);
						break;
						sdrop:
						freemsg (mp);
						mp = NULL;
					}
					break;
				case HDR_XDATA:
					{
						isdn3_conn conn = isdn3_findminor (hdr.hdr_detach.minor);
						isdn3_talk talk;

						if (conn == NULL) {
							if (0)
								printf ("XData: Conn for minor %d nf\n", hdr.hdr_detach.minor);
							break;
						}
						if ((talk = conn->talk) == NULL) {
							printf ("XData m %d: No Talker\n", hdr.hdr_detach.minor);
							break;
						}
						if (talk->hndl->send != NULL && (*talk->hndl->send) (conn, mp) == 0)
							mp = NULL;
					}
					break;
				case HDR_DATA:
					{
						isdn3_card card;
						isdn3_hndl hndl;
						isdn3_talk talk;
						mblk_t *m2;

						card = isdn3_findcard(hdr.hdr_data.card);
						if (card == NULL)
							break;
						if ((hndl = isdn3_findhndl (hdr.hdr_data.SAPI)) == NULL)
							break;
						if ((talk = isdn3_findtalk (card, hndl, NULL, 0)) == NULL)
							break;

						m2 = pullupm(mp,-1);
						if(m2 == NULL) {
							printf("ISDN_3 error: no memory\n");
							break;
						} else
							mp = m2;
						if ((*talk->hndl->recv) (talk, 0, mp) == 0)
							mp = NULL;
					}
					break;
				case HDR_UIDATA:
					{
						isdn3_card card;
						isdn3_hndl hndl;
						isdn3_talk talk;
						mblk_t *m2;

						card = isdn3_findcard(hdr.hdr_uidata.card);
						if (card == NULL)
							break;
						if ((hndl = isdn3_findhndl (hdr.hdr_uidata.SAPI)) == NULL)
							break;
						if ((talk = isdn3_findtalk (card, hndl, NULL, 0)) == NULL)
							break;

						m2 = pullupm(mp,-1);
						if(m2 == NULL) {
							printf("ISDN_3 error: no memory\n");
							break;
						} else
							mp = m2;
						if ((*talk->hndl->recv) (talk, hdr.hdr_uidata.broadcast ? 3 : 1, mp) == 0)
							mp = NULL;
					}
					break;
				case HDR_RAWDATA:
					{
						isdn3_card card;
						mblk_t *mx;

						card = isdn3_findcard(hdr.hdr_rawdata.card);
						if (card == NULL)
							break;

						mx = allocb(64,BPRI_LO);
						if(mx != NULL) {
							m_putid(mx,IND_TRACE);
							m_putsx(mx,ARG_CARD);
							m_putlx(mx,card->id);
							m_putsx(mx,ARG_SUBCARD);
							m_puti(mx,hdr.hdr_rawdata.dchan);
							if(hdr.hdr_rawdata.flags & 01)
								m_putsx(mx,PROTO_OUTGOING);
							else
								m_putsx(mx,PROTO_INCOMING);
							m_putdelim(mx);
							linkb(mx,mp);
							putnext(q,mx);
							mp = NULL;
						}
					}
					break;
				case HDR_OPEN:
					{
						isdn3_conn conn;
						mblk_t *mx;

						if (hdr.hdr_open.minor > NMINOR) {
							printf ("Open: Minor %d out of range\n", hdr.hdr_open.minor);
							break;
						}
						if (minorflags[hdr.hdr_open.minor] & MINOR_OPEN) {
							printf ("Open: Minor %d open\n", hdr.hdr_open.minor);
							break;
						}

						minorflags[hdr.hdr_open.minor] |= MINOR_OPEN;
						if (!(minorflags[hdr.hdr_open.minor] & MINOR_WILL_OPEN) && ((mx = allocb (32, BPRI_MED)) != NULL)) {
							m_putid (mx, IND_OPEN);
							m_putsx (mx, ARG_MINOR);
							m_puti (mx, hdr.hdr_open.minor);
							m_putsx (mx, ARG_FLAGS);
							m_putx (mx, hdr.hdr_open.flags);
							m_putsx (mx, ARG_UID);
							m_puti (mx, hdr.hdr_open.uid);
#if 0
							m_putsx (mx, ARG_GID);
							m_puti (mx, hdr.hdr_open.gid);
#endif
							putnext (q, mx);
						}
						minorflags[hdr.hdr_open.minor] &= ~MINOR_WILL_OPEN;
						if ((conn = isdn3_findminor (hdr.hdr_open.minor)) != NULL)
							isdn3_setup_conn (conn, EST_NO_CHANGE);
					}
					break;
				case HDR_CLOSE:
					{
						isdn3_conn conn = isdn3_findminor (hdr.hdr_close.minor);
						mblk_t *mx;

						printf("\n*** Closed %d\n",hdr.hdr_close.minor);
						minorflags[hdr.hdr_close.minor] = 0;
						if (conn != 0 && conn->minor == hdr.hdr_close.minor) {
							SUBDEV cm = conn->minor;

							isdn3_killconn (conn, 0); /* XXX */
							if(cm != 0) {
printf(" *SM %d: %d %d.%d\n",__LINE__,conn->conn_id,conn->minor,conn->fminor);
								conn->minor = conn->fminor;
								conn->fminor = 0;
							}
						}
						while ((conn = isdn3_findatcmd(hdr.hdr_close.minor)) != NULL) {
							conn->fminor = 0;
							isdn3_killconn (conn, 0);
						}
						if ((mx = allocb (32, BPRI_MED)) != NULL) {
							m_putid (mx, IND_CLOSE);
							m_putsx (mx, ARG_MINOR);
							m_puti (mx, hdr.hdr_close.minor);
							putnext (q, mx);
						}
					}
					break;
				case HDR_ATTACH:
					{
						printf ("Attach coming up ??\n");
					}
					break;
				case HDR_DETACH:
					{
						isdn3_conn conn = isdn3_findminor (hdr.hdr_detach.minor);

						if (conn == NULL) {
							if (0)
								printf ("Detach: Conn for minor %d nf\n", hdr.hdr_detach.minor);
							break;
						}
						isdn3_killconn (conn, 0);
					}
					break;
				case HDR_CARD:
					{
						isdn3_card card;
						mblk_t *mx;

						card = isdn3_findcard(hdr.hdr_card.card);
						if (card != NULL) {
							printf ("Hdr_Card: Card %d busy\n", hdr.hdr_card.card);
							break;
						}
						if (hdr.hdr_card.id == 0) {
							printf ("Hdr_Card: BadID Card %d\n", hdr.hdr_card.card);
							break;
						}
						card = malloc(sizeof(*card));
						if (card == NULL) {
						}
						memset(card,0,sizeof(*card));
						card->nr = hdr.hdr_card.card;
						card->id = hdr.hdr_card.id;
						card->TEI = TEI_BROADCAST;
						card->dchans = hdr.hdr_card.dchans;
						card->bchans = hdr.hdr_card.bchans;
						card->modes = hdr.hdr_card.modes;
						card->next = isdn_card;
						isdn_card = card;
						if ((mx = allocb (32, BPRI_MED)) != 0) {
							m_putid (mx, IND_CARD);
							m_putlx (mx, hdr.hdr_card.id);
							m_puti (mx, hdr.hdr_card.dchans);
							m_puti (mx, hdr.hdr_card.bchans);
							m_putx (mx, hdr.hdr_card.modes);
							putnext (q, mx);
						} else
							printf ("Hdr_Card: No mem to reply\n");
					}
					break;
				case HDR_NOCARD:
					{
						isdn3_card card;
						isdn3_talk talk;
						mblk_t *mx;

						card = isdn3_findcard(hdr.hdr_nocard.card);
						if (card == NULL) {
							printf ("Hdr_NoCard: Card %d not busy\n", hdr.hdr_nocard.card);
							break;
						}
						while ((talk = card->talk) != NULL)
							isdn3_killtalk (talk);
						if ((mx = allocb (32, BPRI_MED)) != 0) {
							m_putid (mx, IND_NOCARD);
							m_putlx (mx, card->id);
							putnext (q, mx);
						}
						if(isdn_card == card)
							isdn_card = card->next;
						else {
							isdn3_card prevcd;
							for(prevcd = isdn_card; prevcd != NULL; prevcd = prevcd->next) {
								if(prevcd->next == card) {
									prevcd->next = card->next;
									break;
								}
							}
						}
						free(card);
					}
					break;
				case HDR_TEI:
					{
						isdn3_card card;

						card = isdn3_findcard(hdr.hdr_tei.card);
						if (card == NULL) {
							printf ("Hdr_TEI: Card %d not busy\n", hdr.hdr_tei.card);
							break;
						}
						if (hdr.hdr_tei.TEI == TEI_BROADCAST)
							tei_getid (card);
					}
					break;
				case HDR_OPENPROT:
					{
						isdn3_talk talk;
						isdn3_hndl hndl;
						isdn3_card card;

						card = isdn3_findcard(hdr.hdr_openprot.card);
						if (card == NULL)
							break;
						if ((hndl = isdn3_findhndl (hdr.hdr_openprot.SAPI)) == NULL)
							break;
						if ((talk = isdn3_findtalk (card, hndl, NULL, 0)) == NULL) {
							printf ("OpenProt: NotFound %d:%02x\n", hdr.hdr_openprot.card, hdr.hdr_openprot.SAPI);
							break;
						}
						(*talk->hndl->chstate) (talk, hdr.hdr_openprot.ind, 1);
					}
					break;
				case HDR_CLOSEPROT:
					{
						isdn3_talk talk;
						isdn3_hndl hndl;
						isdn3_card card;

						card = isdn3_findcard(hdr.hdr_closeprot.card);
						if ((hndl = isdn3_findhndl (hdr.hdr_closeprot.SAPI)) == NULL)
							break;
						if ((talk = isdn3_findtalk (card, hndl, NULL, 0)) == NULL) {
							printf ("CloseProt: NotFound %d:%02x\n", hdr.hdr_closeprot.card, hdr.hdr_closeprot.SAPI);
							break;
						}
						(*talk->hndl->chstate) (talk, hdr.hdr_closeprot.ind, 1);
					}
					break;
				case HDR_NOTIFY:
					{
						isdn3_talk talk;
						isdn3_hndl hndl;
						isdn3_card card;

						card = isdn3_findcard(hdr.hdr_notify.card);
						if (card == NULL)
							break;

						if (hdr.hdr_notify.SAPI == SAPI_INVALID) {
							for (talk = card->talk; talk != NULL; talk = talk->next) {
								printf("chstate for SAPI %x\n",talk->hndl->SAPI);
								(*talk->hndl->chstate) (talk, hdr.hdr_notify.ind, hdr.hdr_notify.add);
							}
							switch (hdr.hdr_notify.ind) {
							case PH_ACTIVATE_IND:
							case PH_ACTIVATE_CONF:
								card->is_up = 1;
								break;
							case PH_DEACTIVATE_IND:
							case PH_DEACTIVATE_CONF:
							case PH_DISCONNECT_IND:
								card->is_up = 0;
								break;
							}
						} else {
							if ((hndl = isdn3_findhndl (hdr.hdr_notify.SAPI)) == NULL)
								break;
							if ((talk = isdn3_findtalk (card, hndl, NULL, 0)) == NULL) {
								printf ("Notify: NotFound %d:%02x\n", hdr.hdr_notify.card, hdr.hdr_notify.SAPI);
								break;
							}
							(*talk->hndl->chstate) (talk, hdr.hdr_notify.ind, hdr.hdr_notify.add);
						}
					}
					break;
				case HDR_INVAL:
					{
						mblk_t *mx;
						if ((mp != NULL) && (mx = allocb (64, BPRI_MED)) != 0) {
							mblk_t *mq;
							if(hdr.hdr_inval.error != 0) {
								m_putid (mx, IND_ERR);
								m_putsx (mx, ARG_ERRNO);
								m_puti (mx, hdr.hdr_inval.error);
							} else {
								m_putid (mx, IND_NOERR);
							}

							mq = pullupm (mp, sizeof(hdr));
							if(mq != NULL) {
								mp = mq;
								hdr = *((isdn23_hdr) mp->b_rptr)++;
								m_putsx(mx,ARG_ERRHDR);
								m_puti(mx,hdr.key);

								switch(hdr.key) {
								case HDR_UIDATA:
									{
										isdn3_card card = isdn3_findcard(hdr.hdr_uidata.card);
										if(card != NULL) {
											m_putsx(mx,ARG_CARD);
											m_putlx(mx,card->id);
										}
									}
									m_putsx(mx,ARG_PROTOCOL);
									m_puti(mx,hdr.hdr_uidata.SAPI);
									m_putsx(mx,ARG_LENGTH);
									m_puti(mx,hdr.hdr_uidata.len);
									break;
								case HDR_DATA:
									{
										isdn3_card card = isdn3_findcard(hdr.hdr_data.card);
										if(card != NULL) {
											m_putsx(mx,ARG_CARD);
											m_putlx(mx,card->id);
										}
									}
									m_putsx(mx,ARG_PROTOCOL);
									m_puti(mx,hdr.hdr_data.SAPI);
									m_putsx(mx,ARG_LENGTH);
									m_puti(mx,hdr.hdr_data.len);
									break;
								case HDR_OPENPROT:
									{
										isdn3_card card = isdn3_findcard(hdr.hdr_openprot.card);
										if(card != NULL) {
											m_putsx(mx,ARG_CARD);
											m_putlx(mx,card->id);
											m_putsx(mx,ARG_PROTOCOL);
											m_puti(mx,hdr.hdr_openprot.SAPI);
										}
									}
									break;
								case HDR_CLOSEPROT:
									{
										isdn3_card card = isdn3_findcard(hdr.hdr_closeprot.card);
										if(card != NULL) {
											m_putsx(mx,ARG_CARD);
											m_putlx(mx,card->id);
											m_putsx(mx,ARG_PROTOCOL);
											m_puti(mx,hdr.hdr_closeprot.SAPI);
										}
									}
									break;
								case HDR_NOTIFY:
									{
										isdn3_card card = isdn3_findcard(hdr.hdr_notify.card);
										if(card != NULL) {
											m_putsx(mx,ARG_CARD);
											m_putlx(mx,card->id);
											m_putsx(mx,ARG_PROTOCOL);
											m_puti(mx,hdr.hdr_notify.SAPI);
										}
									}
									break;
								case HDR_TEI:
									{
										isdn3_card card = isdn3_findcard(hdr.hdr_tei.card);
										if(card != NULL) {
											m_putsx(mx,ARG_CARD);
											m_putlx(mx,card->id);
										}
									}
									break;
								case HDR_LOAD:
									{
										isdn3_card card = isdn3_findcard(hdr.hdr_load.card);
										if(card != NULL) {
											m_putsx(mx,ARG_CARD);
											m_putlx(mx,card->id);
											m_putsx(mx,ARG_SEQNUM);
											m_puti(mx,hdr.hdr_load.seqnum);
											m_putsx(mx,ARG_OFFSET);
											m_puti(mx,hdr.hdr_load.foffset);
											m_putsx(mx,ARG_LENGTH);
											m_puti(mx,hdr.hdr_load.len);
										}
									}
									break;
								case HDR_CARD:
									{
										isdn3_card card = isdn3_findcard(hdr.hdr_card.card);
										if(card != NULL) {
											m_putsx(mx,ARG_CARD);
											m_putlx(mx,card->id);
											m_putsx(mx,ARG_CHANNEL);
											m_puti(mx,hdr.hdr_card.bchans);
										}
									}
									break;
								case HDR_ATCMD:
									m_putsx(mx,ARG_MINOR);
									m_puti(mx,hdr.hdr_atcmd.minor);
									m_putsx(mx,ARG_LENGTH);
									m_puti(mx,hdr.hdr_atcmd.len);
									break;
								case HDR_PROTOCMD:
									if(hdr.hdr_protocmd.minor == 0) {
										isdn3_card card = isdn3_findcard(hdr.hdr_protocmd.card);
										if(card != NULL) {
											m_putsx(mx,ARG_CARD);
											m_putlx(mx,card->id);
											m_putsx(mx,ARG_CHANNEL);
											m_puti(mx,hdr.hdr_protocmd.channel);
										}
									} else {
										m_putsx(mx,ARG_MINOR);
										m_puti(mx,hdr.hdr_protocmd.minor);
									}
									m_putsx(mx,ARG_LENGTH);
									m_puti(mx,hdr.hdr_protocmd.len);
									break;
								case HDR_RAWDATA:
									{
										isdn3_card card = isdn3_findcard(hdr.hdr_rawdata.card);
										if(card != NULL) {
											m_putsx(mx,ARG_CARD);
											m_putlx(mx,card->id);
										}
									}
									m_putsx(mx,ARG_LENGTH);
									m_puti(mx,hdr.hdr_rawdata.len);
									break;
								case HDR_XDATA:
									m_putsx(mx,ARG_MINOR);
									m_puti(mx,hdr.hdr_xdata.minor);
									m_putsx(mx,ARG_LENGTH);
									m_puti(mx,hdr.hdr_xdata.len);
									break;
								case HDR_OPEN:
									m_putsx(mx,ARG_MINOR);
									m_puti(mx,hdr.hdr_open.minor);
									break;
								case HDR_CLOSE:
									m_putsx(mx,ARG_MINOR);
									m_puti(mx,hdr.hdr_close.minor);
									break;
								case HDR_ATTACH:
									m_putsx(mx,ARG_MINOR);
									m_puti(mx,hdr.hdr_attach.minor);
									{
										isdn3_card card = isdn3_findcard(hdr.hdr_attach.card);
										if(card != NULL) {
											m_putsx(mx,ARG_CARD);
											m_putlx(mx,card->id);
											m_putsx(mx,ARG_CHANNEL);
											m_puti(mx,hdr.hdr_attach.chan);
										}
									}
									break;
								case HDR_DETACH:
									m_putsx(mx,ARG_MINOR);
									m_puti(mx,hdr.hdr_detach.minor);
									break;
								}
							}
							putnext (q, mx);
						}
					}
					break;
				default:
					break;
				}
				if (mp != NULL)
					freemsg (mp);
			}
			break;
		default:
			putnext (q, mp);
			continue;
		}
	}
	return;
}



#ifdef sun
/* Standard code to dynamically load a Streams module */

#include <sys/conf.h>
#include <sys/buf.h>
#include <sundev/mbvar.h>
#include <sun/autoconf.h>
#include <sun/vddrv.h>

static struct vdldrv vd =
{
		VDMAGIC_PSEUDO,
		"isdn_3",
		NULL, NULL, NULL, 0, 0, NULL, NULL, 0, 0,
};

static struct fmodsw *isdn3_fmod;

isdn3_load (fc, vdp, vdi, vds)
	unsigned int fc;
	struct vddrv *vdp;
	addr_t vdi;
	struct vdstat *vds;
{
	switch (fc) {
	case VDLOAD:
		{
			int dev, i;

			for (dev = 0; dev < fmodcnt; dev++) {
				if (fmodsw[dev].f_str == NULL)
					break;
			}
			if (dev == fmodcnt)
				return (ENODEV);
			isdn3_fmod = &fmodsw[dev];
			for (i = 0; i <= FMNAMESZ; i++)
				isdn3_fmod->f_name[i] = isdn3_minfo.mi_idname[i];
			isdn3_fmod->f_str = &isdn3_l3info;
		}
		vdp->vdd_vdtab = (struct vdlinkage *) & vd;
		return 0;
	case VDUNLOAD:
		{
			int dev;

			for (dev = 0; dev < NISDN3; dev++)
				if (isdn3_q != NULL)
					return (EIO);
		}
		isdn3_fmod->f_name[0] = '\0';
		isdn3_fmod->f_str = NULL;
		return 0;
	case VDSTAT:
		return 0;
	default:
		return -EIO;
	}
}

#endif
