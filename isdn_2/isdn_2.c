/* needs the "strlog" Stremas module, with log_printmsg visible, for now */

/*
 * isdn_2
 *
 * Copyright (c) 1993-1995, Matthias Urlichs <urlichs@noris.de>.
 *
 */

/*
   TODO: C_lock_up is deactivated -> remove?
 */

#define UAREA

#include "f_module.h"
#include "kernel.h"
#include "primitives.h"
#include "isdn_2.h"
#include <sys/time.h>
#include "f_signal.h"
#include "f_malloc.h"
#include <sys/sysmacros.h>
#include "streams.h"
#include "stropts.h"
/* #ifdef DONT_ADDERROR */
#include "f_user.h"
/* #endif */
#include <sys/errno.h>
#ifndef __linux__
#include <sys/reg.h>
#include <sys/var.h>
#endif
#include <sys/file.h>
#include <fcntl.h>
#include <stddef.h>
#include "streamlib.h"
#include "isdn_23.h"
#include "isdn_12.h"
#include "x75lib.h"
#include "smallq.h"
#include "lap.h"
#include "sapi.h"
#include "isdn_limits.h"
#include "isdn_proto.h"
#include <sys/termios.h>

ushort_t hdrseq = 1;

extern void log_printmsg (void *log, const char *text, mblk_t * mp, const char*);
extern void logh_printmsg (void *log, const char *text, mblk_t * mp);

#ifdef CONFIG_DEBUG_ISDN
void logh__printmsg(unsigned int line,void *log,const char *text, mblk_t *mb)
{
	printf("* %d",line);
	logh_printmsg(log,text,mb);
}
#define logh_printmsg(a,b,c) logh__printmsg(__LINE__,a,b,c)
#endif

int isdn2_log = 0x00;
int isdn2_debug = 
#ifdef CONFIG_DEBUG_ISDN
	0x5016;
#else
	0;
#endif

#ifdef DO_MULTI_TEI
#define State(card,ch) (card->state[ch])
#define N_TEI MAX_B /* zero is free */
#else
#define State(card,ch) (card->state[0])
#define N_TEI 0
#endif

/*
 * Data.
 */
static struct _isdn2_card *isdn_card = NULL;
static struct _isdn2_chan isdn_chan = {};
static isdn2_chan isdnchan[NPORT] = { NULL,};
/* static struct _isdn2_state *isdn_state = NULL; */

/*
 * Standard Streams driver information.
 */
static struct module_info isdn2_minfo =
{
	0, "isdn", 0, INFPSZ, 200,100
};

static struct module_info isdn2_mtinfo =
{
	0, "tisdn", 0, INFPSZ, 200,100
};

static qf_open isdn2_open;
static qf_close isdn2_close;
static qf_srv isdn2_wsrv, isdn2_rsrv;
static qf_put isdn2_wput;

static struct qinit isdn2_rinit =
{
		putq, isdn2_rsrv, isdn2_open, isdn2_close, NULL, &isdn2_minfo, NULL
};

static struct qinit isdn2_winit =
{
		isdn2_wput, isdn2_wsrv, NULL, NULL, NULL, &isdn2_minfo, NULL
};

static struct qinit isdn2_rtinit =
{
		putq, isdn2_rsrv, isdn2_open, isdn2_close, NULL, &isdn2_mtinfo, NULL
};

static struct qinit isdn2_wtinit =
{
		isdn2_wput, isdn2_wsrv, NULL, NULL, NULL, &isdn2_mtinfo, NULL
};

struct streamtab isdn_2info =
{&isdn2_rinit, &isdn2_winit, NULL, NULL};

struct streamtab isdn_2tinfo =
{&isdn2_rtinit, &isdn2_wtinit, NULL, NULL};

/* Flag for delaying sending up card info on open */
static char isdn2_notsent;

static void isdn2_sendcard (isdn2_card card);

/*
 * Forward declarations for the X75/Q.921 interface
 */
static int D_state (isdn2_state state, uchar_t ind, short add);
static int D_cansend (isdn2_state state);
static int D_canrecv (isdn2_state state);
static int D_send (isdn2_state state, char cmd, mblk_t * data);
static int D_recv (isdn2_state state, char isUI, mblk_t * data);
static int D_backenable (isdn2_state state);

#ifdef CONFIG_DEBUG_ISDN
#define D_L1_up(a) deb_D_L1_up(__FILE__,__LINE__,(a))
static int deb_D_L1_up (const char *deb_file, unsigned int deb_line, isdn2_card card);
#define D_L1_down(a) deb_D_L1_down(__FILE__,__LINE__,(a))
static int deb_D_L1_down (const char *deb_file, unsigned int deb_line, isdn2_card card);
#else
static int D_L1_up (isdn2_card card);
static int D_L1_down (isdn2_card card);
#endif
#if 0
static void D_L1_not_up (isdn2_card card);
#endif
static void D_L1_re_up (isdn2_card card);

static void D_checkactive (isdn2_card card);
static void D_takedown (isdn2_card card);

static void poplist (queue_t * q, char flags);
static int isdn2_disconnect (isdn2_chan ch, uchar_t error);
static int pushlist (queue_t * q, mblk_t * mp, char flags);

#ifdef NEW_TIMEOUT
static int timer_sendcards;
#endif


/*
 * Take down this L2 connection
 */
static int
D_kill_one (isdn2_state state, char ind)
{
	isdn2_card card;
	int ms = splstr ();
	int ch;

#ifdef DO_MULTI_TEI
	ch = state->bchan;
#else
	ch = 0;
#endif
	if (isdn2_debug & 0x10)
		printf ("%sD_kill_one %x %p %d\n",KERN_DEBUG, state->SAPI, state, ind);
	
	/* Unhook the state info from the card's chain */

	card = state->card;
	if (card == NULL)			  /* should not happen */
		state = NULL;
	else if (card->state[ch] == state)
		card->state[ch] = state->next;
	else {
		isdn2_state nstate;

		for (nstate = card->state[ch]; nstate != NULL; nstate = nstate->next) {
			if (nstate->next == state) {
				nstate->next = state->next;
				break;
			}
		}
		if (nstate == NULL)
			state = NULL;
	}

	/* Tell X75 that the channel is dropped */

	if (state != NULL) {
		if(!(card->card->modes & CHM_INTELLIGENT))
			x75_changestate (&state->state, ind, 1);
		free(state);
	}
	splx (ms);
	D_checkactive (card);
	if (state != NULL)
		return 0;
	else
		return -ENOENT;
}


/* Take down / notify all of this card's L2 connections */

#ifdef CONFIG_DEBUG_ISDN
#define D_kill_all(a,b) deb_D_kill_all(__FILE__,__LINE__,a,b)
static int
deb_D_kill_all (const char *deb_file, unsigned int deb_line, isdn2_card card, char ind)
#else
static int
D_kill_all (isdn2_card card, char ind)
#endif
{
	int err;
	int i;
	int ch;

	if (isdn2_debug & 0x10) {
#ifdef CONFIG_DEBUG_ISDN
		printf ("%sD_kill_all %s:%d %d:%d\n",KERN_DEBUG, deb_file,deb_line, card->nr, ind);
#else
		printf ("%sD_kill_all %d:%d\n",KERN_DEBUG, card->nr, ind);
#endif
	}
	if (ind == 0) {
		for(ch=0;ch <= N_TEI; ch++) {
			while (card->state[ch] != NULL) {
				if ((err = D_kill_one (card->state[ch], 0)) != 0)
					return err;
			}
		}
	} else {
		for(ch=0;ch <= N_TEI; ch++) {
			isdn2_state state, nstate;

			for (state = card->state[ch]; state != NULL; state = nstate) {
				nstate = state->next;
				if(card->card->modes & CHM_INTELLIGENT)
					D_state(state,ind,0);
				else
					x75_changestate (&state->state, ind, 0);
			}
		}
	}
	/*
	 * Now hit all connections hanging off that card.
	 */
	if ((ind < IND_UP_FIRST) || (ind > IND_UP_LAST)) {
		for (i = 0; i < MAXCHAN; i++) {
			if (card->chan[i] != NULL && card->chan[i]->qptr != NULL) {
#if 0
				putctlerr (card->chan[i]->qptr, -ENXIO);
#else
				isdn2_disconnect (card->chan[i], 0xFF);
#endif
			}
		}
	}
	D_checkactive (card);
	return 0;
}

/*
 * Timeout procedure to kill a card if there's no connection on it for a few
 * seconds. Only called for nonintelligent cards.
 */
static void
D_takedown (isdn2_card card)
{
	if (card->timedown == 1) {
		card->timedown = 0;
		D_L1_down (card);
	}
	return;
}

/*
 * Check if there's a connection running on this card. If not, setup a timeout
 * to take the D channel and L1 down, in a few seconds.
 */
static void
D_checkactive (isdn2_card card)
{
	isdn2_state state;
	uchar_t ch;

	if(card == NULL)
		return;
#if 1							  /* def DO_L2_DOWN */
	if (card->status != C_up) 
		return;
	if (card->timedown == 1) {
		card->timedown = 0;
#ifdef OLD_TIMEOUT
		untimeout (D_takedown, card);
#else
		untimeout (card->timer_takedown);
#endif
	}
	for(ch=0; ch <= N_TEI; ch++) {
		for (state = card->state[ch]; state != NULL; state = state->next) {
			if (state->state.status != S_free && state->state.status != S_down)
				return;
		}
	}
	if (!(card->card->modes & CHM_INTELLIGENT) && !(card->timedown)) {
		card->timedown = 1;
#ifdef NEW_TIMEOUT
		card->timer_takedown =
#endif
			timeout ((void *)D_takedown, card, 2 * HZ);
	}
#endif							/* DO_L2_DOWN */
}

/*
 * Install a handler for connections with this SAPI.
 */
static int
D_register (isdn2_card card, uchar_t SAPI, uchar_t ch, uchar_t broadcast)
{
	isdn2_state state;
	int ms = splstr ();

	if (isdn2_debug & 0x40)
		printf ("%sD_register %d %x/%x\n",KERN_DEBUG,
			card->nr, SAPI,card->TEI[ch]);

	/* Check if this SAPI is in use. */
	if(card->card->modes & CHM_INTELLIGENT) {
		if(card->state[ch] != NULL)
			return -EEXIST;
	} else {
		for (state = card->state[ch]; state != NULL; state = state->next) {
			if (state->SAPI == SAPI) {
				splx (ms);
				if (isdn2_debug & 0x10)
					printf ("%sD_register: State table already present for %d:%x/%x\n",KERN_DEBUG,
						card->nr, SAPI, card->TEI[ch]);
				return -EEXIST;
			}
		}
	}
	state = malloc(sizeof(*state));
	if (state == NULL) {
		splx (ms);
		if (isdn2_debug & 0x10)
			printf ("%sD_register: No state table free for %d:%x/%x\n",KERN_DEBUG,
				card->nr, SAPI, card->TEI[ch]);
		return -ENOMEM;
	}
	bzero(state,sizeof(*state));
	/*
	 * Got a free state. Now setup and install the x75 handler. The protocol is
	 * actually Q.921 but the differences are less than minimal.
	 */
	state->next = card->state[ch];
	card->state[ch] = state;
#ifdef DO_MULTI_TEI
	state->bchan = ch;
#endif
	state->card = card;
	state->SAPI = SAPI;
	if(!(card->card->modes & CHM_INTELLIGENT)) {
		state->state.send = (P_data) & D_send;
		state->state.recv = (P_data) & D_recv;
		state->state.cansend = (P_candata) & D_cansend;
		state->state.canrecv = (P_candata) & D_canrecv;
		state->state.flush = NULL;
		state->state.state = (P_state) & D_state;
		state->state.backenable = (P_backenable) & D_backenable;
		state->state.ref = state;
		state->state.debugnr = SAPI;
		state->state.broadcast = (broadcast != 0);
		state->state.RUN_T1 = 15; /* Yes this is slow. This is intentional. Grrr. */
		state->state.RUN_T3 = 150; /* Ditto. */

		x75_initconn (&state->state);
		if(isdn2_debug & 0x8000)
			state->state.debug = 0xFFF7;
		state->state.wide = 1;
		state->state.offset = 2;
	}

	splx (ms);
	D_checkactive (card);
	return 0;
}



/*
 * State change for this connection. Called by X75.
 */
