#include "phone_ETSI.h"

uchar_t et_idtocause(ushort_t id)
{
	switch(id) {
		default:							return 0;
		case ID_ET_NumberUnassigned:		return ET_NumberUnassigned;
		case ID_ET_NoRouteTransit:			return ET_NoRouteTransit;
		case ID_ET_NoRouteDest:				return ET_NoRouteDest;
		case ID_ET_InvChan:					return ET_InvChan;
		case ID_ET_CallAwarded:				return ET_CallAwarded;
		case ID_ET_NormalClear:				return ET_NormalClear;
		case ID_ET_UserBusy:				return ET_UserBusy;
		case ID_ET_NoUserResponding:		return ET_NoUserResponding;
		case ID_ET_NoAnswer:				return ET_NoAnswer;
		case ID_ET_CallRejected:			return ET_CallRejected;
		case ID_ET_NumberChanged:			return ET_NumberChanged;
		case ID_ET_NonSelected:				return ET_NonSelected;
		case ID_ET_OutOfOrder:				return ET_OutOfOrder;
		case ID_ET_InvNumberFormat:			return ET_InvNumberFormat;
		case ID_ET_FacRejected:				return ET_FacRejected;
		case ID_ET_RestSTATUS:				return ET_RestSTATUS;
		case ID_ET_NormalUnspec:			return ET_NormalUnspec;
		case ID_ET_NoChanAvail:				return ET_NoChanAvail;
		case ID_ET_NetworkDown:				return ET_NetworkDown;
		case ID_ET_TempFailure:				return ET_TempFailure;
		case ID_ET_SwitchCongest:			return ET_SwitchCongest;
		case ID_ET_AccessInfoDiscard:		return ET_AccessInfoDiscard;
		case ID_ET_ChanNotAvail:			return ET_ChanNotAvail;
		case ID_ET_ResourceUnavail:			return ET_ResourceUnavail;
		case ID_ET_QualityUnavail:			return ET_QualityUnavail;
		case ID_ET_FacNotSubscr:			return ET_FacNotSubscr;
		case ID_ET_CapNotAuth:				return ET_CapNotAuth;
		case ID_ET_CapNotAvail:				return ET_CapNotAvail;
		case ID_ET_UnavailUnspec:			return ET_UnavailUnspec;
		case ID_ET_CapNotImpl:				return ET_CapNotImpl;
		case ID_ET_ChanTypeNotImpl:			return ET_ChanTypeNotImpl;
		case ID_ET_FacNotImpl:				return ET_FacNotImpl;
		case ID_ET_RestrictedInfoAvail:		return ET_RestrictedInfoAvail;
		case ID_ET_UnimplUnspec:			return ET_UnimplUnspec;
		case ID_ET_InvCRef:					return ET_InvCRef;
		case ID_ET_ChanNotExist:			return ET_ChanNotExist;
		case ID_ET_WrongCallID:				return ET_WrongCallID;
		case ID_ET_CIDinUse:				return ET_CIDinUse;
		case ID_ET_NoCallSusp:				return ET_NoCallSusp;
		case ID_ET_CIDcleared:				return ET_CIDcleared;
		case ID_ET_DestIncompat:			return ET_DestIncompat;
		case ID_ET_InvTransitNet:			return ET_InvTransitNet;
		case ID_ET_InvalUnspec:				return ET_InvalUnspec;
		case ID_ET_MandatoryMissing:		return ET_MandatoryMissing;
		case ID_ET_MandatoryNotImpl:		return ET_MandatoryNotImpl;
		case ID_ET_MandatoryNotCompatible:	return ET_MandatoryNotCompatible;
		case ID_ET_InfoElemMissing:			return ET_InfoElemMissing;
		case ID_ET_InfoInvalid:				return ET_InfoInvalid;
		case ID_ET_InfoNotCompatible:		return ET_InfoNotCompatible;
		case ID_ET_TimerRecovery:			return ET_TimerRecovery;
		case ID_ET_ProtocolUnspec:			return ET_ProtocolUnspec;
	}
}
 
