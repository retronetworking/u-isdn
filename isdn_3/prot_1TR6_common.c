#include "primitives.h"
#include "phone_1TR6.h"
#include "prot_1TR6_common.h"
#include "q_data.h"
#include "streams.h"
#include "streamlib.h"

#ifndef NULL
#define NULL (void *)0L
#endif

void
report_addnsf (mblk_t * mb, uchar_t * data, ushort_t len)
{
	uchar_t facility;
	ushort_t service = 0;
	char nlen;

	QD_INIT (data, len) return;
	QD {
	  QD_CASE (0, PT_N0_netSpecFac):
		if (qd_len == 0)
			break;
		nlen = *qd_data++;
		if (qd_len < nlen + 1)
			break;
		qd_data += nlen;
		facility = *qd_data++;
		qd_len -= nlen + 1;
		switch (qd_len) {
		case 0:
			service = 0;
			break;
		case 1:
			service = *qd_data++ << 8;
			qd_len -= 1;
			break;
		default:
			service = *qd_data++ << 8;
			service = (service << 8) | *qd_data++;
			qd_len -= 2;
			break;
		}
		m_putsx (mb, ID_N0_netSpecFac);
		m_putx (mb, service);
		switch (facility) {
		case N1_FAC_Sperre:
			m_putsx2 (mb, ID_N1_FAC_Sperre);
			while (qd_len > 0)
				switch (*qd_data++) {
				case N1_FAC_Sperre_All:
					m_putsx2 (mb, ID_N1_FAC_Sperre_All);
					break;
				case N1_FAC_Sperre_Fern:
					m_putsx2 (mb, ID_N1_FAC_Sperre_Fern);
					break;
				case N1_FAC_Sperre_Intl:
					m_putsx2 (mb, ID_N1_FAC_Sperre_Intl);
					break;
				case N1_FAC_Sperre_interk:
					m_putsx2 (mb, ID_N1_FAC_Sperre_interk);
					break;
				}
			break;
		case N1_FAC_Forward1:
			m_putsx2 (mb, ID_N1_FAC_Forward1);
			m_puts (mb, qd_data, qd_len);
			break;
		case N1_FAC_Forward2:
			m_putsx2 (mb, ID_N1_FAC_Forward2);
			m_puts (mb, qd_data, qd_len);
			break;
			break;
		case N1_FAC_Konferenz:
			m_putsx2 (mb, ID_N1_FAC_Konferenz);
			break;
		case N1_FAC_GrabBchan:
			m_putsx2 (mb, ID_N1_FAC_GrabBchan);
			break;
			if (qd_len > 0)
				m_puti (mb, *qd_data);
			break;
		case N1_FAC_Reactivate:
			m_putsx2 (mb, ID_N1_FAC_Reactivate);
			break;
		case N1_FAC_Konferenz3:
			m_putsx2 (mb, ID_N1_FAC_Konferenz3);
			break;
		case N1_FAC_Dienstwechsel1:
			m_putsx2 (mb, ID_N1_FAC_Dienstwechsel1);
			m_puts (mb, qd_data, qd_len);
			break;
			break;
		case N1_FAC_Dienstwechsel2:
			m_putsx2 (mb, ID_N1_FAC_Dienstwechsel2);
			m_puts (mb, qd_data, qd_len);
			break;
			break;
		case N1_FAC_NummernIdent:
			m_putsx2 (mb, ID_N1_FAC_NummernIdent);
			break;
		case N1_FAC_GBG:
			m_putsx2 (mb, ID_N1_FAC_GBG);
			break;
		case N1_FAC_DisplayUebergeben:
			m_putsx2 (mb, ID_N1_FAC_DisplayUebergeben);
			break;
		case N1_FAC_DisplayUmgeleitet:
			m_putsx2 (mb, ID_N1_FAC_DisplayUmgeleitet);
			break;
		case N1_FAC_Unterdruecke:
			m_putsx2 (mb, ID_N1_FAC_Unterdruecke);
			break;
		case N1_FAC_Deactivate:
			m_putsx2 (mb, ID_N1_FAC_Deactivate);
			break;
		case N1_FAC_Activate:
			m_putsx2 (mb, ID_N1_FAC_Activate);
			break;
		case N1_FAC_SVC:
			m_putsx2 (mb, ID_N1_FAC_SVC);
			break;
		case N1_FAC_Rueckwechsel:
			m_putsx2 (mb, ID_N1_FAC_Rueckwechsel);
			break;
		case N1_FAC_Umleitung:
			m_putsx2 (mb, ID_N1_FAC_Umleitung);
			break;
		default:
			m_putx (mb, facility);
		}
	}
}

