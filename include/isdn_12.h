#ifndef _ISDN_12
#define _ISDN_12

#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <sys/types.h>
#endif
#include "streams.h"

/*
 * Interface for ISDN cards.
 * 
 * A card offers a method to take the So channel up or down, and to send and
 * receive HDLC frames on one D channel and HDLC frames and/or transparent data
 * on one or more B channels. Frames are fully transparent, with no address
 * interpretation. This is (a) lowest common denominator and (b) enables us to
 * do some fancy things if we want to. For instance, chips in auto mode don't
 * always report retransmissions, mishandle Q.921, ...
 * 
 * Cards can offer additional B channel features. It is assumed that the B
 * channels are independent. Control of these features is handled by the L4
 * driver.
 * 
 * Success is zero; errors are errno.h values. Freeing mblocks is the
 * responsibility of the caller iff an error is returned.
 */

struct _isdn1_card;

/*
 * Download boot code.
 */
typedef int (*C_boot)(struct _isdn1_card * card, int step, int offset, mblk_t *data);
/*
 * Initiate mode changes. For the D channel, zero means to turn off L1; one
 * turns L1 back on. For the B channels, see M_*. "listen" on the B
 * channels says we can't send yet; on the D channel it doesn't turn on the 
 * channel but it enables interrupts.
 */
typedef int (*C_ch_mode) (struct _isdn1_card * card, short channel, char mode,
		char listen);

/*
 * Sets up a protocol on the specified channel. This is the only mode for
 * talking to "smart" ISDN cards. Fake cards grab the data and send it to the
 * driver program.
 */
typedef int (*C_ch_prot) (struct _isdn1_card * card, short channel, mblk_t * proto, int flags);
#define CHP_FROMSTACK 01 /* travels from the channel to the master */
#define CHP_MODLIST 02 /* module list */
#define CHP_TOCARD 04 /* internal to isdn_2.c: send to card first, if there's a handler */

/*
 * Check if the card has data for us.
 */
typedef int (*C_poll) (struct _isdn1_card * card, short channel);

/*
 * Check if buffer space is available
 */
typedef int (*C_candata) (struct _isdn1_card * card, short channel);

/*
 * Enqueue the data.
 */
typedef int (*C_data) (struct _isdn1_card * card, short channel, mblk_t * data);

/*
 * Flush the send queue.
 */
typedef int (*C_flush) (struct _isdn1_card * card, short channel);

struct _isdn1_card {
	uchar_t nr_chans;			  /* total per card */
	uchar_t nr_dchans;			  /* zero == one. */
	void *ctl;				  /* Pointer for L2 data structures, card
								   * drivers must not touch this. */
	ulong_t modes;				  /* Modes available on this card. */

	C_ch_mode ch_mode;
	C_ch_prot ch_prot;
	C_data send;
	C_flush flush;
	C_candata cansend;
	C_poll poll; /* check if the card can send more data */
	C_boot boot;
};

/**
 ** Callup routines for card-initiated actions.
 **/

/*
 * Register the card. The ID identifies the card to L4 and should be unique
 * within the system.
 */
extern int isdn2_register (struct _isdn1_card *card, long id);

/*
 * Unregister to make the card permanently unavailable.
 */
extern int isdn2_unregister (struct _isdn1_card *card);

/*
 * Report change of level 1 state. Zero is down.
 */
extern void isdn2_new_state (struct _isdn1_card *card, char state);

/*
 * Report errors / changes in card state, for intelligent cards
 */
extern void isdn2_chstate (struct _isdn1_card *card, uchar_t ind, short add);

/*
 * Callback to test availability of queue space. Cards are not required to call
 * this.
 */
extern int isdn2_canrecv (struct _isdn1_card *card, short channel);

/*
 * Enqueue incoming data.
 */
extern int isdn2_recv (struct _isdn1_card *card, short channel, mblk_t * data);

/*
 * Call backenable if enough buffer space is available (after having blocked an
 * attempt to send by card->cansend returning false or card->send returning an
 * error).
 * 
 * The channel parameter can be -1 if the card has common buffers, zero for the D
 * channel, or >0 for the appropriate B channel.
 */
extern int isdn2_backenable (struct _isdn1_card *card, short channel);

/*
 * Callback for protocol setup.
 */
extern int isdn2_chprot (struct _isdn1_card * card, short channel, mblk_t * proto, int flags);

#endif							/* _ISDN_12 */