ushort_t et_causetoid(uchar_t id)
{
	switch(id) {
		default:						return CHAR2('?','?');
		case ET_NumberUnassigned:		return ID_ET_NumberUnassigned;
		case ET_NoRouteTransit:			return ID_ET_NoRouteTransit;
		case ET_NoRouteDest:			return ID_ET_NoRouteDest;
		case ET_InvChan:				return ID_ET_InvChan;
		case ET_CallAwarded:			return ID_ET_CallAwarded;
		case ET_NormalClear:			return ID_ET_NormalClear;
		case ET_UserBusy:				return ID_ET_UserBusy;
		case ET_NoUserResponding:		return ID_ET_NoUserResponding;
		case ET_NoAnswer:				return ID_ET_NoAnswer;
		case ET_CallRejected:			return ID_ET_CallRejected;
		case ET_NumberChanged:			return ID_ET_NumberChanged;
		case ET_NonSelected:			return ID_ET_NonSelected;
		case ET_OutOfOrder:				return ID_ET_OutOfOrder;
		case ET_InvNumberFormat:		return ID_ET_InvNumberFormat;
		case ET_FacRejected:			return ID_ET_FacRejected;
		case ET_RestSTATUS:				return ID_ET_RestSTATUS;
		case ET_NormalUnspec:			return ID_ET_NormalUnspec;
		case ET_NoChanAvail:			return ID_ET_NoChanAvail;
		case ET_NetworkDown:			return ID_ET_NetworkDown;
		case ET_TempFailure:			return ID_ET_TempFailure;
		case ET_SwitchCongest:			return ID_ET_SwitchCongest;
		case ET_AccessInfoDiscard:		return ID_ET_AccessInfoDiscard;
		case ET_ChanNotAvail:			return ID_ET_ChanNotAvail;
		case ET_ResourceUnavail:		return ID_ET_ResourceUnavail;
		case ET_QualityUnavail:			return ID_ET_QualityUnavail;
		case ET_FacNotSubscr:			return ID_ET_FacNotSubscr;
		case ET_CapNotAuth:				return ID_ET_CapNotAuth;
		case ET_CapNotAvail:			return ID_ET_CapNotAvail;
		case ET_UnavailUnspec:			return ID_ET_UnavailUnspec;
		case ET_CapNotImpl:				return ID_ET_CapNotImpl;
		case ET_ChanTypeNotImpl:		return ID_ET_ChanTypeNotImpl;
		case ET_FacNotImpl:				return ID_ET_FacNotImpl;
		case ET_RestrictedInfoAvail:	return ID_ET_RestrictedInfoAvail;
		case ET_UnimplUnspec:			return ID_ET_UnimplUnspec;
		case ET_InvCRef:				return ID_ET_InvCRef;
		case ET_ChanNotExist:			return ID_ET_ChanNotExist;
		case ET_WrongCallID:			return ID_ET_WrongCallID;
		case ET_CIDinUse:				return ID_ET_CIDinUse;
		case ET_NoCallSusp:				return ID_ET_NoCallSusp;
		case ET_CIDcleared:				return ID_ET_CIDcleared;
		case ET_DestIncompat:			return ID_ET_DestIncompat;
		case ET_InvTransitNet:			return ID_ET_InvTransitNet;
		case ET_InvalUnspec:			return ID_ET_InvalUnspec;
		case ET_MandatoryMissing:		return ID_ET_MandatoryMissing;
		case ET_MandatoryNotImpl:		return ID_ET_MandatoryNotImpl;
		case ET_MandatoryNotCompatible:	return ID_ET_MandatoryNotCompatible;
		case ET_InfoElemMissing:		return ID_ET_InfoElemMissing;
		case ET_InfoInvalid:			return ID_ET_InfoInvalid;
		case ET_InfoNotCompatible:		return ID_ET_InfoNotCompatible;
		case ET_TimerRecovery:			return ID_ET_TimerRecovery;
		case ET_ProtocolUnspec:			return ID_ET_ProtocolUnspec;
	}
}
 
