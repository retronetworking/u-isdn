/******************************************************************************
*
*       (C)opyright 1993 BinTec Computersysteme GmbH
*       All Rights Reserved
*
******************************************************************************/

#ifndef _APIDEF_H_
# define _APIDEF_H_
/* static char _sccsid_apidef_h[] = "@(#)apidef.h	1.28"; */

#if defined(__GNUC__) && (__GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 6))
# define PACK __attribute__ ((packed))
# define _PACK
#else
# define PACK
# define _PACK
#endif


#ifndef WINDOWS
# ifdef __MSDOS__
#  define FAR far
# else
#  define FAR
# endif
#endif

#define CAPI_MAXMSGLEN           180
#define CAPI_MINMSGLEN           8

/************************************************************************/
/*                                                                      */
/*              1TR6 specific defines                                   */
/*                                                                      */
/************************************************************************/
/*-----------------------------*/
/* CONNECT_REQ bchannel        */
/*-----------------------------*/
#define CAPI_ANYBCHANNEL         0x83    

/*-----------------------------*/
/* INFO_REQ infotypes          */
/*-----------------------------*/
#define AI_CAUSE        0x0008          /* codeset 0 */
#define AI_DISPLAY      0x0028          /* codeset 0 */
#define AI_DAD          0x0070          /* codeset 0 */
#define AI_UUINFO       0x007e          /* codeset 0 */
#define AI_CHARGE       0x0602          /* codeset 6 */
#define AI_DATE         0x0603          /* codeset 6 */
#define AI_CPS          0x0607          /* codeset 6 */

/*-----------------------------*/
/* CONNECT_REQ, IND services   */ 
/*-----------------------------*/
#define SI_PHONE        1
#define SI_ABSERVICES   2
#define SI_X21          3
#define SI_FAXG4        4
#define SI_BTX          5
#define SI_DATA         7
#define SI_X25          8
#define SI_TELETEX      9
#define SI_MIXEDMODE    10
#define SI_REMOTECTRL   13
#define SI_GRAPHTEL     14
#define SI_VIDEOTEXT    15
#define SI_VIDEOPHONE   16





/************************************************************************/
/*                                                                      */
/*              CAPI infomask bit settings                              */
/*                                                                      */
/************************************************************************/
#define CAPI_ICHARGE             0x01L           /* BIT 0  */
#define CAPI_IDATE               0x02L           /* BIT 1  */
#define CAPI_IDISPLAY            0x04L           /* BIT 2  */
#define CAPI_IUUINFO             0x08L           /* BIT 3  */
#define CAPI_ICAUSE              0x10L           /* BIT 4  */
#define CAPI_ISTATE              0x20L           /* BIT 5  */
#define CAPI_IDESTINATION        0x40L           /* BIT 6  */
#define CAPI_IDTMF               0x80L           /* BIT 7  */
#define CAPI_ISPV                0x40000000L     /* BIT 30 */
#define CAPI_ISUBADDR            0x80000000L     /* BIT 31 */

#define CAPI_INFOMASK            ( CAPI_ICHARGE  | CAPI_IDATE    | \
				  CAPI_IDISPLAY | CAPI_IUUINFO  | \
				  CAPI_ICAUSE   | CAPI_ISTATE   | \
				  CAPI_ISPV     | CAPI_ISUBADDR )

#define CAPI_ALLINFOMASK         ( CAPI_ICHARGE  | CAPI_IDATE    | \
				  CAPI_IDISPLAY | CAPI_IUUINFO  | \
				  CAPI_ICAUSE   | CAPI_ISTATE   | \
				  CAPI_ISPV     | CAPI_ISUBADDR | \
				  CAPI_IDTMF    | CAPI_IDESTINATION)

#define CAPI_SIMASK              0xe7bf
#define CAPI_EAZMASK             0x3ff