static int
sendstate (isdn2_card card, uchar_t ch, uchar_t SAPI, uchar_t ind, short add)
{
	isdn23_hdr hdr;
	mblk_t *mb;

	if (isdn2_debug & 0x40)
		printf ("%ssendstate %d %x:%x\n",KERN_DEBUG, card->nr, ind, add);
	if (ind == MDL_ERROR_IND) {
		if(add & (ERR_C | ERR_D | ERR_G /* | ERR_H */ | ERR_I )) {
			printf("%s\nISDN Fatal Error, TEI cleared\n",KERN_DEBUG);
			card->TEI[ch] = TEI_BROADCAST;
		}
	}

	mb = allocb (sizeof (struct _isdn23_hdr), BPRI_HI);

	if (mb == NULL) {
		if (isdn2_debug & 0x10)
			printf ("%ssendstate: nomem to send ind %x:%x for %d:%x/%x\n",KERN_DEBUG, ind, add,
					card->nr, SAPI, card->TEI[ch]);
		return -ENOMEM;
	}
	hdr = ((isdn23_hdr) mb->b_wptr)++;
	switch (ind) {
	case DL_ESTABLISH_IND:
	case DL_ESTABLISH_CONF:
		hdr->key = HDR_OPENPROT;
		hdr->seqnum = hdrseq; hdrseq += 2;
		hdr->hdr_openprot.card = card->nr;
		hdr->hdr_openprot.SAPI = SAPI;
#ifdef DO_MULTI_TEI
		hdr->hdr_openprot.bchan = ch;
#endif
		hdr->hdr_openprot.ind = ind;
		break;
	case DL_RELEASE_IND:
	case DL_RELEASE_CONF:
		hdr->key = HDR_CLOSEPROT;
		hdr->seqnum = hdrseq; hdrseq += 2;
		hdr->hdr_closeprot.card = card->nr;
		hdr->hdr_closeprot.SAPI = SAPI;
		hdr->hdr_closeprot.ind = ind;
#ifdef DO_MULTI_TEI
		hdr->hdr_closeprot.bchan = ch;
#endif
		break;
	default:
		hdr->key = HDR_NOTIFY;
		hdr->seqnum = hdrseq; hdrseq += 2;
		hdr->hdr_notify.card = card->nr;
		hdr->hdr_notify.SAPI = SAPI;
#ifdef DO_MULTI_TEI
		hdr->hdr_notify.bchan = ch;
#endif
		hdr->hdr_notify.ind = ind;
		hdr->hdr_notify.add = add;
		break;
	}
	if (isdn_chan.qptr != NULL) {
		if(isdn2_debug & 0x2000) logh_printmsg (NULL, "Up", mb);
		if(canput(isdn_chan.qptr->q_next))
			putnext (isdn_chan.qptr, mb);
		else {
			freemsg(mb);
			return -ENOSPC;
		}
	} else {
		freemsg (mb);
		return -ENXIO;
	}
	return 0;
}


/*
 * State change for this connection. Called by X75.
 */
static int
D_state (isdn2_state state, uchar_t ind, short add)
{
	uchar_t ch;
	int err;

#ifdef DO_MULTI_TEI
	ch = state->bchan;
#else
	ch = 0;
#endif
	err = sendstate(state->card,ch,state->SAPI,ind,add);

	D_checkactive (state->card);
	return err;
}

void
isdn2_chstate (struct _isdn1_card *card, uchar_t ind, short add)
{
	isdn2_card ctl;

	ctl = (isdn2_card) card->ctl;
	if (ctl == NULL)
		return;
	sendstate(ctl,0,0,ind,add);
}

static const char *
statename(enum C_state status) 
{
	switch (status) {
	case C_lock_up:
		return "C_lock_up";
		break;
	case C_up:
		return "C_up";
		break;
	case C_down:
		return "C_down";
		break;
	case C_wont_up:
		return "C_wont_up";
		break;
	case C_wont_down:
		return "C_wont_down";
		break;
	case C_await_up:
		return "C_await_up";
		break;
	case C_await_down:
		return "C_await_down";
		break;
	default:
		return "C_unknown";
		break;
	}
}

#ifdef CONFIG_DEBUG_ISDN
#define set_card_status(a,b) deb_set_card_status(__FILE__,__LINE__,a,b)
#endif
/*
 * Set card status, logging
 */
static void
#ifdef CONFIG_DEBUG_ISDN
deb_set_card_status (const char *deb_file, unsigned int deb_line, isdn2_card card, enum C_state status)
#else
set_card_status (isdn2_card card, enum C_state status)
#endif
{
	char cid[5];

	*(unsigned long *)cid = card->id;
	cid[4]='\0';

	if(isdn2_debug & 0x10) {
		const char *sold = statename(card->status);
		const char *snew = statename(status);
#ifdef CONFIG_DEBUG_ISDN
		printf ("%s!!! %4s: %s -> %s at %s %d\n",KERN_DEBUG, cid, sold,snew, deb_file,deb_line);
#else
		printf ("%s!!! %4s: %s -> %s\n",KERN_DEBUG, cid, sold,snew);
#endif
	}
	if (status == card->status) 
		return;
	switch(card->status) {
	case C_await_up:
#if 0
		if(card->timeup) {
			card->timeup = 0;
#ifdef OLD_TIMEOUT
			untimeout (D_L1_not_up, crd);
#else
			untimeout (card->timer_not_up);
#endif
		}
#endif
		break;
	case C_wont_up:
		if(card->timeup) {
			card->timeup = 0;
#ifdef OLD_TIMEOUT
			untimeout (D_L1_re_up, crd);
#else
			untimeout (card->timer_not_up);
#endif
		}
		break;
	default:;
	}
	card->status = status;
	if(card->status != C_up) {
		if (card->timedown == 1) {
			card->timedown = 0;
#ifdef OLD_TIMEOUT
			untimeout (D_takedown, card);
#else
			untimeout (card->timer_takedown);
#endif
		}
	}
	switch(card->status) {
	case C_await_up:
#if 0
		card->timeup = 1;
#ifdef NEW_TIMEOUT
		card->timer_not_up =
#endif
			timeout ((void *)D_L1_not_up, card, (HZ*3)/2);
#endif
		break;
	case C_wont_up:
		card->timeup = 1;
#ifdef NEW_TIMEOUT
		card->timer_not_up =
#endif
			timeout ((void *)D_L1_re_up, card, HZ * 30);
		break;
	default:;
	}
}

static void
D_L1_re_up(isdn2_card card)
{
	if(!card->timeup)
		return;
	card->timeup = 0;
	set_card_status(card,C_down);
	if(isdn_chan.qptr != NULL)
		D_L1_up(card);
	else
		card->offline = 0;
}

#if 0
/*
 * Timeout procedure to notify upper layers when L1 doesn't come up. The usual
 * reason for this is that somebody pulled the ISDN cable.
 */
static void
D_L1_not_up (isdn2_card card)
{
	mblk_t *mb;
	isdn23_hdr hdr;

	if(!card->timeup)
		return;
	card->timeup = 0;
	if(card->status != C_await_up) {
		printf("%sD_L1_not_up called at wrong time! S %d\n",KERN_DEBUG,card->status);
		return;
	}
	if (isdn2_debug & 0x10)
		printf ("%sisdn: Card %d: Disconnected??\n",KERN_DEBUG, card->nr);
	if(!card->offline)
		D_kill_all (card, PH_DISCONNECT_IND);
	set_card_status (card, C_wont_up);
	D_L1_down (card);

	if(!card->offline) {
		card->offline = 1;
		mb = allocb (sizeof (struct _isdn23_hdr), BPRI_HI);
		if (mb == NULL) {
			if (isdn2_debug & 0x10)
				printf ("%sD_L1_not_up: no memory\n",KERN_DEBUG);
			return;
		}
		hdr = ((isdn23_hdr) mb->b_wptr)++;
		hdr->key = HDR_NOCARD;
		hdr->seqnum = hdrseq; hdrseq += 2;
		hdr->hdr_nocard.card = card->nr;
		if (isdn_chan.qptr != NULL) {
			if(isdn2_debug & 0x2000) logh_printmsg (NULL, "Up", mb);
			if(canput(isdn_chan.qptr->q_next)) {
				putnext (isdn_chan.qptr, mb);
				return;
			}
		} 
		freemsg (mb);
	}
}
#endif


/*
 * Upper level wants to send data -- (re)start L1.
 */
static int
#ifdef CONFIG_DEBUG_ISDN
deb_D_L1_up (const char *deb_file, unsigned int deb_line, isdn2_card card)
#else
D_L1_up (isdn2_card card)
#endif
{
	if (isdn2_debug & 0x48)
		printf ("%sD_L1_up %d: S was %d\n",KERN_DEBUG, card->nr,card->status);
	if (card->card == NULL) {
		if (isdn2_debug & 0x10)
			printf ("%sD_L1_up: Card %d not registered\n",KERN_DEBUG, card->nr);
		return -ENXIO;
	}
	switch (card->status) {
	case C_await_up:
#ifdef CONFIG_DEBUG_ISDN
		deb_set_card_status (deb_file,deb_line, card, C_await_up);  /* Re-set the timeout */
#else
		set_card_status (card, C_await_up);  /* Re-set the timeout */
#endif
		break;
	case C_up:
	case C_lock_up:
		break;
	case C_wont_up:
		return -EIO;
	case C_await_down:
	case C_down:
	case C_wont_down:
		if(!(card->card->modes & CHM_INTELLIGENT)) {
#ifdef CONFIG_DEBUG_ISDN
			deb_set_card_status (deb_file,deb_line, card, C_await_up);
#else
			set_card_status (card, C_await_up);
#endif
			(*card->card->ch_mode) (card->card, 0, M_ON, 0);
		}
		break;
	}
	D_checkactive (card);
	return 0;
}

/*
 * Take the card down. Called via timeout or when there are no active
 * connections for a few seconds.
 */
static int
#ifdef CONFIG_DEBUG_ISDN
deb_D_L1_down (const char *deb_file, unsigned int deb_line, isdn2_card card)
#else
D_L1_down (isdn2_card card)
#endif
{
	if (isdn2_debug & 0x40)
		printf ("%sD_L1_down %d\n",KERN_DEBUG, card->nr);
	if (card->card == NULL) {
		if (isdn2_debug & 0x10)
			printf ("%sD_L1_down: Card %d not registered\n",KERN_DEBUG, card->nr);
		return -ENXIO;
	}
	if (isdn2_debug & 0x10)
		printf ("%sD_L1_Down: State was %d\n",KERN_DEBUG, card->status);
	switch (card->status) {
	case C_await_down:
	case C_down:
	case C_wont_up:
		(*card->card->ch_mode) (card->card, 0, M_OFF, 0);
		return 0;
	case C_wont_down:
	case C_lock_up:
		break;
	case C_await_up:
#ifdef CONFIG_DEBUG_ISDN
		deb_set_card_status (deb_file,deb_line, card, C_down);
#else
		set_card_status (card, C_down);
#endif
		if (isdn_chan.qptr != NULL)
			(*card->card->ch_mode) (card->card, 0, M_STANDBY, 1);
		else
			(*card->card->ch_mode) (card->card, 0, M_OFF, 0);
		break;
	case C_up:
		if (isdn_chan.qptr != NULL) {
			if(card->card->modes & CHM_INTELLIGENT) {
#ifdef CONFIG_DEBUG_ISDN
				deb_set_card_status(deb_file,deb_line, card, C_down);
#else
				set_card_status(card, C_down);
#endif
			} else {
#ifdef CONFIG_DEBUG_ISDN
				deb_set_card_status (deb_file,deb_line, card, C_await_down);
#else
				set_card_status (card, C_await_down);
#endif
			}
			(*card->card->ch_mode) (card->card, 0, M_STANDBY, 1);
		} else {
#ifdef CONFIG_DEBUG_ISDN
			deb_set_card_status (deb_file,deb_line, card, C_down);
#else
			set_card_status (card, C_down);
#endif
			(*card->card->ch_mode) (card->card, 0, M_OFF, 0);
		}
		break;
	}
	D_checkactive (card);
	return 0;
}

/*
 * Data has been sent; enable the downstream queue. Called from X75.
 */
static int
D_backenable (isdn2_state state)
{
	if (isdn2_debug & 0x40)
		printf ("%sD_backenable %p\n",KERN_DEBUG, state);
	if (isdn_chan.qptr != NULL)
		qenable (WR (isdn_chan.qptr));
	return 0;
}

/*
 * Check if there's room on the upstream queue. Called from X75.
 */
static int
D_canrecv (isdn2_state state)
{
	if (isdn2_debug & 0x40)
		printf ("%sD_canrecv %d ",KERN_DEBUG, state->card->nr);
	if (isdn_chan.qptr == 0 || isdn_chan.qptr->q_next == NULL) {
#ifdef CONFIG_DEBUG_ISDN
		printf("%s-NoStream",KERN_DEBUG);
#endif
		return 0;
	} else
		return (canput (isdn_chan.qptr->q_next));
}

/*
 * Send data upstream. Called from X75.
 */
static int
D_recv (isdn2_state state, char isUI, mblk_t * mb)
{
	mblk_t *mb2;
	isdn23_hdr hdr;

#ifdef CONFIG_DEBUG_STREAMS
	if(msgdsize(mb) < 0)
		return 0;
#endif
	if (isdn2_debug & 0x40)
		printf ("%sD_recv %d %d %d\n",KERN_DEBUG, state->card->nr, msgdsize(mb), isUI);
	mb2 = allocb (sizeof (struct _isdn23_hdr), BPRI_MED);

	if (mb2 == NULL) {
		if (isdn2_debug & 0x10)
			printf ("%sD_recv: no mblk for header\n",KERN_DEBUG);
		return -ENOMEM;
	}
	hdr = ((isdn23_hdr) mb2->b_wptr)++;
	if (isUI) {
		hdr->key = HDR_UIDATA;
		hdr->seqnum = hdrseq; hdrseq += 2;
		hdr->hdr_uidata.card = state->card->nr;
		hdr->hdr_uidata.SAPI = state->SAPI;
#ifdef DO_MULTI_TEI
		hdr->hdr_uidata.bchan = state->bchan;
#endif
		hdr->hdr_uidata.len = dsize (mb);
		hdr->hdr_uidata.broadcast = (isUI & 2);
	} else {
		hdr->key = HDR_DATA;
		hdr->seqnum = hdrseq; hdrseq += 2;
		hdr->hdr_data.card = state->card->nr;
		hdr->hdr_data.SAPI = state->SAPI;
#ifdef DO_MULTI_TEI
		hdr->hdr_data.bchan = state->bchan;
#endif
		hdr->hdr_data.len = dsize (mb);
	}
	linkb (mb2, mb);
	if (isdn_chan.qptr != NULL) {
		if(isdn2_debug & 0x2000) logh_printmsg (NULL, "Up", mb2);
		if(canput(isdn_chan.qptr->q_next)) {
			putnext (isdn_chan.qptr, mb2);
			return 0;
		}
		freeb(mb2);
		return -ENOSPC;
	}
	freeb (mb2);
	return -ENXIO;
}

