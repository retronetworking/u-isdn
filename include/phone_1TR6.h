#ifndef __POSTL3_H
#define __POSTL3_H

#include "compat.h"
#include "primitives.h"
#include <isdn_34.h>

/* CallRef */
#define CR_f_Orig  0x80
#define CR_is_Orig 0x00

/* MsgType N0 */
#define MT_N0_REG_IND 61
#define ID_N0_REG_IND CHAR2('R','I')
#define MT_N0_CANC_IND 62
#define ID_N0_CANC_IND CHAR2('C','I')
#define MT_N0_FAC_STA 63
#define ID_N0_FAC_STA CHAR2('F','S')
#define MT_N0_STA_ACK 64
#define ID_N0_STA_ACK CHAR2('S','k')
#define MT_N0_STA_REJ 65
#define ID_N0_STA_REJ CHAR2('S','R')
#define MT_N0_FAC_INF 66
#define ID_N0_FAC_INF CHAR2('F','I')
#define MT_N0_INF_ACK 67
#define ID_N0_INF_ACK CHAR2('I','A')
#define MT_N0_INF_REJ 68
#define ID_N0_INF_REJ CHAR2('I','R')
#define MT_N0_CLOSE 75
#define ID_N0_CLOSE CHAR2('c','l')
#define MT_N0_CLO_ACK 77
#define ID_N0_CLO_ACK CHAR2('c','A')

/* MsgType N1 */
#define MT_N1_ESC 0x00
#define ID_N1_ESC CHAR2('E','S')
#define MT_N1_ALERT 0x01
#define ID_N1_ALERT CHAR2('A','l')
#define MT_N1_CALL_SENT 0x02
#define ID_N1_CALL_SENT CHAR2('C','S')
#define MT_N1_CONN 0x07
#define ID_N1_CONN CHAR2('C','o')
#define MT_N1_CONN_ACK 0x0F
#define ID_N1_CONN_ACK IND_CONN_ACK
#define MT_N1_SETUP 0x05
#define ID_N1_SETUP CHAR2('S','e')
#define MT_N1_SETUP_ACK 0x0D
#define ID_N1_SETUP_ACK CHAR2('S','A')
#define MT_N1_RES 0x26
#define ID_N1_RES CHAR2('R','e')
#define MT_N1_RES_ACK 0x2E
#define ID_N1_RES_ACK CHAR2('R','A')
#define MT_N1_RES_REJ 0x22
#define ID_N1_RES_REJ CHAR2('R','R')
#define MT_N1_SUSP 0x25
#define ID_N1_SUSP CHAR2('R','u')
#define MT_N1_SUSP_ACK 0x2D
#define ID_N1_SUSP_ACK CHAR2('S','a')
#define MT_N1_SUSP_REJ 0x21
#define ID_N1_SUSP_REJ CHAR2('S','R')
#define MT_N1_USER_INFO 0x20
#define ID_N1_USER_INFO CHAR2('U','I')
#define MT_N1_DET 0x40
#define ID_N1_DET CHAR2('D','e')
#define MT_N1_DISC 0x45
#define ID_N1_DISC CHAR2('D','i')
#define MT_N1_REL 0x4D
#define ID_N1_REL CHAR2('r','l')
#define MT_N1_REL_ACK 0x5A
#define ID_N1_REL_ACK CHAR2('r','A')
#define MT_N1_CANC_ACK 0x6E
#define ID_N1_CANC_ACK CHAR2('x','A')
#define MT_N1_CANC_REJ 0x67
#define ID_N1_CANC_REJ CHAR2('x','R')
#define MT_N1_CON_CON 0x69
#define ID_N1_CON_CON CHAR2('C','C')
#define MT_N1_FAC 0x60
#define ID_N1_FAC CHAR2('F','a')
#define MT_N1_FAC_ACK 0x68
#define ID_N1_FAC_ACK CHAR2('F','A')
#define MT_N1_FAC_CAN 0x66
#define ID_N1_FAC_CAN CHAR2('F','C')
#define MT_N1_FAC_REG 0x64
#define ID_N1_FAC_REG CHAR2('F','R')
#define MT_N1_FAC_REJ 0x65
#define ID_N1_FAC_REJ CHAR2('F','J')
#define MT_N1_INFO 0x6D
#define ID_N1_INFO CHAR2('I','n')
#define MT_N1_REG_ACK 0x6C
#define ID_N1_REG_ACK CHAR2('G','A')
#define MT_N1_REG_REJ 0x6F
#define ID_N1_REG_REJ CHAR2('G','J')
#define MT_N1_STAT 0x63
#define ID_N1_STAT CHAR2('S','t')