/************************************************************************/
/*                                                                      */
/*              CAPI error codes                                        */
/*                                                                      */
/************************************************************************/
#define   ID_E_REGISTER 					CHAR2('%','Y')
#define CAPI_E_REGISTER                  0x1001
#define   ID_E_APPLICATION 					CHAR2('%','X')
#define CAPI_E_APPLICATION               0x1002
#define   ID_E_MSGLENGTH 					CHAR2('%','W')
#define CAPI_E_MSGLENGTH                 0x1003
#define   ID_E_COMMAND 						CHAR2('%','V')
#define CAPI_E_COMMAND                   0x1004
#define   ID_E_QUEUEFULL 					CHAR2('%','U')
#define CAPI_E_QUEUEFULL                 0x1005
#define   ID_E_NOMSG 						CHAR2('%','T')
#define CAPI_E_NOMSG                     0x1006
#define   ID_E_MSGOVERFLOW 					CHAR2('%','S')
#define CAPI_E_MSGOVERFLOW               0x1007
#define   ID_E_DEINSTALL 					CHAR2('%','R')
#define CAPI_E_DEINSTALL                 0x1008
#define   ID_E_CONTROLLER 					CHAR2('%','Q')
#define CAPI_E_CONTROLLER                0x2001
#define   ID_E_PLCI 						CHAR2('%','P')
#define CAPI_E_PLCI                      0x2002
#define   ID_E_NCCI 						CHAR2('%','O')
#define CAPI_E_NCCI                      0x2003
#define   ID_E_TYPE     					CHAR2('%','4')
#define CAPI_E_TYPE                      0x2004
#define   ID_E_BCHANNEL 					CHAR2('%','N')
#define CAPI_E_BCHANNEL                  0x3101
#define   ID_E_INFOMASK 					CHAR2('%','M')
#define CAPI_E_INFOMASK                  0x3102
#define   ID_E_EAZMASK 						CHAR2('%','L')
#define CAPI_E_EAZMASK                   0x3103
#define   ID_E_SIMASK 						CHAR2('%','K')
#define CAPI_E_SIMASK                    0x3104
#define   ID_E_B2PROTO 						CHAR2('%','J')
#define CAPI_E_B2PROTO                   0x3105
#define   ID_E_DLPD 						CHAR2('%','I')
#define CAPI_E_DLPD                      0x3106
#define   ID_E_B3PROTO 						CHAR2('%','H')
#define CAPI_E_B3PROTO                   0x3107
#define   ID_E_NCPD 						CHAR2('%','G')
#define CAPI_E_NCPD                      0x3108
#define   ID_E_NCPI 						CHAR2('%','F')
#define CAPI_E_NCPI                      0x3109
#define   ID_E_DATAB3FLAGS 					CHAR2('%','E')
#define CAPI_E_DATAB3FLAGS               0x310a
#define   ID_E_CONTROLLERFAILED 			CHAR2('%','D')
#define CAPI_E_CONTROLLERFAILED          0x3201
#define   ID_E_REGCONFLICT 					CHAR2('%','C')
#define CAPI_E_REGCONFLICT               0x3202
#define   ID_E_CMDNOTSUPPORTED 				CHAR2('%','B')
#define CAPI_E_CMDNOTSUPPORTED           0x3203
#define   ID_E_PLCIACT 						CHAR2('%','A')
#define CAPI_E_PLCIACT                   0x3204
#define   ID_E_NCCIACT 						CHAR2('%','z')
#define CAPI_E_NCCIACT                   0x3205
#define   ID_E_B2NOTSUPPORT 				CHAR2('%','y')
#define CAPI_E_B2NOTSUPPORT              0x3206
#define   ID_E_B2STATE 						CHAR2('%','x')
#define CAPI_E_B2STATE                   0x3207
#define   ID_E_B3NOTSUPPORT 				CHAR2('%','w')
#define CAPI_E_B3NOTSUPPORT              0x3208
#define   ID_E_B3STATE 						CHAR2('%','v')
#define CAPI_E_B3STATE                   0x3209
#define   ID_E_B2DLPDPARA 					CHAR2('%','u')
#define CAPI_E_B2DLPDPARA                0x320a
#define   ID_E_B3NCPDPARA 					CHAR2('%','t')
#define CAPI_E_B3NCPDPARA                0x320b
#define   ID_E_B3NCPIPARA 					CHAR2('%','5')
#define CAPI_E_B3NCPIPARA                0x320c
#define   ID_E_DATALEN 						CHAR2('%','s')
#define CAPI_E_DATALEN                   0x320d
#define   ID_E_DTMF 						CHAR2('%','q')
#define CAPI_E_DTMF                      0x320e
#define   ID_E_NOL1 						CHAR2('%','p')
#define CAPI_E_NOL1                      0x3301
#define   ID_E_NOL2 						CHAR2('%','o')
#define CAPI_E_NOL2                      0x3302
#define   ID_E_SETUPBCHANLAYER1 			CHAR2('%','n')
#define CAPI_E_SETUPBCHANLAYER1          0x3303
#define   ID_E_SETUPBCHANLAYER2 			CHAR2('%','m')
#define CAPI_E_SETUPBCHANLAYER2          0x3304
#define   ID_E_ABORTDCHANLAYER1 			CHAR2('%','1')
#define CAPI_E_ABORTDCHANLAYER1          0x3305
#define   ID_E_ABORTDCHANLAYER2 			CHAR2('%','2')
#define CAPI_E_ABORTDCHANLAYER2          0x3306
#define   ID_E_ABORTDCHANLAYER3 			CHAR2('%','3')
#define CAPI_E_ABORTDCHANLAYER3          0x3307
#define   ID_E_ABORTBCHANLAYER1 			CHAR2('%','l')
#define CAPI_E_ABORTBCHANLAYER1          0x3308
#define   ID_E_ABORTBCHANLAYER2 			CHAR2('%','k')
#define CAPI_E_ABORTBCHANLAYER2          0x3309
#define   ID_E_ABORTBCHANLAYER3 			CHAR2('%','j')
#define CAPI_E_ABORTBCHANLAYER3          0x330a
#define   ID_E_REBCHANLAYER2 				CHAR2('%','6')
#define CAPI_E_REBCHANLAYER2             0x330b
#define   ID_E_REBCHANLAYER3 				CHAR2('%','i')
#define CAPI_E_REBCHANLAYER3             0x330c
#define   ID_E_NOFAX 						CHAR2('%','h')
#define CAPI_E_NOFAX                     0x4001
#define   ID_E_BADLINE 						CHAR2('%','g')
#define CAPI_E_BADLINE                   0x4004
#define   ID_E_NOANSWER 					CHAR2('%','f')
#define CAPI_E_NOANSWER                  0x4008
#define   ID_E_REMDISC 						CHAR2('%','e')
#define CAPI_E_REMDISC                   0x4009
#define   ID_E_NOCMD 						CHAR2('%','d')
#define CAPI_E_NOCMD                     0x400a
#define   ID_E_INCOMPAT 					CHAR2('%','c')
#define CAPI_E_INCOMPAT                  0x400b
#define   ID_E_BADDATA 						CHAR2('%','b')
#define CAPI_E_BADDATA                   0x400c
#define   ID_E_PROTO 						CHAR2('%','a')
#define CAPI_E_PROTO                     0x400d


