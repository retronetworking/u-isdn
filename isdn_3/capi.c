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
#include "isdn_proto.h"
#include "../cards/capi/capi.h"
#include "isdn_12.h"
#include "asm/byteorder.h"
#include "sapi.h"
#include <ctype.h>

#define PPP_IP_VANJ CHAR2('i','v')
#define PPP_IP      CHAR2('i','n')
#define PPP_DO_PAP  CHAR2('p','p')
#define PPP_DO_CHAP CHAR2('p','c')

#define OPT_LOC_ALLOW   0x01 /* Option ist erlaubt (kann gesendet werden) */
#define OPT_LOC_MAND    0x02 /* Option ist notwendig (muss ge-ackt werden) */
#define OPT_LOC_WANT    0x04 /* Option ist erwuenscht (wird initial gesendet) */
#define OPT_LOC_NEG     0x08 /* Option wurde bestaetigt (Status) */
#define OPT_REM_ALLOW   0x10 /* Option ist erlaubg (kann empfangen werden) */
#define OPT_REM_MAND    0x20 /* Option ist notwendig (sonst NAK) */
#define OPT_REM_NAK     0x40 /* Option wurde genak't (internal only) */
#define OPT_REM_NEG     0x80 /* Option wurde bestaetigt (Status) */

#define ALWAYS_ACTIVE /* CONNECTB3_REQ statt LISTENB3_REQ bei passivem Verbindungsaufbau */

#define NBOARD 4 /* number of D channels on one card */

#define ST_pbx 2 /* Bits! */

#if NSTALK < NBOARD
#error "Need NSTALK >= NBOARD"
#endif

#if NITALK <=NBOARD+5
#error "Need NITALK > NBOARD+5"
#endif
#define tappl talki
#define regnum talki[NBOARD+0] /* current interface I'm talking about */
#define message_id talki[NBOARD+1] /* Expected msgid for current interface */
#define tstate talki[NBOARD+2] /* State of the current interface */
#define chanmask talki[NBOARD+3] /* channels we have used up */

#if NICONN <= 18
#error "Need NICONN > 18"
#endif
#if NBCONN <= 1
#error "Need NBCONN > 1"
#endif
#define ncci0 conni[0]
#define waitflags conni[1] /* what're we waiting for */
#define msgid0 conni[2] /* for connect_resp */
#define hl_id conni[3] /* for higher-level stuff, like PPP */
#define capi_callref conni[4] /* for higher-level stuff, like PPP */
#define WF_CONNECTACTIVE_IND    5 /* conni[WF_*] for IDs pertaining to that message */
#define WF_CONNECTB3ACTIVE_IND  6
#define WF_DISCONNECTB3_IND     7
#define WF_DISCONNECT_IND       8
#define WF_SELECTB2_CONF        9
#define WF_SELECTB3_CONF       10
#define WF_LISTENB3_CONF       11
#define WF_CONNECTB3_IND       12
#define WF_CONNECTB3_CONF      13
#define WF_DISCONNECTB3_CONF   14
#define WF_DISCONNECT_CONF     15
#define WF_CONNECT_CONF        16
#define WF_CONTROL_EAZ         17
#define WF_PROTOCOLS           18

#define modlist connb
#define nmodlist NBCONN

/* Card states */
#define STATE_BOOTING 1
#define STATE_OPENING 2
#define STATE_REGISTER 3
#define STATE_DO_TRACE 4

#define STATE_CONF_FIRST 10
#define STATE_CONF_LAST 99
#define STATE_RUNNING 100

#define STATE_DEAD 255

#define RUN_CAPI_TCONN 01
#define RUN_CAPI_TFOO 02
#define RUN_CAPI_TWAITLOCAL 04
#define RUN_CAPI_TWAITFIRSTLOCAL 010

#define VAL_CAPI_TCONN ( 40 *HZ)    /* timer for delaying an ALERT response */
#define VAL_CAPI_TWAITLOCAL ( 5 *HZ)    /* wait for additional digits */
#define VAL_CAPI_TWAITFIRSTLOCAL ( HZ/2)    /* wait for the first digit */
#define VAL_CAPI_TFOO ( 10 * HZ)    /* timer for waiting for teardown */

static void CAPI_TCONN (isdn3_conn conn);
static void CAPI_TWAITLOCAL (isdn3_conn conn);
static void CAPI_TWAITFIRSTLOCAL (isdn3_conn conn);
static void CAPI_TFOO (isdn3_conn conn);

/* Connection states:
  0: unknown

  1: outgoing, remember to wait for the protocols
               send CONNECT_REQ
  2: outgoing, wait for CONNECT_CONF CONNECTACTIVE_IND protocols
               send SELECTB2_REQ SELECTB3_REQ
  3: outgoing, wait for SELECTB2_CONF SELECTB3_CONF
               send CONNECTB3_REQ
  4: outgoing, wait for CONNECTB3_CONF CONNECTB3ACTIVE_IND
               say "CONNECTED"

  6: incoming, wait for EAZ, remember to wait for the protocols
  7: incoming, wait for master program and protocols
               send SELECTB2_REQ SELECTB3_REQ
  8: incoming, wait for SELECTB2_CONF SELECTB3_CONF
               send LISTENB3_REQ CONNECT_RESP
  9: incoming, wait for LISTENB3_CONF CONNECTACTIVE_IND CONNECTB3_CONF CONNECTB3ACTIVE_IND
               say "CONNECTED"

 15: CONNECTED

 20: wait for DISCONNECTB3_CONF DISCONNECTB3_IND
 21: wait for DISCONNECT_CONF DISCONNECT_IND
 22: wait for DISCONNECTB3_CONF DISCONNECTB3_IND DISCONNECT_CONF DISCONNECT_IND
 99: delay wait 
*/

struct capi_info {
	unsigned short service;
	unsigned char subcard;
	unsigned char bchan; /* the _real_ channel */
	unsigned char flags;
#define INF_SPV 01
	unsigned char lnr[MAXNR];
	unsigned char nr[MAXNR];
	unsigned short waitlocal;
};

struct trace_timer {
	isdn3_talk talk;
#ifdef NEW_TIMEOUT
	long timer;
#endif
	uchar_t dchan;
};


extern void log_printmsg (void *log, const char *text, mblk_t * mp, const char*);

ushort_t capi_infotoid(ushort_t info);
static int report_incoming (isdn3_conn conn);
static int send_disconnect(isdn3_conn conn, char do_L3, ushort_t cause);
static void checkterm (isdn3_conn conn);

static void
capi_timerup (isdn3_conn conn)
{
    struct capi_info *info = conn->p_data;

    rtimer (CAPI_TCONN, conn);
    rtimer (CAPI_TWAITFIRSTLOCAL, conn);
	if(info == NULL)
    	rtimer (CAPI_TWAITLOCAL, conn);
	else
		rntimer(CAPI_TWAITLOCAL, conn, info->waitlocal);
    rtimer (CAPI_TFOO, conn);
}

#define setstate(a,b) Xsetstate(__LINE__,(a),(b))
static void
Xsetstate(unsigned int deb_line, isdn3_conn conn, uchar_t state)
{
    struct capi_info *info = conn->p_data;

printf ("Conn CAPI:%d %05lx: State %d --> %d\n", deb_line, conn->capi_callref, conn->state, state);

	if(conn->state == state)
		return;
	if(conn->state >= 1 && conn->state < 15 && (state < 1 || state >= 15))
		untimer(CAPI_TCONN, conn);
	switch(conn->state) {
	case 6:
		untimer(CAPI_TWAITLOCAL, conn);
		untimer(CAPI_TWAITFIRSTLOCAL, conn);
		break;
	case 99:
		untimer(CAPI_TFOO, conn);
		break;
	}
	if(state >= 1 && state < 15 && (conn->state < 1 || conn->state >= 15))
		timer(CAPI_TCONN,conn);

	conn->state = state;

	switch(state) {
	case 6:
		if(info != NULL) {
			ntimer(CAPI_TWAITLOCAL,conn,info->waitlocal);
			if(info->lnr == '\0')
				timer(CAPI_TWAITFIRSTLOCAL,conn);
		} else
			timer(CAPI_TWAITLOCAL,conn);
		break;
	case 7:
		if(conn->waitflags & 1<<WF_PROTOCOLS)
			isdn3_setup_conn (conn, EST_SETUP);
		break;
	case 99:
		timer(CAPI_TFOO,conn);
		break;
	}

	if(state >= 20)
		isdn3_setup_conn (conn, EST_DISCONNECT);

	/* Select/free a fake B channel. This is _not_ related to the channel
	   the ISDN is actually using, as it's shared if there's more than one
	   ISDN on the card. */
	if(state == 0 || state == 99) {
		if(conn->bchan != 0) {
			conn->talk->chanmask &=~ (1<<(conn->bchan-1));
			conn->bchan = 0;
			conn->minorstate &=~ MS_BCHAN;
			/* XXX send a clearing msg down? */
		}
	} else {
		if((conn->bchan == 0) && (state > 0 && state <= 15)) {
			int ch; unsigned long chm;
			for(ch=1,chm = 1;chm; chm <<= 1, ch++) {
				if(!(conn->talk->chanmask & chm)) {
					conn->bchan = ch;
					break;
				}
			}
		}
		if(conn->bchan != 0) {
			if(!(conn->minorstate & MS_BCHAN)) {
				conn->minorstate |= MS_BCHAN;
				conn->talk->chanmask |= 1<<(conn->bchan-1);
				if(conn->state != 6)
					isdn3_setup_conn (conn, EST_SETUP);
				else
					isdn3_setup_conn (conn, EST_NO_CHANGE);
			} else
				isdn3_setup_conn (conn, EST_NO_CHANGE);
		}
	}
}

static ushort_t
newmsgid(isdn3_talk talk)
{
	talk->message_id = ((talk->message_id + 1) & 0x3FFF) | 0x4000;

	return talk->message_id;
}

static int
putnr(uchar_t *nrto, uchar_t *nrfrom)
{
	uchar_t *nrorig = nrto;
	switch(*nrfrom) {
	case '0': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
		*nrto++ = 0x80; break;
	case '+': /* international */
		*nrto++ = 0x91; nrfrom++; break;
	case '=': /* national */
		*nrto++ = 0xA1; nrfrom++; break;
	case '-': /* subscriber */
		*nrto++ = 0xC1; nrfrom++; break;
	case '.': /* abbreviated */
	case '/': /* abbreviated */
		*nrto++ = 0xE1; nrfrom++; break;
	default:
		*nrto++ = 0x80; nrfrom++;
		break;
	}
	while(*nrfrom)
		*nrto++ = *nrfrom++;

	return (nrto - nrorig);
}


static int
getnr(uchar_t *nrto, uchar_t *nrfrom, int islocal, int flags)
{
	uchar_t *nrorig = nrfrom;
	int len = *nrfrom++;
	if(*nrto == '\0') {
		if(!islocal && !(flags & FL_BUG1)) {
			switch(*nrfrom & 0x70) {
			case 0x00: /* unknown */
				if(nrfrom[0] == 0x00 && nrfrom[1] == 0x83)
					*nrto++ = '='; /* at least one PBX is stupid */
				else if(nrfrom[0] == 0x81)
					*nrto++='.'; /* the very same PBX */
				break;
			case 0x10: /* international */
				*nrto++='+';
				break;
			case 0x20: /* national */
				*nrto++='=';
				break;
			case 0x30: /* network specific */
				break;
			case 0x40: /* subscriber */
				*nrto++='-';
				break;
			case 0x60: /* abbreviated */
				*nrto++='.';
				break;
			case 0x70: /* extension */
				*nrto++='.';
				break;
			}
		}
	} else {
		while(*nrto) nrto++; /* number becomes longer */
	}
	while (len-- > 0 && (*nrfrom++ & 0x80) == 0) ;
	while (len-- > 0)
		*nrto++ = *nrfrom++;
	*nrto = '\0';
	return nrfrom - nrorig;
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
	capi->appl = appl;
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
	if(data != NULL) {
		capi->datalen = msgdsize(data);
		linkb(mb,data);
	}
	printf(">CONTROL_REQ ");
	if((err = capi_send(talk,appl,CAPI_CONTROL_REQ,mb,msgid)) < 0)
		freeb(mb);
	return err;
}
/* open:
 * 0001 ID LLHH capiLen LLHH appl LLHH type LLHH messid  LLHH control
 * LLHH type CCCC protocol ...
 */

static void
talk_timer(struct trace_timer *tt)
{
	isdn3_talk talk = tt->talk;

	capi_sendctl(talk, talk->tappl[tt->dchan], CONTROL_TRACEREC_PLAY, NULL,newmsgid(talk));
}