uchar_t
report_addcause (mblk_t * mb, uchar_t * data, int len)
{
	int qd_len;
	uchar_t *qd_data;

	qd_data = qd_find (data, len, 0, PT_N0_cause, &qd_len);
	if (qd_data == NULL)
		return 0;
	if (qd_len < 1)
		return 0;
	m_putsx (mb, ARG_CAUSE);
	m_putsx2(mb,n1_causetoid(*qd_data & 0x7F));
	do {
	} while(--qd_len > 0 && !(*qd_data++ & 0x80));
	if(qd_len > 0) 
		m_putx(mb,*qd_data & 0x0F);

	return *qd_data;
}

void
report_addisplay (mblk_t * mb, uchar_t * data, int len)
{
	int qd_len;
	uchar_t *qd_data;

	qd_data = qd_find (data, len, 0, PT_N0_display, &qd_len);
	if (qd_data == NULL)
		return;
	if (qd_len < 1)
		return;
	m_putsx (mb, ID_N0_display);
	m_puts (mb, qd_data, qd_len);
}

void
report_adddate (mblk_t * mb, uchar_t * data, int len)
{
	int qd_len;
	uchar_t *qd_data;

	qd_data = qd_find (data, len, 6, PT_N6_date, &qd_len);
	if (qd_data == NULL)
		return;
	if (qd_len < 1)
		return;
	m_putsx (mb, ID_N6_date);
	m_puts (mb, qd_data, qd_len);
}

void
report_addcost (mblk_t * mb, uchar_t * data, int len)
{
	int qd_len;
	uchar_t *qd_data;

	qd_data = qd_find (data, len, 6, PT_N6_chargingInfo, &qd_len);
	if (qd_data == NULL)
		return;
	if (qd_len < 2)
		return;
	m_putsx (mb, ARG_CHARGE);
	m_puts (mb, qd_data + 1, qd_len - 1);
}

void
report_addstatus (mblk_t * mb, uchar_t * data, int len)
{
	int qd_len;
	uchar_t *qd_data;

	qd_data = qd_find (data, len, 6, PT_N6_statusCalled, &qd_len);
	if (qd_data == NULL)
		return;
	if (qd_len < 1)
		return;
	m_putsx (mb, ID_N6_statusCalled);
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
}

ushort_t
n1_causetoid(uchar_t id)
{
	switch(id) {
		default:					return CHAR2('?','?');
		case N1_InvCRef:			return ID_N1_InvCRef;
		case N1_BearerNotImpl:		return ID_N1_BearerNotImpl;
		case N1_CIDinUse:			return ID_N1_CIDinUse;
		case N1_CIDunknown:			return ID_N1_CIDunknown;
		case N1_NoChans:			return ID_N1_NoChans;
		case N1_FacNotImpl:			return ID_N1_FacNotImpl;
		case N1_FacNotSubscr:		return ID_N1_FacNotSubscr;
		case N1_OutgoingBarred:		return ID_N1_OutgoingBarred;
		case N1_UserAssessBusy:		return ID_N1_UserAssessBusy;
		case N1_NegativeGBG:		return ID_N1_NegativeGBG;
		case N1_UnknownGBG:			return ID_N1_UnknownGBG;
		case N1_NoSPVknown:			return ID_N1_NoSPVknown;
		case N1_DestNotObtain:		return ID_N1_DestNotObtain;
		case N1_NumberChanged:		return ID_N1_NumberChanged;
		case N1_OutOfOrder:			return ID_N1_OutOfOrder;
		case N1_NoUserResponse:		return ID_N1_NoUserResponse;
		case N1_UserBusy:			return ID_N1_UserBusy;
		case N1_IncomingBarred:		return ID_N1_IncomingBarred;
		case N1_CallRejected:		return ID_N1_CallRejected;
		case N1_NetworkCongestion:	return ID_N1_NetworkCongestion;
		case N1_RemoteUser:			return ID_N1_RemoteUser;
		case N1_LocalProcErr:		return ID_N1_LocalProcErr;
		case N1_RemoteProcErr:		return ID_N1_RemoteProcErr;
		case N1_RemoteUserSuspend:	return ID_N1_RemoteUserSuspend;
		case N1_RemoteUserResumed:	return ID_N1_RemoteUserResumed;
		case N1_UserInfoDiscarded:	return ID_N1_UserInfoDiscarded;
		/* case N1_St_Unknown:		return ID_N1_St_Unknown; */
		case N1_St_Calling:			return ID_N1_St_Calling;
	}
}