/* Param */
#define PS_Shift 0x1

#define PS0_More 0x2
#define PS0_Congest 0x3

#define PT_N0_cause 0x08
#define ID_N0_cause CHAR2('C','a')
#define PT_N0_connAddr 0x0C
#define ID_N0_connAddr CHAR2('c','A')
#define PT_N0_callID 0x10
#define ID_N0_callID CHAR2('c','i')
#define PT_N0_chanID 0x18
#define ID_N0_chanID CHAR2('C','i')
#define PT_N0_netSpecFac 0x20
#define ID_N0_netSpecFac CHAR2('N','S')
#define PT_N0_display 0x28
#define ID_N0_display CHAR2('d','i')
#define PT_N0_keypad 0x2C
#define ID_N0_keypad CHAR2('k','p')
#define PT_N0_origAddr 0x6C
#define ID_N0_origAddr CHAR2('o','A')
#define PT_N0_destAddr 0x70
#define ID_N0_destAddr CHAR2('d','A')
#define PT_N0_userInfo 0x7E
#define ID_N0_userInfo CHAR2('U','I')

#define PT_N0_moreData 0xA0
#define ID_N0_moreData CHAR2('m','d')
#define PT_N0_congestLevel 0xB0
#define ID_N0_congestLevel CHAR2('c','l')

#define PT_N6_serviceInd 0x01
#define ID_N6_serviceInd CHAR2('s','I')
#define PT_N6_chargingInfo 0x02
#define ID_N6_chargingInfo CHAR2('C','I')
#define PT_N6_date 0x03
#define ID_N6_date CHAR2('D','a')
#define PT_N6_facSelect 0x05
#define ID_N6_facSelect CHAR2('F','S')
#define PT_N6_facStatus 0x06
#define ID_N6_facStatus CHAR2('F','s')
#define PT_N6_statusCalled 0x07
#define ID_N6_statusCalled CHAR2('S','C')
#define PT_N6_addTransAttr 0x08
#define ID_N6_addTransAttr CHAR2('t','A')

/* FacCodes */
#define N1_FAC_Sperre 0x01
#define ID_N1_FAC_Sperre CHAR2('S','p')
#define N1_FAC_Sperre_All 0x02
#define ID_N1_FAC_Sperre_All CHAR2('A','l')
#define N1_FAC_Sperre_Fern 0x03
#define ID_N1_FAC_Sperre_Fern CHAR2('F','e')
#define N1_FAC_Sperre_Intl 0x04
#define ID_N1_FAC_Sperre_Intl CHAR2('I','n')
#define N1_FAC_Sperre_interk 0x05
#define ID_N1_FAC_Sperre_interk CHAR2('I','K')

#define N1_FAC_Forward1 0x02
#define ID_N1_FAC_Forward1 CHAR2('F','1')
#define N1_FAC_Forward2 0x03
#define ID_N1_FAC_Forward2 CHAR2('F','2')
#define N1_FAC_Konferenz 0x06
#define ID_N1_FAC_Konferenz CHAR2('K','o')
#define N1_FAC_GrabBchan 0x0F
#define ID_N1_FAC_GrabBchan CHAR2('G','C')
#define N1_FAC_Reactivate 0x10
#define ID_N1_FAC_Reactivate CHAR2('R','e')
#define N1_FAC_Konferenz3 0x11
#define ID_N1_FAC_Konferenz3 CHAR2('K','3')
#define N1_FAC_Dienstwechsel1 0x12
#define ID_N1_FAC_Dienstwechsel1 CHAR2('W','1')
#define N1_FAC_Dienstwechsel2 0x13
#define ID_N1_FAC_Dienstwechsel2 CHAR2('W','2')
#define N1_FAC_NummernIdent 0x14
#define ID_N1_FAC_NummernIdent CHAR2('N','I')
#define N1_FAC_GBG 0x15
#define ID_N1_FAC_GBG CHAR2('G','B')
#define N1_FAC_DisplayUebergeben 0x17
#define ID_N1_FAC_DisplayUebergeben CHAR2('D','U')
#define N1_FAC_DisplayUmgeleitet 0x1A
#define ID_N1_FAC_DisplayUmgeleitet CHAR2('D','R')
#define N1_FAC_Unterdruecke 0x1B
#define ID_N1_FAC_Unterdruecke CHAR2('S','u')
#define N1_FAC_Deactivate 0x1E
#define ID_N1_FAC_Deactivate CHAR2('D','e')
#define N1_FAC_Activate 0x1D
#define ID_N1_FAC_Activate CHAR2('A','c')
#define N1_FAC_SVC 0x1F
#define ID_N1_FAC_SVC CHAR2('S','V')
#define N1_FAC_Rueckwechsel 0x23
#define ID_N1_FAC_Rueckwechsel CHAR2('R','W')
#define N1_FAC_Umleitung 0x24
#define ID_N1_FAC_Umleitung CHAR2('U','m')