static int
slam_conn(isdn3_talk talk, ushort_t appl, ushort_t plci, int cause)
{
	struct CAPI_disconnect_req *c3;
	int err;
	mblk_t *m3;
	
	m3 = allocb(sizeof(*c3),BPRI_MED);
	if(m3 == NULL)
		return -ENOMEM;

	c3 = ((typeof(c3))m3->b_wptr)++;
	bzero(c3,sizeof(*c3));
	c3->plci = plci;
	c3->cause = cause;
	printf(">DISCONNECT_REQ ");
	if((err = capi_send(talk,appl,CAPI_DISCONNECT_REQ,m3,
			newmsgid(talk))) < 0) 
		freemsg(m3);
	return err;
}

static void
CAPI_TWAITLOCAL(isdn3_conn conn)
{
	printf("CAPI_TWAITLOCAL %05lx\n",conn->capi_callref);
	conn->timerflags &= ~RUN_CAPI_TWAITLOCAL;
	if(conn->state != 6)
		return;
	setstate(conn,7);
	report_incoming(conn);
}

static void
CAPI_TWAITFIRSTLOCAL(isdn3_conn conn)
{
	printf("CAPI_TWAITFIRSTLOCAL %05lx\n",conn->capi_callref);
	conn->timerflags &= ~RUN_CAPI_TWAITFIRSTLOCAL;
	if(conn->state != 6)
		return;
	report_incoming(conn);
}

static void
CAPI_TFOO(isdn3_conn conn)
{
	printf("CAPI_TFOO %05lx\n",conn->capi_callref);
	conn->timerflags &= ~RUN_CAPI_TFOO;
	setstate(conn,0);
    isdn3_killconn (conn, 1);
}

static void
CAPI_TCONN(isdn3_conn conn)
{
	printf("CAPI_TCONN %05lx\n",conn->capi_callref);
	conn->timerflags &= ~RUN_CAPI_TCONN;
	if(conn->state < 1 || conn->state >= 15)
		return;
	send_disconnect(conn,0,0);
}

static int
checknrlen(isdn3_conn conn)
{
    struct capi_info *info = conn->p_data;

	if(info == NULL)
		return -ENXIO;
	if(!(conn->talk->state & (1<<(info->subcard+ST_pbx))))
		return -EINVAL;
	if(conn->state != 6)
		return -EINVAL;

	setstate(conn,6); /* this kicks the timers */
	return report_incoming(conn);
}

static int
send_setup(isdn3_conn conn)
{
	int err;
	struct CAPI_selectb2_req *c2;
	struct CAPI_selectb3_req *c3;
	struct dlpd *dl;
	mblk_t *m2,*m3;

	streamchar *s1,*s2,sx;
	streamchar *s3,*s4,sy = 0;
	mblk_t *mb = conn->modlist[0];

	if(mb == NULL)
		return -EAGAIN;
	if((mb = mb->b_cont) == NULL) {
		ushort_t id;
		streamchar *xi;
		mblk_t *mb1 = dupmsg(conn->modlist[0]);
		mblk_t *mb2 = dupmsg(conn->modlist[0]);
		if(mb1 == NULL || mb2 == NULL) {
			if(mb1 != NULL)
				freemsg(mb1);
			if(mb2 != NULL)
				freemsg(mb2);
			return -ENOMEM;
		}
		xi = mb1->b_rptr;
		while(m_getsx(mb1,&id) == 0) ;
		mb1->b_wptr = mb1->b_rptr;
		mb1->b_rptr = xi;
		while(m_getsx(mb2,&id) == 0) ;
		mb = mb2;
		mb1->b_cont = mb2;
		freemsg(conn->modlist[0]);
		conn->modlist[0] = mb1;
	}
	s1 = mb->b_rptr;
	while(s1 < mb->b_wptr && isspace(*s1))
		s1++;
	s2 = s1;
	while(s2 < mb->b_wptr && !isspace(*s2))
		s2++;
	sx = *s2; *s2 = '\0';

	s3 = s2+1;
	while(s3 < mb->b_wptr && isspace(*s3))
		s3++;
	s4 = s3;
	while(s4 < mb->b_wptr && !isspace(*s4))
		s4++;
	if(s3+1 < s4) {
		sy = *s4; *s4 = '\0';
	} else {
		s4 = NULL;
	}

	m2 = allocb(sizeof(*c2)+sizeof(*dl),BPRI_MED);
	m3 = allocb(sizeof(*c3),BPRI_MED);
	if(m2 == NULL || m3 == NULL) {
		*s2 = sx;
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

	c2->plci = conn->capi_callref;
	if(!strcmp(s1,"frame")) {
		c2->B2_proto = 0x02; /* transparent HDLC */
		c3->B3_proto = 0x04; /* transparent */
		if((s4 != NULL) && !strcmp(s3,"ppp")) 
			c3->B3_proto = 0xF1; /* sync PPP */
	} else if(!strcmp(s1,"modem")) {
		c2->B2_proto = 0xF0; /* V.22bis modem */
		c3->B3_proto = 0x04; /* transparent */
		if((s4 != NULL) && !strcmp(s3,"ppp"))
			c3->B3_proto = 0xF2; /* async PPP */
	} else if(!strcmp(s1,"trans")) {
		c2->B2_proto = 0x03; /* bittransparent */
		c3->B3_proto = 0x04; /* transparent */
	} else if(!strcmp(s1,"transalaw")) {
		c2->B2_proto = 0x03; /* bittransparent -- hmmm, need to send idle? */
		c3->B3_proto = 0x04; /* transparent */
	}
	c2->dlpdlen = sizeof(*dl);
	dl->data_length = 4096;
	c3->plci = conn->capi_callref;
	*s2 = sx;
	if(s4 != NULL)
		*s4 = sy;

	printf(">SELECTB2_REQ ");
	err = capi_send(conn->talk,conn->capi_callref >> 16, CAPI_SELECTB2_REQ, m2, conn->conni[WF_SELECTB2_CONF] = newmsgid(conn->talk));
	if(err < 0) {
		freemsg(m2);
		freemsg(m3);
	} else {
		printf(">SELECTB3_REQ ");
		err = capi_send(conn->talk,conn->capi_callref >> 16, CAPI_SELECTB3_REQ, m3, conn->conni[WF_SELECTB3_CONF] = newmsgid(conn->talk));
		if(err < 0)
			freemsg(m3);
		else {
			conn->waitflags  = 1 << WF_SELECTB2_CONF;
			conn->waitflags |= 1 << WF_SELECTB3_CONF;
			switch(conn->state) {
				case 2:
					setstate(conn,3);
					break;
				case 7:
					setstate(conn,8);
					break;
			}
		}
	}
	return err;
}

static int
send_open(isdn3_talk talk)
{
	struct apiopen *capi;
	int err;
	mblk_t *data;
#ifdef _euro_
#define DEFPROFILE "u_dss1_pmp"
#endif
#ifndef DEFPROFILE
#define DEFPROFILE "u_1tr6_pmp"
#endif
	char profile[32] = DEFPROFILE;
	{	/* Find correct driver name */
		int err; char skip = 0, subskip = 0;
		mblk_t *inf = talk->card->info;
		if(inf != NULL) {
			streamchar *sta = inf->b_rptr;
			ushort_t idx;

			while(m_getsx(inf,&idx) == 0) {
				long sap;
				switch(idx) {
				case ARG_PROTOCOL:
					if (m_geti(inf,&sap) == 0) 
						skip = (sap != SAPI_CAPI);
					break;
				case ARG_SUBPROT:
					if (m_geti(inf,&sap) == 0 && !skip) {
						switch(sap) {
						case SAPI_CAPI_BINTEC:
							skip=0;
							break;
						default:
							/* Wrong card. TODO: Do something! */
							inf->b_rptr = sta;
							return -ENXIO;
						}
					}
					break;
				case ARG_SUBCARD:
					if (m_geti(inf,&sap) == 0 && !skip) 
						subskip = (sap != talk->regnum+1);
					break;
				case ARG_STACK:
					if(skip || subskip)
						break;
					if((err = m_getstr(inf,profile,sizeof(profile)-1)) < 0)
						strcpy(profile,DEFPROFILE);
					break;
				case ARG_PBX:
					if(skip || subskip)
						break;
					talk->state |= 1<<(talk->regnum+ST_pbx);
					break;
				}
			}
			inf->b_rptr = sta;
		}
	}

	data = allocb(sizeof(*capi), BPRI_MED);
	if(data == NULL) /* XXX error processing */
		return -ENOMEM;
	capi = ((struct apiopen *)data->b_wptr)++;
	bzero(capi,sizeof(*capi));
	strcpy(capi->protocol,profile);
	capi->teid = ~0;
	capi->t3id = ~0;
	capi->time = time(NULL);
	printf(">CONTROL_API_OPEN ");
	if((err = capi_sendctl(talk,talk->tappl[talk->regnum],CONTROL_API_OPEN,data,newmsgid(talk))) < 0) {
		freemsg(data);
		return err;
	}
	talk->tstate = STATE_OPENING;
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
		talk->regnum = 0;
		talk->state |= IS_UP;
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
		talk->chanmask = 0;
		send_open(talk);
		break;
	case MDL_ERROR_IND:
	case DL_RELEASE_IND:
    case DL_RELEASE_CONF:
    case PH_DEACTIVATE_CONF:
    case PH_DEACTIVATE_IND:
    case PH_DISCONNECT_IND:
		talk->regnum = 0;
		talk->tstate = STATE_BOOTING;
		talk->state &=~ IS_UP;
		{
			isdn3_conn conn, nconn;
			for(conn = talk->conn; conn != NULL; conn = nconn) {
				nconn = conn->next;
				setstate(conn,0);
				checkterm(conn);
			}
		}
		break;
	}
	return 0;
}

static isdn3_conn
capi_findconn(isdn3_talk talk, ushort_t appl, ushort_t plci, int force)
{
	isdn3_conn conn;
	ulong_t cref = (appl << 16) | plci;

	if(plci == 0)
		return NULL;
	for(conn = talk->conn; conn != NULL; conn = conn->next) {
		if((force || (conn->state != 0)) && (conn->capi_callref == cref)) {
			printf(" (conn %05lx) ",conn->capi_callref);
			return conn;
		}
	}
	return NULL;
}

static isdn3_conn
capi_findconn3(isdn3_talk talk, ushort_t appl, ushort_t ncci, int force)
{
	isdn3_conn conn;

	if(ncci == 0)
		return NULL;
	for(conn = talk->conn; conn != NULL; conn = conn->next) {
		if((force || (conn->state != 0)) && ((conn->capi_callref >> 16) == appl) && (conn->ncci0 == ncci)) {
			printf(" (conn %05lx) ",conn->capi_callref);
			return conn;
		}
	}
	return NULL;
}

static isdn3_conn
capi_findconnm(isdn3_talk talk, ushort_t appl, ushort_t msgid, ushort_t index, int force)
{
	isdn3_conn conn;

	for(conn = talk->conn; conn != NULL; conn = conn->next) {
		if((force || (conn->state != 0)) && (conn->capi_callref >> 16) == appl && conn->conni[index] == msgid) {
			printf(" (conn %05lx) ",conn->capi_callref);
			return conn;
		}
	}
	return NULL;
}

static int
report_conn (isdn3_conn conn)
{
    int err = 0;

    mblk_t *mb = allocb (200, BPRI_MED);

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

static int
report_incoming (isdn3_conn conn)
{
    int err = 0;
    struct capi_info *info = conn->p_data;
	mblk_t *mb;

	if(info == NULL)
		return -ENXIO;

    mb = allocb (256, BPRI_MED);
    if (mb == NULL) {
        setstate (conn, 0);
        return -ENOMEM;
    }
    m_putid (mb, IND_INCOMING);
	if((conn->state == 6) && !(conn->talk->state & (1<<(info->subcard+ST_pbx))))
		m_putsx(mb,ARG_INCOMPLETE);
	
    conn_info (conn, mb);

    if ((err = isdn3_at_send (conn, mb, 0)) != 0) {
        freemsg (mb);
        setstate (conn, 0);
        return err;
    }
    return err;
}

static void 
report_addcause(mblk_t *mb, ushort_t info, ushort_t cause)
{
	if((cause == 0) && ((info == 0) || (info == 0x3400)))
		return;

	m_putsx(mb,ARG_CAUSE);
	if(cause != 0)
		m_putsx2(mb,n1_causetoid(cause&0x7F));
	if((info != 0) && (info != 0x3400))
		m_putsx2(mb,capi_infotoid(info));
}

static void
report_nocard (isdn3_talk talk, ushort_t info)
{
    mblk_t *mb = allocb (64, BPRI_MED);

	talk->tstate = STATE_DEAD;

    m_putid (mb, IND_NOCARD);
	m_putlx (mb, talk->card->id);
	report_addcause(mb,info,0);
    if (isdn3_at_send (NULL, mb, 0) < 0) 
        freemsg (mb);
    return;
}

static void
report_terminate (isdn3_conn conn, ushort_t info, ushort_t cause)
{
    int err = 0;

    mblk_t *mb = allocb (256, BPRI_MED);

	if(conn->state == 99 || conn->state == 0)
		m_putid(mb,IND_DISCONNECT);
	else
		m_putid(mb,IND_DISCONNECTING);
	report_addcause(mb,info,cause);
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
        report_terminate (conn,0,0);
        isdn3_killconn (conn, 1); /* XXX */
    }
}

