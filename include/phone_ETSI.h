#ifndef __ETSI_H
#define __ETSI_H

#include "compat.h"
#include "primitives.h"
#include <isdn_34.h>

/* CallRef */
#define CR_f_Orig  0x80
#define CR_is_Orig 0x00

#define MT_ET_ESC 0x00
#define ID_ET_ESC CHAR2('E','S')
#define MT_ET_ALERT 0x01
#define ID_ET_ALERT CHAR2('A','l')
#define MT_ET_PROCEEDING 0x02
#define ID_ET_PROCEEDING CHAR2('C','p')
#define MT_ET_PROGRESS 0x03
#define ID_ET_PROGRESS CHAR2('C','P')
#define MT_ET_SETUP 0x05
#define ID_ET_SETUP CHAR2('S','e')
#define MT_ET_CONN 0x07
#define ID_ET_CONN CHAR2('C','o')
#define MT_ET_SETUP_ACK 0x0D
#define ID_ET_SETUP_ACK CHAR2('S','A')
#define MT_ET_CONN_ACK 0x0F
#define ID_ET_CONN_ACK IND_CONN_ACK

#define MT_ET_USER_INFO 0x20
#define ID_ET_USER_INFO CHAR2('U','I')
#define MT_ET_SUSP_REJ 0x21
#define ID_ET_SUSP_REJ CHAR2('S','R')
#define MT_ET_RES_REJ 0x22
#define ID_ET_RES_REJ CHAR2('R','R')
#define MT_ET_HOLD 0x24
#define ID_ET_HOLD CHAR2('H','d')
#define MT_ET_SUSP 0x25
#define ID_ET_SUSP CHAR2('R','u')
#define MT_ET_RES 0x26
#define ID_ET_RES CHAR2('R','e')
#define MT_ET_HOLD_ACK 0x2E
#define ID_ET_HOLD_ACK CHAR2('H','A')
#define MT_ET_SUSP_ACK 0x2D
#define ID_ET_SUSP_ACK CHAR2('S','a')
#define MT_ET_RES_ACK 0x2E
#define ID_ET_RES_ACK CHAR2('R','A')
#define MT_ET_HOLD_REJ 0x30
#define ID_ET_HOLD_REJ CHAR2('H','R')
#define MT_ET_RETR 0x31
#define ID_ET_RETR CHAR2('r','e')
#define MT_ET_RETR_ACK 0x33
#define ID_ET_RETR_ACK CHAR2('r','A')
#define MT_ET_RETR_REJ 0x37
#define ID_ET_RETR_REJ CHAR2('r','R')

#define MT_ET_DISC 0x45
#define ID_ET_DISC CHAR2('D','i')
#define MT_ET_RESTART 0x46
#define ID_ET_RESTART CHAR2('R','s')
#define MT_ET_REL 0x4D
#define ID_ET_REL CHAR2('r','l')
#define MT_ET_RESTART_ACK 0x5A
#define ID_ET_RESTART_ACK CHAR2('r','C')
#define MT_ET_REL_COM 0x5A
#define ID_ET_REL_COM CHAR2('r','C')

#define MT_ET_SEGMENT 0x60
#define MT_ET_FAC 0x62
#define ID_ET_FAC CHAR2('F','a')
#define MT_ET_REGISTER 0x64
#define ID_ET_REGISTER CHAR2('G','R')
#define MT_ET_NOTIFY 0x6E
#define ID_ET_NOTIFY CHAR2('G','N')
#define MT_ET_STAT_ENQ 0x75
#define ID_ET_STAT_ENQ CHAR2('S','t')
#define MT_ET_CON_CON 0x79
#define ID_ET_CON_CON CHAR2('C','C')
#define MT_ET_INFO 0x7B
#define ID_ET_INFO CHAR2('I','n')
#define MT_ET_STAT 0x7D
#define ID_ET_STAT CHAR2('S','t')


/* Param */
#ifndef PS_Shift
#define PS_Shift 0x1

#define PS0_More 0x2
#define PS0_Congest 0x3

#endif