/*
 * Send data downstream. Prepend SAPI and TEI. Called from X75.
 */

static int
D_send (isdn2_state state, char cmd, mblk_t * mb)
	/* cmd=2: send as broadcast */
{
	struct _isdn1_card *crd;
	int err;
	uchar_t ch;

#ifdef CONFIG_DEBUG_STREAMS
	if(msgdsize(mb) < 0)
		return 0;
#endif
	if (isdn2_debug & 0x40)
		printf ("%sD_send %d %d %d %d\n",KERN_DEBUG, state->card->nr, state->card->status, msgdsize(mb), cmd);
	if ((crd = state->card->card) == NULL) {
		if (isdn2_debug & 0x10)
			printf ("%s -- no card\n",KERN_DEBUG);
		return -ENXIO;
	}
#ifdef DO_MULTI_TEI
	ch=state->bchan;
#else
	ch=0;
#endif
	if (state->card->status != C_up && state->card->status != C_lock_up) {
		if (isdn2_debug & 0x10)
			printf ("%sD_send: %d:%x/%x sending, state %d\n",KERN_DEBUG, state->card->nr,
					state->SAPI, state->card->TEI[ch], state->card->status);
		if (isdn2_debug & 0x10)
			printf ("%s -- card down 1\n",KERN_DEBUG);
		if ((err = D_L1_up (state->card)) == 0)
			err = -EAGAIN;
		return err;
	}
	if (!(*crd->cansend) (crd, 0)) {
		if (isdn2_debug & 0x10)
			printf ("%s -- can't send\n",KERN_DEBUG);
		return -EAGAIN;
	}

	if (!(crd->modes & CHM_INTELLIGENT) && !(cmd & 2)) {	/* TEI necessary? */
		if (state->card->TEI[ch] == TEI_BROADCAST) {	/* No TEI yet. Alert L3. */
			mblk_t *mp;
			isdn23_hdr hdr;
	
			if (isdn2_debug & 0x10)
				printf ("%sD_send: %d:%x/NO_TEI\n",KERN_DEBUG, state->card->nr, state->SAPI);
	
			if ((mp = allocb (sizeof (struct _isdn23_hdr), BPRI_MED)) == NULL) {
				if (isdn2_debug & 0x10)
					printf ("%sD_recv: no mblk for TEIreq\n",KERN_DEBUG);
				return -EAGAIN;
			}
			hdr = ((isdn23_hdr) mp->b_wptr)++;
			hdr->key = HDR_TEI;
			hdr->seqnum = hdrseq; hdrseq += 2;
			hdr->hdr_tei.card = state->card->nr;
			hdr->hdr_tei.TEI = state->card->TEI[ch];
#ifdef DO_MULTI_TEI
			hdr->hdr_tei.bchan = ch;
#endif
			if (isdn_chan.qptr != NULL) {
				if(isdn2_debug & 0x2000) logh_printmsg (NULL, "Up", mp);
				if(!canput(isdn_chan.qptr->q_next)) {
					freeb(mp);
					return -ENOSPC;
				}
				putnext (isdn_chan.qptr, mp);
				state->card->TEI[ch] = TEI_REQUESTED;
			} else {
				freeb (mp);
				return -ENXIO;
			}
			return -EAGAIN;
		} else if (state->card->TEI[ch] == TEI_REQUESTED)
			return -EAGAIN;
	}
	/*
	 * If there's room in front, don't bother with a new mblk.
	 */
	if(crd->modes & CHM_INTELLIGENT)
		err = (*crd->send)(crd,0,mb);
	else if (DATA_START(mb) + 2 <= mb->b_rptr && DATA_REFS(mb) == 1) {
		*--mb->b_rptr = (((cmd & 2) ? TEI_BROADCAST : state->card->TEI[ch]) << 1) | 1;
		*--mb->b_rptr = (state->SAPI << 2) | ((cmd & 1) ? 0 : 2);
		if(isdn2_log & 0x20) {
			printf ("%s*** %d", KERN_DEBUG,state->card->nr);
			log_printmsg (NULL, " Send", mb, KERN_DEBUG);
		}
		if(state->card->flags & HDR_CARD_DEBUG) {
			isdn23_hdr hdr;
			mblk_t *m1 = allocb(sizeof(*hdr),BPRI_MED);
			mblk_t *m2 = dupmsg(mb);

			if (m1 == NULL || m2 == NULL) {
				if(m1 != NULL)
					freemsg(m1);
				if(m2 != NULL)
					freemsg(m2);
			} else {
				hdr = ((isdn23_hdr) m1->b_wptr)++;
				hdr->key = HDR_RAWDATA;
				hdr->seqnum = hdrseq; hdrseq += 2;
				hdr->hdr_rawdata.card = state->card->nr;
				hdr->hdr_rawdata.dchan = 1;
				hdr->hdr_rawdata.len = dsize (m2);
				hdr->hdr_rawdata.flags = 1;
				linkb (m1, m2);
				if(canput(isdn_chan.qptr->q_next)) 
					putnext (isdn_chan.qptr, m1);
				else
					freemsg(m1);
			}
		}
		if ((err = (*crd->send) (crd, 0, mb)) != 0)
			mb->b_rptr += 2;
	} else {
		mblk_t *mb2 = allocb (2, BPRI_HI);

		if (mb2 == NULL) {
			if (isdn2_debug & 0x10)
				printf ("%sD_send: No mem to send %s to %d:%x/%x\n",KERN_DEBUG, cmd ? "Cmd" : "Resp", state->card->nr, state->SAPI, state->card->TEI[ch]);
			return -ENOMEM;
		}
		*mb2->b_wptr++ = (state->SAPI << 2) | ((cmd & 1) ? 0 : 2);
		*mb2->b_wptr++ = (((cmd & 2) ? TEI_BROADCAST : state->card->TEI[ch]) << 1) | 1;
		linkb (mb2, mb);
		if(isdn2_log & 0x20) {
			printf ("%s*** %d", KERN_DEBUG,state->card->nr);
			log_printmsg (NULL, " Send", mb2, KERN_DEBUG);
		}
		if(state->card->flags & HDR_CARD_DEBUG) {
			isdn23_hdr hdr;
			mblk_t *m1 = allocb(sizeof(*hdr),BPRI_MED);
			mblk_t *m2 = dupmsg(mb2);

			if (m1 == NULL || m2 == NULL) {
				if(m1 != NULL)
					freemsg(m1);
				if(m2 != NULL)
					freemsg(m2);
			} else {
				hdr = ((isdn23_hdr) m1->b_wptr)++;
				hdr->key = HDR_RAWDATA;
				hdr->seqnum = hdrseq; hdrseq += 2;
				hdr->hdr_rawdata.card = state->card->nr;
				hdr->hdr_rawdata.dchan = 1;
				hdr->hdr_rawdata.len = dsize (m2);
				hdr->hdr_rawdata.flags = 1;
				linkb (m1, m2);
				if(canput(isdn_chan.qptr->q_next)) 
					putnext (isdn_chan.qptr, m1);
				else
					freemsg(m1);
			}
		}
		if ((err = (*crd->send) (crd, 0, mb2)) != 0)
			freeb (mb2);
	}
	D_checkactive(state->card);
	if(err != 0)
		printf("%sD_Send Err %d ",KERN_DEBUG,err);
	return err;
}

/*
 * Check if there's room on the downstream queue. Called from X75.
 */
static int
D_cansend (isdn2_state state)
{
	struct _isdn1_card *crd;

	if (isdn2_debug & 0x40)
		printf ("%sD_cansend %d %d\n", KERN_DEBUG,state->card->nr, state->card->status);
	if ((crd = state->card->card) == NULL) {
		if (isdn2_debug & 0x10)
			printf ("%s -- card NULL\n",KERN_DEBUG);
		return 0;
	}
	if(!(state->card->card->modes & CHM_INTELLIGENT)) {
		if (state->card->status != C_up && state->card->status != C_lock_up) {
			if (isdn2_debug & 0x10)
				printf ("%s -- card down 2\n",KERN_DEBUG);
			(void) D_L1_up (state->card);
			return 0;				  /* Yet. */
		}
	}
	if (!(*crd->cansend) (crd, 0)) {
		if (isdn2_debug & 0x10)
			printf ("%s -- card busy\n",KERN_DEBUG);
		return 0;
	}
	return 1;
}

/*
 * Look for the connection.
 */
static isdn2_state
D__findstate (isdn2_card card, uchar_t SAPI, uchar_t ch)
{
	isdn2_state state = card->state[ch];

	if(card->card->modes & CHM_INTELLIGENT)
		return state;
	if(isdn2_debug&0x900) printf("%sD_findstate %d:%x/%x:",KERN_DEBUG,card->nr,SAPI,card->TEI[ch]);
	while (state != NULL) {
		if(isdn2_debug&0x900) printf("%s_%x",KERN_DEBUG,state->SAPI);
		if (state->SAPI == SAPI)
			break;
		state = state->next;
	}
	if (state == NULL) {
		if ((isdn2_debug&0x910) == 0x10)
			printf("%sD_findstate %d:%x/%x:",KERN_DEBUG,card->nr,SAPI,card->TEI[ch]);
		if (isdn2_debug & 0x900)
			printf ("%s not found\n",KERN_DEBUG);
	}
	return state;
}


/**
 ** ISDN card handling
 **/

/* --> isdn_12.h */
int
isdn2_register (struct _isdn1_card *card, long id)
{
	/* int cd; */
	int ms = splstr ();
	isdn2_card crd;
	uchar_t ch;
	uchar_t nr, found_nr;

	if (isdn2_debug & 0x10)
		printf ("%sisdn2_register %p %lx\n",KERN_DEBUG, card, id);
	nr = 1;
	do {
		nr++;
		found_nr = 0;
		for (crd = isdn_card; crd != NULL; crd = crd->next) {
			if (crd->id == id) {
				splx (ms);
				if (isdn2_debug & 0x10)
					printf ("%sISDN register: ID %lx already registered as %d\n",KERN_DEBUG, id, crd->nr);
				return -EEXIST;
			}
			if (crd->nr == nr) {
				found_nr++;
				break;
			}
		}
	} while(found_nr);
	crd = malloc(sizeof(*crd));
	if (crd == NULL) {
		if (isdn2_debug & 0x10)
			printf ("%sisdn2_register: no free card store\n",KERN_DEBUG);
		splx (ms);
		return -EBUSY;
	}
	bzero(crd,sizeof(*crd));
	crd->next = isdn_card;
	isdn_card = crd;

	crd->card = card;
	card->ctl = crd;
	if (card->nr_chans > MAXCHAN - MAX_D)
		card->nr_chans = MAXCHAN - MAX_D;
	set_card_status (crd, C_down);
	crd->id = id;
	crd->nr = nr;
	for(ch=0;ch <= N_TEI; ch++)
		crd->TEI[ch] = TEI_BROADCAST;
	(*card->ch_mode) (card, 0, M_OFF, 1);

	splx (ms);

	isdn2_sendcard (crd);
	return 0;
}


/* --> isdn_12.h */
int
isdn2_unregister (struct _isdn1_card *card)
{
	int ms = splstr ();
	mblk_t *mb;
	isdn2_card crd;
	isdn23_hdr hdr;
	isdn2_card *pcrd = &isdn_card;

	if (isdn2_debug & 0x10)
		printf ("%sisdn2_unregister %p, 2 is %p\n",KERN_DEBUG, card,card->ctl);
	crd = (isdn2_card) card->ctl;
	if (crd == NULL) {
		splx (ms);
		if (isdn2_debug & 0x10)
			printf ("%sisdn2_unregister: card not registered\n",KERN_DEBUG);
		return -ENODEV;
	}
	while(*pcrd != NULL && *pcrd != crd)
		pcrd = &(*pcrd)->next;
	if (*pcrd == NULL) {
		splx (ms);
		printf ("%sisdn2_unregister: card chain broken\n",KERN_DEBUG);
		return -EIO;
	}
	set_card_status(crd,C_down);
	D_kill_all (crd, 0);

	if(!crd->offline) {
		mb = allocb (sizeof (struct _isdn23_hdr), BPRI_HI);

		if (mb == NULL) {
			if (isdn2_debug & 0x10)
				printf ("%sisdn2_unregister: no memory\n",KERN_DEBUG);
			return -ENOMEM;
		}
		hdr = ((isdn23_hdr) mb->b_wptr)++;
		hdr->key = HDR_NOCARD;
		hdr->seqnum = hdrseq; hdrseq += 2;
		hdr->hdr_nocard.card = crd->nr;
		if (isdn_chan.qptr != NULL) {
			if(isdn2_debug & 0x2000) logh_printmsg (NULL, "Up", mb);
			if(canput(isdn_chan.qptr->q_next))
				putnext (isdn_chan.qptr, mb);
			else
				freemsg(mb);
		} else
			freemsg (mb);
	}
	*pcrd = crd->next;

	splx (ms);
	free(crd);

	return 0;
}