static int
send_disconnect(isdn3_conn conn, char do_L3, ushort_t cause)
{
	int err;

	if(conn->state >= 21)
		return 0;
	if(conn->state == 0)
		return 0;
	if((conn->state >= 20) && (cause == N1_LocalProcErr))
		cause = 0;
	conn->waitflags = 0;
	report_terminate(conn,0,cause);
	switch(conn->state) {
	case 2:
		return -EAGAIN; /* no PLCI yet */
	case 6:
	case 7:
	case 8:
		{
			struct CAPI_connect_resp *c3;
			mblk_t *m3 = allocb(sizeof(*c3),BPRI_MED);
			if(m3 == NULL)
				return -ENOMEM;

			c3 = ((typeof(c3))m3->b_wptr)++;
			bzero(c3,sizeof(*c3));
			c3->plci = conn->capi_callref;
			c3->reject = cause ? ((cause >= 0x100) ? cause : cause | 0x3480) : 0x3480|N1_CallRejected;
			printf(">CONNECT_RESP ");
			if((err = capi_send(conn->talk,conn->capi_callref >> 16,CAPI_CONNECT_RESP,m3,
					conn->msgid0)) < 0) {
				setstate(conn,0);
				freemsg(m3);
			} else {
				setstate(conn,99);
        		report_terminate (conn,0,cause);
			}
		}
		break;
	case 15:
		if(do_L3) {
			struct CAPI_disconnectb3_req *c3;
			mblk_t *m3 = allocb(sizeof(*c3),BPRI_MED);
			if(m3 == NULL)
				return -ENOMEM;

			c3 = ((typeof(c3))m3->b_wptr)++;
			bzero(c3,sizeof(*c3));
			c3->ncci = conn->ncci0;
			printf(">DISCONNECTB3_REQ ");
			if((err = capi_send(conn->talk,conn->capi_callref >> 16,CAPI_DISCONNECTB3_REQ,m3,
					conn->conni[WF_DISCONNECTB3_CONF] = newmsgid(conn->talk))) < 0) {
				freemsg(m3);
			} else {
				conn->waitflags |= 1 << WF_DISCONNECTB3_CONF;
				conn->waitflags |= 1 << WF_DISCONNECTB3_IND;
				setstate(conn,20);
			}
		}
		/* FALL THRU */
	default:
		{
			struct CAPI_disconnect_req *c3;
			mblk_t *m3 = allocb(sizeof(*c3),BPRI_MED);
			if(m3 == NULL)
				return -ENOMEM;

			c3 = ((typeof(c3))m3->b_wptr)++;
			bzero(c3,sizeof(*c3));
			c3->plci = conn->capi_callref;
			c3->cause = cause;
			printf(">DISCONNECT_REQ ");
			if((err = capi_send(conn->talk,conn->capi_callref >> 16,CAPI_DISCONNECT_REQ,m3,
					conn->conni[WF_DISCONNECT_CONF] = newmsgid(conn->talk))) < 0) {
				freemsg(m3);
			} else {
				conn->waitflags |= 1 << WF_DISCONNECT_CONF;
				if(((conn->state > 3) && (conn->state < 6)) || (conn->state == 9) || (conn->state == 15))
					conn->waitflags |= 1 << WF_DISCONNECT_IND;

				if(conn->state < 20)
					setstate(conn,21);
				else
					setstate(conn,22);
			}
		}
	}
	return err;	
}

static int
send_dialout(isdn3_conn conn)
{
	int err;
	struct CAPI_connect_req *c2;
	struct capi_info *info = conn->p_data;
	mblk_t *m2;
	
	if(info == NULL)
		return -ENXIO;
	m2 = allocb(sizeof(*c2)+strlen(info->nr)+strlen(info->lnr)+3,BPRI_MED);

	if(m2 == NULL) 
		return -ENOMEM;
	c2 = ((typeof(c2))m2->b_wptr)++;
	bzero(c2,sizeof(*c2));
	c2->infomask = 0xC00000FF;
	{	/* Find correct info mask */
		int err; char skip = 0, subskip = 0;
		mblk_t *inf = conn->talk->card->info;
		if(inf != NULL) {
			streamchar *sta = inf->b_rptr;
			ushort_t idx;

			while(m_getsx(inf,&idx) == 0) {
				long sap;
				switch(idx) {
				case ARG_PROTOCOL:
					if (m_geti(inf,&sap) == 0) 
						skip = (sap != SAPI_CAPI);
					break;
				case ARG_SUBPROT:
					if (m_geti(inf,&sap) == 0 && !skip) {
						switch(sap) {
						case SAPI_CAPI_BINTEC:
							skip=0;
							break;
						default:
							/* Wrong card. TODO: Do something! */
							inf->b_rptr = sta;
							return -ENXIO;
						}
					}
					break;
				case ARG_SUBCARD:
					if (m_geti(inf,&sap) == 0 && !skip) 
						subskip = (sap != info->subcard+1);
					break;
				case ARG_LISTEN:
					if(skip || subskip)
						break;
					{
						long x;
						if((err = m_getx(inf,&x)) >= 0) {
							if((err = m_getx(inf,&x)) >= 0) {
								if((err = m_getx(inf,&x)) >= 0) {
									c2->infomask = x;
								}
							}
						}
					}
					break;
				}
			}
			inf->b_rptr = sta;
		}
	}
	c2->channel = (info->bchan ? info->bchan : CAPI_ANYBCHANNEL);
	c2->DST_service = info->service >> 8;
	c2->DST_addinfo = info->service;
	m2->b_wptr += (c2->telnolen = putnr(m2->b_wptr,info->nr));

	if(info->flags & INF_SPV) {
		c2->telnolen++;
		*m2->b_wptr++ = 'S';
	}
	if((*info->lnr != '\0') && (conn->talk->state & (1<<(info->subcard+ST_pbx)))) {
		uchar_t *lp = m2->b_wptr++;
		m2->b_wptr += (*lp = putnr(m2->b_wptr,info->lnr));
		c2->SRC_eaz = 0;
		c2->infomask |= 0x40;
	} else {
		c2->infomask &=~ 0x40;
		if(*info->lnr != '\0')
			c2->SRC_eaz = info->lnr[strlen(info->lnr)-1];
		else
			c2->SRC_eaz = '0';
	}
	conn->capi_callref = conn->talk->tappl[info->subcard]<<16;
	printf(">CONNECT_REQ ");
	if((err = capi_send(conn->talk,conn->talk->tappl[info->subcard],CAPI_CONNECT_REQ,m2,conn->conni[WF_CONNECT_CONF]=newmsgid(conn->talk))) < 0) 
		freemsg(m2);
	else {
		conn->waitflags |= 1<<WF_CONNECT_CONF;
		conn->waitflags |= 1<<WF_PROTOCOLS;
		setstate(conn,2);
	}
	return err;
}

static int
build_hl(isdn3_conn conn, mblk_t **mss)
{
	streamchar *origmp;
	ushort_t id;
	mblk_t *mp;
	streamchar sname[FMNAMESZ+1];
	int err;
	mblk_t *ms = NULL;
	
	mp = conn->modlist[conn->hl_id];
	if(mp == NULL) 
		return -EAGAIN;

	origmp = mp->b_rptr;
	if((err = m_getid(mp,&id)) < 0) {
		goto ret;
	}
	if(id != PROTO_MODULE) {
		err = -EAGAIN;
		goto ret;
	}
	while((err = m_getsx(mp,&id)) >= 0) {
		if(id == PROTO_MODULE)
			break;
	}
	if(err < 0)
		goto ret;
	if((err = m_getstr(mp,sname,FMNAMESZ)) < 0) 
		goto ret;
	if(!strcmp(sname,"ppp")) {
		ushort_t do_auth = 0;
		streamchar *startmp = mp->b_rptr;
		ushort_t *lenp;

		ms = allocb(200,BPRI_MED);
		if(ms == NULL) {
			err = -ENOMEM;
			goto ret;
		}

		while(m_getsx(mp,&id) >= 0) {
			switch(id) {
			case PPP_DO_PAP:
				do_auth = 0xC221;
				break;
			case PPP_DO_CHAP:
				do_auth = 0xC223;
				break;
			}
		}
		mp->b_rptr = startmp;

		*((ushort_t *)ms->b_wptr)++ = htons(0xC021); /* LCP */

		lenp = (ushort_t *)ms->b_wptr;
		*((ushort_t *)ms->b_wptr)++ = htons(4+20); /* Laenge gesamt */

		if(do_auth) {
			*lenp = htons(ntohs(*lenp)+6);
			*((uchar_t *)ms->b_wptr)++ = 0x03; /* Typ */
			if(conn->state < 5)  /* outgoing */
				*((uchar_t *)ms->b_wptr)++ = OPT_LOC_ALLOW|OPT_REM_ALLOW|OPT_LOC_WANT|OPT_LOC_MAND;
			else 
				*((uchar_t *)ms->b_wptr)++ = OPT_LOC_ALLOW|OPT_REM_ALLOW;
			*((uchar_t *)ms->b_wptr)++ = 2;
			*((uchar_t *)ms->b_wptr)++ = 0;
			*((ushort_t *)ms->b_wptr)++ = htons(do_auth);
		}
		*((ulong_t *)ms->b_wptr)++ = htonl(0x01110000); /* MRU */
		*((ulong_t *)ms->b_wptr)++ = htonl(0x02110000); /* async */
		*((ulong_t *)ms->b_wptr)++ = htonl(0x05130000); /* magic */
		*((ulong_t *)ms->b_wptr)++ = htonl(0x07130000); /* protocol compression */
		*((ulong_t *)ms->b_wptr)++ = htonl(0x08130000); /* ac field compression */

/* 43 00 01 00 83 02 8a a8 64 06 38 c0 21 00 22 01 11 00 00 02 11 00 00 03
11 02 00 00 00 04 00 00 00 05 93 00 00 07 13 00 00 08 13 00 00 80 21 00 16
01 11 00 00 02 11 02 00 00 00 03 95 04 00 00 00 00 00 */
		while(m_getsx(mp,&id) >= 0) {
			ushort_t *len_off;

			switch(id) {
			case PROTO_MODULE: break;
			case PPP_IP_VANJ:
				*((ushort_t *)ms->b_wptr)++ = htons(0x8021); /* IPCP */
				len_off = (ushort_t *)ms->b_wptr;
				*((ushort_t *)ms->b_wptr)++ = htons(8); /* Laenge gesamt */
				*((uchar_t *)ms->b_wptr)++ = 0x02; /* Typ */
				*((uchar_t *)ms->b_wptr)++ = 0x04; /* Laenge */
				*((ushort_t *)ms->b_wptr)++ = htons(0x2d); /* VJ */
				goto ip_go;
			case PPP_IP:
				*((ushort_t *)ms->b_wptr)++ = htons(0x8021); /* IPCP */
				len_off = (ushort_t *)ms->b_wptr;
				*((ushort_t *)ms->b_wptr)++ = htons(4+4); /* Laenge gesamt */
			  ip_go:
				*((ulong_t *)ms->b_wptr)++ = htonl(0x01110000); /* addresses */
			 	{
					unsigned long ipaddr;
					err = m_getip(mp,&ipaddr);
					*len_off = htons(ntohs(*len_off) + 8);
					*((uchar_t *)ms->b_wptr)++ = 0x03;
					if(err >= 0)
						*((uchar_t *)ms->b_wptr)++ = OPT_LOC_ALLOW|OPT_REM_ALLOW|OPT_LOC_WANT;
					else
						*((uchar_t *)ms->b_wptr)++ = OPT_LOC_ALLOW|OPT_REM_ALLOW;
					*((uchar_t *)ms->b_wptr)++ = 4;
					*((uchar_t *)ms->b_wptr)++ = 0;
					if(err > 0)
						*((ulong_t *)ms->b_wptr)++ = htonl(ipaddr);
					else
						*((ulong_t *)ms->b_wptr)++ = 0;
				}
				break;
			case PPP_DO_PAP:
			case PPP_DO_CHAP:
				{
					streamchar usera[20],pwa[20],userb[20],pwb[20];
					streamchar *s;
					
					if((err = m_getstr(mp,usera,sizeof(usera)-1)) < 0)
						goto ret;
					if((err = m_getstr(mp,pwa,sizeof(pwa)-1)) < 0)
						goto ret;
					if((err = m_getstr(mp,userb,sizeof(userb)-1)) < 0)
						goto ret;
					if((err = m_getstr(mp,pwb,sizeof(pwb)-1)) < 0)
						goto ret;

					*((ushort_t *)ms->b_wptr)++ = htons(do_auth);

					if(!strcmp(usera,"-")) *usera = '\0';
					if(!strcmp(pwa,"-")) *pwa = '\0';
					if(!strcmp(userb,"-")) *userb = '\0';
					if(!strcmp(pwb,"-")) *pwb = '\0';
					*((ushort_t *)ms->b_wptr)++ = htons(8+strlen(usera)+strlen(userb)+strlen(pwa)+strlen(pwb)); /* Laenge gesamt */
					*ms->b_wptr++ = strlen(usera);
					for(s=usera;*s;s++) *ms->b_wptr++ = *s;
					*ms->b_wptr++ = strlen(pwa);
					for(s=pwa  ;*s;s++) *ms->b_wptr++ = *s;
					*ms->b_wptr++ = strlen(userb);
					for(s=userb;*s;s++) *ms->b_wptr++ = *s;
					*ms->b_wptr++ = strlen(pwb);
					for(s=pwb  ;*s;s++) *ms->b_wptr++ = *s;
				}
				break;
			default:
				err = -ENXIO;
				goto ret;
			}

		}
		mp->b_rptr = origmp;
	} else {
		err = -ENXIO;
		goto ret;
	}
  ret:
  	if((err < 0) && (ms != NULL))
		freemsg(ms);
	else {
		*mss = ms;
		err = ms->b_wptr - ms->b_rptr;
	}
	mp->b_rptr = origmp;
	return err;
}