#define PT_E0_segmented 0x00
#define PT_E0_bearer_cap 0x04
#define ID_E0_bearer_cap CHAR2('B','c')
#define PT_E0_cause 0x08
#define ID_E0_cause CHAR2('C','a')
#define PT_E0_callID 0x10
#define ID_E0_callID CHAR2('c','i')
#define PT_E0_callState 0x14
#define ID_E0_callState CHAR2('c','s')
#define PT_E0_chanID 0x18
#define ID_E0_chanID CHAR2('C','i')
#define PT_E0_facility 0x1C
#define ID_E0_facility CHAR2('f','c')
#define PT_E0_progress 0x1E
#define ID_E0_progress CHAR2('P','r')
#define PT_E0_netSpecFac 0x20
#define ID_E0_netSpecFac CHAR2('N','S')
#define PT_E0_notifyInd 0x27
#define ID_E0_notifyInd CHAR2('n','i')
#define PT_E0_display 0x28
#define ID_E0_display CHAR2('d','i')
#define PT_E0_date 0x29
#define ID_E0_date CHAR2('d','a')
#define PT_E0_keypad 0x2C
#define ID_E0_keypad CHAR2('k','p')
#define PT_E0_infoRate 0x40
#define ID_E0_infoRate CHAR2('i','r')
#define PT_E0_transitDelay 0x42
#define ID_E0_transitDelay CHAR2('t','D')
#define PT_E0_transitDelaySel 0x43
#define ID_E0_transitDelaySel CHAR2('t','S')
#define PT_E0_packetBin 0x44
#define ID_E0_packetBin CHAR2('p','B')
#define PT_E0_packetWinsize 0x45
#define ID_E0_packetWinsize CHAR2('p','W')
#define PT_E0_packetSize 0x46
#define ID_E0_packetSize CHAR2('p','S')
#define PT_E0_origAddr 0x6C
#define ID_E0_origAddr CHAR2('o','A')
#define PT_E0_origAddrSub 0x6D
#define ID_E0_origAddrSub CHAR2('o','S')
#define PT_E0_destAddr 0x70
#define ID_E0_destAddr CHAR2('d','A')
#define PT_E0_destAddrSub 0x71
#define ID_E0_destAddrSub CHAR2('d','S')
#define PT_E0_redirAddr 0x74
#define ID_E0_redirAddr CHAR2('r','A')
#define PT_E0_transitNet 0x78
#define ID_E0_transitNet CHAR2('t','N')
#define PT_E0_restartInd 0x79
#define ID_E0_restartInd CHAR2('r','!')
#define PT_E0_compatLo 0x7C
#define ID_E0_compatLo CHAR2('c','L')
#define PT_E0_compatHi 0x7D
#define ID_E0_compatHi CHAR2('c','H')
#define PT_E0_userInfo 0x7E
#define ID_E0_userInfo CHAR2('U','I')
#define PT_E0_extension 0x7F

#define PT_E0_moreData 0xA0
#define ID_E0_moreData CHAR2('m','d')
#define PT_E0_congestLevel 0xB0
#define ID_E0_congestLevel CHAR2('c','l')

#define PT_E5_serviceInd 0x01
#define ID_E5_serviceInd CHAR2('s','I')
#define PT_E5_chargingInfo 0x02
#define ID_E5_chargingInfo CHAR2('C','I')
#define PT_E5_Date 0x03
#define ID_E5_Date CHAR2('D','a')
#define PT_E5_FacSelect 0x05
#define ID_E5_FacSelect CHAR2('F','S')
#define PT_E5_FacStatus 0x06
#define ID_E5_FacStatus CHAR2('F','s')
#define PT_E5_StatusCalled 0x07
#define ID_E5_StatusCalled CHAR2('S','C')
#define PT_E5_addTransAttr 0x08
#define ID_E5_addTransAttr CHAR2('t','A')

/* FacCodes */
#if 0
#define N1_FAC_Sperre 0x01
#define ID_N1_FAC_Sperre CHAR2('S','p')
#endif