/* --> isdn_12.h */
void
isdn2_new_state (struct _isdn1_card *card, char state)
{
	isdn23_hdr hdr;
	mblk_t *mb;
	isdn2_card ctl;

	ctl = (isdn2_card) card->ctl;
	if (ctl == NULL)
		return /* ENXIO */ ;

	if (isdn2_debug & 0x40)
		printf ("%sisdn2_new_state %d %d %d\n",KERN_DEBUG, ctl->nr, ctl->status, state);

	mb = allocb (sizeof (struct _isdn23_hdr), BPRI_HI);

	if (mb == NULL) {
		if (state != 1)
			D_kill_all (ctl, state ? PH_DISCONNECT_IND : PH_DEACTIVATE_IND);
		if (isdn2_debug & 0x10)
			printf ("%sNew_State: No hdr mem for %d\n",KERN_DEBUG, ctl->nr);
		return /* ENOMEM */ ;
	}
	hdr = ((isdn23_hdr) mb->b_wptr)++;
	hdr->key = HDR_NOTIFY;
	hdr->seqnum = hdrseq; hdrseq += 2;
	hdr->hdr_notify.card = ctl->nr;
	hdr->hdr_notify.SAPI = SAPI_INVALID;		/* all */
	if (state == 2) { /* Card is bouncing */
		switch (ctl->status) {
		case C_lock_up:
			freemsg (mb);
			return;
		case C_await_down:
			hdr->hdr_notify.ind = PH_DISCONNECT_IND;
			set_card_status (ctl, C_down);
			break;
		case C_up:
		case C_await_up:
			(*card->ch_mode) (card, 0, M_STANDBY, 1);
			set_card_status (ctl, C_wont_up);
			hdr->hdr_notify.ind = PH_DISCONNECT_IND;
			break;
		case C_wont_down:
			set_card_status (ctl, C_down);
			/* FALL THRU */
		case C_down:
			hdr->hdr_notify.ind = PH_DISCONNECT_IND;
			break;
		case C_wont_up:
			freemsg(mb);
			return;
		}
	} else if (state == 0) {			  /* Card is down */
		switch (ctl->status) {
		case C_lock_up:
		case C_await_up:		  /* Might be a delayed response from taking it
								   * down. Do nothing for now. */
		case C_down:
			freemsg (mb);
			return;
		case C_await_down:
			hdr->hdr_notify.ind = PH_DEACTIVATE_CONF;
			set_card_status (ctl, C_down);
			break;
		case C_up:
			(*card->ch_mode) (card, 0, M_STANDBY, 1);
			/* FALL THRU */
		case C_wont_down:
			set_card_status (ctl, C_down);
			/* FALL THRU */
		case C_wont_up:
			hdr->hdr_notify.ind = PH_DEACTIVATE_IND;
			break;
		}
	} else {
		switch (ctl->status) {
		case C_up:
			freemsg (mb);
			return;
		case C_lock_up:
			set_card_status (ctl, C_up);
			hdr->hdr_notify.ind = PH_ACTIVATE_IND;
			break;
		case C_await_up:
			set_card_status (ctl, C_up);
			(*card->ch_mode) (card, 0, M_ON, 0);
			hdr->hdr_notify.ind = PH_ACTIVATE_CONF;
			if(ctl->offline && !isdn2_notsent)
				isdn2_sendcard(ctl);
			break;
		case C_wont_down:
			freemsg (mb);
			mb = NULL;
			break;
		case C_down:
			if(ctl->offline && !isdn2_notsent)
				isdn2_sendcard(ctl);
			/* FALL THRU */
		case C_await_down:		  /* Oops, bus doesn't want to go down.
								   * Continue to listen for incoming frames. */
			if(ctl->card->modes & CHM_INTELLIGENT)
				set_card_status (ctl, C_up);
			else
				set_card_status (ctl, C_wont_down);
			/* (*card->ch_mode) (card, 0, M_ON, 1); */
			hdr->hdr_notify.ind = PH_ACTIVATE_NOTE;
			break;
		case C_wont_up:
			if(!isdn2_notsent)
				isdn2_sendcard(ctl);
			set_card_status (ctl, C_up);
			hdr->hdr_notify.ind = PH_ACTIVATE_IND;
			break;
		}
	}
	if(mb != NULL) {
		D_kill_all (ctl, hdr->hdr_notify.ind);
		hdr->hdr_notify.add = 0;
		if (isdn_chan.qptr != NULL) {
			qenable (WR (isdn_chan.qptr));
			if(isdn2_debug & 0x2000) logh_printmsg (NULL, "Up", mb);
			if(canput(isdn_chan.qptr->q_next))
				putnext (isdn_chan.qptr, mb);
			else
				freemsg(mb);
			D_checkactive (ctl);
		} else
			freemsg (mb);
	}

	return /* 0 */ ;
}

static int
do_chprot (isdn2_card ctl, short channel, mblk_t * proto, int flags)
{
	int err = 0;

	if (isdn2_debug & 0x10)
		printf ("%sdo_chprot %d %d 0%o\n",KERN_DEBUG, ctl ? ctl->nr : -1, channel, flags);
	if (ctl == NULL || (channel == 0 && isdn_chan.qptr == NULL))
		return -ENXIO;
	if(flags & CHP_TOCARD) {
		flags &=~ CHP_TOCARD;
		if((ctl->card != NULL) && (ctl->card->ch_prot != NULL))
			return (*ctl->card->ch_prot)(ctl->card,channel,proto,flags);
	}
	if(flags & CHP_MODLIST) {
		isdn2_chan ch = ctl->chan[channel];

		if (ch == NULL || ch->qptr == NULL)
			return -ENXIO;
		if((flags & (PUSH_BEFORE|PUSH_UPDATE)) == PUSH_BEFORE)
			poplist (ch->qptr, PUSH_BEFORE);
		if(!(flags & PUSH_UPDATE))
			err = pushlist (ch->qptr, proto, flags);
		if(err == 0)
			freemsg(proto);
		return err;
	} else if(flags & CHP_FROMSTACK) { /* to the master */
		isdn23_hdr hdr;
		mblk_t *mb;

		mb = allocb (sizeof (struct _isdn23_hdr), BPRI_HI);

		if (mb == NULL) {
			if (isdn2_debug & 0x10)
				printf ("%sisdn2_chprot for %d: No rawhdr mem\n",KERN_DEBUG, ctl->nr);
			return -ENOMEM;
		}
		hdr = ((isdn23_hdr) mb->b_wptr)++;
		hdr->key = HDR_PROTOCMD;
		hdr->seqnum = hdrseq; hdrseq += 2;
		if (channel == 0)		  /* D channel is always ready... */
			hdr->hdr_protocmd.minor = 0;
		else {
			isdn2_chan ch = ctl->chan[channel];

			if (ch == NULL || ch->qptr == NULL)
				hdr->hdr_protocmd.minor = 0;
			else 
				hdr->hdr_protocmd.minor = ch->dev;
		}
		hdr->hdr_protocmd.card = ctl->nr;
		hdr->hdr_protocmd.channel = channel;
		hdr->hdr_protocmd.len = dsize (proto);
		DATA_TYPE(proto) = MSG_DATA;
		linkb (mb, proto);
		if (isdn_chan.qptr != NULL) {
			if(isdn2_debug & 0x2000) logh_printmsg (NULL, "Up", mb);
			if(canput(isdn_chan.qptr->q_next)) {
				putnext (isdn_chan.qptr, mb);
				err = 0;
			} else 
				err = -ENXIO;
		} else 
			err = -EAGAIN;
		return err;
	} else { /* to the stack */
		isdn2_chan ch = ctl->chan[channel];

		if (ch == NULL || ch->qptr == NULL) {
			if(isdn2_debug & 0x10)printf ("%sisdn2_chprot for %d: No qptr\n",KERN_DEBUG, ctl->nr);
			return -ENXIO;
		}
		DATA_TYPE(proto) = MSG_PROTO;
		putq (ch->qptr, proto);
		return 0;
	}
}

/* --> isdn_12.h */
extern int
isdn2_chprot (struct _isdn1_card *card, short channel, mblk_t * proto, int flags)
{
	isdn2_card ctl;

	ctl = (isdn2_card) card->ctl;
	if (ctl == NULL || (channel == 0 && isdn_chan.qptr == NULL))
		return -ENXIO;
	return do_chprot(ctl,channel,proto,flags &~ CHP_TOCARD /* precaution against loops */ );
}

/* --> isdn_12.h */
int
isdn2_canrecv (struct _isdn1_card *card, short channel)
{
	isdn2_card ctl;

	ctl = (isdn2_card) card->ctl;
	if (ctl == NULL)
		return 1;				  /* will get flushed in isdn2_recv(); avoid
								   * clogging the card here */
	else if (channel == 0) {	  /* D channel is always ready... */
		if(isdn_chan.qptr == NULL)
			return 0;
		return canput(isdn_chan.qptr->q_next);
		/* XXX: For dumb cards, I should ask X.75, but at this point I
		        don't know the SAPI. */
	} else {
		isdn2_chan ch = ctl->chan[channel];

		if (ch == NULL || ch->qptr == NULL)
			return 1;			  /* avoid clogging the card; will get flushed
								   * in isdn2_recv() */
		return canput (ch->qptr);
	}
}

/* --> isdn_12.h */
int
isdn2_recv (struct _isdn1_card *card, short channel, mblk_t * data)
{
	int err = 0;
	isdn2_card ctl;
	uchar_t ch;

#ifdef CONFIG_DEBUG_STREAMS
	if(msgdsize(data) < 0)
		return 0;
#endif
	ctl = (isdn2_card) card->ctl;
	if (ctl == NULL || (channel == 0 && isdn_chan.qptr == NULL)) {
		return -ENXIO;
	} else {
		if (channel == 0) {		  /* D Channel */
			uchar_t SAPI, TEI, x1, x2;
			char cmd;
			isdn2_state state;

#ifdef CONFIG_DEBUG_STREAMS
			if(msgdsize(data) < 0)
				return 0;
#endif
			if(isdn2_log & 0x10) {
				printf ("%s*** %d", KERN_DEBUG,ctl->nr);
				log_printmsg (NULL, " Recv", data, KERN_DEBUG);
			}
			(void)msgdsize(data);
			if (card->modes & CHM_INTELLIGENT) {
				state = ctl->state[0];
				if(state != NULL)
					err = D_recv(state,0,data);
				else
					err = -ENXIO;
			} else {
				if(ctl->flags & HDR_CARD_DEBUG) {
					isdn23_hdr hdr;
					mblk_t *m1 = allocb(sizeof(*hdr),BPRI_MED);
					mblk_t *m2 = dupmsg(data);

					if (m1 == NULL || m2 == NULL) {
						if(m1 != NULL)
							freemsg(m1);
						if(m2 != NULL)
							freemsg(m2);
					} else {
						hdr = ((isdn23_hdr) m1->b_wptr)++;
						hdr->key = HDR_RAWDATA;
						hdr->seqnum = hdrseq; hdrseq += 2;
						hdr->hdr_rawdata.card = ctl->nr;
						hdr->hdr_rawdata.dchan = 1;
						hdr->hdr_rawdata.len = dsize (m2);
						hdr->hdr_rawdata.flags = 0;
						linkb (m1, m2);
						if(canput(isdn_chan.qptr->q_next)) 
							putnext (isdn_chan.qptr, m1);
						else
							freemsg(m1);
					}
				}

				if (ctl->status != C_up && ctl->status != C_wont_down) {
					if(isdn2_debug & 0x80)
						printf("%s  -- L1 up\n",KERN_DEBUG);
					(void) D_L1_up (ctl);
				}

				(void)msgdsize(data);
				data = pullupm (data, 0);
				if (data == NULL)	  /* Packet too short */
					return 0;
				x1 = SAPI = *data->b_rptr++;
				if (SAPI & 0x01) {	  /* TODO: X25 packet? */
					if (isdn2_debug & 0x10)
						printf ("%sisdn2_recv %d: SAPI %x invalid\n",KERN_DEBUG, ctl->nr, SAPI);
					freemsg (data);
					return 0 /* ESRCH */ ;
				}
				data = pullupm (data, 0);
				if (data == NULL)
					return 0;
				x2 = TEI = *data->b_rptr++;
				if ((TEI & 0x01) == 0) {
					if (isdn2_debug & 0x10)
						printf ("%sisdn2_recv %d: TEI %x invalid\n",KERN_DEBUG, ctl->nr, TEI);
					goto rawdata;
				}
				cmd = (SAPI & 0x02) ? 1 : 0;
				SAPI >>= 2;
				TEI >>= 1;
				for(ch=0;ch <= N_TEI; ch++) {
					if (ctl->TEI[ch] == TEI || TEI == TEI_BROADCAST)
						break;
				}
				if(ch > N_TEI) {
					if (isdn2_debug & 0x100)
						printf("%sisdn2_recv %d: %02x: not my TEI (%02x)\n",KERN_DEBUG,ctl->nr,TEI,ctl->TEI[0]);
					freemsg (data);
					return 0;
				}
				data = pullupm (data, 0);
				if (data == NULL)
					return 0;

				state = D__findstate (ctl, SAPI,ch);
				if (state != NULL) {
					if (TEI == TEI_BROADCAST)
						cmd |= 2;
					err = x75_recv (&state->state, cmd, data);
				} else if (TEI == TEI_BROADCAST && isdn_chan.qptr != NULL) {
				rawdata:
					err = -ENXIO;
				} else {
					err = 0;		  /* Not an error */
					freemsg (data);
				}
			}
		} else {				  /* B Channel */
			isdn2_chan chn = ctl->chan[channel];

			if (chn != NULL && chn->qptr != NULL) {
				putq (chn->qptr, data);
				err = 0;
			} else {
				err = -ENXIO;
				(*card->ch_mode) (card, channel, M_FREE, 0);	/* No B Channel. Take it
																 * down. (Needless to
																 * say, this shouldn't
																 * happen.) */
			}
		}
	}
	return err;
}