static int
after_active(isdn3_conn conn, int send_assoc)
{
	int err;

	switch(conn->state) {
	default:
		err = -EIO;
		if(send_disconnect(conn,0,N1_LocalProcErr) < 0)
			setstate(conn,0);
		break;
	case 3:
		if(conn->waitflags)
			return 0; /* not yet */
		{
			struct CAPI_connectb3_req *c2;
			mblk_t *m2;

			m2 = allocb(sizeof(*c2),BPRI_MED);
			if(m2 == NULL)
				return -ENOMEM;
			c2 = ((typeof(c2))m2->b_wptr)++;
			bzero(c2,sizeof(*c2));

			c2->plci = conn->capi_callref;
			if(conn->hl_id) {
				mblk_t *m3;
				err = build_hl(conn,&m3);
				if(err >= 0) {
					c2->ncpilen = err;
					linkb(m2,m3);
				}
			}
			printf(">CONNECTB3_REQ ");
			err = capi_send(conn->talk,conn->capi_callref >> 16, CAPI_CONNECTB3_REQ, m2, conn->conni[WF_CONNECTB3_CONF] = newmsgid(conn->talk));
			if(err < 0) 
				freemsg(m2);
			else {
				conn->waitflags  = 1<<WF_CONNECTB3_CONF;
				conn->waitflags |= 1<<WF_CONNECTB3ACTIVE_IND;
				setstate(conn,4);
			}
		}
		break;
	case 8:
		if(conn->waitflags)
			return 0; /* not yet */
		{
#ifdef ALWAYS_ACTIVE
			struct CAPI_connectb3_req *c2;
#else
			struct CAPI_listenb3_req *c2;
#endif
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
			c2->plci = conn->capi_callref;
			c3->plci = conn->capi_callref;
#ifdef ALWAYS_ACTIVE
			if(conn->hl_id) {
				mblk_t *m4;
				err = build_hl(conn,&m4);
				if(err >= 0) {
					c2->ncpilen = err;
					linkb(m3,m4);
				}
			}
#endif
			printf(">CONNECT_RESP ");
			if((err = capi_send(conn->talk,conn->capi_callref>>16,CAPI_CONNECT_RESP,m3,conn->msgid0)) < 0) {
				freemsg(m2);
				freemsg(m3);
			} else
#ifdef ALWAYS_ACTIVE
				if(printf(">CONNECTB3_REQ "),(err = capi_send(conn->talk,conn->capi_callref>>16,CAPI_CONNECTB3_REQ,m2,conn->conni[WF_CONNECTB3_CONF] = newmsgid(conn->talk))) < 0)
#else
				if(printf(">LISTENB3_REQ "),(err = capi_send(conn->talk,conn->capi_callref>>16,CAPI_LISTENB3_REQ,m2,conn->conni[WF_LISTENB3_CONF] = newmsgid(conn->talk))) < 0)
#endif
				freemsg(m2);
			else {
#ifdef ALWAYS_ACTIVE
				conn->waitflags  = 1<<WF_CONNECTB3_CONF;
#else
				conn->waitflags  = 1<<WF_LISTENB3_CONF;
				conn->waitflags |= 1<<WF_CONNECTB3_IND;
#endif
				conn->waitflags |= 1<<WF_CONNECTACTIVE_IND;
				conn->waitflags |= 1<<WF_CONNECTB3ACTIVE_IND;
				setstate(conn,9);
			}
		}
	case 4:
	case 9:
		if(send_assoc && (conn->minor == 0)) {
			printf("CAPI: minor zero in send_assoc!\n");
			return -EIO;
		}
		if(send_assoc) {
			mblk_t *mz = allocb(64,BPRI_MED);
			if(mz == NULL)
				return -ENOMEM;

            isdn3_setup_conn (conn, EST_LISTEN);

			m_putid(mz,CMD_CARDPROT);
			m_putsx(mz,ARG_ASSOC);
			m_puti(mz,conn->capi_callref >> 16);
			m_puti(mz,conn->capi_callref & 0xFFFF);
			m_puti(mz,conn->ncci0);
			err = isdn3_send_conn(conn->minor,AS_PROTO,mz);
			if(err < 0) {
				freemsg(mz);
				return err;
			}
		}
		if(conn->waitflags)
			return 0; /* not yet */
		setstate(conn,15);
		report_conn(conn);
		isdn3_setup_conn (conn, EST_CONNECT);
		err = 0;
		break;
	}
	return err;
}