/* Cause codes */
#define ET_NumberUnassigned 0x01
#define ID_ET_NumberUnassigned CHAR2('n','x')
#define ET_NoRouteTransit 0x02
#define ID_ET_NoRouteTransit CHAR2('r','t')
#define ET_NoRouteDest 0x03
#define ID_ET_NoRouteDest CHAR2('r','d')
#define ET_InvChan 0x06
#define ID_ET_InvChan CHAR2('i','C')
#define ET_CallAwarded 0x07
#define ID_ET_CallAwarded CHAR2('c','A')
#define ET_NormalClear 0x10
#define ID_ET_NormalClear CHAR2('c','_')
#define ET_UserBusy 0x11
#define ID_ET_UserBusy CHAR2('U','b')
#define ET_NoUserResponding 0x12
#define ID_ET_NoUserResponding CHAR2('n','r')
#define ET_NoAnswer 0x13
#define ID_ET_NoAnswer CHAR2('a','_')
#define ET_CallRejected 0x15
#define ID_ET_CallRejected CHAR2('c','r')
#define ET_NumberChanged 0x16
#define ID_ET_NumberChanged CHAR2('N','c')
#define ET_NonSelected 0x1A
#define ID_ET_NonSelected CHAR2('s','l')
#define ET_OutOfOrder 0x1B
#define ID_ET_OutOfOrder CHAR2('o','o')
#define ET_InvNumberFormat 0x1C
#define ID_ET_InvNumberFormat CHAR2('i','n')
#define ET_FacRejected 0x1D
#define ID_ET_FacRejected CHAR2('f','r')
#define ET_RestSTATUS 0x1E
#define ID_ET_RestSTATUS CHAR2('s','T')
#define ET_NormalUnspec 0x1F
#define ID_ET_NormalUnspec CHAR2('n','u')
#define ET_NoChanAvail 0x22
#define ID_ET_NoChanAvail CHAR2('n','c')
#define ET_NetworkDown 0x26
#define ID_ET_NetworkDown CHAR2('n','D')
#define ET_TempFailure 0x29
#define ID_ET_TempFailure CHAR2('t','f')
#define ET_SwitchCongest 0x2A
#define ID_ET_SwitchCongest CHAR2('s','C')
#define ET_AccessInfoDiscard 0x2B
#define ID_ET_AccessInfoDiscard CHAR2('a','x')
#define ET_ChanNotAvail 0x2C
#define ID_ET_ChanNotAvail CHAR2('c','y')
#define ET_ResourceUnavail 0x2F
#define ID_ET_ResourceUnavail CHAR2('r','y')
#define ET_QualityUnavail 0x31
#define ID_ET_QualityUnavail CHAR2('q','y')
#define ET_FacNotSubscr 0x32
#define ID_ET_FacNotSubscr CHAR2('f','s')
#define ET_CapNotAuth 0x39
#define ID_ET_CapNotAuth CHAR2('d','A')
#define ET_CapNotAvail 0x3A
#define ID_ET_CapNotAvail CHAR2('c','x')
#define ET_UnavailUnspec 0x3F
#define ID_ET_UnavailUnspec CHAR2('u','x')
#define ET_CapNotImpl 0x41
#define ID_ET_CapNotImpl CHAR2('d','y')
#define ET_ChanTypeNotImpl 0x42
#define ID_ET_ChanTypeNotImpl CHAR2('t','y')
#define ET_FacNotImpl 0x45
#define ID_ET_FacNotImpl CHAR2('f','i')
#define ET_RestrictedInfoAvail 0x46
#define ID_ET_RestrictedInfoAvail CHAR2('r','i')
#define ET_UnimplUnspec 0x4F
#define ID_ET_UnimplUnspec CHAR2('u','?')
#define ET_InvCRef 0x51
#define ID_ET_InvCRef CHAR2('i','r')
#define ET_ChanNotExist 0x52
#define ID_ET_ChanNotExist CHAR2('C','x')
#define ET_WrongCallID 0x53
#define ID_ET_WrongCallID CHAR2('w','c')
#define ET_CIDinUse 0x54
#define ID_ET_CIDinUse CHAR2('d','u')
#define ET_NoCallSusp 0x55
#define ID_ET_NoCallSusp CHAR2('o','s')
#define ET_CIDcleared 0x56
#define ID_ET_CIDcleared CHAR2('x','C')
#define ET_DestIncompat 0x58
#define ID_ET_DestIncompat CHAR2('d','i')
#define ET_InvTransitNet 0x5B
#define ID_ET_InvTransitNet CHAR2('T','x')
#define ET_InvalUnspec 0x5F
#define ID_ET_InvalUnspec CHAR2('i','U')
#define ET_MandatoryMissing 0x60
#define ID_ET_MandatoryMissing CHAR2('m','M')
#define ET_MandatoryNotImpl 0x61
#define ID_ET_MandatoryNotImpl CHAR2('m','I')
#define ET_MandatoryNotCompatible 0x62
#define ID_ET_MandatoryNotCompatible CHAR2('m','C')
#define ET_InfoElemMissing 0x63
#define ID_ET_InfoElemMissing CHAR2('i','M')
#define ET_InfoInvalid 0x64
#define ID_ET_InfoInvalid CHAR2('i','X')
#define ET_InfoNotCompatible 0x65
#define ID_ET_InfoNotCompatible CHAR2('j','C')
#define ET_TimerRecovery 0x66
#define ID_ET_TimerRecovery CHAR2('t','R')
#define ET_ProtocolUnspec 0x6F
#define ID_ET_ProtocolUnspec CHAR2('p','U')
#define ET_InterworkingUnspec 0x7F
#define ID_ET_InterworkingUnspec CHAR2('i','u')

/* Status Codes */
#if 0
#define N1_St_Unknown 01
#define ID_N1_St_Unknown CHAR2('x','x')
#define N1_St_Calling 02
#define ID_N1_St_Calling CHAR2('r','i')
#endif

uchar_t et_idtocause(ushort_t id);
ushort_t et_causetoid(uchar_t id);

#endif							/* __ETSI_H */
