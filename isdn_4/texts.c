/*
 * This file is part of the ISDN master program.
 *
 * Copyright (C) 1995 Matthias Urlichs.
 * See the file COPYING for license details.
 */

#include "master.h"
#ifdef _capi_
#include "../cards/capi/capi.h"
#endif

/* Textual representation of a connection state */
char *state2str(CState state) {
	switch(state) {
	case c_up: return "up";
	case c_off: return "off";
	case c_down: return "down";
	case c_offdown: return ">off";
	case c_forceoff: return "XXX";
	case c_going_up: return ">up";
	case c_going_down: return ">down";
	default: return "unknown";
	}
}

/* Textual representation of a L2<->3 information element */
char *HdrName (int hdr)
{
	switch(hdr) {
	default: {
	}
	case -1: return "-";
	case HDR_ATCMD: return "AT Cmd";
	case HDR_DATA: return "Data";
	case HDR_XDATA: return "XData";
	case HDR_UIDATA: return "UI Data";
	case HDR_RAWDATA: return "Raw Data";
	case HDR_OPEN: return "Open";
	case HDR_CLOSE: return "Close";
	case HDR_ATTACH: return "Attach";
	case HDR_DETACH: return "Detach";
	case HDR_CARD: return "Card";
	case HDR_NOCARD: return "No Card";
	case HDR_OPENPROT: return "Open Protocol";
	case HDR_CLOSEPROT: return "Close Protocol";
	case HDR_NOTIFY: return "Notify";
	case HDR_INVAL: return "Invalid";
	case HDR_TEI: return "TEI";
	case HDR_PROTOCMD: return "Proto Cmd";
	}
}

/* Textual representation of flags */
char *FlagInfo(int flag)
{
	static char fbuf[30];

	fbuf[0]='\0';
	if (flag & F_INCOMING)     strcat(fbuf, ":in");
	if (flag & F_OUTGOING)     strcat(fbuf, ":ou");
	if (flag & F_PERMANENT)    strcat(fbuf, ":dP");
	if (flag & F_LEASED)       strcat(fbuf, ":dL");
	if (flag & F_MULTIDIALUP)  strcat(fbuf, ":dM");
	if (flag & F_DIALUP)       strcat(fbuf, ":dD");
	if (flag & F_INTERRUPT)    strcat(fbuf, ":is");
	if (flag & F_PREFOUT)      strcat(fbuf, ":xi");
	if (flag & F_FORCEOUT)     strcat(fbuf, ":yi");
	if (flag & F_BACKCALL)     strcat(fbuf, ":bi");
	if (flag & F_IGNORELIMIT)  strcat(fbuf, ":il");
	if (flag & F_FASTDROP)     strcat(fbuf, ":fX");
	if (flag & F_FASTREDIAL)   strcat(fbuf, ":fr");
	if (flag & F_CHANBUSY)     strcat(fbuf, ":ib");
	if (flag & F_NRCOMPLETE)   strcat(fbuf, ":nc");
	if (flag & F_LNRCOMPLETE)  strcat(fbuf, ":lc");
	if (flag & F_OUTCOMPLETE)  strcat(fbuf, ":oc");
	if (flag & F_SETINITIAL)   strcat(fbuf, ":si");
	if (flag & F_SETLATER)     strcat(fbuf, ":sl");
	if (flag & F_NOREJECT)     strcat(fbuf, ":nj");

	if(fbuf[0]=='\0')
		strcpy(fbuf,"-");
	return fbuf;
}