/************************************************************************/
/*                                                                      */
/*              CAPI 1.1 primitives                                     */
/*                                                                      */
/************************************************************************/
#define CAPI_CONNECT_REQ                 0x0002
#define CAPI_CONNECT_CONF                0x0102
#define CAPI_CONNECT_IND                 0x0202
#define CAPI_CONNECT_RESP                0x0302
#define CAPI_CONNECTINFO_REQ             0x0009
#define CAPI_CONNECTINFO_CONF            0x0109
#define CAPI_CONNECTACTIVE_IND           0x0203
#define CAPI_CONNECTACTIVE_RESP          0x0303
#define CAPI_DISCONNECT_REQ              0x0004
#define CAPI_DISCONNECT_CONF             0x0104
#define CAPI_DISCONNECT_IND              0x0204
#define CAPI_DISCONNECT_RESP             0x0304
#define CAPI_LISTEN_REQ                  0x0005
#define CAPI_LISTEN_CONF                 0x0105
#define CAPI_GETPARAMS_REQ               0x0006
#define CAPI_GETPARAMS_CONF              0x0106
#define CAPI_INFO_REQ                    0x0007
#define CAPI_INFO_CONF                   0x0107
#define CAPI_INFO_IND                    0x0207
#define CAPI_INFO_RESP                   0x0307
#define CAPI_DATA_REQ                    0x0008
#define CAPI_DATA_CONF                   0x0108
#define CAPI_DATA_IND                    0x0208
#define CAPI_DATA_RESP                   0x0308
#define CAPI_SELECTB2_REQ                0x0040
#define CAPI_SELECTB2_CONF               0x0140
#define CAPI_SELECTB3_REQ                0x0080
#define CAPI_SELECTB3_CONF               0x0180
#define CAPI_LISTENB3_REQ                0x0081
#define CAPI_LISTENB3_CONF               0x0181
#define CAPI_CONNECTB3_REQ               0x0082
#define CAPI_CONNECTB3_CONF              0x0182
#define CAPI_CONNECTB3_IND               0x0282
#define CAPI_CONNECTB3_RESP              0x0382
#define CAPI_CONNECTB3ACTIVE_IND         0x0283
#define CAPI_CONNECTB3ACTIVE_RESP        0x0383
#define CAPI_DISCONNECTB3_REQ            0x0084
#define CAPI_DISCONNECTB3_CONF           0x0184
#define CAPI_DISCONNECTB3_IND            0x0284
#define CAPI_DISCONNECTB3_RESP           0x0384
#define CAPI_GETB3PARAMS_REQ             0x0085
#define CAPI_GETB3PARAMS_CONF            0x0185
#define CAPI_DATAB3_REQ                  0x0086
#define CAPI_DATAB3_CONF                 0x0186
#define CAPI_DATAB3_IND                  0x0286
#define CAPI_DATAB3_RESP                 0x0386
#define CAPI_RESETB3_REQ                 0x0001
#define CAPI_RESETB3_CONF                0x0101
#define CAPI_RESETB3_IND                 0x0201
#define CAPI_RESETB3_RESP                0x0301
#define CAPI_HANDSET_IND                 0x0287
#define CAPI_HANDSET_RESP                0x0387
#define CAPI_DTMF_REQ                    0x000a
#define CAPI_DTMF_CONF                   0x010a
#define CAPI_DTMF_IND                    0x020a
#define CAPI_DTMF_RESP                   0x030a


/************************************************************************/
/*                                                                      */
/*              BinTec specific CAPI primitives                         */      
/*                                                                      */
/************************************************************************/
#define CAPI_CONTROL_REQ                 0x00ff
#define CAPI_CONTROL_CONF                0x01ff
#define CAPI_CONTROL_IND                 0x02ff
#define CAPI_CONTROL_RESP                0x03ff


#define CAPI_OPERATION_MSG               0xffe0  /* start of operation msg */

#define CAPI_ALIVE_IND                   0xfff0
#define CAPI_ALIVE_RESP                  0xfff1
#define CAPI_REGISTER_REQ                0xfff2
#define CAPI_REGISTER_CONF               0xfff3
#define CAPI_RELEASE_REQ                 0xfff4
#define CAPI_RELEASE_CONF                0xfff5
#define CAPI_SETSIGNAL_REQ               0xfff6
#define CAPI_SETSIGNAL_CONF              0xfff7
#define CAPI_DEINSTALL_REQ               0xfff8
#define CAPI_DEINSTALL_CONF              0xfff9
#define CAPI_GETMANUFACT_REQ             0xfffa
#define CAPI_GETMANUFACT_CONF            0xfffb
#define CAPI_GETVERSION_REQ              0xfffc
#define CAPI_GETVERSION_CONF             0xfffd
#define CAPI_GETSERIAL_REQ               0xfffe
#define CAPI_GETSERIAL_CONF              0xffff