static int
recv (isdn3_talk talk, char isUI, mblk_t * data)
{
	struct CAPI_every_header *capi;
	streamchar *origmb;
	int err = 0;
	isdn3_conn conn = 0;

	if(talk->tstate == STATE_DEAD) 
		return -ENXIO;
	
	printf("CAPI: recv %d, in state %ld: ",isUI,talk->tstate);
	origmb = data->b_rptr;
	if(data->b_wptr-data->b_rptr < sizeof(*capi)) 
		goto less_room;

	capi = ((typeof(capi))data->b_rptr)++;

	switch(capi->PRIM_type) {
	default:
	  	printf("Unknown primary type 0x%04x\n",capi->PRIM_type);
		err = -ENXIO;
		goto printit;
	
	case CAPI_DISCONNECTB3_CONF:
		printf("DISCONNECTB3_CONF: ");
		{
			struct CAPI_disconnectb3_conf *c2;
			if(data->b_wptr-data->b_rptr < sizeof(*c2))
				goto less_room;
			c2 = ((typeof(c2))data->b_rptr)++;
			if((conn = capi_findconn3(talk,capi->appl,c2->ncci,0)) == NULL) {
				printf("CAPI error: DISCONNECTB3_CONF has unknown cref3 %x%04x\n",capi->appl,c2->ncci);
				err = -ENXIO;
				break;
			}
			if(c2->info != 0) {
				printf("CAPI error: DISCONNECTB3_CONF returns %04x\n",c2->info);
#if 0
				send_disconnect(conn,0,N1_OutOfOrder);
				break;
#endif
			}
			if(conn->waitflags & (1<<WF_DISCONNECTB3_CONF)) {
				conn->waitflags &=~ (1<<WF_DISCONNECTB3_CONF);
				if(conn->waitflags == 0) {
					setstate(conn,0);
					break;
				}

			} else {
				printf("CAPI error: DISCONNECTB3_CONF in wrong state %d, wf 0x%lx\n",conn->state,conn->waitflags);
				err = -EIO;
				break;
			}
		}
		break;
	case CAPI_DISCONNECTB3_IND:
		printf("DISCONNECTB3_IND: ");
		{
			struct CAPI_disconnectb3_ind *c2;
			if(data->b_wptr-data->b_rptr < sizeof(*c2))
				goto less_room;
			c2 = ((typeof(c2))data->b_rptr)++;
			if((conn = capi_findconn3(talk,capi->appl,c2->ncci,0)) == NULL) {
				printf("CAPI error: DISCONNECTB3_IND has unknown cref3 %x%04x\n",capi->appl,c2->ncci);
				err = -ENXIO;
				break;
			}
			if((c2->info == 0) && (conn->waitflags & (1<<WF_DISCONNECTB3_IND))) {
				conn->waitflags &=~ (1<<WF_DISCONNECTB3_IND);
				if(conn->waitflags == 0) {
					if(conn->state == 20) {
						err = send_disconnect(conn,0,0);
						if(err < 0)
							setstate(conn,0);
					} else 
						setstate(conn,99);
					report_terminate(conn,0,0);
				}
			} else if((c2->info != 0) || (conn->state >= 20)) {
				printf("CAPI error: DISCONNECTB3_IND in wrong state %d, info %04x\n",conn->state,c2->info);
				isdn3_setup_conn (conn, EST_DISCONNECT);
				send_disconnect(conn,0,N1_OutOfOrder);
				report_terminate(conn,c2->info,0);
			}
			{
				int err3 = 0;
				struct CAPI_disconnectb3_resp *c3;
				mblk_t *m3 = allocb(sizeof(*c3),BPRI_MED);
				if(m3 == NULL) 
					err3 = -ENOMEM;
				if(err3 == 0) {
					c3 = ((typeof(c3))m3->b_wptr)++;
					bzero(c3,sizeof(*c3));
					c3->ncci = c2->ncci;
					printf(">DISCONNECTB3_RESP ");
					if((err3 = capi_send(talk,capi->appl,CAPI_DISCONNECTB3_RESP,m3,capi->messid)) < 0)
						freemsg(m3);
				}
				if(err == 0)
					err = send_disconnect(conn,0,0);
				if(err == 0)
					err = err3;
			}
		}
		break;
	case CAPI_DISCONNECT_IND:
		printf("DISCONNECT_IND: ");
		{
			struct CAPI_disconnect_ind *c2;
			if(data->b_wptr-data->b_rptr < sizeof(*c2))
				goto less_room;
			c2 = ((typeof(c2))data->b_rptr)++;
			if((conn = capi_findconn(talk,capi->appl,c2->plci,0)) == NULL) {
				printf("CAPI error: DISCONNECT_IND has unknown cref %x%04x\n",capi->appl,c2->plci);
				err = -ENXIO;
				break;
			}
			if(conn->waitflags & (1<<WF_DISCONNECT_IND)) {
				conn->waitflags &=~ (1<<WF_DISCONNECT_IND);
				if(conn->waitflags == 0) {
					setstate(conn,99);
					report_terminate(conn,c2->info,0);
				}
			} else if((conn->state != 0) && (conn->state != 99) && ((c2->info != 0) || (conn->state >= 21))) {
				conn->waitflags &=~ (1<<WF_DISCONNECT_IND);
				setstate(conn,99);
				isdn3_setup_conn (conn, EST_DISCONNECT);
				report_terminate(conn,c2->info,0);
			} else if(conn->state != 0 && conn->state != 99) {
				setstate(conn,99);
				report_terminate(conn,c2->info,0);
			}
			{
				int err3 = 0;
				struct CAPI_disconnect_resp *c3;
				mblk_t *m3 = allocb(sizeof(*c3),BPRI_MED);
				if(m3 == NULL) 
					err3 = -ENOMEM;
				if(err3 == 0) {
					c3 = ((typeof(c3))m3->b_wptr)++;
					bzero(c3,sizeof(*c3));
					c3->plci = c2->plci;
					printf(">DISCONNECT_RESP ");
					if((err3 = capi_send(talk,capi->appl,CAPI_DISCONNECT_RESP,m3,capi->messid)) < 0)
						freemsg(m3);
				}
				if(err == 0)
					err = err3;
			}
		}
		break;

	case CAPI_DISCONNECT_CONF:
		printf("DISCONNECT_CONF: ");
		{
			struct CAPI_disconnect_conf *c2;
			if(data->b_wptr-data->b_rptr < sizeof(*c2))
				goto less_room;
			c2 = ((typeof(c2))data->b_rptr)++;
			if((conn = capi_findconn(talk,capi->appl,c2->plci,0)) == NULL) {
				printf("CAPI error: DISCONNECT_CONF has unknown cref %x%04x\n",capi->appl,c2->plci);
				err = -ENXIO;
				break;
			}
			if((c2->info == 0) || (conn->waitflags & (1<<WF_DISCONNECT_CONF))) {
				conn->waitflags &=~ (1<<WF_DISCONNECT_CONF);
				if(conn->waitflags == 0) {
					setstate(conn,99);
					report_terminate(conn,c2->info,0);
				}
			} else {
				printf("CAPI error: DISCONNECT_CONF in wrong state %d, info %04x, wf 0x%lx\n",conn->state,c2->info,conn->waitflags);
				isdn3_setup_conn (conn, EST_DISCONNECT);
				setstate(conn,0);
				report_terminate(conn,c2->info,0);
			}
		}
		break;

	case CAPI_CONNECT_CONF:
		printf("CONNECT_CONF: ");
		{
			struct CAPI_connect_conf *c2;
			if(data->b_wptr-data->b_rptr < sizeof(*c2))
				goto less_room;
			c2 = ((typeof(c2))data->b_rptr)++;
			if((c2->info == 0) && ((conn =
				capi_findconn(talk,capi->appl,c2->plci,0)) != NULL)) {
				printf("CAPI error: CONNECT_CONF has known cref %x%04x\n",capi->appl,c2->plci);
				setstate(conn,0);
				report_terminate(conn,c2->info,0);
				err = -ENXIO;
				break;
			}
			if((conn =
				capi_findconnm(talk,capi->appl,capi->messid,WF_CONNECT_CONF,0)) == NULL) {
				printf("CAPI error: CONNECT_CONF has unknown msgid %04x.%04x\n",capi->appl,capi->messid);
				slam_conn(talk,capi->appl,c2->plci,0);
				err = -ENXIO;
				break;
			}
			conn->capi_callref = (capi->appl << 16) | c2->plci;
			if((c2->info == 0) && (conn->waitflags & (1<<WF_CONNECT_CONF))) {
				conn->waitflags &=~ (1<<WF_CONNECT_CONF);
				if(conn->waitflags == 0) {
					if((err = send_setup(conn)) < 0) {
						if(send_disconnect(conn,0,N1_OutOfOrder) < 0) {
							setstate(conn,99);
						}
					} else {
						conn->waitflags |= 1<<WF_CONNECTACTIVE_IND;
					}
				}
			} else {
				printf("CAPI error: CONNECT_CONF in wrong state %d, info %04x, wf 0x%lx\n",conn->state,c2->info,conn->waitflags);
				isdn3_setup_conn (conn, EST_DISCONNECT);
				if(send_disconnect(conn,0,N1_LocalProcErr) < 0)
					setstate(conn,99);
				report_terminate(conn,c2->info,0);
			}
		}
		break;

	case CAPI_SELECTB2_CONF:
		printf("SELECTB2_CONF: ");
		{
			struct CAPI_selectb2_conf *c2;
			if(data->b_wptr-data->b_rptr < sizeof(*c2))
				goto less_room;
			c2 = ((typeof(c2))data->b_rptr)++;
			if((conn = capi_findconn(talk,capi->appl,c2->plci,0)) == NULL) {
				printf("CAPI error: SELECTB2_CONF has unknown cref %x%04x\n",capi->appl,c2->plci);
				slam_conn(talk,capi->appl,c2->plci,0);
				err = -ENXIO;
				break;
			}
			if((c2->info == 0) && (conn->waitflags & (1<<WF_SELECTB2_CONF))) {
				conn->waitflags &=~ (1<<WF_SELECTB2_CONF);
				err = after_active(conn,0);
			} else {
				printf("CAPI error: SELECTB2_CONF in wrong state %d, info %04x, wf 0x%lx\n",conn->state,c2->info,conn->waitflags);
				if(send_disconnect(conn,0,N1_OutOfOrder) < 0) 
					setstate(conn,99);
				report_terminate(conn,c2->info,0);
			}
		}
		break;
	case CAPI_SELECTB3_CONF:
		printf("SELECTB3_CONF: ");
		{
			struct CAPI_selectb3_conf *c2;
			if(data->b_wptr-data->b_rptr < sizeof(*c2))
				goto less_room;
			c2 = ((typeof(c2))data->b_rptr)++;
			if((conn = capi_findconn(talk,capi->appl,c2->plci,0)) == NULL) {
				printf("CAPI error: SELECTB3_CONF has unknown cref %x%04x\n",capi->appl,c2->plci);
				slam_conn(talk,capi->appl,c2->plci,0);
				err = -ENXIO;
				break;
			}
			if((c2->info == 0) && (conn->waitflags & (1<<WF_SELECTB3_CONF))) {
				conn->waitflags &=~ (1<<WF_SELECTB3_CONF);
				err = after_active(conn,0);
			} else {
				printf("CAPI error: SELECTB3_CONF in wrong state %d, info %04x, wf 0x%lx\n",conn->state,c2->info,conn->waitflags);
				if(conn->state < 15) {
					if(send_disconnect(conn,0,N1_OutOfOrder) < 0) 
						setstate(conn,99);
					report_terminate(conn,c2->info,0);
				}
			}
		}
		break;
	case CAPI_CONNECTACTIVE_IND:
		printf("CONNECTACTIVE_IND: ");
		{
			struct CAPI_connectactive_ind *c2;
			if(data->b_wptr-data->b_rptr < sizeof(*c2))
				goto less_room;
			c2 = ((typeof(c2))data->b_rptr)++;
			if((conn = capi_findconn(talk,capi->appl,c2->plci,0)) == NULL) {
				printf("CAPI error: CONNECTACTIVE_IND has unknown cref %x%04x\n",capi->appl,c2->plci);
				slam_conn(talk,capi->appl,c2->plci,0);
				err = -ENXIO;
				break;
			}
			if(conn->waitflags & (1<<WF_CONNECTACTIVE_IND)) {
				conn->waitflags &=~ (1<<WF_CONNECTACTIVE_IND);
				err = after_active(conn,0);
			} else  {
				printf("CAPI error: CONNECTACTIVE_IND in wrong state %d, wf 0x%lx\n",conn->state,conn->waitflags);
				if(send_disconnect(conn,0,N1_OutOfOrder) < 0) 
					setstate(conn,99);
			}
			{
				int err3 = 0;
				struct CAPI_connectactive_resp *c3;
				mblk_t *m3 = allocb(sizeof(*c3),BPRI_MED);
				if(m3 == NULL) 
					err3 = -ENOMEM;
				if(err3 == 0) {
					c3 = ((typeof(c3))m3->b_wptr)++;
					bzero(c3,sizeof(*c3));
					c3->plci = c2->plci;
					printf(">CONNECTACTIVE_RESP ");
					if((err3 = capi_send(talk,capi->appl,CAPI_CONNECTACTIVE_RESP,m3,capi->messid)) < 0)
						freemsg(m3);
				}
				if(err == 0)
					err = err3;
			}
		}
		break;
	case CAPI_CONNECTB3_IND:
		printf("CONNECTB3_IND: ");
		{
			struct CAPI_connectb3_ind *c2;
			if(data->b_wptr-data->b_rptr < sizeof(*c2))
				goto less_room;
			c2 = ((typeof(c2))data->b_rptr)++;
			if((conn = capi_findconn(talk,capi->appl,c2->plci,0)) == NULL) {
				printf("CAPI error: CONNECTB3_IND has unknown cref %x%04x\n",capi->appl,c2->plci);
				slam_conn(talk,capi->appl,c2->plci,0);
				err = -ENXIO;
				break;
			}
			if(conn->waitflags & (1<<WF_CONNECTB3_IND)) {
				conn->waitflags &=~ (1<<WF_CONNECTB3_IND);
				conn->ncci0 = c2->ncci;
				err = after_active(conn,1);
			} else  {
				printf("CAPI error: CONNECTB3_IND in wrong state %d, wf 0x%lx\n",conn->state,conn->waitflags);
				if(send_disconnect(conn,0,N1_OutOfOrder) < 0) 
					setstate(conn,99);
			}
			{
				int err3 = 0;
				struct CAPI_connectb3_resp *c3;
				mblk_t *m3 = allocb(sizeof(*c3),BPRI_MED);
				if(m3 == NULL) 
					err3 = -ENOMEM;
				if(err3 == 0) {
					c3 = ((typeof(c3))data->b_wptr)++;
					bzero(c3,sizeof(*c3));
					c3->ncci = c2->ncci;
					if(err)
						c3->reject = 1;
					printf(">CONNECTB3_RESP ");
					if((err3 = capi_send(talk,capi->appl,CAPI_CONNECTB3_RESP,m3,capi->messid)) < 0)
						freemsg(m3);
				}
				if(err == 0)
					err = err3;
			}
		}
		break;
	case CAPI_LISTENB3_CONF:
		printf("LISTENB3_CONF: ");
		{
			struct CAPI_listenb3_conf *c2;
			if(data->b_wptr-data->b_rptr < sizeof(*c2))
				goto less_room;
			c2 = ((typeof(c2))data->b_rptr)++;
			if((conn = capi_findconn(talk,capi->appl,c2->plci,0)) == NULL) {
				printf("CAPI error: LISTENB3_CONF has unknown cref %x%04x\n",capi->appl,c2->plci);
				slam_conn(talk,capi->appl,c2->plci,0);
				err = -ENXIO;
				break;
			}
			if((c2->info == 0) && (conn->waitflags & (1<<WF_LISTENB3_CONF))) {
				conn->waitflags &=~ (1<<WF_LISTENB3_CONF);
				err = after_active(conn,0);
			} else  {
				err = -EIO;
				printf("CAPI error: LISTENB3_CONF in wrong state %d, info %04x, wf 0x%lx\n",conn->state,c2->info,conn->waitflags);
				if(send_disconnect(conn,0,N1_OutOfOrder) < 0) 
					setstate(conn,99);
			}
		}
		break;
	case CAPI_CONNECTB3_CONF:
		printf("CONNECTB3_CONF: ");
		{
			struct CAPI_connectb3_conf *c2;
			if(data->b_wptr-data->b_rptr < sizeof(*c2))
				goto less_room;
			c2 = ((typeof(c2))data->b_rptr)++;
			if((conn = capi_findconn(talk,capi->appl,c2->plci,0)) == NULL) {
				printf("CAPI error: CONNECTB3_CONF has unknown cref %x%04x\n",capi->appl,c2->plci);
				slam_conn(talk,capi->appl,c2->plci,0);
				err = -ENXIO;
				break;
			}
			if((c2->info == 0) && (conn->waitflags & (1<<WF_CONNECTB3_CONF))) {
				conn->waitflags &=~ (1<<WF_CONNECTB3_CONF);
				conn->ncci0 = c2->ncci;
				err = after_active(conn,1);
			} else  {
				err = -EIO;
				printf("CAPI error: CONNECTB3_CONF in wrong state %d, info %04x, wf 0x%lx\n",conn->state,c2->info,conn->waitflags);
				if(send_disconnect(conn,0,N1_OutOfOrder) < 0) 
					setstate(conn,99);
			}
		}
		break;

	case CAPI_CONNECTB3ACTIVE_IND:
		printf("CONNECTB3ACTIVE_IND: ");
		{
			struct CAPI_connectb3active_ind *c2;
			if(data->b_wptr-data->b_rptr < sizeof(*c2))
				goto less_room;
			c2 = ((typeof(c2))data->b_rptr)++;
			if((conn = capi_findconn3(talk,capi->appl,c2->ncci,0)) == NULL) {
				printf("CAPI error: CONNECTB3ACTIVE_IND has unknown cref3 %x%04x\n",capi->appl,c2->ncci);
				err = -ENXIO;
				break;
			}
			if(conn->waitflags & (1<<WF_CONNECTB3ACTIVE_IND)) {
				conn->waitflags &=~ (1<<WF_CONNECTB3ACTIVE_IND);
				err = after_active(conn,0);
			} else  {
				printf("CAPI error: CONNECTB3ACTIVE_IND in wrong state %d, wf 0x%lx\n",conn->state,conn->waitflags);
				if(send_disconnect(conn,0,N1_OutOfOrder) < 0) 
					setstate(conn,99);
			}
			{
				int err3 = 0;
				struct CAPI_connectb3active_resp *c3;
				mblk_t *m3 = allocb(sizeof(*c3),BPRI_MED);
				if(m3 == NULL) 
					err3 = -ENOMEM;
				if(err3 == 0) {
					c3 = ((typeof(c3))m3->b_wptr)++;
					bzero(c3,sizeof(*c3));
					c3->ncci = c2->ncci;
					printf(">CONNECTB3ACTIVE_RESP ");
					if((err3 = capi_send(talk,capi->appl,CAPI_CONNECTB3ACTIVE_RESP,m3,capi->messid)) < 0)
						freemsg(m3);
				}
				if(err == 0)
					err = err3;
			}
		}
		break;

	case CAPI_CONNECT_IND:
		printf("CONNECT_IND: ");
		{
			struct CAPI_connect_ind *c2;

			if(data->b_wptr-data->b_rptr < sizeof(*c2))
				goto less_room;
			c2 = ((typeof(c2))data->b_rptr)++;
			if(data->b_wptr-data->b_rptr < c2->telnolen)
				goto less_room;
			if((conn = capi_findconn(talk,capi->appl,c2->plci,0)) != NULL) { /* Duplicate. Hmmm. */
				printf("CAPI error: Incoming call has dup cref %x%04x for conn %d\n",capi->appl, c2->plci,conn->conn_id);
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

						bzero(info,sizeof(*info));
						{
							int i;
							for(i=0;i < talk->card->dchans; i++) {
								if(capi->appl == talk->tappl[i]) {
									info->subcard = i;
									break;
								}
							}
						}
						info->waitlocal = VAL_CAPI_TWAITLOCAL/HZ;
						{	/* get real wait value */
							int err; char skip = 0, subskip = 0;
							mblk_t *inf = talk->card->info;
							if(inf != NULL) {
								streamchar *sta = inf->b_rptr;
								ushort_t idx;

								while(m_getsx(inf,&idx) == 0) {
									long sap;
									switch(idx) {
									case ARG_PROTOCOL:
										if (m_geti(inf,&sap) == 0) 
											skip = (sap != SAPI_CAPI);
										break;
									case ARG_SUBPROT:
										if (m_geti(inf,&sap) == 0 && !skip) {
											switch(sap) {
											case SAPI_CAPI_BINTEC:
												skip=0;
												break;
											default:
												/* Wrong card. TODO: Do something! */
												inf->b_rptr = sta;
												return -ENXIO;
											}
										}
										break;
									case ARG_SUBCARD:
										if (m_geti(inf,&sap) == 0 && !skip) 
											subskip = (sap != info->subcard+1);
										break;
									case ARG_LWAIT:
										if(skip || subskip)
											break;
										{
											long x;
											if((err = m_geti(inf,&x)) >= 0)
												info->waitlocal = x;
										}
										break;
									}
								}
								inf->b_rptr = sta;
							}
						}
						if(c2->DST_eaz) {
							info->lnr[0] = '/';
							info->lnr[1] = c2->DST_eaz;
							info->lnr[2] = '\0';
						} else
							info->lnr[0] = '\0';
						info->service = (c2->DST_service << 8) | c2->DST_addinfo;
						if(c2->telnolen > 1) {
							int nrlen;
							long flags = isdn3_flags(conn->card->info,-1,-1);

							--data->b_rptr;
							data->b_rptr += getnr(info->nr,data->b_rptr,0, flags);
							nrlen = strlen(info->nr);
							if(nrlen > 0 && (info->nr[nrlen-1] == 'S' || info->nr[nrlen-1] == 's'))
								info->nr[nrlen-1] = '\0';
						}

						conn->capi_callref = (capi->appl << 16) | c2->plci;
						conn->msgid0 = capi->messid;
						conn->minorstate |= MS_INCOMING;

						if(err >= 0) {
							if(c2->DST_eaz) {
								setstate(conn,7);
								report_incoming(conn);
							} else if(conn->talk->state & (1<<(info->subcard+ST_pbx))) {
								setstate(conn,6); /* no report yet */
							} else {
								setstate(conn,6);
								report_incoming(conn);
							}
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
					printf(">CONNECT_RESP ");
					if((err = capi_send(talk,capi->appl,CAPI_CONNECT_RESP,mp,capi->messid)) < 0)
						freemsg(mp);
				}
			}
		}
		break;
	
	case CAPI_INFO_IND:
		printf("INFO_IND: ");
		{
			struct CAPI_info_ind *c2;
			struct CAPI_info_resp *c3;
			mblk_t *m3;
			mblk_t *m4;
    		struct capi_info *info;

			if(data->b_wptr-data->b_rptr < sizeof(*c2))
				goto less_room;
			c2 = ((typeof(c2))data->b_rptr)++;
			if((conn = capi_findconn(talk,capi->appl,c2->plci,1)) == NULL) {
				printf("CAPI error: INFO_IND has unknown cref %x%04x\n",capi->appl,c2->plci);
				slam_conn(talk,capi->appl,c2->plci,0);
				err = -ENXIO;
				break;
			}
			if(conn->p_data == NULL) {
				if((conn->p_data = malloc(sizeof(struct capi_info))) == NULL) {
					err = -ENOMEM;
					break;
				}
				bzero(conn->p_data,sizeof(struct capi_info));
			}
			info = conn->p_data;

			m4 = allocb(256,BPRI_MED);
			if(m4 != NULL) {
				switch(c2->info_number) {
				case AI_CAUSE:
					printf("CAUSE ");
					{
						int len = c2->infolen;
						m_putid(m4,IND_INFO);
						m_putid(m4,ID_N1_INFO);
						m_putsx(m4,ARG_CAUSE);
						if(len > 0)
							m_putsx2(m4, n1_causetoid(*data->b_rptr & 0x7F));
						do {
						} while(--len > 0 && !(*data->b_rptr++ & 0x80));
						if(len > 0)
							m_putx(m4,*data->b_rptr & 0x0F);
					}
					break;
				case AI_DISPLAY:
					printf("DISPLAY ");
					{
						m_putid(m4,IND_INFO);
						m_putid(m4,ID_N1_INFO);
						m_putsx(m4,ID_N0_display);
						m_puts(m4,data->b_rptr,c2->infolen);
					}
					break;
				case AI_DAD:
					printf("DAD ");
					{
						long flags = isdn3_flags(conn->card->info,-1,-1);
						--data->b_rptr;
						data->b_rptr += getnr(info->lnr,data->b_rptr,1,flags);
					}
					if(conn->state == 6)
						checknrlen(conn);
					goto empt;
					break;
				case AI_UUINFO:
					printf("UUINFO ");
					break;
				case AI_CHARGE:
					printf("CHARGE ");
					if(c2->infolen > 1) {
						int charge = 0;
						c2->infolen--;
						if(*data->b_rptr++ > c2->infolen) {
							printf("ERROR: short data?\n");
							goto empt;
						}
						while(c2->infolen > 0) {
							charge = 10 * charge + *data->b_rptr++ - '0';
							c2->infolen--;
						}
						m_putid(m4,IND_INFO);
						m_putid(m4,ID_N1_INFO);
						m_putsx(m4,ARG_CHARGE);
						m_puti(m4,charge);
					}
					break;
				case AI_DATE:
					printf("DATE ");
					if(c2->infolen > 0) {
						m_putid(m4,IND_INFO);
						m_putid(m4,ID_N1_INFO);
						m_putsx(m4,ID_N6_date);
						m_puts(m4,data->b_rptr,c2->infolen);
					} else
						goto empt;
					break;
				case AI_CPS:
					printf("CPS ");
					break;
				default:
					printf("unknown INFO_IND ID %x\n",c2->info_number);
				  empt:
					freemsg(m4); m4 = NULL;
					break;
				}
			}
			if(m4 != NULL) {
    			conn_info (conn, m4);
    			if ((err = isdn3_at_send (conn, m4, 0)) < 0)
					freemsg(m4);
			}

			if((m3 = allocb(sizeof(*c3),BPRI_MED)) == NULL)
				return -ENOMEM;

			c3 = ((typeof(c3))m3->b_wptr)++;
			bzero(c3,sizeof(*c3));
			c3->plci = c2->plci;
			printf(">INFO_RESP ");
			if((err = capi_send(conn->talk,conn->capi_callref >> 16,CAPI_INFO_RESP,m3,
					capi->messid)) < 0) {
				/* TODO: Hmmm... */
				freemsg(m3);
			}
		}
		break;

	/* Non-connection-related messages */
	case CAPI_REGISTER_CONF:
		printf("REGISTER_CONF: ");
		if(talk->tstate != STATE_REGISTER) {
			printf("CAPI error: bad state\n");
			break;
		}
#if 0
		if(talk->message_id != capi->messid) {
			printf("CAPI error: register bad ID %d, want %ld\n",capi->messid,talk->message_id);
			break;
		}
#endif
		talk->tappl[talk->regnum] = capi->appl;
	  dolisten:
		{
			int dodebug = 0;
			{
				mblk_t *inf = talk->card->info;
				if(inf != NULL) {
					streamchar *sta = inf->b_rptr;
					ushort_t idx;
					char skip = 0, subskip = 0;

					while(m_getsx(inf,&idx) == 0) {
						long sap;
						switch(idx) {
						case ARG_PROTOCOL:
							if (m_geti(inf,&sap) == 0) 
								skip = (sap != SAPI_CAPI);
							break;
						case ARG_SUBPROT:
							if (m_geti(inf,&sap) == 0 && !skip) {
								switch(sap) {
								case SAPI_CAPI_BINTEC:
									skip=0;
									break;
								default:
									/* Wrong card. Do something! */
									inf->b_rptr = sta;
									return -ENXIO;
								}
							}
							break;
						case ARG_SUBCARD:
							if (m_geti(inf,&sap) == 0 && !skip) 
								subskip = (sap != talk->regnum+1);
							break;
						case ARG_DEBUG:
							if(skip || subskip)
								break;
							dodebug = 1;
							break;
						case ARG_EAZ:
							if(skip || subskip)
								break;
							if(talk->state & 1<<(talk->regnum+ST_pbx))
								break;
							{
								char eaz;
								struct eazmapping *ce;
								mblk_t *mp;
								int len;

								if((err = m_getc(inf,&eaz)) < 0)
									break;
								if((len = m_getstrlen(inf)) < 0)
									break;
								mp = allocb(sizeof(*ce)+len, BPRI_MED);
								if(mp == NULL)
									break;
								ce = ((struct eazmapping *)mp->b_wptr)++;
								bzero(ce,sizeof(*ce));
								ce->eaz = eaz;
								ce->telnolen = len;
								if((err = m_getstr(inf,mp->b_wptr,len)) < 0) {
									freemsg(mp);
									break;
								}
								mp->b_wptr += len;
								printf(">CONTROL_EAZMAPPING ");
								if((err = capi_sendctl(talk,talk->tappl[talk->regnum],CONTROL_EAZMAPPING,mp,newmsgid(talk))) < 0) {
									freemsg(mp);
									break;
								}
							}
							break; /* ARG_EAZ */
						}
					}
					inf->b_rptr = sta;
				}
			}
			if((talk->tstate == STATE_REGISTER) && dodebug) {
				struct traceopen *td;
				mblk_t *dap = allocb(sizeof(*td), BPRI_MED);
				if(dap == NULL) /* XXX error processing */
					return -ENOMEM;
				td = ((typeof(td))dap->b_wptr)++;
				bzero(td,sizeof(*td));
				td->channel = 0;
				td->maxlen = -1;
				printf(">CONTROL_TRACEREC_ON ");
				if((err = capi_sendctl(talk,talk->tappl[talk->regnum],CONTROL_TRACEREC_ON,dap,newmsgid(talk))) < 0) {
					freemsg(dap);
					return err;
				}
				talk->tstate = STATE_DO_TRACE;
				return err;
			}
			talk->tstate = STATE_CONF_LAST;

			{	/* Tell the board to listen to everything */
				struct CAPI_listen_req *c2;
				mblk_t *mp = allocb(sizeof(*c2),BPRI_MED);

				if(mp == NULL)
					return -ENOMEM;
				c2 = ((struct CAPI_listen_req *)mp->b_wptr)++;
				bzero(c2,sizeof(*c2));
				c2->info_mask = 0xC00000FF;
				c2->eaz_mask = 0x03FF;
				c2->service_mask = 0xE7BF;
				{	/* Find correct masks */
					int err; char skip = 0, subskip = 0;
					mblk_t *inf = talk->card->info;
					if(inf != NULL) {
						streamchar *sta = inf->b_rptr;
						ushort_t idx;

						while(m_getsx(inf,&idx) == 0) {
							long sap;
							switch(idx) {
							case ARG_PROTOCOL:
								if (m_geti(inf,&sap) == 0) 
									skip = (sap != SAPI_CAPI);
								break;
							case ARG_SUBPROT:
								if (m_geti(inf,&sap) == 0 && !skip) {
									switch(sap) {
									case SAPI_CAPI_BINTEC:
										skip=0;
										break;
									default:
										/* Wrong card. TODO: Do something! */
										inf->b_rptr = sta;
										return -ENXIO;
									}
								}
								break;
							case ARG_SUBCARD:
								if (m_geti(inf,&sap) == 0 && !skip) 
									subskip = (sap != talk->regnum+1);
								break;
							case ARG_LISTEN:
								if(skip || subskip)
									break;
								{
									long x;
									if((err = m_getx(inf,&x)) >= 0) {
										c2->eaz_mask = x;
										if((err = m_getx(inf,&x)) >= 0) {
											c2->service_mask = x;
											if((err = m_getx(inf,&x)) >= 0) {
												c2->info_mask = x;
											}
										}
									}
								}
								break;
							}
						}
						inf->b_rptr = sta;
					}
				}
				if(talk->state & 1<<(talk->regnum+ST_pbx))
					c2->info_mask |= 0x40;
				else
					c2->info_mask &=~ 0x40;

				printf(">LISTEN_REQ ");
				if((err =
					capi_send(talk,talk->tappl[talk->regnum],CAPI_LISTEN_REQ,mp,capi->messid)) < 0) {
					freemsg(mp);
					return err;
				}
				talk->tstate--;
			}
		}
		break;
	case CAPI_LISTEN_CONF:
		printf("LISTEN_CONF: ");
		{
			struct CAPI_listen_conf *c2;
			c2 = ((typeof(c2))data->b_rptr)++;
			if(c2->info != 0) {
				printf("CAPI error: LISTEN failed %04x\n",c2->info);
				report_nocard(talk,c2->info);
				return -EIO;
			}
			if(talk->tstate < STATE_CONF_FIRST || talk->tstate > STATE_CONF_LAST) {
				printf("CAPI error: LISTEN return in bad state\n");
				return -EIO;
			}
			if(++talk->tstate == STATE_CONF_LAST) {
				talk->tstate = STATE_RUNNING;
				if(++talk->regnum < talk->card->dchans) 
					send_open(talk);
			}
		}
		break;
	case CAPI_ALIVE_IND:
		printf("ALIVE_IND: ");
		if(talk->tstate < STATE_CONF_FIRST) {
			printf("CAPI: ALIVE_REQ in state %ld\n",talk->tstate);
			goto printit;
		}
		printf(">ALIVE_RESP ");
		err = capi_send(talk,capi->appl,CAPI_ALIVE_RESP,NULL,capi->messid);
		break;
	case CAPI_CONTROL_IND:
		printf("CONTROL_IND: ");
		{
			struct CAPI_control_ind *c2;
			struct CAPI_control_resp *c3;
			mblk_t *m3;
			if(data->b_wptr-data->b_rptr < sizeof(*c2))
				goto less_room;
			c2 = ((typeof(c2))data->b_rptr)++;
			switch(c2->type) {
			case CONTROL_TRACEREC_PLAY:
				{
					mblk_t *my = allocb(64,BPRI_LO);
					mblk_t *mz;
					struct tracedata *td = (typeof(td))data->b_rptr;
					data->b_rptr += c2->datalen;
					mz = dupmsg(data);

					printf("CAPI: Trace\n");
					if(mz != NULL && my != NULL) {
						m_putid(my,IND_TRACE);
						m_putsx(my,ARG_CARD);
						m_putlx(my,talk->card->id);
						{
							int i;
							for(i=0;i < talk->card->dchans; i++) {
								if(capi->appl == talk->tappl[i]) {
									m_putsx(my,ARG_SUBCARD);
									m_puti(my,i+1);
									break;
								}
							}
						}
						m_putsx(my,ARG_EVENT);
						m_puti(my,td->event);
						m_putsx(my,ARG_MODE);
						m_puti(my,td->ppa);
						switch(td->event) {
						case 0:
							if(td->inout)
								m_putsx(my,PROTO_OUTGOING);
							else
								m_putsx(my,PROTO_INCOMING);
							break;
						case 7:
							m_putsx(my,ARG_SUBEVENT);
							m_puti(my,td->inout);
							break;
						}
						m_putdelim(my);
						linkb(my,mz);
						if(isdn3_at_send (NULL, my, 1) < 0)
							freemsg(my);

					} else {
						if(my != NULL)
							freemsg(my);
						if(mz != NULL)
							freemsg(mz);
					}
				}
				break;
			default:
			    printf("CAPI: Unknown control_ind type 0x%02x\n",c2->type);
				break;
			}

			m3 = allocb(sizeof(*c3),BPRI_MED);
			if(m3 == NULL)
				return -ENOMEM;

			c3 = ((typeof(c3))m3->b_wptr)++;
			bzero(c3,sizeof(*c3));
			c3->type = c2->type;
			printf(">CONTROL_RESP ");
			if((err = capi_send(talk,capi->appl,CAPI_CONTROL_RESP,m3, capi->messid)) < 0) 
				freemsg(m3);
		}
		break;
	case CAPI_CONTROL_CONF:
		printf("CONTROL_CONF: ");
		{
			struct CAPI_control_conf *c2;
			if(data->b_wptr-data->b_rptr < sizeof(*c2))
				goto less_room;
			c2 = ((struct CAPI_control_conf *)data->b_rptr)++;
			switch(c2->type) {
			default:
			    printf("CAPI: Unknown control_conf type 0x%02x\n",c2->type);
				goto printit;
			case CONTROL_TRACEREC_PLAY:
				{
					struct trace_timer *tt = NULL;
					{
						int i;
						for(i=0;i < talk->card->dchans; i++) {
							if(capi->appl == talk->tappl[i]) {
								tt = talk->talks[i];
								break;
							}
						}
					}
					if(tt == NULL)
						break;
#ifdef NEW_TIMEOUT
					tt->timer =
#endif
						timeout(talk_timer,tt,10*HZ);
				}
				break;
			case CONTROL_TRACEREC_ON:
				{
					struct trace_timer *tt;
					if(talk->tstate != STATE_DO_TRACE) {
						printf("CAPI error: API_TRACE reply for bad state\n");
						return -EINVAL;
					}
					tt = malloc(sizeof(*tt));
					if(tt != NULL) {
						talk->talks[talk->regnum] = tt;
						tt->talk = talk;
						tt->dchan = talk->regnum;
#ifdef NEW_TIMEOUT
						tt->timer =
#endif
							timeout(talk_timer,tt,10*HZ);
					}
				}
				goto dolisten;
			case CONTROL_EAZMAPPING:
				{
					if(talk->tstate == STATE_RUNNING) {
						if((conn =
							capi_findconnm(talk,capi->appl,capi->messid,WF_CONTROL_EAZ,0)) == NULL) {
							printf("CAPI error: CONTROL_EAZMAPPING has unknown conn\n");
							err = -ENXIO;
							break;
						}
						if((c2->info == 0) && (conn->waitflags & (1<<WF_CONTROL_EAZ))) {
							conn->waitflags &=~ (1<<WF_CONTROL_EAZ);
							err = after_active(conn,0);
						} else  {
							err = -EIO;
							printf("CAPI error: CONTROL_EAZ in wrong state %d, info %04x, wf 0x%lx\n",conn->state,c2->info,conn->waitflags);
							if(send_disconnect(conn,0,N1_OutOfOrder) < 0) 
								setstate(conn,99);
						}
					} else if(talk->tstate < STATE_CONF_FIRST || talk->tstate > STATE_CONF_LAST) {
						printf("CAPI error: CONTROL_EAZ in wrong tstate %ld\n",talk->tstate);
					}
					if(c2->info != 0) {
						printf("CAPI error: EAZMAPPING failed %04x\n",c2->info);
						report_nocard(talk,c2->info);
						return -EIO;
					}
					if(++talk->tstate == STATE_CONF_LAST) {
						printf("CAPI: EAZMAPPING after LISTEN_CONF ???\n");
						talk->tstate = STATE_RUNNING;
						if(++talk->regnum < talk->card->dchans) 
							send_open(talk);
					}
				}
				break;
			case CONTROL_API_OPEN: /* Boot: Step One */
				if(talk->tstate != STATE_OPENING) {
					printf("CAPI error: API_OPEN reply for bad state\n");
					return -EINVAL;
				}
				if(c2->info <= 0) {
					printf("CAPI error: open failed!\n");
					report_nocard(talk,c2->info);
					return -EIO;
				}
				if(talk->card->dchans != c2->info) {
					printf("CAPI: %d D channels -- that should be %d\n",c2->info,talk->card->dchans);
				}
				{
					struct CAPI_register_req *c3;

					mblk_t *mp = allocb(sizeof(*c3), BPRI_MED);
					if(mp == NULL) /* XXX error processing */
						return -ENOMEM;
					c3 = ((struct CAPI_register_req *)mp->b_wptr)++;
					bzero(c3,sizeof(*c3));
					c3->nmess = 10;
					c3->nconn = talk->card->bchans + (talk->card->bchans >> 1) + 1;;
					c3->ndblock = c3->nconn*10;
					c3->dblocksiz = 4096;
					printf(">REGISTER_REQ ");
					if((err = capi_send(talk,talk->tappl[talk->regnum],CAPI_REGISTER_REQ,mp,newmsgid(talk))) < 0) {
						freemsg(mp);
						break;
					}
					talk->tstate = STATE_REGISTER;
				}
				break;
			}
		}
		break;
	}
	if(err >= 0)
		freemsg(data);
	printf("\n");
	return err;

  less_room:
  	printf("error: too few data bytes\n");
  printit:
  	data->b_rptr = origmb;
	log_printmsg(NULL,"  recvErr",data,"=");
	if(conn != NULL)
		checkterm(conn);
	return err ? err : -EINVAL;
}

