#ifndef _ISDN_23
#define _ISDN_23

#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <sys/types.h>
#endif
#include "config.h"

/**
 ** ISDN driver Streams Interface between levels 2 and 3
 **
 ** Device: /dev/isdnmaster
 **/

/*
 * All messages on this interface must be of type "struct isdn23_hdr" followed
 * by a data message or another header (if appropriate). Set stream head to
 * message mode(keep) for best results.
 */

typedef struct _isdn23_hdr {
	uchar_t key;
	ushort_t seqnum;
	union {						  /* Length param must be first if it's needed */

		struct {				  /* Command mode data/response. */
			ushort_t len;
			SUBDEV minor;
		} _hdr_atcmd;

		struct {				  /* PROTO messages for configuring stream
								   * modules. See below for specials. */
			ushort_t len;
			SUBDEV minor;		  /* Talking to a card if zero */
			uchar_t card;		  /* only significant if minor == zero */
			uchar_t channel;	  /* only significant if minor == zero */
		} _hdr_protocmd;

		struct {				  /* Program data, when talking through a
								   * protocol running on the D channel */
			ushort_t len;
			SUBDEV minor;
		} _hdr_xdata;

		struct {				  /* I data */
			ushort_t len;
			uchar_t card, SAPI;
#ifdef DO_MULTI_TEI
			uchar_t bchan;
#endif
		} _hdr_data;

		struct {				  /* UI data */
			ushort_t len;
			uchar_t card, SAPI;
#ifdef DO_MULTI_TEI
			uchar_t bchan;
#endif
			uchar_t broadcast;	  /* Usually true */
		} _hdr_uidata;

		struct {				  /* raw frame data. Currently unsupported;
								   * useful for XID frames and (probably)
								   * alerting the user when seeing unknown
								   * protocols. */
			ushort_t len;
			uchar_t card;
			uchar_t dchan;
			uchar_t flags;		  /* 01: outgoing */
		} _hdr_rawdata;

		struct {				  /* A device has been opened. */
			SUBDEV minor;		  /* Minor number of device; assigned via clone
								   * driver */
			int flags;			  /* from open(2) */
			uid_t uid;			  /* the obvious */
		} _hdr_open;			  /* Upstream only. */

		struct {				  /* Channel is closed */
			SUBDEV minor;		  /* Channel to kick */
			ushort_t error;		  /* Error to send to the user. If zero,
								   * HANGUP. */
		} _hdr_close;			  /* Used in both directions. */

		struct {				  /* Attach B/D channel */
			SUBDEV minor;		  /* Device to attach to */
			ulong_t connref;      /* for syncronization with detach */
			uchar_t card;		  /* which card? */
			uchar_t chan;		  /* B/D channel to attach. Zero: D chan, else * B. */
			char listen;		  /* listen-only mode? (Bit 1) Force channel? (bit 2) */
		} _hdr_attach;			  /* Downstream only. */

		struct {				  /* disconnect B/D channel */
			SUBDEV minor;		  /* what to disconnect */
			ulong_t connref;      /* for syncronization with attach */
			uchar_t error;		  /* force error? */
			uchar_t perm;		  /* also take down protocol stack */
		} _hdr_detach;			  /* Usually downstream. Upstream if card
								   * vanishes. */

		struct {				  /* Card present. Request: L1 Up, bchans
								   * compared */
			uchar_t card;		  /* Card ID. Counting up from zero. */
			uchar_t dchans;		  /* for the card */
			uchar_t bchans;		  /* per D channel */
			uchar_t flags;		  /* Some stuff */
#define HDR_CARD_DEBUG 01		  /* send D channel trace up */
			long id;			  /* Unique ID of this card, for hardware
								   * config. Usually the first three characters
								   * identify the card and the last is a serial
								   * number. */
			ulong_t modes;		  /* Mask of modes the card can support. */
		} _hdr_card;			  /* Upstream */

		struct {				  /* Card detached. */
			uchar_t card;
		} _hdr_nocard;			  /* Upstream */

		struct {				  /* Attach protocol handler */
			uchar_t card, SAPI;	  /* Card and SAPI are unique. TEIs are
								   * assigned per card. */
			uchar_t broadcast;	  /* No VCs */
#ifdef DO_MULTI_TEI
			uchar_t bchan;
#endif
			uchar_t ind;		  /* Indicator. See primitives.h. */
		} _hdr_openprot;		  /* Downstream: Register. Upstream: Info.  */

		struct {				  /* Remove protocol handler */
			uchar_t card, SAPI;
#ifdef DO_MULTI_TEI
			uchar_t bchan;
#endif
			uchar_t ind;
		} _hdr_closeprot;		  /* Downstream: Kill protocol. Up: Info. */

		/*
		 * Note: if the indicator field is nonzero, openprot and closeprot
		 * (downstream) are synonymous with notify.
		 */

		struct {				  /* General notification on protocol state
								   * change */
			uchar_t card, SAPI;
#ifdef DO_MULTI_TEI
			uchar_t bchan;
#endif
			uchar_t ind;		  /* indication / request / whatevver */
			short add;			  /* additional flags depending on indicator */
		} _hdr_notify;

		struct {				  /* Report an error: invalid command */
			uchar_t error;
			/* invalid isdn23_hdr follows */
		} _hdr_inval;			  /* Note: There's usually no need to analyze
								   * the errors. */
		struct {				  /* Get/Set the TEI for a card */
			uchar_t card;
			uchar_t TEI;		  /* TEI_BROADCAST means the TEI is unassigned */
#ifdef DO_MULTI_TEI
			uchar_t bchan;		  /* TEI_BROADCAST means the TEI is unassigned */
#endif
		} _hdr_tei;				  /* TEI management logically is a level 2
								   * feature. However, the implementation gets
								   * somewhat cleaner if the TEI negotiation
								   * handler is just another L3 protocol */
		struct {
			ushort_t len;
			uchar_t card;
			int seqnum;
			int foffset;
		} _hdr_load;			  /* download some code to the card */
		/*
		 * Setting the TEI to TEI_BROADCAST does the Right Thing WRT to
		 * protocol handlers.
		 */

	} sel;
}
#if defined(__GNUC__) && (__GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 7))
	__attribute__((packed))