/************************************************************************/
/*                                                                      */
/*              CAPI function codes                                     */      
/*                                                                      */
/************************************************************************/
#define CAPI_REGISTER                    0x01
#define CAPI_RELEASE                     0x02
#define CAPI_PUTMESSAGE                  0x03
#define CAPI_GETMESSAGE                  0x04
#define CAPI_SETSIGNAL                   0x05
#define CAPI_DEINSTALL                   0x06
#define CAPI_GETMANUFACT                 0xf0
#define CAPI_GETVERSION                  0xf1
#define CAPI_GETSERIAL                   0xf2
#define CAPI_MANUFACTURER                0xff




/************************************************************************/
/*                                                                      */
/*              Flags for CAPI_DATAB3_REQ/IND                            */      
/*                                                                      */
/************************************************************************/
#define CAPI_QUALIFIER                   0x01
#define CAPI_MORE                        0x02
#define CAPI_DELIVERY                    0x04
#define CAPI_ALLFLAGS            (CAPI_QUALIFIER | CAPI_MORE | CAPI_DELIVERY)


/************************************************************************/
/*                                                                      */
/*              BinTec specific control type values                     */      
/*                                                                      */
/************************************************************************/
#define CONTROL_APIREC_ON               0x01
#define CONTROL_APIREC_OFF              0x02
#define CONTROL_APIREC_PLAY             0x03
#define CONTROL_TRACELEVEL              0x04
#define CONTROL_API_OPEN                0x05
#define CONTROL_API_CLOSE               0x06
#define CONTROL_LOOPBACK                0x08
#define CONTROL_APISTATE                0x09
#define CONTROL_SHOWMSG                 0x0a
#define CONTROL_SHOWMEM                 0x0b
#define CONTROL_TRACEREC_ON             0x0c
#define CONTROL_TRACEREC_OFF            0x0d
#define CONTROL_TRACEREC_PLAY           0x0e
#define CONTROL_ACCOUNT_PLAY            0x0f
#define CONTROL_STATIST                 0x10
#define CONTROL_EAZMAPPING              0x11

/************************************************************************/
/*                                                                      */
/*      flags for apiopen.flags                                         */
/*      CAPI behaviour flags                                            */
/*                                                                      */
/************************************************************************/
#define COMPAT_NODLPDCHECK                      0x0001
#define COMPAT_FAXMAXSPEED                      0x0002
#define COMPAT_TRANSREVBIT                      0x0004
#define COMPAT_X25CALLNODBIT                    0x0008
#define COMPAT_MAPNOEAZTOZERO                   0x0010
#define COMPAT_NOQ931ON                         0x0020
#define COMPAT_NOIEDATE                         0x0040
#define COMPAT_NOALIVEIND                       0x0080
#define COMPAT_ALERTING                         0x0100


/************************************************************************/
/*                                                                      */
/*              CAPI protocol types                                     */      
/*                                                                      */
/************************************************************************/
enum CAPI_l2prots {      L2X75           = 0x01, 
			L2HDLCCRC       = 0x02,  
			L2TRANS         = 0x03,
			L2SDLC          = 0x04, /* not yet implemented */   
			L2X75BTX        = 0x05,
			L2FAX           = 0x06,
			L2LAPD          = 0x07,
			L2V110TRANS     = 0x08,
			L2V110SDLC      = 0x09, /* not yet implemented */  
			L2V110X75       = 0x0a,
			L2TXONLY        = 0x0b,
			L2MODEM         = 0xf0
};

enum CAPI_l3prots {      L3T70NL         = 0x01, 
			L3ISO8208       = 0x02,
			L3T90           = 0x03,
			L3TRANS         = 0x04,
			L3T30           = 0x05,
			L3T70ASS        = 0xf0  /* assemble t70 packets */
};




/************************************************************************/
/*                                                                      */
/*              CAPI userdata structures                                */      
/*                                                                      */
/************************************************************************/
struct userdata {
    unsigned char data[ CAPI_MAXMSGLEN ] _PACK;
} PACK ;

struct telno {
    unsigned char info                  _PACK;
    unsigned char no[32]                _PACK;
} PACK ;

struct dlpd {
    unsigned short data_length          _PACK;
    unsigned char  link_addr_a          _PACK;
    unsigned char  link_addr_b          _PACK;
    unsigned char  modulo_mode          _PACK;
    unsigned char  window_size          _PACK;
    unsigned char  xid                  _PACK;
} PACK ;

struct dlpd_v110 {
    unsigned short data_length          _PACK;
    unsigned char  link_addr_a          _PACK;
    unsigned char  link_addr_b          _PACK;
    unsigned char  modulo_mode          _PACK;
    unsigned char  window_size          _PACK;
    unsigned char  user_rate            _PACK;   /* V110 */
    unsigned char  xid                  _PACK;
} PACK ;

struct ncpd_x25 {
    unsigned short lic                  _PACK;
    unsigned short hic                  _PACK;
    unsigned short ltc                  _PACK;
    unsigned short htc                  _PACK;
    unsigned short loc                  _PACK;
    unsigned short hoc                  _PACK;
    unsigned char  modulo_mode          _PACK;
} PACK ;

struct ncpd_t30 {
    unsigned char  resolution           _PACK;
    unsigned char  max_speed            _PACK;
    unsigned char  format               _PACK;
    unsigned char  xmit_level           _PACK;
    unsigned char  station_id_length    _PACK;
} PACK ;