/* Textual representation of ISDN causes */
const char *CauseInfo(int cause, char *pri)
{
	if (cause == 999999) return "-";
	switch(cause) {
	case 0: return "OK";
	case ID_priv_Busy:					return "Local Busy";
	case ID_priv_Print:					if(pri == NULL) return "???"; else if(isdigit(*pri)) return pri+1; else return pri;
	case ID_NOCARD:						return "card is not connected";
	case ID_NOREPLY:					return "network doesn't answer";
	case CHAR2('?','?'):				return "-unknown-";

#ifdef _capi_
	case ID_E_REGISTER:					return "application registration";
	case ID_E_APPLICATION:				return "wrong application ID";
	case ID_E_MSGLENGTH:				return "message error";
	case ID_E_COMMAND:					return "wrong API command";
	case ID_E_QUEUEFULL:				return "message queue full";
	case ID_E_NOMSG:					return "message queue empty";
	case ID_E_MSGOVERFLOW:				return "messages lost";
	case ID_E_DEINSTALL:				return "error during deinstallation";
	case ID_E_CONTROLLER:				return "wrong controller";
	case ID_E_PLCI:						return "wrong PLCI";
	case ID_E_NCCI:						return "wrong NCCI";
	case ID_E_TYPE:						return "wrong type";
	case ID_E_BCHANNEL:					return "wrong B channel";
	case ID_E_INFOMASK:					return "wrong info mask";
	case ID_E_EAZMASK:					return "wrong EAZ mask";
	case ID_E_SIMASK:					return "wrong Service ID mask";
	case ID_E_B2PROTO:					return "B2 protocol incorrect";
	case ID_E_DLPD:						return "DLPD incorrect";
	case ID_E_B3PROTO:					return "B3 protocol incorrect";
	case ID_E_NCPD:						return "NCPD incorrect";
	case ID_E_NCPI:						return "NCPI incorrect";
	case ID_E_DATAB3FLAGS:				return "B3 flags incorrect";
	case ID_E_CONTROLLERFAILED:			return "controller error";
	case ID_E_REGCONFLICT:				return "registration conflict";
	case ID_E_CMDNOTSUPPORTED:			return "command not supported";
	case ID_E_PLCIACT:					return "PLCI not active";
	case ID_E_NCCIACT:					return "NCCI not active";
	case ID_E_B2NOTSUPPORT:				return "B2 protocol not supported";
	case ID_E_B2STATE:					return "change of B2 protocol not possible";
	case ID_E_B3NOTSUPPORT:				return "B3 protocol not supported";
	case ID_E_B3STATE:					return "change of B3 protocol not possible";
	case ID_E_B2DLPDPARA:				return "parameters not supported in DLPD";
	case ID_E_B3NCPDPARA:				return "parameters not supported in NCPD";
	case ID_E_B3NCPIPARA:				return "parameters not supported in NCPI";
	case ID_E_DATALEN:					return "data length not supported";
	case ID_E_DTMF:						return "DTMF problem";
	case ID_E_NOL1:						return "L1 setup failed";
	case ID_E_NOL2:						return "L2 setup failed";
	case ID_E_SETUPBCHANLAYER1:			return "B1 setup failed";
	case ID_E_SETUPBCHANLAYER2:			return "B2 setup failed";
	case ID_E_ABORTDCHANLAYER1:			return "L1 D channel aborted";
	case ID_E_ABORTDCHANLAYER2:			return "L2 D channel aborted";
	case ID_E_ABORTDCHANLAYER3:			return "L3 D channel aborted";
	case ID_E_ABORTBCHANLAYER1:			return "L1 B channel aborted";
	case ID_E_ABORTBCHANLAYER2:			return "L2 B channel aborted";
	case ID_E_ABORTBCHANLAYER3:			return "L3 B channel aborted";
	case ID_E_REBCHANLAYER2:			return "L2 B channel reestablished";
	case ID_E_REBCHANLAYER3:			return "L3 B channel reestablished";
	case ID_E_NOFAX:					return "?Fax not supported";
	case ID_E_BADLINE:					return "?Bad Line";
	case ID_E_NOANSWER:					return "?Nobody answers";
	case ID_E_REMDISC:					return "?Remote Disconnect";
	case ID_E_NOCMD:					return "?No Command";
	case ID_E_INCOMPAT:					return "?Incompatible";
	case ID_E_BADDATA:					return "?Bad Data";
	case ID_E_PROTO:					return "?Protocol";
#endif
	case ID_ET_AccessInfoDiscard:		return "Access info discarded";
	case ID_N1_BearerNotImpl:			return "Bearer Service not implemented";
	case ID_N1_CIDinUse:				return "CID in use";
	case ID_N1_CIDunknown:				return "CID unknown";
	case ID_ET_CallAwarded:				return "Call Awarded";
	case ID_ET_CIDcleared:				return "Call ID cleared";
	case ID_ET_CIDinUse:				return "Call ID in use";
/*	case ID_ET_CallRejected:			return "Call rejected"; */
	case ID_N1_CallRejected:			return "Call rejected";
	case ID_ET_CapNotAuth:				return "Capability not authorized";
	case ID_ET_CapNotAvail:				return "Capability not available";
	case ID_ET_CapNotImpl:				return "Capability not implemented";
	case ID_ET_ChanNotExist:			return "Channel does not exist";
	case ID_ET_ChanNotAvail:			return "Channel not available";
	case ID_ET_ChanTypeNotImpl:			return "Channel type not implemented";
	case ID_ET_DestIncompat:			return "Destination incompatible";
	case ID_N1_DestNotObtain:			return "Destination not obtainable";
/*	case ID_ET_FacNotImpl:				return "Facility not implemented"; */
	case ID_N1_FacNotImpl:				return "Facility not implemented";
/*	case ID_ET_FacNotSubscr:			return "Facility not subscribed"; */
	case ID_N1_FacNotSubscr:			return "Facility not subscribed";
	case ID_ET_FacRejected:				return "Facility rejected";
	case ID_N1_IncomingBarred:			return "Incoming calls barred";
	case ID_ET_InfoElemMissing:			return "Info element missing";
	case ID_ET_InfoInvalid:				return "Info invalid";
	case ID_ET_InfoNotCompatible:		return "Info not compatible";
	case ID_ET_InterworkingUnspec:		return "Interworking unspecified";
	case ID_N1_InvCRef:					return "Invalid Call Reference";
/*	case ID_ET_InvCRef:					return "Invalid call reference"; */
	case ID_ET_InvChan:					return "Invalid channel";
	case ID_ET_InvNumberFormat:			return "Invalid number format";
	case ID_ET_InvTransitNet:			return "Invalid transit network";
	case ID_ET_InvalUnspec:				return "Invalid unspecified";
	case ID_N1_LocalProcErr:			return "Local procedure error";
	case ID_ET_MandatoryMissing:		return "Mandatory missing";
	case ID_ET_MandatoryNotCompatible:	return "Mandatory not compatible";
	case ID_ET_MandatoryNotImpl:		return "Mandatory not implemented";
	case ID_N1_NegativeGBG:				return "Negative GBG";
	case ID_N1_NetworkCongestion:		return "Network congestion";
	case ID_ET_NetworkDown:				return "Network down";
	case ID_N1_NoSPVknown:				return "No SPV known";
	case ID_ET_NoAnswer:				return "No answer";
	case ID_ET_NoCallSusp:				return "No call suspended";
	case ID_ET_NoChanAvail:				return "No channel available";
/*	case ID_N1_NoChans:					return "No channels"; */
	case ID_ET_NoRouteDest:				return "No route to destination";
	case ID_ET_NoRouteTransit:			return "No route to transit network";
	case ID_ET_NoUserResponding:		return "No user responding";
/*	case ID_N1_NoUserResponse:			return "No user response"; */
	case ID_ET_NormalClear:				return "Normal Clear";
	case ID_ET_NormalUnspec:			return "Normal unspecified";
	case ID_ET_NonSelected:				return "Not selected";
	case ID_ET_NumberChanged:			return "Number changed";
/*	case ID_N1_NumberChanged:			return "Number changed"; */
	case ID_ET_NumberUnassigned:		return "Number unassigned";
	case ID_ET_OutOfOrder:				return "Out of order";
/*	case ID_N1_OutOfOrder:				return "Out of order"; */
	case ID_N1_OutgoingBarred:			return "Outgoing calls barred";
	case ID_ET_ProtocolUnspec:			return "Protocol unspecified";
	case ID_ET_QualityUnavail:			return "Quality unavailable";
	case ID_N1_RemoteProcErr:			return "Remote procedure error";
	case ID_N1_RemoteUserResumed:		return "Remote user resumed";
	case ID_N1_RemoteUserSuspend:		return "Remote user suspend";
	case ID_N1_RemoteUser:				return "Remote user";
	case ID_ET_ResourceUnavail:			return "Resource unavailable";
	case ID_ET_RestSTATUS:				return "Restart STATUS";
	case ID_ET_RestrictedInfoAvail:		return "Restricted info available";
	case ID_ET_SwitchCongest:			return "Switch congestion";
	case ID_ET_TempFailure:				return "Temporary failure";
	case ID_ET_TimerRecovery:			return "Timer recovery";
	case ID_ET_UnavailUnspec:			return "Unavailable unspecified";
	case ID_ET_UnimplUnspec:			return "Unimplemented unspecified";
	case ID_N1_UnknownGBG:				return "Unknown GBG";
	case ID_N1_UserAssessBusy:			return "User assess busy";
	case ID_ET_UserBusy:				return "User busy";
/*	case ID_N1_UserBusy:				return "User busy"; */
	case ID_N1_UserInfoDiscarded:		return "User info discarded";
	case ID_ET_WrongCallID:				return "Wrong call ID";
	default: {
		static char buf[10];
		sprintf(buf,"0x%02x",cause);
		return buf;
		}
	}
}