/* --> isdn_12.h */
int
isdn2_backenable (struct _isdn1_card *card, short channel)
{
	isdn2_card ctl = (isdn2_card) card->ctl;
	isdn2_chan chan;

	if (ctl == NULL)
		return -ENXIO;
	if (isdn2_debug & 0x80)
		printf ("%sisdn2_backenable %d %d\n",KERN_DEBUG, ctl->nr, channel);
	if (channel > 0 && (unsigned) channel <= ctl->card->nr_chans) {
		if ((chan = ctl->chan[channel]) != NULL && chan->qptr != NULL)
			qenable (WR (chan->qptr));
	} else if (isdn_chan.qptr != NULL) {
		qenable (WR (isdn_chan.qptr));
		if (channel == -1)
			channel = 0;		  /* All channels */
		else
			channel = ctl->card->nr_chans+1;		/* All D channels */
		for (; channel <= MAXCHAN; channel++) {
			if ((chan = ctl->chan[channel]) != NULL && chan->qptr != NULL)
				qenable (WR (chan->qptr));
		}
	}
	return 0;
}

/**
 ** ISDN driver handling
 **/


int
isdn2_init (void)
{
	printf ("%sISDN library present.\n Max %d channels per card, min %d D channel connections\n",KERN_DEBUG, MAXCHAN, MAX_D);
	bzero (&isdn_chan, sizeof (isdn_chan));
	bzero (isdnchan, sizeof (isdnchan));

	isdn2_notsent = 0;
	return 0;
}

/*
 * Send info about this card up.
 */
static void
isdn2_sendcard (isdn2_card card)
{
	isdn23_hdr hdr;
	mblk_t *mb;
	ushort_t i;

	for(i=1; i <= card->card->nr_chans; i++)
		(*card->card->ch_mode)(card->card, i, M_OFF, 0);

	card->offline = 0;
	if ((mb = allocb (sizeof (struct _isdn23_hdr), BPRI_HI)) == NULL) {
		if (isdn2_debug & 0x10)
			printf ("%sisdn2_sendcard: no memory\n",KERN_DEBUG);
		return /* ENOMEM */ ;
	}

	hdr = ((isdn23_hdr) mb->b_wptr)++;
	hdr->key = HDR_CARD;
	hdr->seqnum = hdrseq; hdrseq += 2;
	hdr->hdr_card.card = card->nr;
	hdr->hdr_card.id = card->id;
	if(card->card->nr_dchans < 2) {
		hdr->hdr_card.dchans = 1;
		hdr->hdr_card.bchans = card->card->nr_chans;
	} else {
		hdr->hdr_card.dchans = card->card->nr_dchans;
		hdr->hdr_card.bchans = card->card->nr_chans / card->card->nr_dchans;
	}
	hdr->hdr_card.modes = card->card->modes;
	if (isdn_chan.qptr != NULL) {
		if(isdn2_debug & 0x2000) logh_printmsg (NULL, "Up", mb);
		if(canput(isdn_chan.qptr->q_next)) {
			putnext (isdn_chan.qptr, mb);
			(*card->card->ch_mode) (card->card, 0, M_STANDBY, 1);
			return;
		}
	}
	freemsg (mb);
	(*card->card->ch_mode) (card->card, 0, M_OFF, 0);
	return /* ENXIO / ENOSPC */ ;
}

/*
 * Send info about all cards up.
 */
static void
isdn2_sendcards (void *ignored)
{
	isdn2_card crd;

	isdn2_notsent = 0;
	for (crd = isdn_card; crd != NULL; crd = crd->next) {
		if (!crd->offline) {
			isdn2_sendcard (crd);
		}
	}
	return;
}

/* Streams code to open the driver. */
static int
isdn2_open (queue_t * q, dev_t dev, int flag, int sflag ERR_DECL)
{
	isdn2_chan ch;
	char isinit = (q->q_ptr == NULL);

	if (sflag == CLONEOPEN || sflag == MODOPEN) {
#ifndef MODULE
		static unsigned char nr = 0;
		do { dev = ++nr; } while (minor(dev) == 0);
#else
		printf ("%sisdn2_open: CLONE/MODOPEN\n",KERN_DEBUG);
		ERR_RETURN(-ENXIO);
#endif
	}
	dev = minor (dev);
	/*
		* The master driver can't be opened more than once.
		*/
	if (dev >= NPORT) {
		printf ("%sisdn2_open: dev %d > NPORT %d\n",KERN_DEBUG,dev,NPORT);
		ERR_RETURN(-ENXIO);
	}
	if (dev == 0) {
		if (isdn_chan.qptr != NULL) {
			ERR_RETURN(-EBUSY);
		}
		ch = &isdn_chan;
	} else { /* dev > 0; require a new device for O_EXCL */
		if (isdn_chan.qptr == NULL) {
			ERR_RETURN(-ENXIO);
		}
		if ((ch = (isdn2_chan)q->q_ptr) == NULL) {
#if 0
			if (!(flag & O_EXCL)) {
				printf ("%sisdn2_open: open must be exclusive\n",KERN_DEBUG,dev,nport);
				ERR_RETURN(-ENXIO);
			}
#endif
			ch = malloc(sizeof(*ch));
			if (ch == NULL) {
				ERR_RETURN(-ENOMEM);
			}
			memset(ch,0,sizeof(*ch));
			ch->dev = dev;
		} else {
			if((flag & O_EXCL)) {
				ERR_RETURN(-EBUSY);
			}
		}
	}

	if (ch->qptr == NULL) {		  /* /dev/isdn -- somebody wants to open a
								   * connection. */
		if (dev > 0) {
			poplist (q, 0);
			if (isdn_chan.qptr != NULL) {
				isdn23_hdr hdr;
				mblk_t *mb = allocb (sizeof (struct _isdn23_hdr), BPRI_MED);

				if (mb != NULL) {
					hdr = ((isdn23_hdr) mb->b_wptr)++;
					hdr->key = HDR_OPEN;
					hdr->seqnum = hdrseq; hdrseq += 2;
					hdr->hdr_open.minor = dev;
					hdr->hdr_open.flags = flag;
#ifdef __linux__
					hdr->hdr_open.uid = (current->suid == 0) ? current->suid : current->uid;
					if(0) printk("%s2_open:uid %d\n",KERN_DEBUG,hdr->hdr_open.uid);
#else
					hdr->hdr_open.uid = u.u_uid;
#endif
					if(isdn2_debug & 0x2000) logh_printmsg (NULL, "Up", mb);
					if(canput(isdn_chan.qptr->q_next))
						putnext (isdn_chan.qptr, mb);
					else {
						freemsg(mb);
						ERR_RETURN(-ENOSPC);
					}
				} else {
					if (isdn2_debug & 0x10)
						printf ("%sisdn2_open %d: Can't notify controller: no mem\n",KERN_DEBUG, dev);
					ERR_RETURN(-ENOMEM);
				}
			}
		}
		ch->qptr = q;
		ch->oflag = flag;
		if (dev == 0)
			ch->status = M_D_ctl;
		else {
			ch->status = M_free;
			isdnchan[dev] = ch;
		}
	}
	WR (q)->q_ptr = (caddr_t) ch;
	q->q_ptr = (caddr_t) ch;

	if (isinit && (dev == 0)) {				  /* /dev/isdnmaster */
		isdn2_card crd;

		for (crd = isdn_card; crd != NULL; crd = crd->next) {
			int j;
			for (j=0; j <= N_TEI; j++)
				crd->TEI[j] = TEI_BROADCAST;
		}
		isdn2_notsent = 1;
#ifdef NEW_TIMEOUT
		timer_sendcards =
#endif
				timeout (isdn2_sendcards, NULL, HZ * 2);
	}
	if (isdn2_debug & 0x10)
		printf ("%sISDN %d open (%p)\n",KERN_DEBUG, dev, q);
	MORE_USE;
	return dev;
}

/*
 * Send this status message upstream to a device.
 */
#if 0 /* not needed right now */

static void
putproto (SUBDEV minor, ushort_t inf)
{
	queue_t *q;
	mblk_t *mb;

	if (isdn2_debug & 0x100)
		printf ("%sputproto %d %d\n",KERN_DEBUG, minor, inf);
	if (minor >= NPORT)
		return;
	else if (isdnchan[minor] == NULL || (q = isdnchan[minor]->qptr) == NULL)
		return;
	if ((mb = allocb (3, BPRI_HI)) == NULL)
		return;
	m_putid (mb, inf);
	DATA_TYPE(mb) = MSG_PROTO;
	putnext (q, mb);
}
#endif

/*
 * Take down a connection.
 */
static int
isdn2_disconnect (isdn2_chan ch, uchar_t error)
{
	int ms = splstr ();

	if (isdn2_debug & 0x100) {
		printf ("%sisdn2_disconnect %p %d\n",KERN_DEBUG, ch, error);
		if(ch->card != NULL)
			printf ("%sDisconnect dev %d card %d chan %d\n",KERN_DEBUG, ch->dev, ch->card->nr, ch->channel);
	}
	if (ch->status != M_free && (ch->channel > MAXCHAN)) {
		splx (ms);
		printf ("%sSevere problem: isdn2_disconnect: Channel %d !?!\n",KERN_DEBUG, ch->channel);
		return -EFAULT;
	}
	switch (ch->status) {
	case M_B_conn:
		{
			struct _isdn1_card *chx;

			if (ch->card != NULL && (chx = ch->card->card) != NULL) 
				(*chx->ch_mode) (chx, ch->channel, M_FREE, 0);
		}
		/* FALL THRU */
	case M_D_conn:
		if (ch->card != NULL) {
			if (ch->card->chan[ch->channel] != ch) {
				isdn2_chan cho;
				printf ("%s*** Chan ptr in disconnect bad!  dev %d card %d chan %d  points to  ",KERN_DEBUG, ch->dev, ch->card->nr, ch->channel);
				if((cho = ch->card->chan[ch->channel]) != NULL)
					printf ("dev %d, card %d chan %d\n", cho->dev, cho->card->nr, cho->channel);
				else
					printf("NULL\n");
			} else
				ch->card->chan[ch->channel] = NULL;
		}
		/* FALL THRU */
	case M_free:
		{
			/* putproto (ch ->dev, PROTO_DISCONNECT); */
			/* ch->chan = NULL; */
			ch->card = NULL;
			ch->status = M_free;
			if (isdn_chan.qptr != NULL && error != 0xFF) {
				isdn23_hdr hdr;
				mblk_t *mb = allocb (sizeof (struct _isdn23_hdr), BPRI_MED);

				if (mb != NULL) {
					hdr = ((isdn23_hdr) mb->b_wptr)++;
					hdr->key = HDR_DETACH;
					hdr->seqnum = hdrseq; hdrseq += 2;
					hdr->hdr_detach.connref = ch->connref;
					hdr->hdr_detach.minor = ch->dev;
					hdr->hdr_detach.error = error;
					hdr->hdr_detach.perm = 0;
					if(isdn2_debug & 0x2000) logh_printmsg (NULL, "Up", mb);
					if(canput(isdn_chan.qptr->q_next))
						putnext (isdn_chan.qptr, mb);
					else
						freemsg(mb);
				} else {
					if (isdn2_debug & 0x10)
						printf ("%sisdn2_disconnect %d: Can't notify controller: no mem\n",KERN_DEBUG, ch->dev);
				}
			}
		}
		break;
	case M_D_ctl:
		{
			int i;
			queue_t *q;
			isdn2_card crd;

			for (crd = isdn_card; crd != NULL; crd = crd->next) {
				D_kill_all (crd, 0);
				D_L1_down (crd);
			}

			for (i = 1; i < NPORT; i++)
				if ((isdnchan[i] != NULL) && ((q = isdnchan[i]->qptr) != NULL)) {
					if (isdn2_debug & 0x10)
						printf ("%sHang 2\n",KERN_DEBUG);
					putctlx (q, M_HANGUP);
				}
			if (isdn2_notsent) {
#ifdef OLD_TIMEOUT
				untimeout (isdn2_sendcards, NULL);
#else
				untimeout (timer_sendcards);
#endif
			}
		}
		break;
	default:;
	}
	ch->connref = 0;
	ch->status = M_free;
	splx (ms);
	return 0;
}

