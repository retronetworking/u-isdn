#include "primitives.h"
#include "phone.h"
#include "streamlib.h"
#include "phone_ETSI.h"
#include "q_data.h"
#include "isdn_23.h"
#include "isdn3_phone.h"
#include "isdn_34.h"
#include <sys/errno.h>
#include <sys/param.h>
#include <sys/types.h>
#include "q_data.h"
#include "phone_ETSI.h"
#include "sapi.h"

#undef HAS_SUSPEND

#define FAC_PENDING 002

#define RUN_ET_T301 01
#define RUN_ET_T302 02
#define RUN_ET_T303 04
#define RUN_ET_T304 010
#define RUN_ET_T305 020
#define RUN_ET_T308 040
#ifdef HAS_RECOVERY
#define RUN_ET_T309 0100
#endif
#define RUN_ET_T310 0200
#define RUN_ET_T313 0400
#define RUN_ET_T314 01000
#define RUN_ET_T316 02000
#define RUN_ET_T317 04000
#ifdef HAS_SUSPEND
#define RUN_ET_T318 010000
#define RUN_ET_T319 020000
#endif
#define RUN_ET_T321 040000
#define RUN_ET_T322 0100000

#define RUN_ET_TCONN 0200000
#define RUN_ET_TALERT 0400000

#define REP_T303 01000000
#define RUN_ET_TFOO 02000000
#if 0
#define SVC_PENDING 080000
#endif

#define VAL_ET_T301 ( 180 *HZ)	/* timeout when calling out */
#define VAL_ET_T302 ( 15 *HZ)
#define VAL_ET_T303 ( 10 *HZ)	  /* was 4. L1 startup, TEI, L2 startup ... */
#define VAL_ET_T304 ( 30 *HZ)
#define VAL_ET_T305 ( 30 *HZ)
#define VAL_ET_T308 ( 8 *HZ)	/* was 4. Hmmm... */
#ifdef HAS_RECOVERY
#define VAL_ET_T309 ( 90 *HZ)
#endif
#define VAL_ET_T310 ( 50 *HZ)
#define VAL_ET_T313 ( 8 *HZ)	  /* Should be 4 */
#define VAL_ET_T316 ( 120 *HZ)
#define VAL_ET_T317 ( 90 *HZ)
#ifdef HAS_SUSPEND
#define VAL_ET_T318 ( 4 *HZ)
#define VAL_ET_T319 ( 4 *HZ)
#endif
#define VAL_ET_T321 ( 30 *HZ)
#define VAL_ET_T322 ( 4 *HZ)
#define VAL_ET_TCONN ( 40 *HZ)	  /* timer for connection establishment */
#define VAL_ET_TALERT ( 1 *HZ)	  /* timer for delaying an ALERT response */
#define VAL_ET_TFOO ( 7*HZ)

static void ET_T301 (isdn3_conn conn);
static void ET_T302 (isdn3_conn conn);
static void ET_T303 (isdn3_conn conn);
static void ET_T304 (isdn3_conn conn);
static void ET_T305 (isdn3_conn conn);
static void ET_T308 (isdn3_conn conn);
#ifdef HAS_RECOVERY
static void ET_T309 (isdn3_conn conn);
#endif
static void ET_T310 (isdn3_conn conn);
static void ET_T313 (isdn3_conn conn);
static void ET_T316 (isdn3_conn conn);
static void ET_T317 (isdn3_conn conn);
#ifdef HAS_SUSPEND
static void ET_T318 (isdn3_conn conn);
static void ET_T319 (isdn3_conn conn);
#endif
static void ET_T321 (isdn3_conn conn);
static void ET_T322 (isdn3_conn conn);

static void ET_TCONN (isdn3_conn conn);
static void ET_TALERT (isdn3_conn conn);
static void ET_TFOO (isdn3_conn conn);

static int send_disc (isdn3_conn conn, char release, mblk_t * data);
static void Xreport_terminate (isdn3_conn conn, uchar_t * data, int len, ushort_t cause, int deb_line);

struct e_info {
	unsigned char flags;
	unsigned char bearer_len,llc_len,ulc_len;
	unsigned char bearer[30],llc[10],ulc[10];
	unsigned char nr[MAXNR+2];
	unsigned char lnr[MAXNR+2];
};

static void
phone_timerup (isdn3_conn conn)
{
	rtimer (ET_T301, conn);
	rtimer (ET_T302, conn);
	rtimer (ET_T303, conn);
	rtimer (ET_T304, conn);
	rtimer (ET_T305, conn);
	rtimer (ET_T308, conn);
#ifdef HAS_RECOVERY
	rtimer (ET_T309, conn);
#endif
	rtimer (ET_T310, conn);
	rtimer (ET_T313, conn);
	rtimer (ET_T316, conn);
	rtimer (ET_T317, conn);
#ifdef HAS_SUSPEND
	rtimer (ET_T318, conn);
	rtimer (ET_T319, conn);
#endif
	rtimer (ET_T321, conn);
	rtimer (ET_T322, conn);
	rtimer (ET_TCONN, conn);
	rtimer (ET_TALERT, conn);
	rtimer (ET_TFOO, conn);
}

#define setstate(a,b) Xsetstate((a),(b),__LINE__)
static void
Xsetstate (isdn3_conn conn, uchar_t state, int deb_line)
{
	if(log_34 & 2)printf ("Conn PostET:%d %ld: State %d --> %d\n", deb_line, conn->call_ref, conn->state, state);
	if (conn->state == state)
		return;
	switch (conn->state) {
	case 1:
		untimer (ET_T303, conn);
		break;
	case 2:
		untimer (ET_T304, conn);
		break;
	case 3:
		untimer (ET_T310, conn);
		break;
	case 8:
		untimer (ET_T313, conn);
		break;
	case 7:
		untimer (ET_TCONN, conn);
		break;
	case 6:
		untimer (ET_TCONN, conn);
		untimer (ET_TALERT, conn);
		break;
	case 11:
		untimer (ET_T305, conn);
		break;
#ifdef HAS_SUSPEND
	case 15:
		untimer (ET_T319, conn);
		break;
	case 17:
		untimer (ET_T318, conn);
		break;
#endif
	case 19:
		untimer (ET_T308, conn);
		break;
	case 25:
		untimer (ET_T302, conn);
		break;
	case 99:
		untimer (ET_TFOO, conn);
		break;
	}
	conn->state = state;
	if(state == 0 || state > 10)
		Xreport_terminate (conn, NULL,0,0, deb_line);
	switch (conn->state) {
	case 1:
		timer (ET_T303, conn);
		break;
	case 2:
		timer (ET_T304, conn);
		break;
	case 3:
		timer (ET_T310, conn);
		break;
	case 6:
		ftimer (ET_TALERT, conn);
		/* FALL THRU */
	case 7:
		ftimer (ET_TCONN, conn);
		break;
	case 8:
		timer (ET_T313, conn);
		break;
	case 11:
		timer (ET_T305, conn);
		break;
#ifdef HAS_SUSPEND
	case 15:
		timer (ET_T319, conn);
		break;
	case 17:
		timer (ET_T318, conn);
		break;
#endif
	case 19:
		timer (ET_T308, conn);
		break;
	case 25:
		timer (ET_T302, conn);
		break;
	case 99:
		timer (ET_TFOO, conn);
		break;
	}
}