struct ncpi {
    unsigned char  data[180]            _PACK;
} PACK ;

struct ncpi_t30 {
    unsigned char  resolution           _PACK;
    unsigned char  speed                _PACK;
    unsigned char  format               _PACK;
    unsigned char  pages                _PACK;
    unsigned char  receiver_id_length   _PACK;
} PACK ;

struct dtmfnum {
    unsigned char numbers[32]           _PACK;
} PACK ;

struct sff_doc_header {
    unsigned long  sff_id               _PACK;
    unsigned char  version              _PACK;
    unsigned char  reserved             _PACK;
    unsigned short user_info            _PACK;
    unsigned short num_pages            _PACK;
    unsigned short first_page           _PACK;
    unsigned long  last_page            _PACK;
    unsigned long  file_size            _PACK;
} PACK ;

struct sff_page_header {
    unsigned char  vert_res             _PACK;
    unsigned char  horiz_res            _PACK;
    unsigned char  coding               _PACK;
    unsigned char  specials             _PACK;
    unsigned short linelen              _PACK;
    unsigned short pagelen              _PACK;
    unsigned long  prev_page            _PACK;
    unsigned long  next_page            _PACK;
} PACK ;




/************************************************************************/
/*                                                                      */
/*              BinTec specific CAPI data structures                    */      
/*                                                                      */
/************************************************************************/

/* this is not a real message, only an identifier for DOS CAPI.EXE */
struct apiinitpara {
    char                identifier[64]  _PACK;
    unsigned short      memaddr         _PACK;
    unsigned char       intr            _PACK;
    unsigned char       type            _PACK;
    unsigned char       bdnum           _PACK;
    unsigned long       ctrlmask        _PACK;
} PACK ;



struct apiopen {
    char                protocol[32]    _PACK;
    unsigned short      teid            _PACK;
    unsigned short      b3pl            _PACK;
    unsigned short      t3id            _PACK;
    unsigned short      contrl          _PACK;
    char                bindaddr[32]    _PACK;
    unsigned long       time            _PACK;
    unsigned long       flags           _PACK;
    unsigned char       spid1[20]       _PACK;
    unsigned char       spid2[20]       _PACK;
} PACK ;

struct traceopen {
    unsigned char       contrl          _PACK;
    unsigned char       channel         _PACK;
    unsigned char       dummy           _PACK;
    short               maxlen          _PACK;   /* -1 if not specified */
} PACK ;

struct tracedata {
    unsigned char       blknum          _PACK;
    unsigned long       timer           _PACK;
    unsigned long       ppa             _PACK;
    unsigned long       event           _PACK;
    unsigned long       inout           _PACK;
#ifdef __MSDOS__
    char far            *dataPtr;
#else
    unsigned long       dataPtr         _PACK;
#endif
    unsigned short      datalen         _PACK;
} PACK ;

struct apiaccount {
    unsigned short      appl            _PACK;
    unsigned short      plci            _PACK;
    unsigned short      ncci            _PACK;
    unsigned long       callref         _PACK;
    unsigned char       l2prots         _PACK;
    unsigned char       l3prots         _PACK;
    unsigned short      cause           _PACK;
    unsigned short      charge          _PACK;
    unsigned short      time            _PACK;
    unsigned char       date[20]        _PACK;
    unsigned char       service         _PACK;
    unsigned char       addinfo         _PACK;
    unsigned char       eaz             _PACK;
    unsigned char       channel         _PACK;
    struct telno        telno           _PACK;
} PACK ;    


struct apistatist {
    unsigned char       contrl          _PACK;   /* capi controllernumber */
    unsigned char       usedbchannels   _PACK;
    unsigned char       l1stat          _PACK;
    unsigned long       xpkts[3]        _PACK;   /* packets transmit */
    unsigned long       rpkts[3]        _PACK;   /* packets received */
    unsigned long       xerrs[3]        _PACK;   /* errors transmit */
    unsigned long       rerrs[3]        _PACK;   /* errors received */   

    unsigned long       xthr[2]         _PACK;   /* throughput send */
    unsigned long       rthr[2]         _PACK;   /* throughput receive */
    unsigned short      plci[2]         _PACK;   /* plci with used bchan */
    unsigned char       pstate[2]       _PACK;   /* plci state */
    unsigned long       incon[2]        _PACK;   /* number of incoming */
    unsigned long       outcon[2]       _PACK;   /* number of outgoing */
} PACK ;

struct eazmapping {
    unsigned char       contrl          _PACK;
    unsigned char       eaz             _PACK;
    unsigned char       telnolen        _PACK;
} PACK ;

/************************************************************************/
/*                                                                      */
/*              BinTec specific CAPI message structures                 */      
/*                                                                      */
/************************************************************************/
struct CAPI_every_header {
    unsigned short len          _PACK;
    unsigned short appl         _PACK;   /* applid from the pc */
    unsigned short PRIM_type    _PACK;
    unsigned short messid       _PACK;
} PACK;

struct CAPI_register_req {
    unsigned long  buffer       _PACK; 
    unsigned short nmess        _PACK;
    unsigned short nconn        _PACK;
    unsigned short ndblock      _PACK;
    unsigned short dblocksiz    _PACK;
} PACK ;
 
struct CAPI_register_conf {
} PACK ;
 
struct CAPI_release_req {
    unsigned short relappl      _PACK;           /* appl to release */   
} PACK ;
 