uchar_t
n1_idtocause(ushort_t id)
{
	switch(id) {
		default:						return 0;
		case ID_N1_InvCRef:				return N1_InvCRef;
		case ID_N1_BearerNotImpl:		return N1_BearerNotImpl;
		case ID_N1_CIDinUse:			return N1_CIDinUse;
		case ID_N1_CIDunknown:			return N1_CIDunknown;
		case ID_N1_NoChans:				return N1_NoChans;
		case ID_N1_FacNotImpl:			return N1_FacNotImpl;
		case ID_N1_FacNotSubscr:		return N1_FacNotSubscr;
		case ID_N1_OutgoingBarred:		return N1_OutgoingBarred;
		case ID_N1_UserAssessBusy:		return N1_UserAssessBusy;
		case ID_N1_NegativeGBG:			return N1_NegativeGBG;
		case ID_N1_UnknownGBG:			return N1_UnknownGBG;
		case ID_N1_NoSPVknown:			return N1_NoSPVknown;
		case ID_N1_DestNotObtain:		return N1_DestNotObtain;
		case ID_N1_NumberChanged:		return N1_NumberChanged;
		case ID_N1_OutOfOrder:			return N1_OutOfOrder;
		case ID_N1_NoUserResponse:		return N1_NoUserResponse;
		case ID_N1_UserBusy:			return N1_UserBusy;
		case ID_N1_IncomingBarred:		return N1_IncomingBarred;
		case ID_N1_CallRejected:		return N1_CallRejected;
		case ID_N1_NetworkCongestion:	return N1_NetworkCongestion;
		case ID_N1_RemoteUser:			return N1_RemoteUser;
		case ID_N1_LocalProcErr:		return N1_LocalProcErr;
		case ID_N1_RemoteProcErr:		return N1_RemoteProcErr;
		case ID_N1_RemoteUserSuspend:	return N1_RemoteUserSuspend;
		case ID_N1_RemoteUserResumed:	return N1_RemoteUserResumed;
		case ID_N1_UserInfoDiscarded:	return N1_UserInfoDiscarded;
		case ID_N1_St_Unknown:			return N1_St_Unknown;
		case ID_N1_St_Calling:			return N1_St_Calling;
	}
}

ushort_t
n1_facsubtoid(uchar_t id)
{
	switch(id) {
		default:					return CHAR2('?','?');
		case N1_FAC_Sperre_All:		return ID_N1_FAC_Sperre_All;
		case N1_FAC_Sperre_Fern:	return ID_N1_FAC_Sperre_Fern;
		case N1_FAC_Sperre_Intl:	return ID_N1_FAC_Sperre_Intl;
		case N1_FAC_Sperre_interk:	return ID_N1_FAC_Sperre_interk;
	}
}