static void
isdn2_unblock (isdn2_chan ch)
{
	if (ch->status == M_blocked)
		ch->status = M_free;
	return;
}

/* Streams code to close the driver. */
static void
isdn2_close (queue_t *q, int dummy)
{
	isdn2_chan ch = (isdn2_chan) q->q_ptr;
	int ms = splstr ();

	ch->qptr = NULL;
	if (isdn2_debug & 0x10)
		printf ("%sISDN %d: Closed.\n",KERN_DEBUG, ch->dev);
	switch (ch->status) {
	case M_D_ctl:
	case M_B_conn:
	case M_D_conn:
		isdn2_disconnect (ch, 0);
		break;
	default:;
	}

	if (isdn_chan.qptr != NULL) {
		isdn23_hdr hdr;
		mblk_t *mb = allocb (sizeof (struct _isdn23_hdr), BPRI_MED);

		if (mb != NULL) {
			hdr = ((isdn23_hdr) mb->b_wptr)++;
			hdr->key = HDR_CLOSE;
			hdr->seqnum = hdrseq; hdrseq += 2;
			hdr->hdr_close.minor = ch->dev;
			hdr->hdr_close.error = 0;
			if(isdn2_debug & 0x2000) logh_printmsg (NULL, "Up", mb);
			if(canput(isdn_chan.qptr->q_next))
				putnext (isdn_chan.qptr, mb);
			else
				freemsg(mb);
		} else if (isdn2_debug & 0x10)
			printf ("%sisdn2_close %d: Can't notify controller: no mem\n",KERN_DEBUG, ch->dev);
		ch->status = M_blocked;
#ifdef NEW_TIMEOUT
		ch->timer_unblock =
#endif
			timeout ((void *)isdn2_unblock, ch, HZ * 10);
	} else
		ch->status = M_free;
	if (ch->dev > 0)
		isdnchan[ch->dev] = NULL;

	splx (ms);
	LESS_USE;
	return;
}

#ifdef __linux__
#define c_setq setq
#else
/*
 * Set up a Streams queue.
 * 
 * This code shamelessly stolen from Sys5R3.
 */
static void
c_setq (queue_t * rq, struct qinit *rinit, struct qinit *winit)
{
	queue_t *wq = WR (rq);

	rq->q_qinfo = rinit;
	rq->q_hiwat = rinit->qi_minfo->mi_hiwat;
	rq->q_lowat = rinit->qi_minfo->mi_lowat;
	rq->q_minpsz = rinit->qi_minfo->mi_minpsz;
	rq->q_maxpsz = rinit->qi_minfo->mi_maxpsz;
	wq->q_qinfo = winit;
	wq->q_hiwat = winit->qi_minfo->mi_hiwat;
	wq->q_lowat = winit->qi_minfo->mi_lowat;
	wq->q_minpsz = winit->qi_minfo->mi_minpsz;
	wq->q_maxpsz = winit->qi_minfo->mi_maxpsz;
}
#endif

/*
 * Attach a module to a Streams queue.
 * 
 */
static int
c_qattach (struct streamtab *qinfo, queue_t * qp, int flag)
{
	queue_t *rq;
	long s;
	int sflg;
	int err;

	if (!(rq = allocq ())){
		printf (" :allocq");
		return (0);
	}
	sflg = 0;
	s = splstr ();
	rq->q_next = qp;
	WR (rq)->q_next = WR (qp)->q_next;
	if (WR (qp)->q_next) {
		OTHERQ (WR (qp)->q_next)->q_next = rq;
		sflg = MODOPEN;
	}
	WR (qp)->q_next = WR (rq);
	c_setq (rq, qinfo->st_rdinit, qinfo->st_wrinit);
	rq->q_flag |= QWANTR;
	WR (rq)->q_flag |= QWANTR;

	if ((err = (*rq->q_qinfo->qi_qopen) (rq, 0, flag, sflg)) < 0) {
		printf ("%s :No Open %d %p",KERN_DEBUG, err, rq->q_qinfo->qi_qopen);
		qdetach (rq, 0, 0);
		splx (s);
		return err;
	}
	splx (s);
	return 0;
}

/*
 * Pop all modules off a queue.
 * 
 * Push the modules named in the mblk onto the queue.
 * 
 * Warning -- these modules must not sleep.
 */

static int
pushlist (queue_t * q, mblk_t * mp, char flags)
{
	int ms = splstr ();
	queue_t *xq;
	int err;

#ifdef CONFIG_DEBUG_STREAMS
	if(msgdsize(mp) < 0)
		return 0;
#endif
	if (isdn2_debug & 0x20)
		printf ("%sQ Push %p %p 0%o: ",KERN_DEBUG, q, mp, flags);

	xq = q->q_next;				  /* for qattach */

	while (mp->b_rptr < mp->b_wptr) {

		if(isdn2_debug & 0x20)printf (" ");

		while (mp->b_rptr < mp->b_wptr) {
			if (*mp->b_rptr <= ' ')
				mp->b_rptr++;
			else
				break;
		}
		if (mp->b_rptr < mp->b_wptr) {
			streamchar chx, *ch1 = mp->b_rptr, *ch2 = ch1;
			struct fmodsw *fm;

			while(ch2 < mp->b_wptr && *ch2 > ' ')
				ch2++;
			chx = *ch2; *ch2 = '\0';
			if(flags & PUSH_AFTER) {
				if(!strcmp(ch1,"reconn")) {
					if(isdn2_debug & 0x20) printf(" <skip head>");
					flags &=~ PUSH_AFTER;
				} else
					goto nx;
			} else if(flags & PUSH_BEFORE) {
				if(!strcmp(ch1,"reconn"))  {
					*ch2 = chx;
					if(isdn2_debug & 0x20) printf(" <skip rest>");
					goto fin;
				}
			}

			for (fm = fmod_sw; fm < &fmod_sw[fmodcnt]; fm++) {
				if (fm->f_str == NULL)
					continue;
				if(!strcmp(ch1,fm->f_name))
					goto found;
			}
			printf ("%sQ_Push: %s -- not found\n", KERN_ERR,ch1);
			*ch2 = chx;
			return -ENOENT;
		  found:
			if(isdn2_debug & 0x20)printf (" %s", fm->f_name);
			if ((err = c_qattach (fm->f_str, xq, 0)) < 0) {
				*ch2 = chx;
				splx (ms);
				printf ("%sQ_Push: %s -- can't attach\n", KERN_WARNING,fm->f_name);
				return err;
			}
		  nx:
			*ch2 = chx; 
			mp->b_rptr = ch2;
		}
	}
  fin:
	splx (ms);
	if(flags & PUSH_AFTER) if(isdn2_debug & 0x20) printf(" <skipped all>");
	if(isdn2_debug & 0x20)printf ("\n");
	return 0;
}


static void
poplist (queue_t * q, char flags)
{
	if (q == NULL || q->q_next == NULL) {
		printf ("%sErr PopList NULL! %p\n",KERN_DEBUG, q);
		return;
	}
	while (q->q_next->q_next) {
		char *n = q->q_next->q_qinfo->qi_minfo->mi_idname;

		if(flags & PUSH_BEFORE) 
			if (!strcmp(n,"reconn"))
				break;
		if (!strcmp(n,"proto"))
			break;
		qdetach (q->q_next, 1, 0);
	}

	if (!(flags & PUSH_BEFORE)) {
		mblk_t *mb;

		if ((mb = allocb (32, BPRI_MED)) != NULL) {
			if (isdn2_log & 4)
				m_putsz(mb,"strlog");
			m_putsz(mb,"proto");
			if (isdn2_log & 1)
				m_putsz(mb,"strlog");
			if (isdn2_log & 2) 
				m_putsz(mb,"qinfo");
			pushlist (q, mb, flags);
			freeb (mb);
		}
else printf("%sPOPLIST: could not alloc\n",KERN_DEBUG);
	}
}

/* Streams code to write data. */
static void
isdn2_wput (queue_t *q, mblk_t *mp)
{
	isdn2_chan ch = (isdn2_chan) q->q_ptr;

#ifdef CONFIG_DEBUG_STREAMS
	if(msgdsize(mp) < 0)
		return;
#endif
	if (isdn2_debug & 0x100)
		printf ("%sisdn2_wput %p.%p.%p %d of %d\n",KERN_DEBUG, mp,DATA_BLOCK(mp),mp->b_cont, msgdsize(mp),DATA_TYPE(mp));
	switch (DATA_TYPE(mp)) {
	case M_IOCTL:
		DATA_TYPE(mp) = M_IOCNAK;
		((struct iocblk *)mp->b_rptr)->ioc_error =
#ifdef linux
			ENOIOCTLCMD
#else
			EINVAL
#endif
				;
		qreply (q, mp);
		break;
	case CASE_DATA:
		mp = pullupm (mp, 1);
		if ((ch == &isdn_chan) && ((isdn23_hdr) mp->b_rptr)->key != HDR_DATA)
			DATA_TYPE(mp) = MSG_DATA;
		putq (q, mp);
		break;
	case MSG_PROTO:
		putq (q, mp);
		break;
	case M_FLUSH:
		if(ch->status == M_D_ctl)
			sysdump("FLUSH wq",NULL,0);
		if (*mp->b_rptr & FLUSHW) {
			isdn2_chan chan = (isdn2_chan) q->q_ptr;
			struct _isdn1_card *crd1;

			flushq (q, 0);
			if (chan->status == M_B_conn && chan->card != NULL && (crd1 = chan->card->card) != NULL) {
				(*crd1->flush) (crd1, chan->channel);
			}
		}
		if (*mp->b_rptr & FLUSHR) {
			flushq (RD (q), 0);
			*mp->b_rptr &= ~FLUSHW;
			qreply (q, mp);
		} else
			freemsg (mp);
		break;
	default:{
			log_printmsg (NULL, "Strange ISDN", mp, KERN_WARNING);
			/* putctlerr(RD(q)->b_next, -ENXIO); */
			freemsg (mp);
			break;
		}
	}
	return;
}

/*
 * If there was an error, send back the header as invalid.
 */
#ifdef CONFIG_DEBUG_ISDN
#define h_reply(a,b,c) deb_h_reply(__LINE__,a,b,c)
static int
deb_h_reply (unsigned int deb_line, queue_t * q, isdn23_hdr hdr, short err)
#else
static int
h_reply (queue_t * q, isdn23_hdr hdr, short err)
#endif
{
	isdn23_hdr hd;
	mblk_t *mb;

#if 0
	if (err == 0)
		return 0;
#endif
	if(hdr->key & HDR_NOERROR)
		return 0;

	if (isdn2_debug & 0x100)
		printf ("%sh_reply %p %p %d\n",KERN_DEBUG, q, hdr, err);

	if ((mb = allocb (2 * sizeof (struct _isdn23_hdr), BPRI_HI)) == NULL)
		 return -ENOMEM;

	hd = ((isdn23_hdr) mb->b_wptr)++;
	hd->key = HDR_INVAL;
	hd->seqnum = hdrseq; hdrseq += 2;
	if(err < 0)
		err = -err;
	hd->hdr_inval.error = err;
	*((isdn23_hdr) mb->b_wptr)++ = *hdr;
	if (!(q->q_flag & QREADR))
		q = RD (q);
	
#ifdef CONFIG_DEBUG_ISDN
	if(isdn2_debug & 0x2000) logh__printmsg (deb_line,NULL, "Up", mb);
#else
	if(isdn2_debug & 0x2000) logh_printmsg (NULL, "Up", mb);
#endif
	putnext (q, mb);
	return 0;
}

/* Streams code to scan the write queue. */
static void
isdn2_wsrv (queue_t *q)
{
	mblk_t *mp;

	while ((mp = getq (q)) != NULL) {
		mblk_t *mp2;
		isdn2_chan chan = (isdn2_chan) q->q_ptr;
		struct _isdn23_hdr hdr;
		int err;
		isdn2_state state;
		isdn2_card crd;
	/*
	 * This collects the appropriate amout of characters as told by the header.
	 *
	 * Actually, we don't collect anything; the stuff is just discarded.
	 */
#define XLENHDR(_len) do {					\
	if(dsize(mp) < _len) {					\
		freemsg(mp);						\
		printf("%s -- dsize %d, len %d\n",KERN_DEBUG,dsize(mp),_len); \
		h_reply(q,&hdr,EIO);				\
		mp = NULL; goto free_it;			\
	}; 										\
	if(_len > 0)							\
		mp = pullupm(mp,1);					\
	} while(0)								/**/
#define LENHDR(_who) XLENHDR(hdr.hdr_##_who.len)

#define NOLENHDR() do {						\
	mp = pullupm(mp,0);						\
} while(0)									/**/

	/* Get the card pointer off the header */
#define CARD(_who) do { int i = hdr.hdr_##_who.card; 					\
	for(crd = isdn_card; crd != NULL; crd = crd->next) 					\
		if (crd->nr == i) break;										\
	if(crd == NULL) {													\
		if(isdn2_debug&1)printf("%s -- Card %d\n",KERN_DEBUG,i);						\
		h_reply(q,&hdr,ENXIO); goto free_it; }							\
	} while(0)					  /**/

	/* Get the minor number */