struct CAPI_release_conf {
    unsigned short info         _PACK;           /* for error reports */
} PACK ;

struct CAPI_deinstall_req {
} PACK ;

struct CAPI_deinstall_conf {
    unsigned short info         _PACK;
} PACK ;
 
struct CAPI_getmanufact_req {
} PACK ;

struct CAPI_getmanufact_conf {
    unsigned char datalen       _PACK;
} PACK ;

struct CAPI_getversion_req {
} PACK ;

struct CAPI_getversion_conf {
    unsigned char datalen       _PACK;
} PACK ;

struct CAPI_getserial_req {
} PACK ;

struct CAPI_getserial_conf {
    unsigned char datalen       _PACK;
} PACK ;


struct CAPI_control_req {
    unsigned short contrl       PACK;
    unsigned short type         _PACK;
    unsigned char  datalen      _PACK;
} PACK ;

struct CAPI_control_conf {
    unsigned short contrl       _PACK;
    unsigned short type         _PACK;
    unsigned short info         _PACK;
} PACK ;

struct CAPI_control_ind {
    unsigned short contrl       _PACK;
    unsigned short type         _PACK;
    unsigned char  datalen      _PACK;
} PACK ;

struct CAPI_control_resp {
    unsigned short contrl       _PACK;
    unsigned short type         _PACK;
    unsigned char  datalen      _PACK;
} PACK ;
 
struct CAPI_alive_ind {
} PACK ;

struct CAPI_alive_resp {
} PACK ;


 

/************************************************************************/
/*                                                                      */
/*              CAPI message structures                                 */      
/*                                                                      */
/************************************************************************/
struct CAPI_connect_req {
    unsigned char contrl        _PACK;
    unsigned char channel       _PACK;
    unsigned long infomask      _PACK;
    unsigned char DST_service   _PACK;
    unsigned char DST_addinfo   _PACK;
    unsigned char SRC_eaz       _PACK;

    unsigned char telnolen      _PACK;
} PACK ;

struct CAPI_connect_conf {
    unsigned short plci         _PACK;   
    unsigned short info         _PACK;
} PACK ;

struct CAPI_connect_ind {
    unsigned short plci         _PACK;
    unsigned char  contrl       _PACK;
    unsigned char  DST_service  _PACK;
    unsigned char  DST_addinfo  _PACK;
    unsigned char  DST_eaz      _PACK;

    unsigned char  telnolen     _PACK;
} PACK ;

struct CAPI_connect_resp {
    unsigned short plci         _PACK;
    unsigned char  reject       _PACK;   /* 0: accept, != 0: reject */
} PACK ;

struct CAPI_connectinfo_req {
    unsigned short plci         _PACK;
    unsigned char  telnolen     _PACK;
} PACK ;

struct CAPI_connectinfo_conf {
    unsigned short plci         _PACK;
    unsigned short info         _PACK;
} PACK ;

struct CAPI_connectactive_ind {
    unsigned short plci         _PACK;
    unsigned char  telnolen     _PACK;
} PACK ;

struct CAPI_connectactive_resp {
    unsigned short plci         _PACK;
} PACK ;

struct CAPI_disconnect_req {
    unsigned short plci         _PACK;
    unsigned char cause         _PACK;
} PACK ;

struct CAPI_disconnect_conf {
    unsigned short plci         _PACK;
    unsigned short info         _PACK;
} PACK ;

struct CAPI_disconnect_ind {
    unsigned short plci         _PACK;
    unsigned short info         _PACK;
} PACK ;

struct CAPI_disconnect_resp {
    unsigned short plci         _PACK;
} PACK ;

struct CAPI_listen_req {
    unsigned char contrl        _PACK;
    unsigned long info_mask     _PACK;
    unsigned short eaz_mask     _PACK;
    unsigned short service_mask _PACK;
} PACK ;

struct CAPI_listen_conf {
    unsigned char contrl        _PACK;
    unsigned short info         _PACK;
} PACK ;

struct CAPI_getparams_req {
    unsigned short plci         _PACK;
} PACK ;

struct CAPI_getparams_conf {
    unsigned short plci         _PACK;
    unsigned char contrl        _PACK;
    unsigned char chan          _PACK;
    unsigned short info         _PACK;
    unsigned char B3_linkcnt    _PACK;
    unsigned char service       _PACK;
    unsigned char addinfo       _PACK;
    unsigned char eaz           _PACK;
    unsigned char telnolen      _PACK;
} PACK ;

struct CAPI_info_req {
    unsigned short plci         _PACK;
    unsigned long info_mask     _PACK;
} PACK ;

struct CAPI_info_conf {
    unsigned short plci         _PACK;
    unsigned short info         _PACK;
} PACK ;

struct CAPI_info_ind {
    unsigned short plci         _PACK;
    unsigned short info_number  _PACK;
    unsigned char  infolen      _PACK;
} PACK ;

struct CAPI_info_resp {
    unsigned short plci         _PACK;
} PACK ;

struct CAPI_data_req {
    unsigned short plci         _PACK;
    unsigned char  datalen      _PACK;
} PACK ;

struct CAPI_data_conf {
    unsigned short plci         _PACK;
    unsigned short info         _PACK;
} PACK ;

struct CAPI_data_ind {
    unsigned short plci         _PACK;
    unsigned char datalen       _PACK;
} PACK ;

struct CAPI_data_resp {
    unsigned short plci         _PACK;
} PACK ;

