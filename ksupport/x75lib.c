#include "f_module.h"
#include "primitives.h"
#include "streams.h"
#include <sys/errno.h>
#ifndef linux
#include <sys/var.h>
#endif

#include "x75lib.h"
#include "streamlib.h"
#include "lap.h"
#include "f_malloc.h"
#ifndef KERNEL
#include "kernel.h"
#endif

#ifdef linux
#ifdef KERNEL
#include <linux/sched.h>
#else
#define jiffies 0
#endif
#else
#define jiffies 0
#endif

/**
 ** X75 / X25 / Q921 support library.
 **/

/*
 * Warning: This code is implemented along the lines of the Q.921 specs and
 * SDL/GR diagrams. As such, it's a little redundant, but much easier to
 * verify. GCC 2 does pretty good optimizing work with it.
 * 
 * NB: The code was lots terser a few hours ago, before I decided to run it
 * through indent...
 */

/*
 * Timeout prototypes
 */
static void x75_T1 (x75 state);
static void x75_T3 (x75 state);

static char *x75_sname[]=
	{"S_free", "S_down", "S_await_up", "S_await_down", "S_up", "S_recover",};
/*
 * State change.
 */

static void
x75_setstate (x75 state, x75_status status)
{

	if (state->debug & 0x02)
		printf ("%sx75.%d Setstate %d/%s -> %d/%s\n", KERN_DEBUG,state->debugnr, state->status, x75_sname[state->status], status, x75_sname[status]);
	if(state->status != S_free) {
		state->status = status;
		if(state->status == S_down)
			state->errors = 0;
	}
}

/*
 * Macros for timeouts
 */

#ifdef OLD_TIMEOUT