#define xMINOR(_who) do {													\
	minor = hdr.hdr_##_who.minor;											\
	if ((minor < 1) || (minor >= NPORT) ||									\
			((chan = isdnchan[minor]) == NULL) || (chan->qptr) == NULL) {	\
		if(isdn2_debug&1)printf("%s -- Minor %d\n",KERN_DEBUG,minor);					\
		h_reply (q, &hdr, ENXIO); goto free_it; }							\
	} while(0)					  /**/


		if (isdn2_debug & 0x8)
			printf ("%swsrv %p.%p.%p\n",KERN_DEBUG, mp, DATA_BLOCK(mp), mp->b_cont);
		
		if(q == WR(isdn_chan.qptr))
			if(isdn2_debug & 0x2000) logh_printmsg (NULL, "Down", mp);
		switch (DATA_TYPE(mp)) {
		default:
			freemsg (mp);
			break;
		case MSG_PROTO:
			{
				/* status == M_D_conn */
				isdn23_hdr hdr2;
#if 0
				switch (*(ushort_t *) mp->b_rptr) {
				case PROTO_DISCONNECT:
					{
						mblk_t *mz = copymsg (mp);

						if (mz != NULL)
							qreply (q, mz);
					}
					break;
				}
#endif
				{
					if (chan->card != NULL) {
						if((err = do_chprot(chan->card,chan->channel,mp,CHP_FROMSTACK|CHP_TOCARD)) < 0) {
							printf("%sChProtErr isdn_2.c %d %d\n",KERN_DEBUG,__LINE__,err);
							freemsg(mp);
							/* TODO: Kill me. */
						}
					} else {
						mblk_t *mb = allocb (sizeof (struct _isdn23_hdr), BPRI_MED);
						if (mb == NULL) {
							printf("%sNoMemHdr isdn_2.c %d\n",KERN_DEBUG,__LINE__);
							putbqf (q, mp);
							return;
						}
						if(isdn2_debug & 0x10)
							printf("%sMsgProt NoCard %d isdn_2.c %d\n",KERN_DEBUG,chan->dev,__LINE__);

						hdr2 = ((isdn23_hdr) mb->b_wptr)++;
						hdr2->key = HDR_PROTOCMD;
						hdr2->seqnum = hdrseq; hdrseq += 2;
						hdr2->hdr_protocmd.minor = chan->dev;
						hdr2->hdr_protocmd.card = 0;
						hdr2->hdr_protocmd.channel = 0;
						hdr2->hdr_protocmd.len = dsize (mp);
						linkb (mb, mp);
						do {
							DATA_TYPE(mp) = M_DATA;
							mp = mp->b_cont;
						} while(mp != NULL);
						if (isdn_chan.qptr != NULL) {
							if(isdn2_debug & 0x2000) logh_printmsg (NULL, "Up", mb);
							if(canput(isdn_chan.qptr->q_next)) {
								putnext (isdn_chan.qptr, mb);
							} else {
								freemsg(mb);
							}
						} else {
							freemsg (mb);
							printf ("%sHang 4\n",KERN_DEBUG);
							putctlx (RD (q), M_HANGUP);
						}
					}
				}
			}
			break;
		case CASE_DATA:
			{
				mp = pullupm (mp, 0);
				if (mp != NULL) {
					SUBDEV minor;

					DATA_TYPE(mp) = M_DATA;
					switch (chan->status) {
					default:
						{
							freemsg (mp);
							mp = NULL;
							if (isdn2_debug & 0x10)
								printf ("%sISDN %d: Msg arrived for status %d\n",KERN_DEBUG, chan->dev, chan->status);
						}
						break;		  /* message deleted below */
					case M_D_ctl:
						{
							mp2 = mp;
							mp = pullupm (mp, sizeof (struct _isdn23_hdr));

							if (mp == NULL) {	/* Not a good message..? */
								if(isdn2_debug&0x200) printk(".NullCtl.");
								freemsg (mp2);
								break;
							}
							hdr = *((isdn23_hdr) mp->b_rptr)++;
							if (mp->b_rptr > mp->b_wptr) {
								if(isdn2_debug&0x200) printk(".OverMsg.");
								freemsg (mp);
								mp = NULL;
								break;
							}
							if(isdn2_debug & 0x100)
								printf("%sDispatch %x\n",KERN_DEBUG,hdr.key);

							switch (hdr.key & ~HDR_FLAGS) {
							default:
								if (isdn2_debug & 0x100)
									printf ("%sunknown key %d\n",KERN_DEBUG, hdr.key);
								h_reply (q, &hdr, EINVAL);
								break;
#if 0
								{ /* redo */
									mblk_t *mz = allocb (sizeof (struct _isdn23_hdr), BPRI_MED);

									if (mz == NULL) {
										printf("%sFreeReturn\n",KERN_DEBUG);
										freemsg (mp);
										return;
									}
									*((isdn23_hdr) mz->b_wptr)++ = hdr;
									if (mp != NULL) {
										linkb (mz, mp);
										DATA_TYPE(mz) = DATA_TYPE(mp);
										mp = NULL;
									}
									putbqf (q, mz);
								}
								return;
#endif
							case HDR_ATCMD:
								{
									mblk_t *mz = NULL;

									LENHDR (atcmd);
									xMINOR (atcmd);
									mz = allocb (4, BPRI_MED);

									if (mz != NULL) {
										m_putid (mz, PROTO_AT);
										DATA_TYPE(mz) = MSG_PROTO;
										linkb (mz, mp);
									} else
										freemsg (mp);
									mp = NULL;
								}
								break;
							case HDR_PROTOCMD:
								{
									streamchar *oldhd;
									ushort_t id, xid;
									int minor;
									long ismodlist;
									LENHDR (protocmd);

									oldhd = mp->b_rptr;
									if ((err = m_getid(mp,&id)) != 0) {
										h_reply (q, &hdr, err);
										break;
									}
									ismodlist = (id == PROTO_MODLIST);
									minor = hdr.hdr_protocmd.minor;

									if ((minor != 0) && (isdnchan[minor] != NULL) && (isdnchan[minor]->card != NULL)) {
										/* This is a temporary kludge */
										hdr.hdr_protocmd.card = isdnchan[minor]->card->nr;
										hdr.hdr_protocmd.channel = isdnchan[minor]->channel;
										hdr.hdr_protocmd.minor = 0;
									}
									if(ismodlist) {
										if((err = m_geti(mp,&ismodlist)) != 0) {
											h_reply (q, &hdr, err);
											break;
										}
										ismodlist |= CHP_MODLIST;
										while(m_getsx(mp,&xid) == 0) ;
									} else
										mp->b_rptr = oldhd;

									if (hdr.hdr_protocmd.minor == 0) {
										CARD (protocmd);
										if (hdr.hdr_protocmd.channel > crd->card->nr_chans) {
											printf ("%s -- bad channel\n",KERN_DEBUG);
											hdr.hdr_protocmd.minor = minor;
											h_reply (q, &hdr, -EINVAL);
											break;
										}
										if((err = do_chprot(crd,hdr.hdr_protocmd.channel,mp,CHP_TOCARD|ismodlist)) < 0) {
											printf ("%s -- Err SetMode %d\n",KERN_DEBUG,err);
											hdr.hdr_protocmd.minor = minor;
										} else 
											mp = NULL;
										h_reply (q, &hdr, err);
										break;
									}
									xMINOR (protocmd);
									if(ismodlist) {
										if(!(ismodlist & PUSH_UPDATE))
											err = pushlist(chan->qptr,mp,ismodlist);
										else
											err = 0;
									} else {
										oldhd = mp->b_rptr;
										DATA_TYPE(mp) = MSG_PROTO;
										if(canput(chan->qptr->q_next)) {
											putnext(chan->qptr,mp);
											mp = NULL;
											err = 0;
										} else
											err = -ENOSPC;
									}
									h_reply (q, &hdr, err);
								}
								break;
							case HDR_XDATA:
								{
									LENHDR (xdata);
									xMINOR (xdata);
									putnext (chan->qptr, mp);
									mp = NULL;
									break;
								}
								break;
							case HDR_UIDATA:
								{
									LENHDR (uidata);
									CARD (uidata);
#ifdef DO_MULTI_TEI
									state = D__findstate (crd, hdr.hdr_uidata.SAPI,hdr-hdr_uidata.bchan);
#else
									state = D__findstate (crd, hdr.hdr_uidata.SAPI,0);
#endif
									if (state == NULL || state->state.status == S_free) {
										h_reply (q, &hdr, EINVAL);
										break;
									}
									if (!((crd->card->modes & CHM_INTELLIGENT)
											? D_cansend(state)
											: x75_cansend (&state->state, 1))) {
										h_reply (q, &hdr, (crd->status == C_wont_up) ? EIO : ENOMEM);
										break;
									}
									DATA_TYPE(mp) = M_DATA;
									if (crd->card->modes & CHM_INTELLIGENT)
										err = D_send (state, hdr.hdr_uidata.broadcast, mp);
									else
										err = x75_send (&state->state, hdr.hdr_uidata.broadcast ? 3 : 1, mp);
									if (err != 0) {
										h_reply (q, &hdr, err);
									} else
										mp = NULL;
								}
								break;
							case HDR_DATA:
								{
									LENHDR (data);
									CARD (data);
#ifdef DO_MULTI_TEI
									state = D__findstate (crd, hdr.hdr_data.SAPI,hdr.hdr_data.bchan);
#else
									state = D__findstate (crd, hdr.hdr_data.SAPI,0);
#endif
									if (state == NULL) {
										h_reply (q, &hdr, EINVAL);
										break;
									}
									if (!((crd->card->modes & CHM_INTELLIGENT)
											? D_cansend(state)
											: x75_cansend (&state->state, 0))) {
										h_reply (q, &hdr, (crd->status == C_wont_up) ? EIO : ENOMEM);
										break;
									}
									DATA_TYPE(mp) = M_DATA;
									if (crd->card->modes & CHM_INTELLIGENT)
										err = D_send(state,0,mp);
									else
										err = x75_send (&state->state, 0, mp);
									if (err != 0)
										h_reply (q, &hdr, err);
									else
										mp = NULL;
								}
								break;
							case HDR_RAWDATA:
								{
									LENHDR (rawdata);
									CARD (rawdata);
									if ((err = (*crd->card->send) (crd->card, 0, mp)) != 0) {
										h_reply (q, &hdr, err);
									} else
										mp = NULL;
									break;
								}
								break;
							case HDR_CLOSE:
								{
									queue_t *qq;

									NOLENHDR ();
									xMINOR (close);

									isdn2_disconnect (chan, hdr.hdr_close.error);

									if ((isdnchan[minor] != NULL) && ((qq = isdnchan[minor]->qptr) != NULL)) {
										if (hdr.hdr_close.error != 0)
											putctlerr (qq, hdr.hdr_close.error);
										else
											putctlx (qq, M_HANGUP);
									} else {
										if(isdn2_debug&1)printf("%s-- not open\n",KERN_DEBUG);
										h_reply (q, &hdr, ENXIO);
									}
								}
								break;
							case HDR_ATTACH:
								{
									int ms;

									NOLENHDR ();
									xMINOR (attach);
									CARD (attach);

									if (isdn2_debug & 0x10)
										printf ("%sAttach card %d channel %d to minor %d connref %ld %s%s\n",KERN_DEBUG,
											hdr.hdr_attach.card, hdr.hdr_attach.chan, minor, hdr.hdr_attach.connref,
											(hdr.hdr_attach.listen & 2) ? "setup" : ((hdr.hdr_attach.listen & 1) ? "listen" : "talk"),
											(hdr.hdr_attach.listen & 4) ? " force" : "");

									ms = splstr ();
									if (chan->card != NULL && chan->card != crd) {
										printf ("%s -- minor not free\n",KERN_DEBUG);
										h_reply (q, &hdr, EBUSY);
										splx (ms);
										break;
									} else if (hdr.hdr_attach.chan > crd->card->nr_chans) {
										printf ("%s -- bad channel (%d > %d)\n",KERN_DEBUG, hdr.hdr_attach.chan, crd->card->nr_chans);
										h_reply (q, &hdr, EINVAL);
										splx (ms);
										break;
									}
									if ((crd->status != C_up) && !(hdr.hdr_attach.listen & 2)) {
										printf("%s -- card down 3",KERN_DEBUG);
										if(hdr.hdr_attach.listen & 4) 
											err = (*crd->card->ch_mode) (crd->card, 0, M_ON, 0);
										else
											err = -ENXIO;
										if(err != 0) {
											h_reply (q, &hdr, err);
											splx (ms);
											break;
										}
									}
									if (hdr.hdr_attach.chan > 0) {		/* B channel */
										if (crd->chan[hdr.hdr_attach.chan] != NULL && crd->chan[hdr.hdr_attach.chan] != chan) {
#if 1
											printf ("%s -- Err Chan busy\n",KERN_DEBUG);
											h_reply (q, &hdr, EBUSY);
											splx (ms);
											break;
#else
											isdn2_disconnect (chan, 0xFF);
#endif
										}
										if (hdr.hdr_attach.connref != 0 && chan->connref != 0 && chan->connref != hdr.hdr_attach.connref) {
#if 1
											printf ("%s -- Err Chan attached %ld\n",KERN_DEBUG,chan->connref);
											h_reply (q, &hdr, EBUSY);
											splx (ms);
											break;
#else
											isdn2_disconnect (chan, 0xFF);
#endif
										}
										if ((err = (*crd->card->ch_mode) (crd->card, hdr.hdr_attach.chan, M_ON, hdr.hdr_attach.listen & 3)) != 0) {
											printf ("%s -- Err SetMode\n",KERN_DEBUG);
											h_reply (q, &hdr, EIO);
											splx (ms);
											break;
										}
										crd->chan[hdr.hdr_attach.chan] = chan;
										chan->status = M_B_conn;
										chan->card = crd;
										chan->channel = hdr.hdr_attach.chan;
										if(hdr.hdr_attach.connref != 0)
											chan->connref = hdr.hdr_attach.connref;
									} else {	/* D channel */
										int i;

										for (i = crd->card->nr_chans+1; i <= MAXCHAN; i++)
											if (crd->chan[i] == NULL || crd->chan[i] == chan)
												break;
										if (i >= MAXCHAN) {
											if (isdn2_debug & 0x10)
												printf ("%s -- Err no chan free\n",KERN_DEBUG);
											h_reply (q, &hdr, EBUSY);
											splx (ms);
											break;
										}
										crd->chan[i] = chan;
										chan->status = M_D_conn;
										chan->card = crd;
										chan->channel = i;
										chan->connref = hdr.hdr_attach.connref;
										/* putproto (minor, PROTO_CONNECTED); */
										if (isdn2_debug & 0x10)
											printf ("%s -- Conn D\n",KERN_DEBUG);
									}
									h_reply(q,&hdr,0);
									splx (ms);
								}
								break;
							case HDR_DETACH:
								{
									NOLENHDR ();
									xMINOR (detach);
									if (isdn2_debug & 0x10)
										printf ("%sDetach minor %d connref %ld%s\n",KERN_DEBUG,
											minor, hdr.hdr_detach.connref, hdr.hdr_detach.perm ? " force" : "");
									if ((hdr.hdr_detach.connref != 0) && (chan->connref != 0) && (chan->connref != hdr.hdr_detach.connref)) {
										printf ("%s -- bad connref\n",KERN_DEBUG);
										h_reply (q, &hdr, EBUSY);
										break;
									}
									if (hdr.hdr_detach.perm)
										poplist (chan->qptr, PUSH_BEFORE);
									isdn2_disconnect (chan, hdr.hdr_detach.error);
									h_reply(q,&hdr,0);
								}
								break;
							case HDR_LOAD:
								{
									LENHDR (load);
									CARD (load);
									if(crd->card->boot == NULL) {
										printf ("%s -- Load: no loader\n",KERN_DEBUG);
										h_reply (q, &hdr, EIO);
									}
									if(hdr.hdr_load.seqnum == 1 && hdr.hdr_load.foffset == 0)
										isdn2_new_state(crd->card,0);
									if((err = (*crd->card->boot) (crd->card, hdr.hdr_load.seqnum, hdr.hdr_load.foffset, mp)) != 0) {
										printf ("%s -- Err Load %d\n",KERN_DEBUG,err);
										h_reply (q, &hdr, err);
									} else {
										mp = NULL;
										h_reply (q, &hdr, 0);
									}
									break;
								}
							case HDR_TEI:
								{
									uchar_t ch;
									NOLENHDR ();
									CARD (tei);
#ifdef DO_MULTI_TEI
									ch=hdr.hdr_tei.bchan;
#else
									ch=0;
#endif
									if (hdr.hdr_tei.TEI != TEI_BROADCAST && crd->TEI[ch] != TEI_REQUESTED &&
											crd->TEI[ch] != TEI_BROADCAST && hdr.hdr_tei.TEI != crd->TEI[ch]) {
										h_reply (q, &hdr, EINVAL);
										break;
									}
									crd->TEI[ch] = hdr.hdr_tei.TEI;
									if (hdr.hdr_tei.TEI == TEI_BROADCAST) {
										/*
										* Duplicate TEI, or TEI not assignable.
										* Deassign. Kick all states.
										*/
										int ms = splstr ();

										if (crd->card->modes & CHM_INTELLIGENT) {
											if(crd->state[ch] != NULL)
												D_state(crd->state[ch],MDL_REMOVE_REQ,0);
										} else {
											for (state = crd->state[ch]; state != NULL; state = state->next)
												x75_changestate (&state->state, MDL_REMOVE_REQ, 0);
										}
										splx (ms);
									} else {
										/*
										* TEI (re)assigned. Reestablish.
										*/
										int ms = splstr ();

										if (crd->card->modes & CHM_INTELLIGENT) {
											if(crd->state[ch] != NULL)
												D_state (crd->state[ch], DL_ESTABLISH_IND, 0);
										} else {
											for (state = crd->state[ch]; state != NULL; state = state->next) {
												if(isdn2_debug&0x40) 
													printf("%sWakeup %d/%d/%d",KERN_DEBUG,state->card->nr, state->SAPI, *state->card->TEI);
												if(state->card != crd)
													printf("\n*!*Bad state for card!\n");
												x75_changestate (&state->state, DL_ESTABLISH_IND, 0);
											}
										}
										splx (ms);
									}
								}
								break;
							case HDR_CARD:
								{
									NOLENHDR ();
									CARD (card);
									if (hdr.hdr_card.bchans != crd->card->nr_chans) {
										h_reply (q, &hdr, EINVAL);
										break;
									}
									crd->flags = hdr.hdr_card.flags;
									if (crd->status != C_up)
										h_reply (q, &hdr, D_L1_up (crd));
								}
								break;
							case HDR_NOCARD:
								{
									NOLENHDR ();
									CARD (nocard);
									h_reply (q, &hdr, D_L1_down (crd));
								}
								break;
							case HDR_OPENPROT:
								{
									NOLENHDR ();
									CARD (openprot);
									if((hdr.hdr_notify.SAPI == SAPI_INVALID) || (hdr.hdr_notify.SAPI == SAPI_FIXED)) {
										if(crd->timedown == 1) {
#ifdef OLD_TIMEOUT
											untimeout (D_takedown, card);
#else
											untimeout (crd->timer_takedown);
#endif
										}
										crd->timedown = 2;
										D_L1_up(crd);
									} else {
#ifdef DO_MULTI_TEI
										state = D__findstate (crd, hdr.hdr_openprot.SAPI,hdr.hdr_openprot.bchan);
#else
										state = D__findstate (crd, hdr.hdr_openprot.SAPI,0);
#endif
										if ((state == NULL) != (hdr.hdr_openprot.ind == 0)) {
#ifdef CONFIG_DEBUG_ISDN
											printf("%sErr state 0x%p ind 0x%x\n",KERN_DEBUG,state,hdr.hdr_openprot.ind);
#endif
											h_reply (q, &hdr, EINVAL);
											break;
										}
										if (state == NULL) {
#ifdef DO_MULTI_TEI
											h_reply (q, &hdr, D_register (crd, hdr.hdr_openprot.SAPI, hdr.hdr_openprot.bchan, hdr.hdr_openprot.broadcast));
#else
											h_reply (q, &hdr, D_register (crd, hdr.hdr_openprot.SAPI, 0, hdr.hdr_openprot.broadcast));
#endif
										} else {
											if (crd->card->modes & CHM_INTELLIGENT) 
												D_state(state,hdr.hdr_openprot.ind,0);
											else
												x75_changestate (&state->state, hdr.hdr_openprot.ind, 0);
										}
										D_checkactive (crd);
									}
								}
								break;
							case HDR_CLOSEPROT:
								{
									NOLENHDR ();
									CARD (closeprot);
									if((hdr.hdr_notify.SAPI == SAPI_INVALID) || (hdr.hdr_notify.SAPI == SAPI_FIXED)) {
										if(crd->timedown == 2) 
											crd->timedown = 0;
										D_L1_down(crd);
									} else {
#ifdef DO_MULTI_TEI
										state = D__findstate (crd, hdr.hdr_closeprot.SAPI,hdr.hdr_closeprot.bchan);
#else
										state = D__findstate (crd, hdr.hdr_closeprot.SAPI,0);
#endif
										if (state == NULL) {
											h_reply (q, &hdr, EINVAL);
											break;
										}
										if (hdr.hdr_closeprot.ind == 0)
											h_reply (q, &hdr, D_kill_one (state, hdr.hdr_closeprot.ind));
										else if (crd->card->modes & CHM_INTELLIGENT) 
											D_state(state,hdr.hdr_closeprot.ind,0);
										else
											x75_changestate (&state->state, hdr.hdr_closeprot.ind, 0);
									}
									D_checkactive (crd);
								}
								break;
							case HDR_NOTIFY:
								{
									NOLENHDR ();
									CARD (notify);
#ifdef DO_MULTI_TEI
									state = D__findstate (crd, hdr.hdr_notify.SAPI,hdr.hdr_notify.bchan);
#else
									state = D__findstate (crd, hdr.hdr_notify.SAPI,0);
#endif
									if (state == NULL) {
										h_reply (q, &hdr, EINVAL);
										break;
									}
									if (crd->card->modes & CHM_INTELLIGENT) 
										D_state(state,hdr.hdr_notify.ind,0);
									else
										x75_changestate (&state->state, hdr.hdr_notify.ind, 0);
									D_checkactive (crd);
								}
								break;
							case HDR_INVAL:
								{
									XLENHDR (sizeof (struct _isdn23_hdr));

									if (mp != NULL) {
										freemsg (mp);
										mp = NULL;
									}
									h_reply (q, &hdr, EINVAL);
								}
								break;
							}			  /* switch */
						  free_it:
							if (mp != NULL) {
								freemsg (mp);
								mp = NULL;
							}
						}
						break;
					case M_B_conn:
						{
							struct _isdn1_card *crd1;

							if(0)if(isdn2_debug&0x200) printf(".BConn");

							if (chan->card != NULL && (crd1 = chan->card->card) != NULL) {
								if ((*crd1->cansend) (crd1, chan->channel)) {
									if ((*crd1->send) (crd1, chan->channel, mp) != 0) {
										putbqf (q, mp);
										printf (".");
										return;
									}
								} else {
									if(0)printf (",");
									putbqf (q, mp);
									return;
								}
							} else {
								freemsg (mp);
								printf ("Hang 5\n");
								putctlx (RD (q), M_HANGUP);
							}
						}
						mp = NULL;
						break;
					case M_D_conn:
						{
							isdn23_hdr hdr3;
							mblk_t *mz = allocb (sizeof (struct _isdn23_hdr), BPRI_MED);

							if(isdn2_debug&0x200) printf(".DConn");
							if (mz == NULL) {
								putbqf (q, mp);
								return;
							}
							hdr3 = ((isdn23_hdr) mz->b_wptr)++;
							hdr3->key = HDR_XDATA;
							hdr3->seqnum = hdrseq; hdrseq += 2;
							hdr3->hdr_xdata.minor = chan->dev;
							hdr3->hdr_xdata.len = dsize (mp);
							linkb (mz, mp);
							if (isdn_chan.qptr != NULL) {
								if(isdn2_debug & 0x2000) logh_printmsg (NULL, "Up", mz);
								if(canput(isdn_chan.qptr->q_next))
									putnext (isdn_chan.qptr, mz);
								else
									freemsg(mz);
							} else {
								freemsg (mz);
								printf ("Hang 6\n");
								putctlx (RD (q), M_HANGUP);
							}
						}
						mp = NULL;
						break;
					case M_free:
						{
							if(isdn2_debug&0x200) printf(".Free");
							freemsg (mp);
							mp = NULL;
						}
						break;
					}
					if (mp != NULL && (mp = pullupm (mp, 0)) != NULL) {
						if(isdn2_debug&0x200) printf(".PutBack2.");
						putbqf (q, mp);
						return;
					}
				} else if(isdn2_debug&0x200) printf(".MsgEmpty.");
			}
		}
	}
	return;
}