struct CAPI_selectb2_req {
    unsigned short plci         _PACK;
    unsigned char B2_proto      _PACK;
    unsigned char dlpdlen       _PACK;
} PACK ;

struct CAPI_selectb2_conf {
    unsigned short plci         _PACK;
    unsigned short info         _PACK;
} PACK ;

struct CAPI_selectb3_req {
    unsigned short plci         _PACK;
    unsigned char B3_proto      _PACK;
    unsigned char ncpdlen       _PACK;
} PACK ;

struct CAPI_selectb3_conf {
    unsigned short plci         _PACK;
    unsigned short info         _PACK;
} PACK ;

struct CAPI_listenb3_req {
    unsigned short plci         _PACK;
} PACK ;

struct CAPI_listenb3_conf {
    unsigned short plci         _PACK;
    unsigned short info         _PACK;
} PACK ;

struct CAPI_connectb3_req {
    unsigned short plci         _PACK;
    unsigned char ncpilen       _PACK;
} PACK ;

struct CAPI_connectb3_conf {
    unsigned short plci         _PACK;
    unsigned short ncci         _PACK;
    unsigned short info         _PACK;
} PACK ;

struct CAPI_connectb3_ind {
    unsigned short ncci         _PACK;
    unsigned short plci         _PACK;
    unsigned char ncpilen       _PACK;
} PACK ;

struct CAPI_connectb3_resp {
    unsigned short ncci         _PACK;
    unsigned char  reject       _PACK;
    unsigned char  ncpilen      _PACK;
} PACK ;

struct CAPI_connectb3active_ind {
    unsigned short ncci         _PACK;
    unsigned char  ncpilen      _PACK;
} PACK ;

struct CAPI_connectb3active_resp {
    unsigned short ncci         _PACK;
} PACK ;

struct CAPI_disconnectb3_req {
    unsigned short ncci         _PACK;
    unsigned char  ncpilen      _PACK;
} PACK ;

struct CAPI_disconnectb3_conf {
    unsigned short ncci         _PACK;
    unsigned short info         _PACK;
} PACK ;

struct CAPI_disconnectb3_ind {
    unsigned short ncci         _PACK;
    unsigned short info         _PACK;
    unsigned char  ncpilen      _PACK;
} PACK ;

struct CAPI_disconnectb3_resp {
    unsigned short ncci         _PACK;
} PACK ;

struct CAPI_getb3params_req {
    unsigned short ncci         _PACK;
} PACK ;

struct CAPI_getb3params_conf {
    unsigned short ncci         _PACK;
    unsigned short plci         _PACK;
    unsigned short info         _PACK;
} PACK ;

struct CAPI_datab3_req {
    unsigned short ncci         _PACK;
    unsigned short datalen      _PACK;
#ifdef __MSDOS__
    char far *data              _PACK;
#else
    unsigned long  data         _PACK;
#endif
    unsigned char  blknum       _PACK;
    unsigned short flags        _PACK;   /* MORE_flag ... */

} PACK ;

struct CAPI_datab3_conf {
    unsigned short ncci         _PACK;
    unsigned char  blknum       _PACK;
    unsigned short info         _PACK;
} PACK ;

struct CAPI_datab3_ind {
    unsigned short ncci         _PACK;
    unsigned short datalen      _PACK;
#ifdef __MSDOS__
    char far *data              _PACK;
#else
    unsigned long  data         _PACK;
#endif
    unsigned char  blknum       _PACK;
    unsigned short flags        _PACK;   /* MORE_flag ... */

} PACK ;

struct CAPI_datab3_resp {
    unsigned short ncci         _PACK;
    unsigned char  blknum       _PACK;
} PACK ;

struct CAPI_resetb3_req {
    unsigned short ncci         _PACK;
} PACK ;

struct CAPI_resetb3_conf {
    unsigned short ncci         _PACK;
    unsigned short info         _PACK;
} PACK ;

struct CAPI_resetb3_ind {
    unsigned short ncci         _PACK;
} PACK ;

struct CAPI_resetb3_resp {
    unsigned short ncci         _PACK;
} PACK ;

struct CAPI_handset_ind {
    unsigned short plci         _PACK;
    unsigned char contrl        _PACK;
    unsigned char state         _PACK;
} PACK ;

struct CAPI_handset_resp {
    unsigned short plci         _PACK;
} PACK ;

struct CAPI_dtmf_req {
    unsigned short plci         _PACK;
    unsigned short tonedurat    _PACK;
    unsigned short gapdurat     _PACK;
    unsigned char dtmflen       _PACK;
} PACK ;

struct CAPI_dtmf_conf {
    unsigned short plci         _PACK;
    unsigned short info         _PACK;
} PACK ;

struct CAPI_dtmf_ind {
    unsigned short plci         _PACK;
    unsigned char  dtmflen      _PACK;
} PACK ;

struct CAPI_dtmf_resp {
    unsigned short plci         _PACK;
} PACK ;


struct CAPI_sheader {
} PACK ;

struct CAPI_header {
    unsigned short ident        _PACK;   /* either 'plci' or 'ncci' */
} PACK ;

struct CAPI_cheader {
    unsigned char control       _PACK;   /* controller */
} PACK ;


