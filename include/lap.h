/* Defs for Q.921. */

#define SAPI_INVALID 0xFF
#define TEI_FIXED 0x12 /* if using a fixed TEI value */
#define TEI_BROADCAST 0x7F
#define TEI_REQUESTED 0xFF /* invalid value, used in L2 only
				to remember not to re-request a TEI */

#define L2_RR  0x01
#define L2_RNR 0x05
#define L2_REJ 0x09
#define L2_SARM 0x0F
#define L2_SABM 0x2F
#define L2_SABME 0x6F
#define L2_DM 0x0F
#define L2_UI 0x03
#define L2_DISC 0x43
#define L2_UA 0x63
#define L2_FRMR 0x87
#define L2_XID 0xAF
#define L2_RR 0x01
#define L2_RNR 0x05
#define L2_REJ 0x09
/* for case switches */
#define L2__CMD 0x100

#define L2_PF 0x10				  /* nonextended mode */
#define L2_PF_I 0x01
#define L2_PF_S 0x01
#define L2_PF_U 0x10

#define L2_m_I 0x01
#define L2_is_I 0x00
#define L2_m_SU 0x03
#define L2_is_S 0x01
#define L2_is_U 0x03

#define L2_adr_EXT 0x01
#define L2_adr_CR 0x02