static void
isdn2_rsrv (queue_t * q)
{
	mblk_t *mp;

	while ((mp = getq (q)) != NULL) {
		if (q->q_next == NULL) {
			freemsg (mp);
			continue;
		}
		if (DATA_TYPE(mp) >= QPCTL || canput (q->q_next)) {
			putnext (q, mp);
			continue;
		} else {
			putbq (q, mp);
			break;
		}
	}
	return;
}


#ifdef sun
/*
 * TODO: Put in SunOS autoload code
 */
#endif

void
chkfree (void *x)
{
}


#ifdef MODULE
static int devmajor1 = 0;
static int devmajor2 = 0;

static int do_init_module(void)
{
	int err;
	
	err = register_strdev(0,&isdn_2info,0);
	if(err < 0) return err;
	devmajor1 = err;
	err = register_strdev(0,&isdn_2tinfo,NPORT);
	if(err < 0) {
		unregister_strdev(devmajor1,&isdn_2info,0);
		return err;
	}
	devmajor2 = err;

	return 0;
}

static int do_exit_module(void)
{
	int err1 = unregister_strdev(devmajor1,&isdn_2info,0);
	int err2 = unregister_strdev(devmajor2,&isdn_2tinfo,NPORT);
	return err1 || err2;
}
#endif