union CAPI_primitives {
    char   CAPI_msg[ CAPI_MAXMSGLEN ]                             _PACK;
    struct CAPI_sheader                  sheader                 _PACK;
    struct CAPI_cheader                  cheader                 _PACK;
    struct CAPI_header                   header                  _PACK;

    struct CAPI_connect_req              connect_req             _PACK;
    struct CAPI_connect_conf             connect_conf            _PACK;
    struct CAPI_connect_ind              connect_ind             _PACK;
    struct CAPI_connect_resp             connect_resp            _PACK;
    struct CAPI_connectinfo_req          connectinfo_req         _PACK;
    struct CAPI_connectinfo_conf         connectinfo_conf        _PACK;
    struct CAPI_connectactive_ind        connectactive_ind       _PACK;
    struct CAPI_connectactive_resp       connectactive_resp      _PACK;
    struct CAPI_disconnect_req           disconnect_req          _PACK;
    struct CAPI_disconnect_conf          disconnect_conf         _PACK;
    struct CAPI_disconnect_ind           disconnect_ind          _PACK;
    struct CAPI_disconnect_resp          disconnect_resp         _PACK;
    struct CAPI_listen_req               listen_req              _PACK;
    struct CAPI_listen_conf              listen_conf             _PACK;
    struct CAPI_getparams_req            getparams_req           _PACK;
    struct CAPI_getparams_conf           getparams_conf          _PACK;
    struct CAPI_info_req                 info_req                _PACK;
    struct CAPI_info_conf                info_conf               _PACK;
    struct CAPI_info_ind                 info_ind                _PACK;
    struct CAPI_info_resp                info_resp               _PACK;
    struct CAPI_data_req                 data_req                _PACK;
    struct CAPI_data_conf                data_conf               _PACK;
    struct CAPI_data_ind                 data_ind                _PACK;
    struct CAPI_data_resp                data_resp               _PACK;
    struct CAPI_selectb2_req             selectb2_req            _PACK;
    struct CAPI_selectb2_conf            selectb2_conf           _PACK;
    struct CAPI_selectb3_req             selectb3_req            _PACK;
    struct CAPI_selectb3_conf            selectb3_conf           _PACK;
    struct CAPI_listenb3_req             listenb3_req            _PACK;
    struct CAPI_listenb3_conf            listenb3_conf           _PACK;
    struct CAPI_connectb3_req            connectb3_req           _PACK;
    struct CAPI_connectb3_conf           connectb3_conf          _PACK;
    struct CAPI_connectb3_ind            connectb3_ind           _PACK;
    struct CAPI_connectb3_resp           connectb3_resp          _PACK;
    struct CAPI_connectb3active_ind      connectb3active_ind     _PACK;
    struct CAPI_connectb3active_resp     connectb3active_resp    _PACK;
    struct CAPI_disconnectb3_req         disconnectb3_req        _PACK;
    struct CAPI_disconnectb3_conf        disconnectb3_conf       _PACK;
    struct CAPI_disconnectb3_ind         disconnectb3_ind        _PACK;
    struct CAPI_disconnectb3_resp        disconnectb3_resp       _PACK;
    struct CAPI_getb3params_req          getb3params_req         _PACK;
    struct CAPI_getb3params_conf         getb3params_conf        _PACK;
    struct CAPI_datab3_req               datab3_req              _PACK;
    struct CAPI_datab3_conf              datab3_conf             _PACK;
    struct CAPI_datab3_ind               datab3_ind              _PACK;
    struct CAPI_datab3_resp              datab3_resp             _PACK;
    struct CAPI_resetb3_req              resetb3_req             _PACK;
    struct CAPI_resetb3_conf             resetb3_conf            _PACK;
    struct CAPI_resetb3_ind              resetb3_ind             _PACK;
    struct CAPI_resetb3_resp             resetb3_resp            _PACK;
    struct CAPI_handset_ind              handset_ind             _PACK;
    struct CAPI_handset_resp             handset_resp            _PACK;
    struct CAPI_dtmf_req                 dtmf_req                _PACK;
    struct CAPI_dtmf_conf                dtmf_conf               _PACK;
    struct CAPI_dtmf_ind                 dtmf_ind                _PACK;
    struct CAPI_dtmf_resp                dtmf_resp               _PACK;

    /* BinTec specific CAPI manufacturer messages */
    struct CAPI_control_req              control_req             _PACK;
    struct CAPI_control_conf             control_conf            _PACK;
    struct CAPI_control_ind              control_ind             _PACK;
    struct CAPI_control_resp             control_resp            _PACK;

    struct CAPI_alive_ind                alive_ind               _PACK;
    struct CAPI_alive_resp               alive_resp              _PACK;

    struct CAPI_register_req             register_req            _PACK;
    struct CAPI_register_conf            register_conf           _PACK;
    struct CAPI_release_req              release_req             _PACK;
    struct CAPI_release_conf             release_conf            _PACK;
    struct CAPI_deinstall_req            deinstall_req           _PACK;
    struct CAPI_deinstall_conf           deinstall_conf          _PACK;
    struct CAPI_getmanufact_req          manufact_req            _PACK;
    struct CAPI_getmanufact_conf         manufact_conf           _PACK;
    struct CAPI_getserial_req            getserial_req           _PACK;
    struct CAPI_getserial_conf           getserial_conf          _PACK;
    struct CAPI_getversion_req           getversion_req          _PACK;
    struct CAPI_getversion_conf          getversion_conf         _PACK;
} PACK;



#endif