ushort_t
n1_factoid(uchar_t id)
{
	switch(id) {
		default:						return CHAR2('?','?');
		case N1_FAC_Sperre:				return ID_N1_FAC_Sperre;
		case N1_FAC_Forward1:			return ID_N1_FAC_Forward1;
		case N1_FAC_Forward2:			return ID_N1_FAC_Forward2;
		case N1_FAC_Konferenz:			return ID_N1_FAC_Konferenz;
		case N1_FAC_GrabBchan:			return ID_N1_FAC_GrabBchan;
		case N1_FAC_Reactivate:			return ID_N1_FAC_Reactivate;
		case N1_FAC_Konferenz3:			return ID_N1_FAC_Konferenz3;
		case N1_FAC_Dienstwechsel1:		return ID_N1_FAC_Dienstwechsel1;
		case N1_FAC_Dienstwechsel2:		return ID_N1_FAC_Dienstwechsel2;
		case N1_FAC_NummernIdent:		return ID_N1_FAC_NummernIdent;
		case N1_FAC_GBG:				return ID_N1_FAC_GBG;
		case N1_FAC_DisplayUebergeben:	return ID_N1_FAC_DisplayUebergeben;
		case N1_FAC_DisplayUmgeleitet:	return ID_N1_FAC_DisplayUmgeleitet;
		case N1_FAC_Unterdruecke:		return ID_N1_FAC_Unterdruecke;
		case N1_FAC_Deactivate:			return ID_N1_FAC_Deactivate;
		case N1_FAC_Activate:			return ID_N1_FAC_Activate;
		case N1_FAC_SVC:				return ID_N1_FAC_SVC;
		case N1_FAC_Rueckwechsel:		return ID_N1_FAC_Rueckwechsel;
		case N1_FAC_Umleitung:			return ID_N1_FAC_Umleitung;
	}
}

uchar_t
n1_idtofacsub(ushort_t id)
{
	switch(id) {
		default:						return 0;
		case ID_N1_FAC_Sperre_All:		return N1_FAC_Sperre_All;
		case ID_N1_FAC_Sperre_Fern:		return N1_FAC_Sperre_Fern;
		case ID_N1_FAC_Sperre_Intl:		return N1_FAC_Sperre_Intl;
		case ID_N1_FAC_Sperre_interk:	return N1_FAC_Sperre_interk;
	}
}

uchar_t
n1_idtofac(ushort_t id)
{
	switch(id) {
		default:							return 0;
		case ID_N1_FAC_Sperre:				return N1_FAC_Sperre;
		case ID_N1_FAC_Forward1:			return N1_FAC_Forward1;
		case ID_N1_FAC_Forward2:			return N1_FAC_Forward2;
		case ID_N1_FAC_Konferenz:			return N1_FAC_Konferenz;
		case ID_N1_FAC_GrabBchan:			return N1_FAC_GrabBchan;
		case ID_N1_FAC_Reactivate:			return N1_FAC_Reactivate;
		case ID_N1_FAC_Konferenz3:			return N1_FAC_Konferenz3;
		case ID_N1_FAC_Dienstwechsel1:		return N1_FAC_Dienstwechsel1;
		case ID_N1_FAC_Dienstwechsel2:		return N1_FAC_Dienstwechsel2;
		case ID_N1_FAC_NummernIdent:		return N1_FAC_NummernIdent;
		case ID_N1_FAC_GBG:					return N1_FAC_GBG;
		case ID_N1_FAC_DisplayUebergeben:	return N1_FAC_DisplayUebergeben;
		case ID_N1_FAC_DisplayUmgeleitet:	return N1_FAC_DisplayUmgeleitet;
		case ID_N1_FAC_Unterdruecke:		return N1_FAC_Unterdruecke;
		case ID_N1_FAC_Deactivate:			return N1_FAC_Deactivate;
		case ID_N1_FAC_Activate:			return N1_FAC_Activate;
		case ID_N1_FAC_SVC:					return N1_FAC_SVC;
		case ID_N1_FAC_Rueckwechsel:		return N1_FAC_Rueckwechsel;
		case ID_N1_FAC_Umleitung:			return N1_FAC_Umleitung;
	}
}