#endif
		*isdn23_hdr;

/* Aliases for writing actual programs. *//* Keys for debugging L2. */
#define hdr_atcmd     sel._hdr_atcmd	/* A */
#define hdr_protocmd  sel._hdr_protocmd	/* S */
#define hdr_xdata     sel._hdr_xdata	/* X */
#define hdr_data      sel._hdr_data		/* D */
#define hdr_uidata    sel._hdr_uidata	/* U */
#define hdr_rawdata   sel._hdr_rawdata	/* R */
#define hdr_open      sel._hdr_open		/* O */
#define hdr_close     sel._hdr_close	/* C */
#define hdr_attach    sel._hdr_attach	/* T */
#define hdr_detach    sel._hdr_detach	/* E */
#define hdr_card      sel._hdr_card		/* Y */
#define hdr_nocard    sel._hdr_nocard	/* N */
#define hdr_openprot  sel._hdr_openprot	/* P */
#define hdr_closeprot sel._hdr_closeprot/* Q */
#define hdr_notify    sel._hdr_notify	/* I */
#define hdr_inval     sel._hdr_inval	/* Z */
#define hdr_tei       sel._hdr_tei		/* K */
#define hdr_load      sel._hdr_load		/* L */

/* Magic numbers. */

#define HDR_ATCMD     1
#define HDR_DATA      2
#define HDR_XDATA     3
#define HDR_UIDATA    4
#define HDR_RAWDATA   5
#define HDR_OPEN      6
#define HDR_CLOSE     7
#define HDR_ATTACH    8
#define HDR_DETACH    9
#define HDR_CARD      10
#define HDR_NOCARD    11
#define HDR_OPENPROT  12
#define HDR_CLOSEPROT 13
#define HDR_NOTIFY    14
#define HDR_INVAL     15
#define HDR_TEI       16
#define HDR_PROTOCMD  17
#define HDR_LOAD	  18

#define HDR_FLAGS 0x80
#define HDR_NOERROR 0x80

#endif							/* _ISDN_23 */