#define stop_T(xx,er) do {			\
	int _ms = splstr();				\
	if(state->T##xx) { 				\
		state->T##xx = 0; 			\
		if(state->debug & 0x08)		\
			printf("%sStop%d T"#xx" %d\n",KERN_DEBUG,state->debugnr,__LINE__);	\
		untimeout((void *)x75_T##xx,state); \
	} splx(_ms);					\
	(er)=0;							\
	} while(0)

#define start_T(xx,er) do { 		\
	int _ms = splstr();				\
	if(! state->T##xx) { 			\
		state->T##xx = 1; 			\
		if(state->debug & 0x08) 		\
			printf("%sStart%d T"#xx" %d %d\n",KERN_DEBUG,state->debugnr,state->RUN_T##xx, __LINE__);	\
		timeout((void *)x75_T##xx,state,(state->RUN_T##xx * HZ) / 10); 	\
	} splx(_ms);					\
	(er)=0;							\
	} while(0)

#define restart_T(xx,er) do {		\
	int _ms = splstr();				\
	if(state->T##xx) 				\
		untimeout((void *)x75_T##xx,state); \
	state->T##xx = 1; 				\
	if(state->debug & 0x08)			\
		printf("%sRestart%d T"#xx" %d %d\n",KERN_DEBUG,state->debugnr,state->RUN_T##xx, __LINE__);	\
	timeout((void *)x75_T##xx,state,(state->RUN_T##xx * HZ) / 10); 	\
	splx(_ms);						\
	} while(0)

#else /* NEW_TIMEOUT */

#define stop_T(xx,er) do {			\
	int _ms = splstr();				\
	if(state->T##xx) { 				\
		state->T##xx = 0; 			\
		if(state->debug & 0x08)		\
			printf("%sStop%d T"#xx" %d\n",KERN_DEBUG,state->debugnr,__LINE__);	\
		untimeout(state->timer_T##xx); \
	} splx(_ms);					\
	(er)=0;							\
	} while(0)

#define start_T(xx,er) do { 		\
	int _ms = splstr();				\
	if(! state->T##xx) { 			\
		state->T##xx = 1; 			\
		if(state->debug & 0x08) 		\
			printf("%sStart%d T"#xx" %d %d\n",KERN_DEBUG,state->debugnr,state->RUN_T##xx,__LINE__);	\
		state->timer_T##xx = timeout((void *)x75_T##xx,state,(state->RUN_T##xx * HZ) / 10); 	\
	} splx(_ms);					\
	(er)=0;							\
	} while(0)

#define restart_T(xx,er) do {		\
	int _ms = splstr();				\
	if(state->T##xx) 				\
		untimeout(state->timer_T##xx); \
	state->T##xx = 1; 				\
	if(state->debug & 0x08)			\
		printf("%sRestart%d T"#xx" %d %d\n",KERN_DEBUG,state->debugnr,state->RUN_T##xx,__LINE__);	\
	state->timer_T##xx = timeout((void *)x75_T##xx,state,(state->RUN_T##xx * HZ) / 10); 	\
	splx(_ms);						\
	(er)=0;							\
	} while(0)

#endif
/*
 * Send indication up.
 */
#define msg_up(state,ind,add) (*state->state)(state->ref,ind,add)

/*
 * Clear state machine -- connection down.
 */
static int
kill_me (x75 state, char ind)
	/* Abort the connection, reset everything */
{
	int err2 = 0;
	int ms = splstr ();
	x75_status oldstate = state->status;

	S_flush (&state->I);
	S_flush (&state->UI);
	x75_setstate(state, S_down);
	stop_T (1, err2);
	stop_T (3, err2);
	if (ind != 0 && oldstate != S_free && oldstate != S_down)
		msg_up (state, ind, 0);

	splx (ms);
	return 0;
}

/*
 * Clear exception conditions.
 */
static int
clr_except (x75 state)
{
	state->RNR = 0;
	state->sentRR = 1;
	state->inREJ = 0;
	state->ack_pend = 0;

	return 0;
}

/*
 * Flush I queue.
 */
static int
flush_I (x75 state)
{
	S_flush (&state->I);
	state->v_r = state->v_s = state->v_a = 0;
	if(state->backenable)
		(*state->backenable) (state->ref);

	return 0;
}

/*
 * Start retransmission.
 */
static int
retransmit (x75 state)
{
#if 0
	if (state->flush != NULL && state->v_s != state->v_a)
		(*state->flush) (state->ref);
#endif

	state->v_s = state->v_a;

	return 0;
}

/*
 * Send 3-byte header. Actually enqueue only one byte -- the caller is
 * responsible for attaching the address bytes. However, we preallocate them in
 * order to go easy on allocb().
 */

static int
xmit3 (x75 state, char cmd, uchar_t what)
{
	mblk_t *mb;
	int err;

	if (state->debug & 0x80)
		printf ("%sX%d%c%x ", KERN_DEBUG,state->debugnr, cmd ? 'c' : 'r', what);
	mb = allocb (state->offset + 1, BPRI_HI);
	if (mb == NULL) {
		if(state->debug & 0x01)
			printf("%sNX4 NoMem ",KERN_WARNING);
		return -ENOENT;
	}
	mb->b_rptr += state->offset;
	mb->b_wptr += state->offset;
	*mb->b_wptr++ = what;
	if ((err = (*state->send) (state->ref, cmd, mb)) != 0)
		freemsg (mb);
	return err;
}

/*
 * Send 4-byte header. Actually enqueue only two bytes -- the caller is
 * responsible for attaching the address bytes.
 */
static int
xmit4 (x75 state, char cmd, uchar_t what1, uchar_t what2)
{
	mblk_t *mb;
	int err;

	if (state->debug & 0x80)
		printf ("%sX%d%c%x.%x ", KERN_DEBUG,state->debugnr, cmd ? 'c' : 'r', what1, what2);
	mb = allocb (state->offset + 2, BPRI_HI);
	if (mb == NULL) {
		if(state->debug & 0x01)
			printf("%sNX4 NoMem ",KERN_WARNING);
		return -ENOENT;
	}
	mb->b_rptr += state->offset;
	mb->b_wptr += state->offset;
	*mb->b_wptr++ = what1;
	*mb->b_wptr++ = what2;
	if ((err = (*state->send) (state->ref, cmd, mb)) != 0)
		freemsg (mb);
	return err;
}

#define establish(s) Xestablish(s,__LINE__)
/*
 * Connection established.
 */
static int
Xestablish (x75 state, int line)
{
	int err, err2;

	if (state->debug & 0x10)
		printf ("%sEstablish%d %d\n", KERN_EMERG,state->debugnr, line);
	if(state->broadcast) {
		return -ENXIO;
	}
	err = clr_except (state);
	state->RC = 0;
	x75_setstate(state, S_await_up);
	if((state->errors += 10) >= 100) {
		x75_setstate(state, S_down);
		printf("%sERR_G 1, %d\n",KERN_INFO,state->errors);
		state->errors = 0;
		msg_up (state, MDL_ERROR_IND, ERR_G);
		msg_up (state, DL_RELEASE_IND, 0);
		x75_setstate(state, S_down);
		return -ETIMEDOUT;
	}
#if 0
if(!state->wide) { int i; printf("Xestablish %d\n",jiffies);
       	for (i=0;i<64;i++) {
               	printf("%08x ",*(i-1+(unsigned long *)&state));
               	if(!((i+1) & 0x03))
                       	printf("\n");
       	}
}
#endif
	err2 = xmit3 (state, 1, (state->wide ? L2_SABME : L2_SABM) | L2_PF_U);
	if (err == 0)
		err = err2;
	restart_T (1, err2);
	if (err == 0)
		err = err2;
	stop_T (3, err2);
	if (err == 0)
		err = err2;
	return err;
}

#define recover_NR(s) Xrecover_NR(s,__LINE__)
/*
 * Reestablish the connection due to lost N_R synchronisation.
 */
static int
Xrecover_NR (x75 state, int line)
{
	int err;

	if (state->flush != NULL)
		(*state->flush) (state->ref);
	printf("%sERR_J 1\n",KERN_INFO);
	msg_up (state, MDL_ERROR_IND, ERR_J);
	err = Xestablish (state, line);
	state->L3_req = 0;
	return err;
}

/*
 * Force sending an enquiry packet (P bit set)
 */
static int
enquiry (x75 state)
{
	int err, err2;

	if (state->wide)
		err = xmit4 (state, 1, ((state->sentRR = (state->canrecv == NULL || (*state->canrecv) (state->ref))) ? L2_RR : L2_RNR), (state->v_r << 1) | L2_PF_S);
	else
		err = xmit3 (state, 1, ((state->sentRR = (state->canrecv == NULL || (*state->canrecv) (state->ref))) ? L2_RR : L2_RNR) | (state->v_r << 5) | L2_PF);
	if(err == 0)
		state->ack_pend = 0;
	start_T (1, err2);
	if (err == 0)
		err = err2;
	return err;
}

/*
 * Respond to an enquiry packet (F bit set)
 */
static int
enq_resp (x75 state)
{
	int err;

	if (state->wide)
		err = xmit4 (state, 0, ((state->sentRR = (state->canrecv == NULL || (*state->canrecv) (state->ref))) ? L2_RR : L2_RNR), (state->v_r << 1) | L2_PF_S);
	else
		err = xmit3 (state, 0, ((state->sentRR = (state->canrecv == NULL || (*state->canrecv) (state->ref))) ? L2_RR : L2_RNR) | (state->v_r << 5) | L2_PF);
	if(err == 0)
		state->ack_pend = 0;
	return err;
}

/*
 * T1 (T201) resends packets because no ack has arrived for them.
 */
static void
x75_T1 (x75 state)
{
	int err2 = 0;

	state->T1 = 0;
	if (state->debug & 0x10)
		printf ("%sT%d.1 %d RC %d\n", KERN_DEBUG,state->debugnr, state->status, state->RC);
	switch (state->status) {
	case S_await_up:
		if (state->RC != 0) { /* temporary kludge */
			if (state->RC < state->N1) {
				state->RC++;

				if(!state->wide) {
#ifdef linux
					printf("%sXtimeout %ld\n",KERN_DEBUG,jiffies);
#endif
#if 0
					{
						int i;
						for (i=0;i<64;i++) {
								printf("%08x ", *(i-1+(unsigned long *)&state));
								if(!((i+1) & 0x03))
										printf("\n");
						}
					}
#endif
				}
				err2 = xmit3 (state, 1, (state->wide ? L2_SABME : L2_SABM) | L2_PF_U);
				if (err2 == -EAGAIN)
					state->RC--;
				start_T (1, err2);
			} else {
				flush_I (state);
				printf("%sERR_G 2, %d\n",KERN_INFO,state->N1);
				msg_up (state, MDL_ERROR_IND, ERR_G);
				msg_up (state, DL_RELEASE_IND, 0);
				x75_setstate(state, S_down);
			}
		} else {
			state->RC = 1;
			start_T (1, err2);
			break;
		}
		break;
	case S_up:
		/*
		 * Implementation decision time. Retransmit the last frame? We choose
		 * not to because we are unable to clear the xmit queue.
		 */
		state->RC = 1;
		enquiry (state);
		start_T (1, err2);
		x75_setstate(state, S_recover);
		break;
	case S_await_down:
		if (state->RC < state->N1) {
			state->RC++;
			xmit3 (state, 1, L2_DISC | L2_PF_U);
			start_T (1, err2);
		} else {
			printf("%sERR_H 1\n",KERN_INFO);
			msg_up (state, MDL_ERROR_IND, ERR_H);
			msg_up (state, DL_RELEASE_CONF, 0);
			x75_setstate(state, S_down);
		}
		break;
	case S_recover:
		if (state->RC < state->N1) {
			enquiry (state);
			state->RC++;
			start_T (1, err2);
		} else {
			printf("%sERR_I 1\n",KERN_INFO);
			msg_up (state, MDL_ERROR_IND, ERR_I);
			establish (state);
			state->L3_req = 0;
		}
		break;
	default:;
	}
	x75_check_pending (state, 0);
	return;
}

/*
 * T3/T203 periodically sends an enquiry to make sure that the connection is
 * still alive.
 */
static void
x75_T3 (x75 state)
{
	state->T3 = 0;
	if (state->debug & 0x10)
		printf ("%sT%d.3 %d\n", KERN_DEBUG,state->debugnr, state->status);
	switch (state->status) {
	case S_up:
		x75_setstate(state, S_recover);
		(void) enquiry (state);	  /* Errors are handled by retransmission
								   * through T1 */
		state->RC = 0;
		break;
	default:;
	}
	return;
}

/*
 * "OOPS" time. The other side sent a bad frame.
 * 
 * There are some differences between X.25, X.75 and Q.921 in this area,
 * but given a conforming implementation on the other side this code
 * should not be executed anyway. (Yeah, right...)
 */
static int
send_FRMR (x75 state, uchar_t pf, uchar_t cntl1, uchar_t cntl2, uchar_t cmd, uchar_t w, uchar_t x, uchar_t y, uchar_t z)
{
	int err = 0;
	mblk_t *mb = allocb (state->offset + (state->wide ? 6 : 4), BPRI_HI);

	if (mb == NULL)
		return -ENOMEM;
	mb->b_rptr += state->offset;
	mb->b_wptr += state->offset;
	*mb->b_wptr++ = L2_FRMR | (pf ? L2_PF : 0);
	*mb->b_wptr++ = cntl1;
	if (state->wide) {
		*mb->b_wptr++ = cntl2;

		*mb->b_wptr++ = state->v_s << 1;
		*mb->b_wptr++ = (state->v_r << 1) | (cmd ? 1 : 0);
	} else {
		*mb->b_wptr++ = (state->v_r << 5) | (cmd ? 0x10 : 0) | (state->v_s << 1);
	}
	*mb->b_wptr++ = (w ? 1 : 0) | (x ? 2 : 0) | (y ? 4 : 0) | (z ? 8 : 0);
	if ((err = (*state->send) (state->ref, 0, mb)) != 0)
		freemsg (mb);
	return err;
}

/*
 * Send pending frames.
 */
#ifdef CONFIG_DEBUG_ISDN
int
deb_x75_check_pending (const char *deb_file, unsigned int deb_line, x75 state, char fromLow)
#else
int
x75_check_pending (x75 state, char fromLow)
#endif
{
	mblk_t *mb, *mb2;
	char did = 0;
	int k_now;
	int err = 0, err2;

#if 0 /* def CONFIG_DEBUG_ISDN */
	if(state->debug & 1)
		printf("%sCP%d %s:%d  ",KERN_DEBUG,state->debugnr,deb_file,deb_line);
#ifdef CONFIG_DEBUG_STREAMS
	cS_check(deb_file,deb_line,&state->UI,NULL);
#endif
#else
	if(state->debug & 1)
		printf("%sCP%d ",KERN_DEBUG,state->debugnr);
#endif

	if(state->status == S_free)
		return -ENXIO;

	while (state->UI.first != NULL && (state->cansend == NULL || (*state->cansend) (state->ref))) {
		mb2 = S_dequeue (&state->UI);
		if(mb2 == NULL)
			break;
		if( /* XXX */ 0 || DATA_REFS(mb2) > 1 || DATA_START(mb2) > mb2->b_rptr - 1) {
			mb = allocb (state->offset + 1, BPRI_MED);
			if (mb == NULL)
				break;
			mb->b_rptr += state->offset + 1;
			mb->b_wptr += state->offset + 1;
			linkb (mb, mb2);
		} else
			mb = mb2;
		*--mb->b_rptr = L2_UI;
		if (state->debug & 1)
			printf ("%sX%dc%x ", KERN_DEBUG,state->debugnr, mb->b_wptr[-1] & 0xFF);
		if ((err = (*state->send) (state->ref, state->asBroadcast ? 3 : 1, mb)) != 0) {
			if (err == -EAGAIN) { /* Undo the above */
				mb->b_rptr++;
				mb = pullupm(mb,1);
				S_requeue (&state->UI, mb);
			} else
				freemsg (mb);
			return 0;
		} else
		did ++;
	}
	/*
	 * If no connection established, bail out now. If recovering, don't try to
	 * send pending I frames because we're still waiting for an ack.
	 */
	if (state->status != S_up) {
		if(state->I.first != NULL) 
			printf("%sx75.%d: State %d/%s, pending\n",KERN_DEBUG,state->debugnr,state->status,x75_sname[state->status]);
		if ((state->status == S_await_up) && fromLow) {
			stop_T(1,err);
			x75_T1(state);
		}
		if (state->status != S_recover) {
			if(did && state->backenable)
				(*state->backenable) (state->ref);
			return -EAGAIN;
		}
	} else {
		did=0;
		/*
		 * Send frames until queue full or max # of outstanding frames reached.
		 */
		k_now = (state->v_s - state->v_a) & (state->wide ? 0x7F : 0x07);
		/* k_now: Number of sent but unack'd frames. */
		while (k_now < state->k && !state->RNR && (state->cansend == NULL || (*state->cansend) (state->ref))) {
			mb2 = S_nr (&state->I, k_now);
			if (mb2 == NULL)  /* No more work in queue */
				break;
			if( /* XXX */ 0 || DATA_REFS(mb2) > 2 || DATA_START(mb2) > mb2->b_rptr - (state->wide ? 2 : 1)) {
				int off = state->offset + (state->wide ? 2 : 1);
				mb = allocb (off, BPRI_HI);
				if (mb == NULL)
					break;
				mb->b_rptr += off;
				mb->b_wptr += off;
				linkb(mb,mb2);
			} else {
				mb = mb2;
			}
			if (state->wide) {
				*--mb->b_rptr = state->v_r << 1;
				*--mb->b_rptr = state->v_s << 1;
				if (state->debug & 1)
					printf ("%sX%dc%x.%x ", KERN_DEBUG,state->debugnr, mb->b_rptr[0] & 0xFF, mb->b_rptr[1] & 0xFF);
			} else {
				*--mb->b_rptr = (state->v_s << 1) | (state->v_r << 5);
				if (state->debug & 1)
					printf ("%sX%dc%x ", KERN_DEBUG,state->debugnr, mb->b_rptr[0] & 0xFF);
			}
			state->v_s = (state->v_s + 1) & (state->wide ? 0x7F : 0x07);
			if ((err = (*state->send) (state->ref, 1, mb)) != 0) {
				freemsg (mb);
				break;
			}
			k_now++;
			did++;
		}
		/* Start T1 if we're now waiting for an ack. */
		if (did && !state->T1) {
			stop_T (3, err);
			start_T (1, err);
		}
	}
	/*
	 * Send an ack packet if we didn't do it implicitly with a data frame,
	 * above.
	 * 
	 * TODO: Delay the ack if we can determine that an immediate ack is
	 * not needed, i.e. if the line delay is lower than (k-1) times the
	 * average(?) frame length.
	 */
	if (!state->sentRR) {
		if (state->canrecv == NULL || (*state->canrecv) (state->ref)) {
			state->sentRR = 1;
			did = 0;
			state->ack_pend = 1;		/* Send RR now. This makes sure the if statement, below, fires. */
		}
	} else {
		if (state->canrecv != NULL && !(*state->canrecv) (state->ref))
			state->sentRR = 0;
	}
	if (!did && state->ack_pend) {
		if (state->wide)
			err2 = xmit4 (state, 0, (state->sentRR ? L2_RR : L2_RNR), state->v_r << 1);
		else
			err2 = xmit3 (state, 0, (state->sentRR ? L2_RR : L2_RNR) | (state->v_r << 5));
		if(err2 == 0)
			state->ack_pend = 0;
		if (err == 0)
			err = err2;
	}
#if 0 /* def CONFIG_DEBUG_ISDN */
	else if(did) printf("%sNX send ",KERN_DEBUG );
	else printf("%sNX NoAckPend ",KERN_DEBUG );
#endif
	/*
	 * Ugly Hack time. Continuously ask the remote side what's going on while
	 * it is on RNR. This is for the benefit of partners who forget to send RR
	 * when they can accept data again.
	 */
	if (state->RNR && state->poll && state->trypoll) {
		if (state->wide)
			err2 = xmit4 (state, 1, (state->sentRR ? L2_RR : L2_RNR), state->v_r << 1 | L2_PF_S);
		else
			err2 = xmit3 (state, 1, (state->sentRR ? L2_RR : L2_RNR) | (state->v_r << 5) | L2_PF);
		if(err2 == 0) state->ack_pend = 0;
		if (err == 0)
			err = err2;
	}
	if (state->trypoll)
		state->trypoll = 0;
	return err;
}

/*
 * Check if the received N_R is reasonable, i.e. between v_a and v_s.
 */
static int
checkV (x75 state, uchar_t n_r)
{
	if ((n_r == state->v_a) && (n_r == state->v_s))
		return 1;
	if (state->debug & 0x08)
		printf ("%sChk%d %d <= %d <= %d\n",KERN_DEBUG,state->debugnr, state->v_a, n_r, state->v_s);
	if (state->v_a <= state->v_s) {
		if (state->v_a <= n_r && n_r <= state->v_s)
			return 1;
	} else {
		if (state->v_a <= n_r || n_r <= state->v_s)
			return 1;
	}
	printf ("\n%s*** X75-%d Sequence error: V_A %d, N_R %d, V_S %d\n",KERN_WARNING,
			state->debugnr, state->v_a, n_r, state->v_s);
	return 0;
}

/*
 * Deallocate acknowledged frames.
 */
static int
pull_up (x75 state, uchar_t n_r)
{
	int ms;
	char didsome = (state->v_a != n_r);

	if (!didsome)
		return 0;
	ms = splstr ();
	while (state->v_a != n_r && state->v_a != state->v_s &&
			state->I.first != NULL) {
		freemsg (S_dequeue (&state->I));
		if(state->errors > 0)
			--state->errors;
		state->v_a = (state->v_a + 1) & (state->wide ? 0x7F : 0x07);
	}
	if (state->v_a != n_r) {
		printf ("%sx75.%d consistency problem: v_a %d, n_r %d, v_s %d, nblk %d, firstblk %p\n",KERN_WARNING,
				state->debugnr, state->v_a, n_r, state->v_s, state->I.nblocks, state->I.first);
		splx (ms);
		return -EFAULT;
	}
	splx (ms);
	if (didsome && state->backenable)
		(*state->backenable) (state->ref);
	return 0;
}


/*
 * Process incoming frames.
 * 
 * This one's a biggie. Annex B of Q.921 is very helpful if you try to wade
 * through it all. Turning optimization on (having a compiler with a correct
 * optimizer may be necessary...) is a good way to make sure that the kernel
 * likes this code.
 *
 * This code went through GNU indent, which unfortunately doubled its
 * line count... Sometimes, life sucks. ;-)
 */
int
x75_recv (x75 state, char cmd, mblk_t * mb)
{
	uchar_t x1, x2 = 0;
	char pf = 0;
	int err = 0, err2;
	char isbroadcast = (cmd & 2);

	cmd &= 1;

	/*
	 * Currently, this code never returns anything other than zero because it
	 * always deallocates the incoming frame, which is because we always mess
	 * around with it. This may or may not be a good idea. I don't like special
	 * code for the first two or three bytes being continuous. Besides, in most
	 * cases the caller deallocates anyway if there is an error.
	 */
	if((mb = pullupm (mb, 0)) == NULL)
		return 0;

	x1 = *mb->b_rptr++;
	if (state->debug & 0x80) {
		if (state->wide) {
			if ((x1 & L2_m_SU) == L2_is_U) {
				printf ("%sR%d%c%x ",KERN_DEBUG, state->debugnr, cmd ? 'c' : 'r', x1);
			} else {
				if (mb != NULL)
					printf ("%sR%d%c%x.%x ", KERN_DEBUG,state->debugnr, cmd ? 'c' : 'r', x1, *mb->b_rptr & 0xFF);
				else
					printf ("%sR%d.half %x ",KERN_DEBUG, state->debugnr, x1);
			}
		} else {
			printf ("%sR%d%c%x ",KERN_DEBUG, state->debugnr, cmd ? 'c' : 'r', x1);
		}
	}
	mb = pullupm(mb,0);
	if ((x1 & L2_m_I) == L2_is_I) {		/* I frame */
		uchar_t n_r, n_s;

		if (isbroadcast) {		  /* Can't broadcast I frames! */
			if (mb != NULL)
				freemsg (mb);
			return /* EINVAL */ 0;
		}
		/* Extract N_R, N_S, P/F. */
		if (state->wide) {
			if (mb == NULL) {	  /* "I frame" without N_R. Not good. */
				err2 = send_FRMR (state, pf, x1, x2, cmd, 1, 1, 0, 0);
				if (err == 0)
					err = err2;
				return /* err */ 0;
			}
			x2 = *mb->b_rptr++;
			mb = pullupm (mb, 0);
			pf = x2 & L2_PF_I;
			n_s = (x1 >> 1) & 0x7F;
			n_r = (x2 >> 1) & 0x7F;
		} else {
			pf = x1 & L2_PF;
			x2 = 0;
			n_s = (x1 >> 1) & 0x07;
			n_r = (x1 >> 5) & 0x07;
		}
		if (!cmd || mb == NULL) {
			err2 = send_FRMR (state, pf, x1, x2, cmd, 1, 1, 0, 0);
			if (err == 0)
				err = err2;
			if (!cmd) {			  /* we shall process empty I frames. */
				if (mb)
					freemsg (mb);
				return /* err */ 0;
			}
		}
		switch (state->status) {
		case S_up:
			if ((state->sentRR = (state->canrecv == NULL || (*state->canrecv) (state->ref)))) {
					/* Room for the packet upstreams? */
				if (mb != NULL && n_s == state->v_r) {
					if ((err2 = (*state->recv) (state->ref, 0, mb)) != 0) {
						/* Hmmm... Assume I'm not ready after all. */
						if (err == 0)
							err = err2;
						goto dropit;	/* This is ugly, but easiest. */
					} else {
						state->v_r = (state->v_r + 1) & (state->wide ? 0x7f : 0x07);
						if(state->errors > 0)
							--state->errors;
						mb = NULL;/* Accepted, so forget about it here. */
					}
					state->inREJ = 0;
					if (pf) {	  /* Want immediate Ack. */
						if (state->wide)
							err2 = xmit4 (state, 0, L2_RR, (state->v_r << 1) | L2_PF_S);
						else
							err2 = xmit3 (state, 0, L2_RR | (state->v_r << 5) | L2_PF);
						if(err2 == 0)
							state->ack_pend = 0;
						if (err == 0)
							err = err2;
					} else {	  /* Remember that we have to ack this. */
						state->ack_pend = 1;
					}
				} else {		  /* Duplicate or early packet? Tell the other
								   * side to resync. */
					if (mb != NULL) {
						freemsg (mb);
						mb = NULL;
					}
					if (state->inREJ) {	/* Don't send more than one REJ; that
										 * would upset the protocol. */
						if (pf) {
							if (state->wide)
								err2 = xmit4 (state, 0, L2_RR, (state->v_r << 1) | L2_PF_S);
							else
								err2 = xmit3 (state, 0, L2_RR | (state->v_r << 5) | L2_PF);
							if(err2 == 0)
								state->ack_pend = 0;
							if (err == 0)
								err = err2;
						}
					} else {	  /* Send REJ. */
						state->inREJ = 1;
						if (state->wide)
							err2 = xmit4 (state, 0, L2_REJ, (state->v_r << 1) | (pf ? L2_PF_S : 0));
						else
							err2 = xmit3 (state, 0, L2_REJ | (state->v_r << 5) | (pf ? L2_PF : 0));
						if(err2 == 0)
							state->ack_pend = 0;
						if (err == 0)
							err = err2;
					}
				}
			} else {			  /* Packet not acceptable. Tell them that we
								   * are busy (or something). */
			  dropit:
				freemsg (mb);
				mb = NULL;
				if (pf) {
					if (state->wide)
						err2 = xmit4 (state, 0, L2_RNR, (state->v_r << 1) | L2_PF_S);
					else
						err2 = xmit3 (state, 0, L2_RNR | (state->v_r << 5) | L2_PF);
					if(err2 == 0)
						state->ack_pend = 0;
					if (err == 0)
						err = err2;
				}
			}
			if (checkV (state, n_r)) {	/* Packet in range */
				if (state->RNR) { /* other side not ready */
					err2 = pull_up (state, n_r);
					if (err == 0)
						err = err2;
				} else {
					if (n_r == state->v_s) {	/* Everything ack'd */
						err2 = pull_up (state, n_r);
						if (err == 0)
							err = err2;
						stop_T (1, err2);
						if (err == 0)
							err = err2;
						restart_T (3, err2);
						if (err == 0)
							err = err2;
					} else {
						if (n_r != state->v_a) {		/* Something ack'd */
							err2 = pull_up (state, n_r);
							if (err == 0)
								err = err2;
							restart_T (1, err2);
							if (err == 0)
								err = err2;
						}
						/* Else if nothing ack'd, do nothing. */
					}
				}
			} else {			  /* Uh oh. The packet is either totally out of
								   * it, or packets got reordered. Both cases
								   * are seriously bad. */
				err2 = recover_NR (state);
				if (err == 0)
					err = err2;
			}
			break;
		default:;
		}
		/* I frames in other states get dropped */
	} else if ((x1 & L2_m_SU) == L2_is_S) {		/* S frame */
		uchar_t n_r;
		uchar_t code;

		if (isbroadcast) {		  /* No broadcast S frames allowed either. */
			if (mb != NULL)
				freemsg (mb);
			return /* EINVAL */ 0;
		}
		if (state->wide) {		  /* "I frame" without N_R. Not good. */
			if (mb == NULL) {
				err2 = send_FRMR (state, pf, x1, x2, cmd, 1, 1, 0, 0);
				if (err == 0)
					err = err2;
				return /* err */ 0;
			}
			x2 = *mb->b_rptr++;
			n_r = (x2 >> 1) & 0x7F;
			code = x1;
			pf = x2 & L2_PF_S;
		} else {
			x2 = 0;
			n_r = (x1 >> 5) & 0x07;
			code = x1 & 0x0F;
			pf = x1 & L2_PF;
		}
		mb = pullupm (mb, 0);
		if (mb != NULL) {		  /* An S Frame with data field? Huh?? */
			err2 = send_FRMR (state, pf, x1, x2, cmd, 1, 1, 0, 0);
			if (err == 0)
				err = err2;
			freemsg (mb);
			return /* err */ 0;
		}
		switch (code) {
		case L2_RR:
			state->trypoll = 0;
			switch (state->status) {
			case S_up:
				if (cmd) {
					if (pf) {
						err2 = enq_resp (state);
						if (err == 0)
							err = err2;
					}
				} else {
					if (pf) {	  /* This should only happen while in the
								   * S_recover state ... or when doing the
								   * force-poll-while-RNR hack. Yes it _is_
								   * ugly. I know that. */
						if (!(state->RNR && state->poll)) {
							printf("%sERR_A 1, RNR %d poll %d\n",KERN_INFO,state->RNR,state->poll);
							err2 = msg_up (state, MDL_ERROR_IND, ERR_A);
							if (err == 0)
								err = err2;
						}
					}
				}
				state->RNR = 0;
				if (checkV (state, n_r)) {
					if (n_r == state->v_s) {
						err2 = pull_up (state, n_r);
						if (err == 0)
							err = err2;
						stop_T (1, err2);
						if (err == 0)
							err = err2;
						restart_T (3, err2);
						if (err == 0)
							err = err2;
					} else {
						if (n_r != state->v_a) {
							err2 = pull_up (state, n_r);
							if (err == 0)
								err = err2;
							restart_T (1, err2);
							if (err == 0)
								err = err2;
						}
					}
				} else {
					err2 = recover_NR (state);
					if (err == 0)
						err = err2;
				}
				break;
			case S_recover:
				state->RNR = 0;
				if (cmd) {
					if (pf) {
						err2 = enq_resp (state);
						if (err == 0)
							err = err2;
					}
					if (checkV (state, n_r)) {
						err2 = pull_up (state, n_r);
						if (err == 0)
							err = err2;
					} else {
						err2 = recover_NR (state);
						if (err == 0)
							err = err2;
					}
				} else {
					if (pf) {
						if (checkV (state, n_r)) {
							err2 = pull_up (state, n_r);
							if (err == 0)
								err = err2;
							stop_T (1, err2);
							if (err == 0)
								err = err2;
							start_T (3, err2);
							if (err == 0)
								err = err2;
							err2 = retransmit (state);
							if (err == 0)
								err = err2;
							x75_setstate(state, S_up);
						} else {
							err2 = recover_NR (state);
							if (err == 0)
								err = err2;
						}
					} else {
						if (checkV (state, n_r)) {
							err2 = pull_up (state, n_r);
							if (err == 0)
								err = err2;
						} else {
							err2 = recover_NR (state);
							if (err == 0)
								err = err2;
						}
					}
				}
				break;
			default:;
			}
			break;
		case L2_RNR:
			state->trypoll = !pf;
			switch (state->status) {
			case S_up:
				if (cmd) {
					if (pf) {
						err2 = enq_resp (state);
						if (err == 0)
							err = err2;
					}
				} else {
					if (pf) {
						if (!(state->poll && state->RNR)) {
							printf("%sERR_A 2\n",KERN_INFO );
							err2 = msg_up (state, MDL_ERROR_IND, ERR_A);
							if (err == 0)
								err = err2;
						}
					}
				}
				state->RNR = 1;
				if (checkV (state, n_r)) {
					err2 = pull_up (state, n_r);
					if (err == 0)
						err = err2;
					stop_T (1, err2);
					if (err == 0)
						err = err2;
					restart_T (3, err2);
					if (err == 0)
						err = err2;
				} else {
					err2 = recover_NR (state);
					if (err == 0)
						err = err2;
				}
				break;
			case S_recover:
				state->RNR = 1;
				if (cmd) {
					if (pf) {
						err2 = enq_resp (state);
						if (err == 0)
							err = err2;
					}
					if (checkV (state, n_r)) {
						err2 = pull_up (state, n_r);
						if (err == 0)
							err = err2;
					} else {
						err2 = recover_NR (state);
						if (err == 0)
							err = err2;
					}
				} else {
					if (pf) {
						if (checkV (state, n_r)) {
							err2 = pull_up (state, n_r);
							if (err == 0)
								err = err2;
							stop_T (1, err2);
							if (err == 0)
								err = err2;
							start_T (3, err2);
							if (err == 0)
								err = err2;
							err2 = retransmit (state);
							if (err == 0)
								err = err2;
							x75_setstate(state, S_up);
						} else {
							err2 = recover_NR (state);
							if (err == 0)
								err = err2;
						}
					} else {
						if (checkV (state, n_r)) {
							err2 = pull_up (state, n_r);
							if (err == 0)
								err = err2;
						} else {
							err2 = recover_NR (state);
							if (err == 0)
								err = err2;
						}
					}
				}
				break;
			default:;
			}
			break;
		case L2_REJ:
			state->trypoll = 0;
			switch (state->status) {
			case S_up:
				if (cmd) {
					if (pf) {
						err2 = enq_resp (state);
						if (err == 0)
							err = err2;
					}
				} else {
					if (pf) {
						if (!(state->poll && state->RNR)) {
							printf("%sERR_A 3\n",KERN_INFO );
							err2 = msg_up (state, MDL_ERROR_IND, ERR_A);
							if (err == 0)
								err = err2;
						}
					}
				}
				state->RNR = 0;
				if (checkV (state, n_r)) {
					err2 = pull_up (state, n_r);
					if (err == 0)
						err = err2;
					stop_T (1, err2);
					if (err == 0)
						err = err2;
					start_T (3, err2);
					if (err == 0)
						err = err2;
					err2 = retransmit (state);
					if (err == 0)
						err = err2;
				} else {
					err2 = recover_NR (state);
					if (err == 0)
						err = err2;
				}
				break;
			case S_recover:
				state->RNR = 0;
				if (cmd) {
					if (pf) {
						err2 = enq_resp (state);
						if (err == 0)
							err = err2;
					}
					if (checkV (state, n_r)) {
						err2 = pull_up (state, n_r);
						if (err == 0)
							err = err2;
					} else {
						err2 = recover_NR (state);
						if (err == 0)
							err = err2;
					}
				} else {
					if (pf) {
						if (checkV (state, n_r)) {
							err2 = pull_up (state, n_r);
							if (err == 0)
								err = err2;
							stop_T (1, err2);
							if (err == 0)
								err = err2;
							start_T (3, err2);
							if (err == 0)
								err = err2;
							err2 = retransmit (state);
							if (err == 0)
								err = err2;
							x75_setstate(state, S_up);
						} else {
							err2 = recover_NR (state);
							if (err == 0)
								err = err2;
						}
					} else {
						if (checkV (state, n_r)) {
							err2 = pull_up (state, n_r);
							if (err == 0)
								err = err2;
						} else {
							err2 = recover_NR (state);
							if (err == 0)
								err = err2;
						}
					}
				}
				break;
			default:;
			}
			break;
		default:
			err2 = send_FRMR (state, pf, x1, x2, cmd, 1, 0, 0, 0);
			if (err == 0)
				err = err2;
			printf("%sERR_L 1\n",KERN_INFO);
			err2 = msg_up (state, MDL_ERROR_IND, ERR_L);
			if (err == 0)
				err = err2;
			break;
		}
	} else {					  /* U frame */
		uchar_t code;

		if (state->wide) {
			pf = (x1 & L2_PF_U);
			code = x1 & ~L2_PF_U;
		} else {
			pf = (x1 & L2_PF);
			code = x1 & ~L2_PF;
		}
		if (isbroadcast && (code != L2_UI || !cmd)) {
			if (mb != NULL)
				freemsg (mb);
			return /* EINVAL */ 0;
		}
#define L2__CMD 0x100		  /* Out of range -- makes for a simpler case
							   * statement */
		switch (code | (cmd ? L2__CMD : 0)) {
		case L2_SABME | L2__CMD:
		case L2_SABM | L2__CMD:
			if (mb != NULL) {
				err2 = send_FRMR (state, pf, x1, 0, cmd, 1, 1, 0, 0);
				if (err == 0)
					err = err2;
				printf("%sERR_N 1\n",KERN_INFO );
				err2 = msg_up (state, MDL_ERROR_IND, ERR_N);
				if (err == 0)
					err = err2;
				break;
			}
			if ((code == L2_SABME) != (state->wide != 0)) {
				/* Configured extended mode, got normal mode, or vice versa */
				printf("%sERR_? 1\n", KERN_INFO );
				err2 = send_FRMR (state, pf, x1, 0, cmd, 1, 0, 0, 0);
				if (err == 0)
					err = err2;
				break;
			}
			switch (state->status) {
			case S_down:
				if(state->broadcast) {
					err2 = xmit3 (state, 0, L2_DM | (pf ? L2_PF : 0));
					if (err == 0)
						err = err2;
					err2 = clr_except (state);
					if (err == 0)
						err = err2;
				} else {
					err2 = xmit3 (state, 0, L2_UA | (pf ? L2_PF : 0));
					if (err == 0)
						err = err2;
					err2 = clr_except (state);
					if (err == 0)
						err = err2;
					err2 = msg_up (state, DL_ESTABLISH_IND, 0);
					if (err == 0)
						err = err2;
					err2 = flush_I (state);
					if (err == 0)
						err = err2;
					stop_T (1, err2);
					if (err == 0)
						err = err2;
					start_T (3, err2);
					if (err == 0)
						err = err2;
					x75_setstate(state, S_up);
					if(state->backenable)
						(*state->backenable) (state->ref);
				}
				break;
			case S_await_up:
				err2 = xmit3 (state, 0, L2_UA | (pf ? L2_PF : 0));
				if (err == 0)
					err = err2;
				break;
			case S_await_down:
				err2 = xmit3 (state, 0, L2_DM | (pf ? L2_PF : 0));
				if (err == 0)
					err = err2;
				break;
			case S_up:
			case S_recover:
				err2 = xmit3 (state, 0, L2_UA | (pf ? L2_PF : 0));
				if (err == 0)
					err = err2;
				err2 = clr_except (state);
				if (err == 0)
					err = err2;
				printf("%sERR_F 1\n",KERN_INFO );
				err2 = msg_up (state, MDL_ERROR_IND, ERR_F);
				if (err == 0)
					err = err2;
				if (state->v_s != state->v_a) {
					err2 = flush_I (state);
					if (err == 0)
						err = err2;
					err2 = msg_up (state, DL_ESTABLISH_IND, 0);
					if (err == 0)
						err = err2;
				} else {
					err2 = flush_I (state);
					if (err == 0)
						err = err2;
				}
				stop_T (1, err2);
				if (err == 0)
					err = err2;
				start_T (3, err2);
				if (err == 0)
					err = err2;
				x75_setstate(state, S_up);
				break;
			case S_free:;
			}
			break;
		case L2_DISC | L2__CMD:
			if (mb != NULL) {
				err2 = send_FRMR (state, pf, x1, x2, cmd, 1, 1, 0, 0);
				if (err == 0)
					err = err2;
				printf("%sERR_N 2\n",KERN_INFO );
				err2 = msg_up (state, MDL_ERROR_IND, ERR_N);
				if (err == 0)
					err = err2;
				break;
			}
			switch (state->status) {
			case S_down:
			case S_await_down:
				err2 = xmit3 (state, 0, L2_UA | (pf ? L2_PF : 0));
				if (err == 0)
					err = err2;
				break;
			case S_await_up:
				err2 = xmit3 (state, 0, L2_DM | (pf ? L2_PF : 0));
				if (err == 0)
					err = err2;
				break;
			case S_up:
				err2 = flush_I (state);
				if (err == 0)
					err = err2;
				err2 = xmit3 (state, 0, L2_UA | (pf ? L2_PF : 0));
				if (err == 0)
					err = err2;
				err2 = msg_up (state, DL_RELEASE_IND, 0);
				if (err == 0)
					err = err2;
				stop_T (1, err2);
				if (err == 0)
					err = err2;
				stop_T (3, err2);
				if (err == 0)
					err = err2;
				x75_setstate(state, S_down);
				break;
			case S_recover:
				err2 = flush_I (state);
				if (err == 0)
					err = err2;
				err2 = xmit3 (state, 0, L2_UA | (pf ? L2_PF : 0));
				if (err == 0)
					err = err2;
				err2 = msg_up (state, DL_RELEASE_IND, 0);
				if (err == 0)
					err = err2;
				stop_T (1, err2);
				if (err == 0)
					err = err2;
				x75_setstate(state, S_down);
				break;
			case S_free:;
			}
			break;
		case L2_DM:
			if (mb != NULL) {
				err2 = send_FRMR (state, pf, x1, x2, cmd, 1, 1, 0, 0);
				if (err == 0)
					err = err2;
				printf("%sERR_N 3\n",KERN_INFO );
				err2 = msg_up (state, MDL_ERROR_IND, ERR_N);
				if (err == 0)
					err = err2;
				break;
			}
			switch (state->status) {
			case S_down:
				if (!pf) {
					err2 = establish (state);
					if (err == 0)
						err = err2;
					state->L3_req = 0;
				}
				break;
			case S_await_up:
				if (pf) {
					err2 = flush_I (state);
					if (err == 0)
						err = err2;
					err2 = msg_up (state, DL_RELEASE_IND, 0);
					if (err == 0)
						err = err2;
					stop_T (1, err2);
					if (err == 0)
						err = err2;
					x75_setstate(state, S_down);
				}
				break;
			case S_await_down:
				if (pf) {
					err2 = flush_I (state);
					if (err == 0)
						err = err2;
					err2 = msg_up (state, DL_RELEASE_CONF, 0);
					if (err == 0)
						err = err2;
					stop_T (1, err2);
					if (err == 0)
						err = err2;
					x75_setstate(state, S_down);
				}
				break;
			case S_up:
			case S_recover:
				if (pf) {
					printf("%sERR_B 1\n",KERN_INFO );
					err2 = msg_up (state, MDL_ERROR_IND, ERR_B);
					if (err == 0)
						err = err2;
				} else {
					printf("%sERR_E 1\n",KERN_INFO );
					err2 = msg_up (state, MDL_ERROR_IND, ERR_E);
					if (err == 0)
						err = err2;
					err2 = establish (state);
					if (err == 0)
						err = err2;
					state->L3_req = 0;
				}
				break;
			case S_free:;
			}
			break;
		case L2_UA:
			if (mb != NULL) {
				err2 = send_FRMR (state, pf, x1, x2, cmd, 1, 1, 0, 0);
				if (err == 0)
					err = err2;
				printf("%sERR_N\n",KERN_INFO );
				err2 = msg_up (state, MDL_ERROR_IND, ERR_N);
				if (err == 0)
					err = err2;
				break;
			}
			switch (state->status) {
			case S_up:
			case S_down:
			case S_recover:
				printf("%sERR_CD 1\n",KERN_INFO );
				err2 = msg_up (state, MDL_ERROR_IND, ERR_C | ERR_D);
				if (err == 0)
					err = err2;
				break;
			case S_await_up:
				if (pf) {
					if (state->L3_req) {
						err2 = msg_up (state, DL_ESTABLISH_CONF, 0);
						if (err == 0)
							err = err2;
					} else if (state->v_s != state->v_a) {
						err2 = flush_I (state);
						if (err == 0)
							err = err2;
						err2 = msg_up (state, DL_ESTABLISH_IND, 0);
						if (err == 0)
							err = err2;
					}
					x75_setstate(state, S_up);
					stop_T (1, err2);
					if (err == 0)
						err = err2;
					start_T (3, err2);
					if (err == 0)
						err = err2;
					state->v_r = state->v_s = state->v_a = 0;
					if(state->backenable)
						(*state->backenable) (state->ref);
				} else {
					printf("%sERR_D 1\n",KERN_INFO );
					err2 = msg_up (state, MDL_ERROR_IND, ERR_D);
				}
				if (err == 0)
					err = err2;
				break;
			case S_await_down:
				if (pf) {
					err2 = msg_up (state, DL_RELEASE_CONF, 0);
					if (err == 0)
						err = err2;
					stop_T (1, err2);
					if (err == 0)
						err = err2;
					x75_setstate(state, S_down);
				} else {
					printf("%sERR_D 2\n",KERN_INFO );
					err2 = msg_up (state, MDL_ERROR_IND, ERR_D);
				}
				if (err == 0)
					err = err2;
				break;
			case S_free:;
			}
			break;
		case L2_UI | L2__CMD:
			if (mb == NULL) {	  /* Missing data. */
				if (!isbroadcast) {
					err2 = send_FRMR (state, pf, x1, x2, cmd, 1, 1, 0, 0);
					if (err == 0)
						err = err2;
				}
				break;
			}
			if ((err2 = (*state->recv) (state->ref, isbroadcast ? 3 : 1, mb)) != 0) {
				if (err == 0)
					err = err2;
				freemsg (mb);
			}
			mb = NULL;
			break;
		case L2_XID | L2__CMD:
		case L2_XID:			  /* TODO: Do something about XID frames. */
			break;
		case L2_FRMR:
		case L2_FRMR | L2__CMD:	/* technically an invalid frame, but replying with 
									FRMR here is _bad_ */
			printf("%sERR_D 3\n",KERN_INFO );
			err2 = msg_up (state, MDL_ERROR_IND, ERR_D);
			if (err == 0)
				err = err2;
			if (state->status == S_up || state->status == S_recover) {
				establish (state);
				state->L3_req = 0;
			}
			break;
		default:
			err = -EINVAL;
			err2 = send_FRMR (state, pf, x1, x2, cmd, 1, 0, 0, 0);
			if (err == 0)
				err = err2;
			printf("%sERR_L 2\n",KERN_INFO );
			err2 = msg_up (state, MDL_ERROR_IND, ERR_L);
			if (err == 0)
				err = err2;
			break;
		}
	}
	err2 = x75_check_pending (state, 0);	/* if (err == 0) err = err2; */
	if (mb != NULL)
		freemsg (mb);
	return /* err */ 0;
}

/*
 * Enqueue frame to be sent out.
 * Empty messages are silently discarded.
 */
int
x75_send (x75 state, char isUI, mblk_t * mb)
{
	if (msgdsize(mb) <= 0) 
		freemsg(mb);
	else if (isUI)
		S_enqueue (&state->UI, mb);
	else {
		if(state->broadcast)
			return -ENXIO;
		S_enqueue (&state->I, mb);
	}

	state->asBroadcast = (isUI > 1);

	(void) x75_check_pending (state, 0);	/* Send the frame, if possible */
	return 0;
}

/*
 * Test if we can send.
 */
int
x75_cansend (x75 state, char isUI)
{
	if(state->cansend != NULL)
		(void)(*state->cansend) (state->ref); /* Trigger bringing L1 up */
	if (isUI)
		return (state->I.nblocks < 3);		/* arbitrary maximum */
	else						  /* This allows us to enqueue one additional
								   * frame, which is a Good Thing. */
		return (state->I.nblocks <= state->k);
}

/*
 * Test if we can receive.
 */
int
x75_canrecv (x75 state)
{
	/* Just ask the upper layer. */
	return (*state->canrecv) (state->ref);
}

/*
 * Take the X75 layer up / down.
 */
#ifdef CONFIG_DEBUG_ISDN
int
deb_x75_changestate (const char *deb_file,unsigned int deb_line, x75 state, uchar_t ind, char isabort)
#else
int
x75_changestate (x75 state, uchar_t ind, char isabort)
#endif
{
	int ms = splstr ();
	int err = 0, err2 = 0;
	int nonestablish = 1;

#ifdef CONFIG_DEBUG_ISDN
	if(state->debug & 0x10)
	    printf("%sx75.%d: State %d/%s for ind %d %d from %s:%d\n",KERN_DEBUG,state->debugnr,state->status,
			x75_sname[state->status],ind,isabort,deb_file,deb_line);
#else
	if(state->debug & 0x10)
	    printf("%sx75.%d: State %d/%s for ind %d %d\n",KERN_DEBUG,state->debugnr,state->status,
			x75_sname[state->status],ind,isabort);
#endif
	if (isabort)
		goto doabort;
	switch (ind) {
	default:
		err = -ENOENT;
		break;
	case PH_ACTIVATE_IND:
	case PH_ACTIVATE_CONF:
		msg_up (state, ind, 0);
		break;
	case DL_ESTABLISH_CONF:	  /* Force establishment, for situations where
								   * we dont need the initial handshake. This
								   * usually happens because an idiot
								   * implementor doesn't want to implement U
								   * frame handling for automode. */
		if (state->status == S_up || state->status == S_recover)
			break;				  /* Already established. */
		if (state->debug & 0x10)
			printf ("%sX75%d: Forced establish.\n",KERN_DEBUG, state->debugnr);
		 /* flush_I (state); */
		state->errors = 0;
		if (state->status != S_down && state->status != S_free)
			stop_T (1, err2);
		if (err == 0)
			err = err2;
		start_T (3, err2);
		if (err == 0)
			err = err2;
		x75_setstate(state, S_up);
		msg_up (state, DL_ESTABLISH_CONF, 0);
		if(state->backenable)
			(*state->backenable) (state->ref);
		break;
	case DL_ESTABLISH_REQ:
	case DL_ESTABLISH_IND:		  /* Take it up. */
		switch (state->status) {
		case S_down:
		case S_await_down:
			if(ind == DL_ESTABLISH_IND /* && state->UI.first == NULL && state->UI.first == NULL */ ) {
				if(0)printf("%sx75.%d: DL_ESTABLISH_IND, down, nothing done\n",KERN_DEBUG,state->debugnr);
				break;
			}
			err = establish (state);
			nonestablish = 0;
			state->L3_req = 1;
			state->errors = 0;
			break;
		case S_await_up:
			if (ind == DL_ESTABLISH_REQ)
				break;
#if 0 /* Q.921 says to do this, but I can't think of a reason to. */
	/* This flush also breaks top-down on-demand connection setup,
	   i.e. starting up the lower layer automatically if the upper
       layer has some data to deliver. */
			err = flush_I (state);
#endif
			state->L3_req = 1;
			break;
		case S_up:
		case S_recover:		  /* L1 reestablishment */
			if (ind == DL_ESTABLISH_REQ)
				break;
			err = flush_I (state);
			err2 = establish (state);
			nonestablish = 0;
			if (err == 0)
				err = err2;
			state->L3_req = 1;
			break;
		default:;
		}
		break;
	case DL_RELEASE_REQ:		  /* Take it down normally. */
		state->errors = 0;
		switch (state->status) {
		case S_down:
			err = msg_up (state, DL_RELEASE_CONF, 0);
			break;
		case S_up:
			x75_setstate(state, S_await_down);
			err = flush_I (state);
			state->RC = 0;
			err2 = xmit3 (state, 1, L2_DISC | L2_PF_U);
			if (err == 0)
				err = err2;
			stop_T (3, err2);
			if (err == 0)
				err = err2;
			restart_T (1, err2);
			if (err == 0)
				err = err2;
			break;
		case S_recover:
			x75_setstate(state, S_await_down);
			err = flush_I (state);
			state->RC = 0;
			err2 = xmit3 (state, 1, L2_DISC | L2_PF_U);
			if (err == 0)
				err = err2;
			restart_T (1, err2);
			if (err == 0)
				err = err2;
			break;
		default:;
		}
		break;

	case PH_DISCONNECT_IND:
	case MDL_REMOVE_REQ:
		switch (state->status) {
		case S_await_up:
		case S_down:
		case S_up:
		case S_recover:
			err = kill_me (state, ind);
			break;
		case S_await_down:
		case S_free:;
		}
	case DL_RELEASE_CONF:
	case PH_DEACTIVATE_IND:
	case PH_DEACTIVATE_CONF:
	  doabort:					  /* Just disconnect. The other side will
								   * either also realize that L1 is down, or
								   * time out eventually. */
		switch (state->status) {
		case S_await_up:
			if (ind == PH_DEACTIVATE_IND)
				break;
		case S_down:
		case S_up:
		case S_recover:
			err = kill_me (state, DL_RELEASE_IND);
			break;
		case S_await_down:
			err = kill_me (state, DL_RELEASE_CONF);
			break;
		case S_free:;
		}
		x75_setstate(state, S_down);
	}
	if (err == 0) {
		err2 = x75_check_pending (state, nonestablish);
		if (err == 0)
			err = err2;
	}
	splx (ms);
	return err;
}

/*
 * Initialize data.
 */
int
x75_initconn (x75 state)
{
	bzero (&state->I, sizeof (struct _smallq));
	bzero (&state->UI, sizeof (struct _smallq));

	state->v_a = 0;
	state->v_s = 0;
	state->v_r = 0;
	state->RC = 0;
	state->status = S_down;
	state->L3_req = 0;
	state->RNR = 0;
	state->sentRR = 1;
	state->errors = 0;
	state->ack_pend = 0;
	state->inREJ = 0;
	state->T1 = 0;
	state->T3 = 0;
	if (state->k == 0 || (state->k > (state->wide ? 127 : 7)))
		state->k = 1;
	if (state->N1 == 0)
		state->N1 = 3;
	if (state->RUN_T1 == 0)
		state->RUN_T1 = 10;
	if (state->RUN_T3 == 0)
		state->RUN_T3 = 100;
	if (state->RUN_T3 < state->RUN_T1 * 2)
		state->RUN_T3 = state->RUN_T1 * 2;
	if (state->send == NULL
			|| state->recv == NULL
			|| state->state == NULL)
		return -EFAULT;
	if(0)printf("%sX75 %d: Init %d %d\n",KERN_DEBUG,state->debugnr,state->RUN_T1,state->RUN_T3);
	return 0;
}


#ifdef MODULE
static int do_init_module(void)
{
	return 0;
}

static int do_exit_module(void)
{
	return 0;
}
#endif