static void
report_addfac (mblk_t * mb, uchar_t * data, ushort_t len)
{

	QD_INIT (data, len) return;
	QD {
	  QD_CASE (0, PT_E0_facility):
		if (qd_len == 0) {
printf("FacL 1 is %d\n",qd_len);
			break;
		}
		if((*qd_data & 0x1F) != 0x11) {
printf("Fac 2 is %x\n",*qd_data);
			break;
		}
		while(qd_len > 0 && !(*qd_data & 0x80)) {
			qd_data++; qd_len--;
		} 
		if(qd_len < 2) {
printf("FacL 2 is %d\n",qd_len);
			break;
		}
		qd_data++; qd_len--;
		if((*qd_data & 0xE0) != 0xA0) {
printf("Fac 3 is %x\n",*qd_data);
			break;
		}
		switch(*qd_data & 0x1F) {
		case 1: /* invoke */
			{
				unsigned char nlen, ilen;
				int ident;

				qd_data++; qd_len--;
				if(qd_len < 1) {
printf("FacL 4 is %d\n",qd_len);
					break;
				}
				if(*qd_data & 0x80) { /* length format */
printf("Fac 4 is %x\n",*qd_data);
					break;
				}
				nlen = *qd_data++; qd_len--;
				if(qd_len < nlen) {
printf("FacL 5 is %d %d\n",qd_len,nlen);
					return;
				}
				qd_len -= nlen;

				if(nlen < 2) {
printf("FacL 6 is %d\n",nlen);
					return;
				}
				if(*qd_data != 0x02) {
printf("Fac 5 is %x\n",*qd_data);
					return;
				}
				qd_data++; nlen--;
				if(*qd_data & 0x80) { /* length format */
printf("Fac 6 is %x\n",*qd_data);
					break;
				}
				ilen = *qd_data++; nlen--;
				if(ilen > nlen || ilen == 0) {
printf("FacL 6a is %d %d\n",ilen,nlen);
					return;
				}
				nlen -= ilen;
				ident = 0;
				while(ilen > 0) {
					ident = (ident << 8) | (*qd_data++ & 0xFF);
					ilen--;
				}
#if 0
				if(ident != 0x7F7D) {
printf("Fac 7 is %x\n",ident);
					return;
				}
#endif

				if(nlen < 2) {
printf("FacL 7 is %d\n",nlen);
					return;
				}
				if(*qd_data != 0x02)
					return;
				qd_data++; nlen--;
				ilen = *qd_data++; nlen--;
				if(ilen > nlen || ilen == 0) {
printf("FacL 8 is %d %d\n",ilen,nlen);
					return;
				}
				nlen -= ilen;
				ident = 0;
				while(ilen > 0) {
					ident = (ident << 8) | (*qd_data++ & 0xFF);
					ilen--;
				}

#if defined(__GNUC__) && (__GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 7))
#define UNUSED __attribute__((unused))
#else
#define UNUSED
#endif

#define FOO1(s,a,b) \
				while(nlen > 1) {									\
					int ilen = qd_data[1];							\
					if(nlen < ilen+2) {								\
printf("FooL" ##s " is %d,%d\n",nlen,ilen); 						\
						return;										\
					}												\
					nlen -= ilen+2;									\
					if((*qd_data & 0xFF) == (a)) {					\
						int nlen UNUSED = ilen;						\
						qd_data += 2;								\
						b;											\
					} else {										\
printf("Foo " ##s " is %x\n",*qd_data & 0xFF); 						\
						qd_data += ilen+2;							\
					}												\
				}
				switch(ident) {
				default:
printf("Fac 8 is %x\n",ident);
					break;
				case 0x22: /* during */
					FOO1("1A",0x30,FOO1("1C",0xA1,FOO1("1D",0x30,FOO1("1E",0x02,({
						ident = 0;
						while(ilen > 0) {
							ident = (ident<<8) | *qd_data++;
							ilen--;
						}
						m_putsx (mb, ARG_CHARGE);
						m_puti (mb, ident);
						})))))
					break;
				case 0x24: /* final */
					FOO1("2A",0x30,FOO1("2B",0x30,FOO1("2C",0xA1,FOO1("2D",0x30,FOO1("2E",0x02,({
						ident = 0;
						while(ilen > 0) {
							ident = (ident<<8) | *qd_data++;
							ilen--;
						}
						m_putsx (mb, ARG_CHARGE);
						m_puti (mb, ident);
						}))))))
					break;
				}
#undef FOO1

			}
			break;
		case 2: /* return result */
printf("Fac 1 is %x\n",*qd_data);
			break;
		case 3: /* return error */
printf("Fac 1 is %x\n",*qd_data);
			break;
		default:
printf("Fac 1 is %x\n",*qd_data);
			break;
		}
	}
}

static uchar_t
report_addcause (mblk_t * mb, uchar_t * data, int len)
{
	int qd_len;
	uchar_t *qd_data;
	uchar_t loc, rec = 0;

	qd_data = qd_find (data, len, 0, PT_E0_cause, &qd_len);
	if (qd_data == NULL)
		return 0;
	if (qd_len < 1)
		return 0;
	m_putsx (mb, ARG_CAUSE);
	loc = *qd_data & 0x7F;
	if (!(loc & 0x80) && (qd_len > 0)) {
		rec = *++qd_data & 0x7F;
		--qd_len;
	}
	if(qd_len > 0)
		m_putsx2(mb, et_causetoid(*qd_data&0x7F));
	while(qd_len > 0 && !(*qd_data++ & 0x80))
		--qd_len;
	m_putx(mb,loc);
	if(rec != 0) {
		m_putx(mb,rec);
	}
	if(qd_len > 0) 
		m_puthex(mb,qd_data,qd_len);

	return *qd_data;
}

static void
report_addisplay (mblk_t * mb, uchar_t * data, int len)
{
	int qd_len;
	uchar_t *qd_data;

	qd_data = qd_find (data, len, 0, PT_E0_display, &qd_len);
	if (qd_data == NULL)
		return;
	if (qd_len < 1)
		return;
	m_putsx (mb, ID_E0_display);
	m_puts (mb, qd_data, qd_len);
}

static void
report_addprogress (mblk_t * mb, uchar_t * data, int len)
{
	QD_INIT (data, len)

	QD {
		QD_CASE (0, PT_E0_progress):
		if (qd_len < 3)
			continue;
		m_putsx (mb, ID_E0_progress);
		m_puti (mb, qd_data[2]);
		m_puti (mb, qd_data[1] & 0x0F);
		m_puti (mb,(qd_data[1] & 0x60) >> 5);
	}
	QD_EXIT;
}

static void
report_adddate (mblk_t * mb, uchar_t * data, int len)
{
	int qd_len;
	uchar_t *qd_data;

	qd_data = qd_find (data, len, 6, PT_E0_date, &qd_len);
	if (qd_data == NULL)
		return;
	if (qd_len < 1)
		return;
	m_putsx (mb, ID_E0_date);
	m_puts (mb, qd_data, qd_len);
}

#if 0
static void
report_addcost (mblk_t * mb, uchar_t * data, int len)
{
	int qd_len;
	uchar_t *qd_data;

	qd_data = qd_find (data, len, 6, PT_E5_chargingInfo, &qd_len);
	if (qd_data == NULL)
		return;
	if (qd_len < 2)
		return;
	m_putsx (mb, ID_E5_chargingInfo);
	m_puts (mb, qd_data + 1, qd_len - 1);
}
#endif

static void
report_addstatus (mblk_t * mb, uchar_t * data, int len)
{
#if 0
	int qd_len;
	uchar_t *qd_data;

	qd_data = qd_find (data, len, 6, PT_N6_StatusCalled, &qd_len);
	if (qd_data == NULL)
		return;
	if (qd_len < 1)
		return;
	m_putsx (mb, ID_N6_StatusCalled);
	switch (*qd_data) {
	case N1_St_Unknown:
		m_putsx2 (mb, ID_N1_St_Unknown);
		break;
	case N1_St_Calling:
		m_putsx2 (mb, ID_N1_St_Calling);
		break;
	default:
		m_putx (mb, *qd_data & 0x7f);
	}
	m_puts (mb, qd_data, qd_len);
#endif
}

static int
get_state(isdn3_conn conn, uchar_t *data, int len, uchar_t *state)
{
	int qd_len;
	uchar_t *qd_data;

	qd_data = qd_find (data, len, 0, PT_E0_callState, &qd_len);
	if (qd_data == NULL)
		return 0;
	if(qd_len < 1)
		return 0;
	*state = *qd_data;
	return 1;
}


static int
get_hex(isdn3_conn conn, uchar_t * data, int len, uchar_t *addr, uchar_t *addrlen, uchar_t maxlen, uchar_t what)
{
	int qd_len;
	uchar_t *qd_data;

	qd_data = qd_find (data, len, 0, what, &qd_len);
	if (qd_data == NULL)
		return 0;
	if(qd_len > maxlen)
		qd_len = maxlen;
	bcopy(qd_data,addr,qd_len);
	*addrlen = qd_len;
	return 1;
}

static int
get_nr (isdn3_conn conn, uchar_t * data, int len, uchar_t *nrpos, uchar_t what)
{
	int qd_len;
	uchar_t *qd_data;

	*nrpos = '\0';
	qd_data = qd_find (data, len, 0, what, &qd_len);
	if (qd_data == NULL)
		return 0;
	if (qd_len > MAXNR)
		qd_len = MAXNR;
	if (qd_len < 1)
		return 0;
	switch(*qd_data & 0x70) {
	case 0x00: /* unknown */
		{
			long flags = isdn3_flags(conn->card->info,-1,-1);
			if(flags & FL_BUG1) {
				if(qd_data[0] == 0x00 && qd_data[1] == 0x83)
					*nrpos++ = '='; /* at least one PBX is stupid */
				else if(qd_data[0] == 0x81)
					*nrpos++='.'; /* the very same PBX */
			}
		}
		break;
	case 0x10: /* international */
		*nrpos++='+';
		break;
	case 0x20: /* national */
		*nrpos++='=';
		break;
	case 0x30: /* network specific */
		break;
	case 0x40: /* subscriber */
		*nrpos++='-';
		break;
	case 0x60: /* abbreviated */
		*nrpos++='.';
		break;
	case 0x70: /* extension */
		*nrpos++='.';
		break;
	}
	while (qd_len-- > 0 && (*qd_data++ & 0x80) == 0) ;
	if (qd_len < 1)
		return 0;
	while (qd_len-- > 0) {
		*nrpos++ = *qd_data++;
	}
	*nrpos = '\0';
	return 1;
}

static int
get_chanID (isdn3_conn conn, uchar_t * data, int len)
{
	int qd_len;
	uchar_t *qd_data;

	qd_data = qd_find (data, len, 0, PT_E0_chanID, &qd_len);
	if (qd_data == NULL)
		return 0;
	if (qd_len < 1)
		return 0;
	switch (*qd_data & 0x60) {
	case 0x00:
	case 0x40:
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
	case 0x60:
	case 0x20:
		if ((*qd_data & 0x03) != 1) {
			conn->bchan = 0;
			break;
		}
		if (*qd_data & 0x40) 
			while((--qd_len > 0) && !(*++qd_data & 0x80))  ;
		if (qd_len < 3)
			return 0;
		if (qd_data[1] != 0x83)
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
	report_addprogress (mb, data, len);

	if ((err = isdn3_at_send (conn, mb, 0)) < 0) {
		freemsg (mb);
		setstate (conn, 99);
		return err;
	}
	return err;
}

static int
report_generic (isdn3_conn conn, uchar_t * data, int len, ushort_t id)
{
	int err = 0;

	mblk_t *mb = allocb (256, BPRI_MED);

	if (mb == NULL) {
		setstate (conn, 0);
		return -ENOMEM;
	}
	m_putid (mb, IND_INFO);
	m_putid (mb, id);

	conn_info (conn, mb);

	report_addisplay (mb, data, len);
	report_addstatus (mb, data, len);
	report_addfac (mb, data, len);
	report_addprogress (mb, data, len);

	if ((err = isdn3_at_send (conn, mb, 0)) < 0) {
		freemsg (mb);
		setstate (conn, 0);
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
	m_putid (mb, ID_ET_USER_INFO);
	conn_info (conn, mb);

	qd_data = qd_find (data, len, 0, PT_E0_userInfo, &qd_len);
	if (qd_data == NULL)
		return 0;
	m_putsx (mb, ID_E0_userInfo);
	m_puti (mb,*qd_data);
	m_puthex (mb, qd_data+1, qd_len-1);

	qd_data = qd_find (data, len, 0, PT_E0_moreData, &qd_len);
	if (qd_data != NULL)
		m_putsx (mb, ID_E0_moreData);

	if ((err = isdn3_at_send (conn, mb, 0)) < 0) {
		freemsg (mb);
		setstate (conn, 99);
		return err;
	}
	return err;
}

static int
report_notify (isdn3_conn conn, uchar_t * data, int len)
{
	int qd_len;
	uchar_t *qd_data;

	qd_data = qd_find (data, len, 0, PT_E0_notifyInd, &qd_len);
	if (qd_data == NULL)
		return 0;
	if(qd_len < 1)
		return 0;
	if ((*qd_data & 0x7F) == 0)
		return (isdn3_setup_conn (conn, EST_DISCONNECT /* was INTERRUPT */ ) != 0);
	else if ((*qd_data & 0x7F) == 1)
		return (isdn3_setup_conn (conn, EST_CONNECT) != 0);
	else
		return 0;
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
#if 0
	report_addnsf (mb, data, len);
#endif
	report_addfac (mb, data, len);
	report_adddate (mb, data, len);

	if ((err = isdn3_at_send (conn, mb, 0)) < 0) {
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
	m_putid (mb, ID_ET_CONN_ACK);
	conn_info (conn, mb);

	report_addisplay (mb, data, len);
	report_addfac (mb, data, len);
	report_adddate (mb, data, len);

	if ((err = isdn3_at_send (conn, mb, 0)) < 0) {
		freemsg (mb);
		setstate (conn, 99);
		return err;
	}
	return err;
}

#if 0 /* unused */
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
	m_putid (mb, ID_ET_STAT);
	conn_info (conn, mb);

	cval = report_addcause (mb, data, len);
	switch (cval) {
	case 0: /* RemoteUserSuspend */
		isdn3_setup_conn (conn, EST_DISCONNECT /* was INTERRUPT */ );
		break;
	case 1: /* ET_RemoteUserResumed */
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

	if ((err = isdn3_at_send (conn, mb, 0)) < 0) {
		freemsg (mb);
		setstate (conn, 99);
		return err;
	}
	return err;
}
#endif


#define report_terminate(a,b,c,d) Xreport_terminate((a),(b),(c),(d),__LINE__)
static void
Xreport_terminate (isdn3_conn conn, uchar_t * data, int len, ushort_t cause, int deb_line)
{
	int err = 0;

	mblk_t *mb = allocb (256, BPRI_MED);
	extern int log_34;

	if(log_34 & 2)
		printf("\nET Terminate at %d.\n",deb_line);
	if (mb == NULL) {
		setstate (conn, 0);
		return;
	}
	if (conn->state == 0 || conn->state == 99)
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
		report_addfac (mb, data, len);
	}
	if ((err = isdn3_at_send (conn, mb, 0)) < 0) {
		freemsg (mb);
		setstate (conn, 99);
		return;
	}
	return;
}

#define checkterm(a,b,c) Xcheckterm((a),(b),(c),__LINE__)
static void
Xcheckterm (isdn3_conn conn, uchar_t * data, int len, int deb_line)
{
	if (conn->state == 0) 
		isdn3_killconn (conn, 1); /* XXX */
}


static void
ET_T301 (isdn3_conn conn)
{
	if(log_34 & 2)printf ("Timer ET_T301\n");
	conn->timerflags &= ~RUN_ET_T301;
	switch (conn->state) {
	}
	checkterm (conn, NULL, 0);
}

static void
ET_T302 (isdn3_conn conn)
{
	if(log_34 & 2)printf ("Timer ET_T302\n");
	conn->timerflags &= ~RUN_ET_T302;
	switch (conn->state) {
	case 25:
		/* Timeout ind */
		/* no state change */
		break;
	}
	checkterm (conn, NULL, 0);
}

static void
ET_T303 (isdn3_conn conn)
{
	if(log_34 & 2)printf ("Timer ET_T303\n");
	/* DSS1: retry the setup. We decide not to bother. */
	conn->timerflags &= ~RUN_ET_T303;
	switch (conn->state) {
	case 1:
		send_disc (conn, 0,NULL);
		isdn3_setup_conn (conn, EST_DISCONNECT);
		setstate (conn, 99);
		report_terminate (conn,NULL,0,ID_NOREPLY);
		break;
	}
	checkterm (conn, NULL, 0);
}

static void
ET_T304 (isdn3_conn conn)
{
	if(log_34 & 2)printf ("Timer ET_T304\n");
	conn->timerflags &= ~RUN_ET_T304;
	switch (conn->state) {
	case 2:
		{
			mblk_t *data = allocb(16,BPRI_MED);
			if(data != NULL) {
				unsigned char * qd_d;
				int qd_len = 0;
				if ((qd_d = qd_insert ((uchar_t *) data->b_rptr, &qd_len, 0, PT_E0_cause, 2, 0)) == NULL) {
					freemsg(data);
					data = NULL;
				} else {
					*qd_d++ = 0x80;
					*qd_d++ = 0x80 | ET_TimerRecovery;
					data->b_wptr += qd_len;
				}
			}
			if(phone_sendback (conn, MT_ET_DISC, data) != 0)
				freemsg(data);
		}
		/* send error up */
		setstate (conn,11);
	}
	checkterm (conn, NULL, 0);
}

static void
ET_T305 (isdn3_conn conn)
{
	if(log_34 & 2)printf ("Timer ET_T305\n");
	conn->timerflags &= ~RUN_ET_T305;
	switch (conn->state) {
	case 11:
	phone_sendback(conn, MT_ET_REL, NULL);
		setstate(conn,19);
	}
	checkterm (conn, NULL, 0);
}

static void
ET_T308 (isdn3_conn conn)
{
	if(log_34 & 2)printf ("Timer ET_T308\n");
	conn->timerflags &= ~RUN_ET_T308;
	switch (conn->state) {
	case 19:
		{
			mblk_t *data;
			data = allocb(16,BPRI_MED);
			if(data != NULL) {
				unsigned char * qd_d;
				int qd_len = 0;
				if ((qd_d = qd_insert ((uchar_t *) data->b_rptr, &qd_len, 0, PT_E0_cause, 2, 0)) == NULL) {
					freemsg(data);
				} else {
					*qd_d++ = 0x80;
					*qd_d++ = 0x80 | ET_TimerRecovery;
					data->b_wptr += qd_len;
					if (phone_sendback (conn, MT_ET_DISC, data) < 0)
						freemsg(data);
				}
			}
		}
		setstate(conn,99);
		break;
	}
	checkterm (conn, NULL, 0);
}

#ifdef HAS_RECOVERY
static void
ET_T309 (isdn3_conn conn)
{
	if(log_34 & 2)printf ("Timer ET_T309\n");
	conn->timerflags &= ~RUN_ET_T309;
	switch (conn->state) {
	}
	checkterm (conn, NULL, 0);
}
#endif

static void
ET_T310 (isdn3_conn conn)
{
	if(log_34 & 2)printf ("Timer ET_T310\n");
	conn->timerflags &= ~RUN_ET_T310;
	switch (conn->state) {
	case 3:
		{
			mblk_t *data = allocb(16,BPRI_MED);
			if(data != NULL) {
				unsigned char * qd_d;
				int qd_len = 0;
				if ((qd_d = qd_insert ((uchar_t *) data->b_rptr, &qd_len, 0, PT_E0_cause, 2, 0)) == NULL) {
					freemsg(data);
					data = NULL;
				} else {
					*qd_d++ = 0x80;
					*qd_d++ = 0x80 | ET_TimerRecovery;
					data->b_wptr += qd_len;
				}
			}
			if(phone_sendback (conn, MT_ET_DISC, data) != 0)
				freemsg(data);
		}
		/* send error up */
		setstate (conn,11);
		break;
	}
	checkterm (conn, NULL, 0);
}

static void
ET_T313 (isdn3_conn conn)
{
	if(log_34 & 2)printf ("Timer ET_T313\n");
	conn->timerflags &= ~RUN_ET_T313;
	switch (conn->state) {
	case 8:
		{
			mblk_t *data = allocb(16,BPRI_MED);
			if(data != NULL) {
				unsigned char * qd_d;
				int qd_len = 0;
				if ((qd_d = qd_insert ((uchar_t *) data->b_rptr, &qd_len, 0, PT_E0_cause, 2, 0)) == NULL) {
					freemsg(data);
					data = NULL;
				} else {
					*qd_d++ = 0x80;
					*qd_d++ = 0x80 | ET_TimerRecovery;
					data->b_wptr += qd_len;
				}
			}
			if(phone_sendback (conn, MT_ET_DISC, data) != 0)
				freemsg(data);
		}
		/* send error up */
		setstate (conn,11);
		break;
	}
	checkterm (conn, NULL, 0);
}

static void
ET_T314 (isdn3_conn conn)
{
	if(log_34 & 2)printf ("Timer ET_T314\n");
	conn->timerflags &= ~RUN_ET_T314;
	switch (conn->state) {
	}
	checkterm (conn, NULL, 0);
}

static void
ET_T316 (isdn3_conn conn)
{
	if(log_34 & 2)printf ("Timer ET_T316\n");
	conn->timerflags &= ~RUN_ET_T316;
	switch (conn->state) {
	}
	checkterm (conn, NULL, 0);
}

static void
ET_T317 (isdn3_conn conn)
{
	if(log_34 & 2)printf ("Timer ET_T317\n");
	conn->timerflags &= ~RUN_ET_T317;
	switch (conn->state) {
	}
	checkterm (conn, NULL, 0);
}

#ifdef HAS_SUSPEND
static void
ET_T318 (isdn3_conn conn)
{
	if(log_34 & 2)printf ("Timer ET_T318\n");
	conn->timerflags &= ~RUN_ET_T318;
	switch (conn->state) {
	case 17:
		/* Err */
		phone_sendback(conn,MT_ET_REL,NULL);
		setstate (conn, 19);
		break;
	}
	checkterm (conn, NULL, 0);
}

static void
ET_T319 (isdn3_conn conn)
{
	if(log_34 & 2)printf ("Timer ET_T319\n");
	conn->timerflags &= ~RUN_ET_T319;
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
ET_T321 (isdn3_conn conn)
{
	if(log_34 & 2)printf ("Timer ET_T321\n");
	conn->timerflags &= ~RUN_ET_T321;
	switch (conn->state) {
	case 17:
		/* Err */
		setstate (conn, 99);
		break;
	}
	checkterm (conn, NULL, 0);
}

static void
ET_T322 (isdn3_conn conn)
{
	if(log_34 & 2)printf ("Timer ET_T322\n");
	conn->timerflags &= ~RUN_ET_T322;
	switch (conn->state) {
	case 17:
		/* Err */
		setstate (conn, 99);
		break;
	}
	checkterm (conn, NULL, 0);
}


static void
release_postET (isdn3_conn conn, uchar_t minor, char force)
{
	switch (conn->state) {
		case 1:
		untimer (ET_T303, conn);
		if (force) {
			/* technically an error */
		}
		break;
	case 2:
		untimer (ET_T304, conn);
		if (force) {
			/* technically an error */
		}
		break;
	case 3:
		untimer (ET_T310, conn);
		if (force) {
			/* technically an error */
		}
		break;
	case 8:
		untimer (ET_T313, conn);
		/* FALL THRU */
	case 4:
	case 7:
	case 10:
		if (force) {
			/* technically an error */
		}
		/* FALL THRU */
	case 14:
		break;
	case 6:
		break;
#ifdef HAS_SUSPEND
	case 15:
		untimer (ET_T319, conn);
		if (force) {
			/* technically an error */
		}
		break;
	case 17:
		untimer (ET_T318, conn);
		if (force) {
			/* technically an error */
		}
		break;
#endif
	case 11:
		untimer (ET_T305, conn);
		if (!force) {
			/* Technically an error */
			force = 1;
		}
	case 12:
		force = 1;
		break;
	case 19:
		/* Technically an error */
		return;
	default:
		setstate (conn, 99);
		return;
	}
	if (force) {
		phone_sendback (conn, MT_ET_REL, NULL);
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
		setstate (conn, 19);
	} else {
		mblk_t *data = allocb(16,BPRI_MED);
		if(data != NULL) {
			unsigned char * qd_d;
			int qd_len = 0;
			if ((qd_d = qd_insert ((uchar_t *) data->b_rptr, &qd_len, 0, PT_E0_cause, 2, 0)) == NULL) {
				freemsg(data);
				data = NULL;
			} else {
				*qd_d++ = 0x80;
				*qd_d++ = 0x80 | ET_TimerRecovery;
				data->b_wptr += qd_len;
			}
		}
		if(phone_sendback (conn, MT_ET_DISC, data) != 0)
			freemsg(data);
		setstate (conn, 11);
	}
}

static void ET_TFOO(isdn3_conn conn)
{
	setstate(conn, 0);
	isdn3_killconn (conn, 1); /* XXX */
}

static void
ET_TCONN (isdn3_conn conn)
{
	if(log_34 & 2)printf ("Timer ET_TCONN\n");
	conn->timerflags &= ~RUN_ET_TCONN;
	conn->lockit++;
	switch (conn->state) {
	case 7: {
		mblk_t *mb = NULL;
		int qd_len = 0;
		streamchar *qd_d;
		if((mb = allocb(20,BPRI_LO)) != NULL) {
			
            if ((qd_d = qd_insert ((uchar_t *) mb->b_rptr, &qd_len, 0, PT_E0_cause, 2, 0)) == NULL) {                                   
                freeb(mb);
                mb = NULL;
            } else {
				*qd_d++ = 0x80; /* CCITT; User */
				if(!(conn->minorstate & MS_BCHAN))
					*qd_d = ET_UserBusy;
				else if(!(conn->minorstate & MS_SETUP_MASK))
					*qd_d = ET_NoAnswer; /* XXX ;-) */
				else
					*qd_d = ET_OutOfOrder;
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
ET_TALERT (isdn3_conn conn)
{
	if(log_34 & 2)printf ("Timer ET_TALERT\n");
	conn->timerflags &= ~RUN_ET_TALERT;
	conn->lockit++;
	switch (conn->state) {
	case 6:
		{
			mblk_t *asn = NULL;
#if 0
			int qd_len = 0;    
			uchar_t *qd_d;

			if ((((struct e_info *)conn->p_data)->flags & SVC_PENDING) && (asn = allocb (32, BPRI_MED)) != NULL) {
				if ((qd_d = qd_insert ((uchar_t *) asn->b_rptr, &qd_len, 0, PT_N0_netSpecFac, 4, 1)) == NULL) {                                   
					freeb(asn);
					asn = NULL;
				} else {
					*(uchar_t *) qd_d++ = 0;                            
					*(uchar_t *) qd_d++ = ET_FAC_SVC;
					*(ushort_t *) qd_d = 0;                     
					asn->b_wptr = asn->b_rptr + qd_len;
				}
			}                                                       
#endif

			if(phone_sendback (conn, MT_ET_ALERT, asn) != 0 && asn != NULL)
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
	struct e_info *info;

printf (" ET: Recv %x in state %d\n", msgtype, conn->state);
	if(conn->p_data == NULL) {
		if((conn->p_data = malloc(sizeof(struct e_info))) == NULL) {
			return -ENOMEM;
		}
		bzero(conn->p_data,sizeof(struct e_info));
	}
	info = (struct e_info *)conn->p_data;
	switch(msgtype) {
	case MT_ET_STAT_ENQ:
		report_generic (conn, data, len, ID_ET_STAT_ENQ);
		{
			mblk_t *asn = NULL;
			int qd_len = 0;    
			uchar_t *qd_d;
			if ((asn = allocb (32, BPRI_MED)) != NULL) {
				if ((qd_d = qd_insert ((uchar_t *) asn->b_rptr, &qd_len, 0, PT_E0_cause, 2, 1)) != NULL) {
					*(uchar_t *) qd_d++ = 0x80;
					*(ushort_t *) qd_d = 0x80;
				}
				if ((qd_d = qd_insert ((uchar_t *) asn->b_rptr, &qd_len, 0, PT_E0_callState, 1, 1)) != NULL) {
					*(ushort_t *) qd_d = conn->state;
				}
				asn->b_wptr = asn->b_rptr + qd_len;
			}
			if(phone_sendback (conn, MT_ET_STAT, asn) != 0 && asn != NULL)
				freemsg(asn);
		}
		break;
	case MT_ET_INFO:
		if (conn->state == 0 || conn->state == 1 || conn->state == 6
					|| conn->state == 17 || conn->state == 19
					|| conn->state == 25)
			goto do_continue;
		report_generic (conn, data, len, ID_ET_INFO);
		break;
	
	case MT_ET_FAC:
		report_generic (conn, data, len, ID_ET_FAC);
		break;

	case MT_ET_STAT:
		{
			int qd_len;
			uchar_t *qd_data;
			uchar_t state = 0;

			qd_data = qd_find (data, len, 0, PT_E0_callState, &qd_len);
			if (qd_data != NULL && qd_len > 0)
				state = *qd_data;

			if((conn->state == 0 || conn->state == 19) && state != 0) {
				phone_sendback (conn, MT_ET_REL, NULL);
				setstate (conn, 19);
			}
		}
		report_generic (conn, data, len, ID_ET_STAT);
		break;
	case MT_ET_REL_COM:
		if (conn->state == 0 || conn->state == 1 || conn->state == 19)
			goto do_continue;
		isdn3_setup_conn (conn, EST_DISCONNECT);
	  ComEx:
		if(conn->state == 6 || conn->state == 7 || conn->state == 99)
			setstate (conn, 99);
		else
			setstate (conn, 0);
		report_terminate (conn, data, len,0);
		break;
	case MT_ET_REL:
		if (conn->state == 0 || conn->state == 1 || conn->state == 6
					|| conn->state == 11 || conn->state == 12
					|| conn->state == 15
					|| conn->state == 17 || conn->state == 19)
			goto do_continue;
		phone_sendback (conn, MT_ET_REL_COM, NULL);
		isdn3_setup_conn (conn, EST_DISCONNECT);
		goto ComEx;
	case MT_ET_DISC:
		if (conn->state == 0 || conn->state == 1 || conn->state == 6
					|| conn->state == 11 || conn->state == 12
					|| conn->state == 15
					|| conn->state == 17 || conn->state == 19)
			goto do_continue;
		isdn3_setup_conn (conn, EST_DISCONNECT);
		setstate (conn, 12);
		report_terminate (conn, data, len,0);
		(void)send_disc (conn, 1, NULL);
		break;
	default:
	do_continue:
		switch (conn->state) {
		case 0:
			switch (msgtype) {
			case MT_ET_SETUP:
				conn->minorstate |= MS_INCOMING;
				setstate (conn, 6);

				(void) get_chanID (conn, data, len);
				if (isdn3_setup_conn (conn, EST_LISTEN) != 0) {
					setstate(conn,99);
					goto pack_err;
				}
				get_nr (conn, data, len, info->nr, PT_E0_origAddr);
				get_nr (conn, data, len, info->lnr, PT_E0_destAddr);
				get_hex(conn,data,len,info->bearer,&info->bearer_len,sizeof(info->bearer),PT_E0_bearer_cap);
				get_hex(conn,data,len,info->llc,&info->llc_len,sizeof(info->llc),PT_E0_compatLo);
				get_hex(conn,data,len,info->ulc,&info->ulc_len,sizeof(info->ulc),PT_E0_compatHi);

#if 0
				/*
				* Check if transferred call. If we are forwarding this B channel
				* to another device, ignore the call.
				*/
				{
					isdn3_conn nconn;

					QD_INIT (data, len)
							break;
					conn->((struct e_info *)conn->p_data) &=~ SVC_PENDING;
					QD {
					QD_CASE (0, PT_N0_netSpecFac):
						if (qd_len < 4)
							continue;
						switch (qd_data[1]) {
						case ET_FAC_SVC:
							conn->((struct e_info *)conn->p_data) |= SVC_PENDING;
							break;
						case ET_FAC_DisplayUebergeben:
							if (!(conn->minorstate & MS_BCHAN))
								break;
							for (nconn = conn->talk->conn; nconn != NULL; nconn = nconn->next)
								if ((nconn->minorstate & MS_FORWARDING) && (nconn->minorstate & MS_BCHAN) && (nconn->bchan == conn->bchan))
									goto out;
						}
					}
					QD_EXIT;
				}
#endif

				{
					long flags = isdn3_flags(conn->card->info,-1,-1);
					if(flags & FL_ANS_IMMED) {
						untimer(ET_TALERT,conn);
						ET_TALERT(conn);
					}
				}
				report_setup (conn, data, len);
				break;
			case MT_ET_REL:
				phone_sendback (conn, MT_ET_REL_COM, NULL);
				report_terminate (conn, data, len,0);
				break;
			case MT_ET_REL_COM:
				break;
			default:
				/* Send CAUSE */
				phone_sendback (conn, MT_ET_REL, NULL);
				setstate (conn, 19);
				break;
			}
			break;
		case 1:
			switch (msgtype) {
			case MT_ET_SETUP_ACK:
				(void) get_chanID (conn, data, len);
				if (isdn3_setup_conn (conn, EST_LISTEN) != 0)
					goto pack_err;
				report_generic (conn, data, len, ID_ET_SETUP_ACK);
				setstate (conn, 2);
				break;
			case MT_ET_PROCEEDING:
				(void) get_chanID (conn, data, len);
				if (isdn3_setup_conn (conn, EST_LISTEN) != 0)
					goto pack_err;
				report_generic (conn, data, len, MT_ET_PROCEEDING);
				setstate (conn, 3);
				break;
			case MT_ET_ALERT:
				(void) get_chanID (conn, data, len);
				if (isdn3_setup_conn (conn, EST_LISTEN) != 0)
					goto pack_err;
				report_generic (conn, data, len, ID_ET_ALERT);
				setstate (conn, 4);
				break;
			case MT_ET_CONN:
				(void) get_chanID (conn, data, len);
				if (!(conn->minorstate & MS_BCHAN))
					goto pack_err;
				if (isdn3_setup_conn (conn, EST_CONNECT) != 0)
					goto pack_err;
				report_conn (conn, data, len);
				phone_sendback (conn, MT_ET_CONN_ACK, NULL);
				setstate (conn, 10);
				break;
			case MT_ET_REL:
				/* send REL up -- done when dropping out below */
				phone_sendback (conn, MT_ET_REL_COM, NULL);
				setstate (conn, 99);
				report_terminate (conn, data, len,0);
				break;
			case MT_ET_DISC:
				isdn3_setup_conn (conn, EST_DISCONNECT);
				setstate (conn, 12);
				report_terminate (conn, data, len,0);
				(void)send_disc (conn, 1, NULL);
				break;
			case MT_ET_REL_COM:
				isdn3_setup_conn (conn, EST_DISCONNECT);
				setstate (conn, 99);
				report_terminate (conn, data, len,0);
				break;
			default:
				goto msg_err;
			}
			break;
		case 2:
			switch (msgtype) {
			case MT_ET_PROGRESS:
				report_generic (conn, data, len, ID_ET_PROGRESS);
				break;
			case MT_ET_PROCEEDING:
				(void) get_chanID (conn, data, len);
				if (isdn3_setup_conn (conn, EST_LISTEN) != 0)
					goto pack_err;
				report_generic (conn, data, len, ID_ET_PROCEEDING);
				setstate (conn, 3);
				break;
			case MT_ET_ALERT:
				(void) get_chanID (conn, data, len);
				if (isdn3_setup_conn (conn, EST_LISTEN) != 0)
					goto pack_err;
				report_generic (conn, data, len, ID_ET_ALERT);
				setstate (conn, 4);
				break;
			case MT_ET_CONN:
				(void) get_chanID (conn, data, len);
				if (!(conn->minorstate & MS_BCHAN))
					goto pack_err;
				if (isdn3_setup_conn (conn, EST_CONNECT) != 0)
					goto pack_err;
				report_conn (conn, data, len);
				phone_sendback (conn, MT_ET_CONN_ACK, NULL);
				setstate (conn, 10);
				break;
			default:
				goto msg_err;
			}
			break;
		case 3:
			switch (msgtype) {
			case MT_ET_ALERT:
				report_generic (conn, data, len, ID_ET_PROGRESS);
				setstate (conn, 4);
				break;
			case MT_ET_CONN:
				(void) get_chanID (conn, data, len);
				if (!(conn->minorstate & MS_BCHAN))
					goto pack_err;
				if (isdn3_setup_conn (conn, EST_CONNECT) != 0)
					goto pack_err;
				report_conn (conn, data, len);
				phone_sendback (conn, MT_ET_CONN_ACK, NULL);
				setstate (conn, 10);
				break;
			default:
				goto msg_err;
			}
			break;
		case 4:
			switch (msgtype) {
			case MT_ET_CONN:
				(void) get_chanID (conn, data, len);
				if (!(conn->minorstate & MS_BCHAN))
					goto pack_err;
				if (isdn3_setup_conn (conn, EST_CONNECT) != 0)
					goto pack_err;
				report_conn (conn, data, len);
				phone_sendback (conn, MT_ET_CONN_ACK, NULL);
				setstate (conn, 10);
				break;
			default:
				goto msg_err;
			}
			break;
		case 6:
			switch (msgtype) {
			case MT_ET_SETUP:
				(void) get_chanID (conn, data, len);
				break;
			case MT_ET_REL:
				phone_sendback (conn, MT_ET_REL_COM, NULL);
				setstate (conn, 99);
				report_terminate (conn, data, len,0);
				break;
			case MT_ET_DISC:
				isdn3_setup_conn (conn, EST_DISCONNECT);
				setstate (conn, 12);
				report_terminate (conn, data, len,0);
				(void)send_disc (conn, 1, NULL);
				break;
			default:
				goto msg_err;
			}
			break;
		case 7:
			switch (msgtype) {
			case MT_ET_SETUP:
				(void) get_chanID (conn, data, len);
				if (isdn3_setup_conn (conn, EST_LISTEN) != 0)
					goto pack_err;
				break;
			case MT_ET_REL:
				/* send REL up */
				phone_sendback (conn, MT_ET_REL_COM, NULL);
				setstate (conn, 99);
				break;
			case MT_ET_DISC:
				isdn3_setup_conn (conn, EST_DISCONNECT);
				setstate (conn, 12);
				report_terminate (conn, data, len,0);
				(void)send_disc (conn, 1, NULL);
				break;
			default:
				goto msg_err;
			}
			break;
		case 8:
			switch (msgtype) {
			case MT_ET_CONN_ACK:
				(void) get_chanID (conn, data, len);
				if (!(conn->minorstate & MS_BCHAN))
					goto pack_err;
				if (isdn3_setup_conn (conn, EST_CONNECT) != 0)
					goto pack_err;
				report_conn_ack (conn, data, len);
				setstate (conn, 10);
				break;
			case MT_ET_SETUP:
				break;
			case MT_ET_REL:
				/* send REL up */
				phone_sendback (conn, MT_ET_REL_COM, NULL);
				setstate (conn, 99);
				break;
			case MT_ET_DISC:
				isdn3_setup_conn (conn, EST_DISCONNECT);
				setstate (conn, 12);
				report_terminate (conn, data, len,0);
				(void)send_disc (conn, 1, NULL);
				break;
			default:
				goto msg_err;
			}
			break;
		case 9:
			switch (msgtype) {
			case MT_ET_CONN_ACK:
				(void) get_chanID (conn, data, len);
				if (!(conn->minorstate & MS_BCHAN))
					goto pack_err;
				if (isdn3_setup_conn (conn, EST_CONNECT) != 0)
					goto pack_err;
				report_conn_ack (conn, data, len);
				setstate (conn, 10);
				break;
			case MT_ET_SETUP:
				break;
			case MT_ET_REL:
				/* send REL up */
				phone_sendback (conn, MT_ET_REL_COM, NULL);
				setstate (conn, 99);
				report_terminate (conn, data, len,0);
				break;
			case MT_ET_DISC:
				isdn3_setup_conn (conn, EST_DISCONNECT);
				setstate (conn, 12);
				report_terminate (conn, data, len,0);
				(void)send_disc (conn, 1, NULL);
				break;
			default:
				goto msg_err;
			}
			break;
		case 10:
			switch (msgtype) {
			case MT_ET_SETUP:
				break;
			case MT_ET_NOTIFY:
				report_notify (conn, data, len);
				break;
			case MT_ET_USER_INFO:
				report_user_info (conn, data, len);
				break;
			case MT_ET_REL:
				phone_sendback (conn, MT_ET_REL_COM, NULL);
				setstate (conn, 99);
				report_terminate (conn, data, len,0);
				break;
			default:
				goto msg_err;
			}
			break;
		case 14:
			switch (msgtype) {
			case MT_ET_SETUP:
				break;
			case MT_ET_CONN_ACK:
				(void) get_chanID (conn, data, len);
				if (!(conn->minorstate & MS_BCHAN))
					goto pack_err;
				if (isdn3_setup_conn (conn, EST_CONNECT) != 0)
					goto pack_err;
				report_conn_ack (conn, data, len);
				setstate (conn, 10);
				break;
			case MT_ET_REL:
				phone_sendback (conn, MT_ET_REL_COM, NULL);
				setstate (conn, 99);
				report_terminate (conn, data, len,0);
				break;
			case MT_ET_DISC:
				isdn3_setup_conn (conn, EST_DISCONNECT);
				setstate (conn, 12);
				report_terminate (conn, data, len,0);
				(void)send_disc (conn, 1, NULL);
				break;
			default:
				goto msg_err;
			}
			break;
#ifdef HAS_SUSPEND
		case 15:
			switch (msgtype) {
			case MT_ET_SETUP:
				break;
			case MT_ET_SUSP_ACK:
				/* send SUSP_ACK up */
				setstate (conn, 99);
				break;
			case MT_ET_SUSP_REJ:
				/* send SUSP_REJ up */
				setstate (conn, 10);
				break;
			case MT_ET_REL:
				phone_sendback (conn, MT_ET_REL_COM, NULL);
				setstate (conn, 99);
				report_terminate (conn, data, len,0);
				break;
			case MT_ET_DISC:
				isdn3_setup_conn (conn, EST_DISCONNECT);
				setstate (conn, 12);
				report_terminate (conn, data, len,0);
				(void)send_disc (conn, 1, NULL);
				break;
			case MT_ET_RES_REJ:
			case MT_ET_REG_ACK:
			case MT_ET_REG_REJ:
			case MT_ET_CANC_ACK:
			case MT_ET_CANC_REJ:
				/* Fehlermeldung */
				break;
			default:
				goto msg_err;
			}
			break;
		case 17:
			switch (msgtype) {
			case MT_ET_RES_ACK:
				/* send RES_ACK up */
				(void) get_chanID (conn, data, len);
				if (isdn3_setup_conn (conn, EST_CONNECT) != 0)
					goto pack_err;
				setstate (conn, 10);
				break;
			case MT_ET_RES_REJ:
				/* send RES_REJ up */
				setstate (conn, 99);
				break;
			case MT_ET_REL:
				phone_sendback (conn, MT_ET_REL_COM, NULL);
				setstate (conn, 99);
				report_terminate (conn, data, len,0);
				break;
			case MT_ET_DISC:
				isdn3_setup_conn (conn, EST_DISCONNECT);
			common_17_REL:
				phone_sendback (conn, MT_ET_REL, NULL);
				setstate (conn, 19);
				report_terminate (conn, data, len,0);
				break;
			case MT_ET_SUSP_ACK:
			case MT_ET_REG_ACK:
			case MT_ET_REG_REJ:
			case MT_ET_CANC_ACK:
			case MT_ET_CANC_REJ:
				/* Fehlermeldung */
				setstate (conn, 4);
				break;
			default:
				goto msg_err;
			}
			break;
#endif
		case 11:
			switch (msgtype) {
			case MT_ET_SETUP:
				break;
			case MT_ET_DISC:
				/* Release B chan */
				phone_sendback (conn, MT_ET_REL, NULL);
				setstate (conn, 19);
				report_terminate (conn, data, len,0);
				break;
			case MT_ET_REL:
				phone_sendback (conn, MT_ET_REL_COM, NULL);
				setstate (conn, 99);
				report_terminate (conn, data, len,0);
				break;
			default:
				goto msg_err;
			}
			break;
		case 12:
			switch (msgtype) {
			case MT_ET_SETUP:
				break;
			case MT_ET_REL:
				phone_sendback (conn, MT_ET_REL_COM, NULL);
				setstate (conn, 99);
				report_terminate (conn, data, len,0);
				break;
			default:
				goto msg_err;
			}
			break;
		case 19:
			switch (msgtype) {
			case MT_ET_SETUP:
				break;
			case MT_ET_REL:
			case MT_ET_REL_COM:
				setstate (conn, 99);
				report_terminate (conn, data, len,0);
				break;
			default:
				goto msg_err;
			}
			break;
#if 0
		case 20:
			switch (msgtype) {
			case MT_ET_REL:
				phone_sendback (conn, MT_ET_REL_COM, NULL);
				setstate (conn, 99);
				report_terminate (conn, data, len,0);
				break;
			case MT_ET_REG_ACK:
				/* send REG_ACK up */
				setstate (conn, 99);
				break;
			case MT_ET_REG_REJ:
				/* send REG_REJ up */
				setstate (conn, 99);
				break;
			case MT_ET_DISC:
				isdn3_setup_conn (conn, EST_DISCONNECT);
				goto common_20_REL_COM;
			common_20_REL_COM:
				phone_sendback (conn, MT_ET_REL, NULL);
				setstate (conn, 19);
				report_terminate (conn, data, len,0);
				break;
			default:
				goto msg_err;
			}
			break;
		case 21:
			switch (msgtype) {
			case MT_ET_REL:
				phone_sendback (conn, MT_ET_REL_COM, NULL);
				setstate (conn, 99);
				report_terminate (conn, data, len,0);
				break;
			case MT_ET_CANC_ACK:
				/* send CANC_ACK up */
				setstate (conn, 99);
				break;
			case MT_ET_CANC_REJ:
				/* send CANC_REJ up */
				setstate (conn, 99);
				break;
			case MT_ET_DISC:
				isdn3_setup_conn (conn, EST_DISCONNECT);
				goto common_21_REL_COM;
			common_21_REL_COM:
				phone_sendback (conn, MT_ET_REL, NULL);
				setstate (conn, 19);
				report_terminate (conn, data, len,0);
				break;
			default:
				goto msg_err;
			}
#endif
			break;
		case 25:
			switch (msgtype) {
			}
			break;
		case 99:
			break;
		default:
			setstate (conn, 99);
			break;
		}
	}
  out:
	checkterm (conn, data, len);
	return 0;
  pack_err:
	release_postET (conn, 0, 1);
	goto out;
  msg_err: /* XXX TODO */
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
				{
					mblk_t *data = allocb(16,BPRI_MED);
					if(data != NULL) {
						unsigned char * qd_d;
						int qd_len = 0;
						if ((qd_d = qd_insert ((uchar_t *) data->b_rptr, &qd_len, 0, PT_E0_cause, 2, 0)) == NULL) {
							freemsg(data);
							data = NULL;
						} else {
							*qd_d++ = 0x80;
							*qd_d++ = 0x80 | ET_TempFailure;
							data->b_wptr += qd_len;
						}
					}
					if(phone_sendback (conn, MT_ET_DISC, data) != 0)
						freemsg(data);
				}
				/* Error */
				setstate (conn, 11);
				break;
			case 7:
				break;
			case 14:
				{
					mblk_t *data = allocb(16,BPRI_MED);
					if(data != NULL) {
						unsigned char * qd_d;
						int qd_len = 0;
						if ((qd_d = qd_insert ((uchar_t *) data->b_rptr, &qd_len, 0, PT_E0_cause, 2, 0)) == NULL) {
							freemsg(data);
							data = NULL;
						} else {
							*qd_d++ = 0x80;
							*qd_d++ = 0x80 | ET_TempFailure;
							data->b_wptr += qd_len;
						}
					}
					if(phone_sendback (conn, MT_ET_DISC, data) != 0)
						freemsg(data);
					timer (ET_T305, conn);
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
	int err = 0;
	mblk_t *owndata = NULL;

	switch (conn->state) {
	case 0:
		break;
#ifdef HAS_SUSPEND
	case 15:
	case 17:
		return -EBUSY;
#endif
	case 12:
		if (((err = phone_sendback (conn, MT_ET_REL, data)) != 0) && (data != NULL))
			freemsg(data);
		setstate (conn, 19);
		break;
	case 11:
		if (release) {
			goto common_off_noconn; /* Is this confirming? */
		} else
			return -EBUSY;
	case 1:
	case 2:
	case 3:
#ifdef HAS_SUSPEND
	case 15:
#endif
	case 4:
	default:
	  common_off:
	    if(data == NULL) {
			owndata = data = allocb(16,BPRI_MED);
			if(data != NULL) {
				unsigned char * qd_d;
				int qd_len = 0;
				if ((qd_d = qd_insert ((uchar_t *) data->b_rptr, &qd_len, 0, PT_E0_cause, 2, 0)) == NULL) {
					data = NULL;
					freemsg(owndata);
				} else {
					*qd_d++ = 0x80;
					*qd_d++ = 0x80 | ET_NormalClear;
					data->b_wptr += qd_len;
				}
			}
		}
		if ((err = phone_sendback (conn, MT_ET_DISC, data)) == 0)
			data = NULL;
		setstate (conn, 11);
		break; /* NO FALL THRU */
	case 6:
	common_off_noconn:
		if(release > 1)
			goto common_off; /* XXX experimental */
		if ((err = phone_sendback (conn, MT_ET_REL_COM, data)) != 0 && data != NULL)
			freemsg(data);
		setstate (conn, 99);
		break;
	case 19:
	case 99:
		break;
	}
	if(data != NULL && data == owndata && err != 0)
		freemsg(owndata);
	return err;
}

static int
sendcmd (isdn3_conn conn, ushort_t id, mblk_t * data)
{
	streamchar *oldpos = NULL;
	int err = 0;
	ushort_t typ;
	uchar_t suppress = 0;
	/* uchar_t svc = 0; */
	struct e_info *info;
	long cause = -1;
	char forceit = 0;

	if(conn->p_data == NULL) {
		if((conn->p_data = malloc(sizeof(struct e_info))) == NULL) {
			return -ENOMEM;
		}
		bzero(conn->p_data,sizeof(struct e_info));
	}
	conn->lockit++;
	info = conn->p_data;
	if (data != NULL) {
		oldpos = data->b_rptr;
		while ((err = m_getsx (data, &typ)) == 0) {
			switch (typ) {
			case ARG_FASTDROP:
				if (conn->state == 6 || conn->state == 7 ||
					conn->state == 8)
					forceit = 1;
				break;
			case ARG_FORCE:
				forceit = 1;
				break;
			case ARG_CAUSE:
				{
					ushort_t causeid;

					if (m_getid (data, &causeid) != 0)
						break;
					cause = et_idtocause(causeid);
				}
				break;
			case ARG_LLC:
				{
					int len = m_gethexlen(data);
					if (len > 0) {
						if (len > sizeof(info->llc))
							len = sizeof(info->llc);
						info->llc_len = len;
						m_gethex(data,info->llc,len);
					}
				}
				break;
			case ARG_ULC:
				{
					int len = m_gethexlen(data);
					if (len > 0) {
						if (len > sizeof(info->ulc))
							len = sizeof(info->ulc);
						info->ulc_len = len;
						m_gethex(data,info->ulc,len);
					}
				}
				break;
			case ARG_BEARER:
				{
					int len = m_gethexlen(data);
					if (len > 0) {
						if (len > sizeof(info->bearer))
							len = sizeof(info->bearer);
						info->bearer_len = len;
						m_gethex(data,info->bearer,len);
					}
				}
				break;
			case ARG_SUPPRESS:
				suppress = 1;
				break;
			case ARG_LNUMBER:
				m_getskip (data);
				if ((err = m_getstr (data, (char *) info->lnr, MAXNR)) != 0) {
					printf("GetStr LNumber: ");
					conn->lockit--;
					return err;
				}
				break;
			case ARG_NUMBER:
				m_getskip (data);
				if ((err = m_getstr (data, (char *) info->nr, MAXNR)) != 0) {
					printf("GetStr Number: ");
					conn->lockit--;
					return err;
				}
				break;
			default:;
			}
		}
		data->b_rptr = oldpos;
	}
	err = 0;
	switch (id) {
	case CMD_DIAL:
		{
			if (data == NULL) {
				printf("DataNull: ");
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

					if (info->bearer_len > 0) {
						if ((qd_d = qd_insert ((uchar_t *) asn->b_rptr, &qd_len, 0, PT_E0_bearer_cap, info->bearer_len, 0)) == NULL) {
							conn->lockit--;
							return -EIO;
						}
						bcopy(info->bearer,qd_d,info->bearer_len);
					}
					if (info->llc_len > 0) {
						if ((qd_d = qd_insert ((uchar_t *) asn->b_rptr, &qd_len, 0, PT_E0_compatLo, info->llc_len, 0)) == NULL) {
							conn->lockit--;
							return -EIO;
						}
						bcopy(info->llc,qd_d,info->llc_len);
					}
					if (info->ulc_len > 0) {
						if ((qd_d = qd_insert ((uchar_t *) asn->b_rptr, &qd_len, 0, PT_E0_compatLo, info->ulc_len, 0)) == NULL) {
							conn->lockit--;
							return -EIO;
						}
						bcopy(info->ulc,qd_d,info->ulc_len);
					}
					if (info->nr[0] != '\0') {
						int i = 0, j;
						uchar_t nrtype;

						switch(info->nr[0]) {
						case '0': case '1': case '2': case '3': case '4':
						case '5': case '6': case '7': case '8': case '9':
							nrtype = 0x00; break;
						case '+': /* international */
							nrtype = 0x11; i = 1; break;
						case '=': /* national */
							nrtype = 0x21; i = 1; break;
						case '-': /* subscriber */
							nrtype = 0x41; i = 1; break;
						case '.': /* abbreviated */
						case '/': /* abbreviated */
							nrtype = 0x61; i = 1; break;
						default:
							nrtype = 0x00; i = 1;
							break;
						}
						for (j = i; j < MAXNR; j++)
							if (info->nr[j] == '\0')
								break;
						if ((qd_d = qd_insert ((uchar_t *) asn->b_rptr, &qd_len, 0, PT_E0_destAddr, j - i + 1, 0)) == NULL) {
							conn->lockit--;
							return -EIO;
						}
						*qd_d++ = nrtype | 0x80;
						qd_d -= i; /* compensate for i-offset of the number */
						while (i <= --j)
							qd_d[j] = info->nr[j];
					}
					if (info->lnr[0] != '\0') {
						int i = 0, j;
						uchar_t nrtype;

						switch(info->lnr[0]) {
						case '0': case '1': case '2': case '3': case '4':
						case '5': case '6': case '7': case '8': case '9':
							nrtype = 0x00; break;
						case '+': /* international */
							nrtype = 0x11; i = 1; break;
						case '=': /* national */
							nrtype = 0x21; i = 1; break;
						case '-': /* subscriber */
							nrtype = 0x41; i = 1; break;
#if 0 /* use "unknown" instead... MSN spec */
						case '.': /* abbreviated, able-to-dial */
						case '/': /* abbreviated, unable-to-dial */
							nrtype = 0x61; i = 1; break;
#endif
						default:
							nrtype = 0x00; i = 1;
							break;
						}
						for (j = i; j < MAXNR; j++)
							if (info->lnr[j] == '\0')
								break;
						if ((qd_d = qd_insert ((uchar_t *) asn->b_rptr, &qd_len, 0, PT_E0_origAddr, j - i + 1, 0)) == NULL) {
							conn->lockit--;
							return -EIO;
						}
						*qd_d++ = nrtype | 0x80;
						qd_d -= i; /* compensate for i-offset of the number */
						while (i <= --j)
							qd_d[j] = info->lnr[j];
					}
					if (conn->bchan != 0) {
						int basic = (conn->card ? conn->card->bchans <= 2 : 1);
						if ((qd_d = qd_insert ((uchar_t *) asn->b_rptr, &qd_len, 0, PT_E0_chanID, basic ? 1 : 3, 0)) == NULL) {
							conn->lockit--;
							return -EIO;
						}
						if (basic) {
							*qd_d = 0x80 | conn->bchan;
						} else {
							*qd_d++ = 0xA1;
							*qd_d++ = 0x83;
							*qd_d++ = conn->bchan;
						}
						conn->bchan = 0;		/* Network will tell us which
												 * to use */
					}
					/* NSF: SemiPerm etc. */
					asn->b_wptr = asn->b_rptr + qd_len;
					if ((err = phone_sendback (conn, MT_ET_SETUP, asn)) != 0) {
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
		case 6:
			{
				mblk_t *asn = NULL;
#if 0
				int qd_len = 0;    
				uchar_t *qd_d;
#endif
				setstate (conn, 7);
#if 0
				if ((conn->((struct e_info *)conn->p_data) & SVC_PENDING) && (asn = allocb (32, BPRI_MED)) != NULL) {
					if ((qd_d = qd_insert ((uchar_t *) asn->b_rptr, &qd_len, 0, PT_N0_netSpecFac, 4, 1)) == NULL) {                                   
						freeb(asn);
						asn = NULL;
					} else {
						*(uchar_t *) qd_d++ = 0;                            
						*(uchar_t *) qd_d++ = ET_FAC_SVC;
						*(ushort_t *) qd_d = 0;                     
						asn->b_wptr = asn->b_rptr + qd_len;
					}
				}                                                       
#endif

				if(phone_sendback (conn, MT_ET_ALERT, asn) != 0 && asn != NULL)
					freemsg(asn);
			}
			break;
		default:
			printf("BadState1: ");
			err = -EINVAL;
		}
		break;
	case CMD_ANSWER:
		{
			mblk_t *asn = NULL;

			if (data != NULL) {
				{
					int qd_len = 0;
					uchar_t *qd_d;

					if ((asn = allocb (256, BPRI_MED)) == NULL) {
						conn->lockit--;
						return -ENOMEM;
					}
					if (info->llc_len > 0) {
						if ((qd_d = qd_insert ((uchar_t *) asn->b_rptr, &qd_len, 6, PT_E0_compatLo, info->llc_len, 0)) == NULL) {
							conn->lockit--;
							return -EIO;
						}
						bcopy(info->llc,qd_d,info->llc_len);
					}
#if 0
					if (info->ulc_len > 0) {
						if ((qd_d = qd_insert ((uchar_t *) asn->b_rptr, &qd_len, 6, PT_E0_compatLo, info->ulc_len, 0)) == NULL) {
							conn->lockit--;
							return -EIO; 
						}
						bcopy(info->ulc,qd_d,info->ulc_len);
					}
					if (info->bearer_len > 0) {
						if ((qd_d = qd_insert ((uchar_t *) asn->b_rptr, &qd_len, 6, PT_E0_bearer_cap, info->bearer_len, 0)) == NULL) {
							conn->lockit--;
							return -EIO;
						}
						bcopy(info->bearer,qd_d,info->bearer_len);
					}
#endif

#if 0 /* error? Seems to be bad */
					if (conn->bchan != 0) {
						int basic = (conn->card ? conn->card->bchans <= 2 : 1);
						if ((qd_d = qd_insert ((uchar_t *) asn->b_rptr, &qd_len, 0, PT_E0_chanID, basic ? 1 : 3, 0)) == NULL) {
							conn->lockit--;
							return -EIO;
						}
						if (basic) {
							*qd_d = 0x80 | conn->bchan;
						} else {
							*qd_d++ = 0xA1;
							*qd_d++ = 0x83;
							*qd_d++ = conn->bchan;
						}
					}
#endif
					asn->b_wptr = asn->b_rptr + qd_len;
				}
			}
			conn->minorstate |= MS_WANTCONN;

			isdn3_setup_conn (conn, EST_SETUP);

			if (((conn->delay > 0) && (conn->minorstate & MS_DELAYING))
			     || !(conn->minorstate & MS_PROTO)
				 || !(conn->minorstate & MS_INITPROTO)
				 || ((conn->state == 7) && !(conn->minorstate & MS_BCHAN))) {
				if (data != NULL)
					data->b_rptr = oldpos;
				isdn3_repeat (conn, id, data);
				if (asn != NULL)
					freemsg (asn);
				conn->lockit--;
				return 0;
			}
			switch (conn->state) {
			case 6:
			case 7:
				if(!(conn->minorstate & MS_BCHAN)) {
					setstate (conn, 7);
					err = phone_sendback (conn, MT_ET_ALERT, asn);
				} else {
					setstate (conn, 8);
					err = phone_sendback (conn, MT_ET_CONN, asn);
				}
				if (err == 0)
					asn = NULL;
				if(conn->state == 7) {
					isdn3_repeat (conn, id, data);
					data = NULL;
				} else
					isdn3_setup_conn (conn, EST_LISTEN);
				break;
			default:
				printf("BadState2 ");
				err = -EINVAL;
				break;
			}
			if (asn != NULL)
				freemsg (asn);
		}
		break;
	case CMD_OFF:
		{
			mblk_t *mb = NULL;

			if(cause != -1) {
				int len;
				uchar_t *dp;

				if ((mb = allocb (16, BPRI_LO)) == NULL)
					break;

				len = mb->b_wptr - mb->b_rptr;
				dp = qd_insert ((uchar_t *) mb->b_rptr, &len, 0, PT_E0_cause, 2, 0);
				if (dp != NULL) {
					mb->b_wptr = mb->b_rptr + len;
					*dp++ = 0x80;
					*dp = cause | 0x80;
				}
			}

			conn->minorstate &= ~MS_WANTCONN;

			/* set Data */
			if (conn->state == 6 && cause == -1) {
				setstate(conn,99);
				if(mb != NULL)
					freemsg(mb);
			} else if (send_disc (conn, forceit << 1, mb) != 0 && mb != NULL)
				freemsg (mb);

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
		untimer (ET_T301, conn);
		untimer (ET_T302, conn);
		untimer (ET_T303, conn);
		untimer (ET_T304, conn);
		untimer (ET_T305, conn);
		untimer (ET_T308, conn);
#ifdef HAS_RECOVERY
		untimer (ET_T309, conn);
#endif
		untimer (ET_T310, conn);
		untimer (ET_T313, conn);
		untimer (ET_T314, conn);
		untimer (ET_T316, conn);
		untimer (ET_T317, conn);
#ifdef HAS_SUSPEND
		untimer (ET_T318, conn);
		untimer (ET_T319, conn);
#endif
		untimer (ET_T321, conn);
		untimer (ET_T322, conn);
		untimer (ET_TALERT, conn);
		untimer (ET_TCONN, conn);
	}
	if(conn->state != 0) {
		if(force)
			(void) phone_sendback (conn, MT_ET_REL, NULL);
		else
			(void) send_disc (conn, 1, NULL);
	}
}


static void report (isdn3_conn conn, mblk_t *mb)
{
	struct e_info *info = conn->p_data;
	if (info == NULL)
		return;
	if(info->bearer_len > 0) {
		m_putsx(mb, ARG_BEARER);
		m_puthex(mb, info->bearer,info->bearer_len);
	}
	if(info->ulc_len > 0) {
		m_putsx(mb, ARG_ULC);
		m_puthex(mb, info->ulc,info->ulc_len);
	}
	if(info->llc_len > 0) {
		m_putsx(mb, ARG_LLC);
		m_puthex(mb, info->llc,info->llc_len);
	}
	if (info->nr[0] != 0) {
		m_putsx (mb, ARG_NUMBER);
		m_putsz (mb, info->nr);
	}
	if (info->lnr[0] != 0) {
		m_putsx (mb, ARG_LNUMBER);
		m_putsz (mb, info->lnr);
	}
}

struct _isdn3_prot prot_ETSI =
{
		NULL, SAPI_PHONE_DSS1,
		NULL, &chstate, &report, &recv, NULL, &sendcmd, &killconn, NULL,
};