static int
send (isdn3_conn conn, mblk_t * data)
{
	printf("CAPI: send %05lx\n",conn->capi_callref);
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
	uchar_t cause = 0;

	if(conn->talk->tstate == STATE_DEAD)
		return -ENXIO;

	printf("CAPI: sendcmd %05lx %04x: ",conn->capi_callref,id);
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
		((struct capi_info *)conn->p_data)->subcard = 0;
	}
	conn->lockit++;
	info = conn->p_data;
    if (data != NULL) {
		unsigned long x;

        oldpos = data->b_rptr;
        while ((err = m_getsx (data, &typ)) == 0) {
			switch(typ) {
            case ARG_CAUSE:
				{
					ushort_t causeid;

					if (m_getid (data, &causeid) != 0)
						break;
					cause = n1_idtocause(causeid);
				}
				break;

            case ARG_SUBCARD:
                if ((err = m_geti (data, &x)) != 0) {
                    printf("GetX Subcard: ");
				  RetErr:
                    data->b_rptr = oldpos;
                    conn->lockit--;
                    return err;
                }
				if(x < 1 || x > conn->talk->card->dchans) {
					err = -ENXIO;
					goto RetErr;
				}
				if(id == CMD_DIAL)
					info->subcard = x-1;
				else if(info->subcard != x-1) {
                    printf("GetX Subcard.: %d != %ld-1",info->subcard,x);
					err = -EINVAL;
					goto RetErr;
				}
                break;
            case ARG_SERVICE:
                if ((err = m_getx (data, &x)) != 0) {
                    printf("GetX Service: ");
                    goto RetErr;
                }
				info->service = x;
                break;
			case ARG_SPV:
				info->flags |= INF_SPV;
                break;
			case ARG_CHANNEL:
                if ((err = m_geti (data, &x)) != 0) {
                    printf("GetX Bchan: ");
					goto RetErr;
                }
				info->bchan = x;
                break;
            case ARG_LNUMBER:
                m_getskip (data);
                if ((err = m_getstr (data, (uchar_t *) info->lnr, MAXNR)) != 0) {
                    printf("GetStr LNumber: ");
					goto RetErr;
                }
                break;
            case ARG_NUMBER:
                m_getskip (data);
                if ((err = m_getstr (data, (uchar_t *) info->nr, MAXNR)) != 0) {
                    printf("GetStr Number: ");
					goto RetErr;
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
			if(conn->talk->tstate != STATE_RUNNING)
				return -ENXIO;
            if (data == NULL) {
                printf("DataNull: ");
                conn->lockit--;
                return -EINVAL;
            }
			if(conn->state != 6 && conn->state != 7) {
				printf("CAPI error: ANSWER in bad state!\n");
				conn->lockit--;
				return -EINVAL;
			}
			conn->minorstate |= MS_WANTCONN;
			conn->waitflags |= 1<<WF_PROTOCOLS;
#if 1
			setstate(conn, 7);
#else
			if(conn->state == 7)
#endif
            	isdn3_setup_conn (conn, EST_SETUP);
			err = 0;
		}
		break;
    case CMD_DIAL:
        {
			if(conn->talk->tstate != STATE_RUNNING)
				return -ENXIO;
            conn->minorstate |= MS_OUTGOING | MS_WANTCONN;

            isdn3_setup_conn (conn, EST_SETUP);

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
			err = send_dialout(conn);
			if(err >= 0)
				isdn3_setup_conn (conn, EST_NO_CHANGE);
			else
				isdn3_setup_conn (conn, EST_DISCONNECT);
		}
		break;
	case CMD_OFF:
		conn->minorstate &=~ MS_WANTCONN;
		isdn3_setup_conn (conn, EST_DISCONNECT);
		err = send_disconnect(conn,!force,cause);
		if(err < 0)
			setstate(conn,0);
		break;
	default:
		err = -ENXIO;
		break;
	}

	conn->lockit--;
	checkterm(conn);
	return err;
}

static void
report (isdn3_conn conn, mblk_t * data)
{
    struct capi_info *info;
    info = conn->p_data;
    if (info == NULL)
        return;
	if (info->nr[0] != 0) {
		m_putsx (data, ARG_NUMBER);
		m_putsz (data, info->nr);
	}
	if (info->lnr[0] != 0) {
		m_putsx (data, ARG_LNUMBER);
		m_putsz (data, info->lnr);
	}
	if (info->service != 0) {
		m_putsx (data, ARG_SERVICE);
		m_putx (data, info->service);
	}
	if(info->subcard != (uchar_t) ~0) {
		m_putsx (data, ARG_SUBCARD);
		m_puti (data,info->subcard+1);
	}
}

static void
ckill (isdn3_talk talk, char force)
{
	printf("CAPI: ckill %d\n",force);
	if(force) {
		int i;
		for(i=0;i<talk->card->dchans;i++) 
			if(talk->talks[i] != NULL) {
#ifdef NEW_TIMEOUT
				untimeout(((struct trace_timer *)talk->talks[i])->timer);
#else
				untimeout(talk_timer,talk->talks[i]);
#endif
		}
	}
}

static void
killconn (isdn3_conn conn, char force)
{
	printf("CAPI: killconn %05lx: %d\n",conn->capi_callref,force);
	conn->lockit++;
    if (force) {
        untimer (CAPI_TCONN, conn);
        untimer (CAPI_TWAITLOCAL, conn);
        untimer (CAPI_TWAITFIRSTLOCAL, conn);
        untimer (CAPI_TFOO, conn);
	}
	if(conn->state == 0) {
		conn->lockit--;
		return;
	}
	send_disconnect(conn, !force, 0);
	conn->lockit--;
	checkterm(conn);
}

static void
hook (isdn3_conn conn)
{
	printf("CAPI: hook %05lx\n",conn->capi_callref);
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


static int
setup_complete(struct _isdn3_conn *conn)
{
	streamchar *s0;
	streamchar *s1,*s2,sx;
	streamchar *s3,*s4,sy=0;
	mblk_t *mb = conn->modlist[0];
	int err;

	if(mb == NULL)
		return -EAGAIN;
	if((mb = mb->b_cont) == NULL) {
		ushort_t id;
		streamchar *xi;
		mblk_t *mb1 = dupmsg(conn->modlist[0]);
		mblk_t *mb2 = dupmsg(conn->modlist[0]);
		if(mb1 == NULL || mb2 == NULL) {
			if(mb1 != NULL)
				freemsg(mb1);
			if(mb2 != NULL)
				freemsg(mb2);
			return -ENOMEM;
		}
		xi = mb1->b_rptr;
		while(m_getsx(mb1,&id) == 0) ;
		mb1->b_wptr = mb1->b_rptr;
		mb1->b_rptr = xi;
		while(m_getsx(mb2,&id) == 0) ;
		mb = mb2;
		mb1->b_cont = mb2;
		conn->modlist[0] = mb1;
	}
	s0 = s1 = mb->b_rptr;
	while(s1 < mb->b_wptr && isspace(*s1))
		s1++;
	s2 = s1;
	while(s2 < mb->b_wptr && !isspace(*s2))
		s2++;
	sx = *s2; *s2 = '\0';

	s3 = s2+1;
	while(s3 < mb->b_wptr && isspace(*s3))
		s3++;
	s4 = s3;
	while(s4 < mb->b_wptr && !isspace(*s4))
		s4++;
	if(s4-1 > s3) {
		sy = *s4; *s4 = '\0';
	} else {
		s4 = NULL;
	}

	conn->hl_id = 0;
	if(!strcmp(s1,"frame")) {
		if(s4 == NULL)
			err = 1;
		else if(!strcmp(s3,"ppp")) {
			int i;

			for(i=1;i<nmodlist;i++) {
				streamchar *origmp;
				ushort_t id;
				mblk_t *mp;
				streamchar sname[FMNAMESZ+1];
				
				mp = conn->modlist[i];
				if(mp == NULL) {
					err = -ENOENT;
					break;
				}
				origmp = mp->b_rptr;
				if((err = m_getid(mp,&id)) < 0) {
					mp->b_rptr = origmp;
					break;
				}
				if(id != PROTO_MODULE) {
					err = -EAGAIN;
					mp->b_rptr = origmp;
					continue;
				}
				while((err = m_getsx(mp,&id)) >= 0) {
					if(id == PROTO_MODULE)
						break;
				}
				if(err < 0)
					break;
				if((err = m_getstr(mp,sname,FMNAMESZ)) < 0) {
					mp->b_rptr = origmp;
					break;
				}
				if(!strcmp(sname,"ppp")) {
					mp->b_rptr = origmp;
					conn->hl_id = i;
					err = 2;
					break;
				}
				mp->b_rptr = origmp;
				err = -ENOENT; /* not found? */
			}
		} else 
			err = 1;
	} else if(!strcmp(s1,"trans"))
		err = 1;
	else if(!strcmp(s1,"transalaw"))
		err = 1;
	else
		err = -ENOENT;
	*s2 = sx;
	if(s4 != NULL)
		*s4 = sy;
	if((err > 0) && (conn->waitflags & (1<<WF_PROTOCOLS))) {
		int err2 = 0, i = 0;
		do {
			mblk_t *ms;

			if((i>0) && (conn->hl_id <= i))
				continue;

			ms = dupmsg(conn->modlist[i]);
			if(ms == NULL)
				return -ENOMEM;
			if(i == 0) {
				if((s4 != NULL) && (err > 1))
					ms->b_cont->b_rptr += s4-s0;
				else
					ms->b_cont->b_rptr += s2-s0;
			}
			err2 = isdn3_send_conn(conn->minor,AS_PROTO,ms);
			if(err2 < 0) {
				freemsg(ms);
				break;
			}
		} while((++i < nmodlist) && (conn->modlist[i] != NULL));
		if(err2 < 0)
			err = err2;
	}
	return err;
}

static int
proto(struct _isdn3_conn * conn, mblk_t **data, char down)
{
	mblk_t *mb = *data;
	streamchar *olds;
	ushort_t id;
	int err;

	printf("CAPI: proto %05lx %s: ",conn->capi_callref, down ? "down" : "up");
	if(mb == NULL) printf("NULL"); else
	while(mb != NULL) {
		dumpascii(mb->b_rptr,mb->b_wptr-mb->b_rptr);
		mb = mb->b_cont;
	}
	printf("\n");

	mb = *data;
	olds = mb->b_rptr;
	if((err = m_getid(mb,&id)) < 0)
		return err;
	mb->b_rptr = olds;
	switch(id) {
	case PROTO_MODLIST:
		if(!down)
			return -EINVAL;
		{
			int i;
			for(i=0;i<nmodlist;i++) {
				if(conn->modlist[i] == NULL)
					break;
				freemsg(conn->modlist[i]);
				conn->modlist[i] = NULL;
			}
		}
		conn->modlist[0] = *data;
		*data = NULL;
		if(err < 0)
			conn->modlist[0] = NULL;

		break;
	case PROTO_MODULE:
		if(conn->modlist[0] == NULL)
			break;
		if(setup_complete(conn) == -ENOENT) {
			int i;
			for(i=1;i<nmodlist; i++) {
				if(conn->modlist[i] == NULL) {
					conn->modlist[i] = mb;
					*data = NULL;
					break;
				}
			}
			if(i == nmodlist) {
				return -EIO;
			}
		}
		break;
	}
	
	if((err = setup_complete(conn)) > 0) {
		if ((conn->waitflags & (1<<WF_PROTOCOLS))) {
			conn->waitflags &=~ (1<<WF_PROTOCOLS);
			if(conn->waitflags == 0) {
				err = send_setup(conn);
				if(err < 0)
					*data = mb;
			}
		}
	}
	return 0;
}




ushort_t
capi_infotoid(ushort_t info)
{
	if((info & 0xFF00) == 0x8000) 
		return et_causetoid(info & 0x7F);
	if((info & 0xFF00) == 0x3400) 
		return n1_causetoid(info & 0x7F);
	switch(info) {
	default: return CHAR2('?','?');
	case CAPI_E_REGISTER:			return ID_E_REGISTER;
	case CAPI_E_APPLICATION:		return ID_E_APPLICATION;
	case CAPI_E_MSGLENGTH:			return ID_E_MSGLENGTH;
	case CAPI_E_COMMAND:			return ID_E_COMMAND;
	case CAPI_E_QUEUEFULL:			return ID_E_QUEUEFULL;
	case CAPI_E_NOMSG:				return ID_E_NOMSG;
	case CAPI_E_MSGOVERFLOW:		return ID_E_MSGOVERFLOW;
	case CAPI_E_DEINSTALL:			return ID_E_DEINSTALL;
	case CAPI_E_CONTROLLER:			return ID_E_CONTROLLER;
	case CAPI_E_PLCI:				return ID_E_PLCI;
	case CAPI_E_NCCI:				return ID_E_NCCI;
	case CAPI_E_TYPE:				return ID_E_TYPE;
	case CAPI_E_BCHANNEL:			return ID_E_BCHANNEL;
	case CAPI_E_INFOMASK:			return ID_E_INFOMASK;
	case CAPI_E_EAZMASK:			return ID_E_EAZMASK;
	case CAPI_E_SIMASK:				return ID_E_SIMASK;
	case CAPI_E_B2PROTO:			return ID_E_B2PROTO;
	case CAPI_E_DLPD:				return ID_E_DLPD;
	case CAPI_E_B3PROTO:			return ID_E_B3PROTO;
	case CAPI_E_NCPD:				return ID_E_NCPD;
	case CAPI_E_NCPI:				return ID_E_NCPI;
	case CAPI_E_DATAB3FLAGS:		return ID_E_DATAB3FLAGS;
	case CAPI_E_CONTROLLERFAILED:	return ID_E_CONTROLLERFAILED;
	case CAPI_E_REGCONFLICT:		return ID_E_REGCONFLICT;
	case CAPI_E_CMDNOTSUPPORTED:	return ID_E_CMDNOTSUPPORTED;
	case CAPI_E_PLCIACT:			return ID_E_PLCIACT;
	case CAPI_E_NCCIACT:			return ID_E_NCCIACT;
	case CAPI_E_B2NOTSUPPORT:		return ID_E_B2NOTSUPPORT;
	case CAPI_E_B2STATE:			return ID_E_B2STATE;
	case CAPI_E_B3NOTSUPPORT:		return ID_E_B3NOTSUPPORT;
	case CAPI_E_B3STATE:			return ID_E_B3STATE;
	case CAPI_E_B2DLPDPARA:			return ID_E_B2DLPDPARA;
	case CAPI_E_B3NCPDPARA:			return ID_E_B3NCPDPARA;
	case CAPI_E_B3NCPIPARA:			return ID_E_B3NCPIPARA;
	case CAPI_E_DATALEN:			return ID_E_DATALEN;
	case CAPI_E_DTMF:				return ID_E_DTMF;
	case CAPI_E_NOL1:				return ID_E_NOL1;
	case CAPI_E_NOL2:				return ID_E_NOL2;
	case CAPI_E_SETUPBCHANLAYER1:	return ID_E_SETUPBCHANLAYER1;
	case CAPI_E_SETUPBCHANLAYER2:	return ID_E_SETUPBCHANLAYER2;
	case CAPI_E_ABORTDCHANLAYER1:	return ID_E_ABORTDCHANLAYER1;
	case CAPI_E_ABORTDCHANLAYER2:	return ID_E_ABORTDCHANLAYER2;
	case CAPI_E_ABORTDCHANLAYER3:	return ID_E_ABORTDCHANLAYER3;
	case CAPI_E_ABORTBCHANLAYER1:	return ID_E_ABORTBCHANLAYER1;
	case CAPI_E_ABORTBCHANLAYER2:	return ID_E_ABORTBCHANLAYER2;
	case CAPI_E_ABORTBCHANLAYER3:	return ID_E_ABORTBCHANLAYER3;
	case CAPI_E_REBCHANLAYER2:		return ID_E_REBCHANLAYER2;
	case CAPI_E_REBCHANLAYER3:		return ID_E_REBCHANLAYER3;
	case CAPI_E_NOFAX:				return ID_E_NOFAX;
	case CAPI_E_BADLINE:			return ID_E_BADLINE;
	case CAPI_E_NOANSWER:			return ID_E_NOANSWER;
	case CAPI_E_REMDISC:			return ID_E_REMDISC;
	case CAPI_E_NOCMD:				return ID_E_NOCMD;
	case CAPI_E_INCOMPAT:			return ID_E_INCOMPAT;
	case CAPI_E_BADDATA:			return ID_E_BADDATA;
	case CAPI_E_PROTO:				return ID_E_PROTO;
	}
}

static void
init (void)
{
	printf("CAPI: init\n");
}

struct _isdn3_hndl CAPI_hndl =
{
		NULL, /* SAPI */ 65,0,
		&init, &newcard, &chstate, &report, &recv, &send,
		&sendcmd, &ckill, &killconn, &hook, &proto,
};