/* Cause codes */
#define N1_InvCRef 0x01
#define ID_N1_InvCRef CHAR2('i','r')
#define N1_BearerNotImpl 0x03
#define ID_N1_BearerNotImpl CHAR2('n','b')
#define N1_CIDinUse 0x08
#define ID_N1_CIDinUse CHAR2('c','u')
#define N1_CIDunknown 0x07
#define ID_N1_CIDunknown CHAR2('c','n')
#define N1_NoChans 0x0A
#define ID_N1_NoChans CHAR2('n','c')
#define N1_FacNotImpl 0x10
#define ID_N1_FacNotImpl CHAR2('f','i')
#define N1_FacNotSubscr 0x11
#define ID_N1_FacNotSubscr CHAR2('f','s')
#define N1_OutgoingBarred 0x20
#define ID_N1_OutgoingBarred CHAR2('o','b')
#define N1_UserAssessBusy 0x21
#define ID_N1_UserAssessBusy CHAR2('u','b')
#define N1_NegativeGBG 0x22
#define ID_N1_NegativeGBG CHAR2('n','G')
#define N1_UnknownGBG 0x23
#define ID_N1_UnknownGBG CHAR2('u','G')
#define N1_NoSPVknown 0x25
#define ID_N1_NoSPVknown CHAR2('n','s')
#define N1_DestNotObtain 0x35
#define ID_N1_DestNotObtain CHAR2('d','o')
#define N1_NumberChanged 0x38
#define ID_N1_NumberChanged CHAR2('N','c')
#define N1_OutOfOrder 0x39
#define ID_N1_OutOfOrder CHAR2('o','o')
#define N1_NoUserResponse 0x3A
#define ID_N1_NoUserResponse CHAR2('n','r')
#define N1_UserBusy 0x3B
#define ID_N1_UserBusy CHAR2('U','b')
#define N1_IncomingBarred 0x3D
#define ID_N1_IncomingBarred CHAR2('i','b')
#define N1_CallRejected 0x3E
#define ID_N1_CallRejected CHAR2('c','r')
#define N1_NetworkCongestion 0x59
#define ID_N1_NetworkCongestion CHAR2('n','C')
#define N1_RemoteUser 0x5A
#define ID_N1_RemoteUser CHAR2('r','u')
#define N1_LocalProcErr 0x70
#define ID_N1_LocalProcErr CHAR2('l','E')
#define N1_RemoteProcErr 0x71
#define ID_N1_RemoteProcErr CHAR2('r','E')
#define N1_RemoteUserSuspend 0x72
#define ID_N1_RemoteUserSuspend CHAR2('R','s')
#define N1_RemoteUserResumed 0x73
#define ID_N1_RemoteUserResumed CHAR2('R','r')
#define N1_UserInfoDiscarded 0x7F
#define ID_N1_UserInfoDiscarded CHAR2('U','D')


/* Status Codes */
#define N1_St_Unknown 01
#define ID_N1_St_Unknown CHAR2('x','x')
#define N1_St_Calling 02
#define ID_N1_St_Calling CHAR2('r','i')


ushort_t n1_causetoid(uchar_t id);
uchar_t n1_idtocause(ushort_t id);

ushort_t n1_facsubtoid(uchar_t id);
ushort_t n1_factoid(uchar_t id);
uchar_t n1_idtofacsub(ushort_t id);
uchar_t n1_idtofac(ushort_t id);

#endif							/* __POSTL3_H */
