#include "primitives.h"
#include "tei.h"
#include "streamlib.h"
#include "isdn_23.h"
#include "isdn_34.h"
#include "lap.h"
#include "dump.h"
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/param.h>
#include <sys/utsname.h>
#include "sapi.h"

#define TEI_IDREQ 1
#define TEI_IDASS 2
#define TEI_IDDENY 3
#define TEI_IDCHECK_REQ 4
#define TEI_IDCHECK_RESP 5
#define TEI_IDREMOVE 6
#define TEI_IDVERIFY 7

#if NITALK <= 2
#error "Need NITALK > 2"
#endif
#define NR talki[0]
#define TEI_id talki[1]
#ifdef NEW_TIMEOUT
#define timer talki[2]
#endif
#define N201 2
#define T201 2
#define N202 4
#define T202 2
#define ST_inited 02
#define ST_T201 04
#define ST_T202 010
#define ST_up 020
#define ST_want_T202 040

static int
tei_send_id (isdn3_card card, uchar_t TEI)
{
	mblk_t *mb;
	isdn23_hdr hdr;
	int err;
	extern int hdrseq;

	mb = allocb (sizeof (struct _isdn23_hdr), BPRI_MED);

	if (mb == NULL)
		return -EAGAIN;
	hdr = ((isdn23_hdr) mb->b_wptr)++;
#ifdef __CHECKER__
	bzero(hdr,sizeof(*hdr));
#endif
	hdr->key = HDR_TEI;
	hdr->seqnum = hdrseq; hdrseq += 2;
	hdr->hdr_tei.card = card->nr;
	hdr->hdr_tei.TEI = TEI;
	if ((err = isdn3_sendhdr (mb)) != 0)
		freemsg (mb);
	card->TEI = TEI;
	return err;
}

static void tei_T201 (isdn3_talk talk);
static void tei_T202 (isdn3_talk talk);

static void
tei_init (isdn3_talk talk)
{
	static ulong_t id = 1234;
	long flags = isdn3_flags(talk->card->info,SAPI_TEI,-1);

	/*
	 * if(!(talk->state & ST_inited)) isdn3_chstate(talk,0,0,CH_OPENPROT);
	 */
	switch(flags & FL_POINTMASK) {
	default:
	case FL_MULTIPOINT2:
		{
			struct utsname foo;
			char *x;

			uname(&foo); id = 0;
			x = foo.sysname   ; while(*x) id = id << 1 ^ id >> 8 ^ *x++;
			x = foo.nodename  ; while(*x) id = id << 2 ^ id >> 7 ^ *x++;
			x = foo.machine   ; while(*x) id = id << 3 ^ id >> 6 ^ *x++;
			x = foo.domainname; while(*x) id = id << 4 ^ id >> 5 ^ *x++;
		}
		break;
	case FL_MULTIPOINT1:
		id = getpid() + time(NULL) + (ulong_t)sbrk(0);
		break;
	}

	talk->TEI_id = id;

	talk->state |= ST_inited;
}

static void
tei_enquiry (isdn3_talk talk)
{
	mblk_t *mb;
	int err;
	long flags = isdn3_flags(talk->card->info,SAPI_TEI,-1);

	if(!(talk->state & ST_inited))
		tei_init(talk);
	if((flags & FL_POINTMASK) == FL_POINTOPOINT) {
		tei_send_id (talk->card,0);
		return;
	}
	if((flags & FL_POINTMASK) == FL_MULTIPOINT3) {
		tei_send_id (talk->card, TEI_FIXED);
		return;
	}
	if(talk->state & ST_T201) {
		talk->state &=~ ST_T201;
#ifdef NEW_TIMEOUT
		untimeout(talk->timer);
#else
		untimeout(tei_T201, talk);
#endif
	}
	mb = allocb (5, BPRI_LO);
	if (mb == NULL) {
		printf ("No Mem TEIassReq\n");
		--talk->NR;
	} else {
		*mb->b_wptr++ = 0x0F;
		*((ushort_t *) mb->b_wptr)++ = (talk->TEI_id &= 0x7F7F);
		*mb->b_wptr++ = TEI_IDREQ;
		*mb->b_wptr++ = 0xFF;
		if ((err = isdn3_send (talk, AS_UIBROADCAST, mb)) != 0) {
			--talk->NR;
			freemsg (mb);
		}
	}
	if (talk->state & ST_up) {
		if(log_34 & 2)printf("Timer T202 started\n");
		talk->state |= ST_T202;
		talk->state &=~ ST_want_T202;
#ifdef NEW_TIMEOUT
		talk->timer = 
#endif
		timeout (tei_T202, talk, T202 * HZ);
	} else  {
		if(0)printf("Wait for ST_up before starting T202\n");
		talk->state |= ST_want_T202;
	}
}

static void
tei_verify (isdn3_talk talk)
{
	mblk_t *mb;
	int err;

	if(talk->state & (ST_T201|ST_T202))
		return;
	else if(!(talk->state & ST_up)) {
		tei_enquiry(talk);
		return;
	}
	mb = allocb (5, BPRI_LO);
	if (mb == NULL) {
		printf ("No Mem TEIassReq\n");
		--talk->NR;
	} else {
		*mb->b_wptr++ = 0x0F;
		*((ushort_t *) mb->b_wptr)++ = talk->TEI_id;
		*mb->b_wptr++ = TEI_IDVERIFY;
		*mb->b_wptr++ = 0xFF;
		if ((err = isdn3_send (talk, AS_UIBROADCAST, mb)) != 0) {
			--talk->NR;
			freemsg (mb);
		}
	}
	talk->state |= ST_T201;
#ifdef NEW_TIMEOUT
	talk->timer =
#endif
	timeout (tei_T201, talk, T201 * HZ);
}

static void
tei_T201 (isdn3_talk talk)
{
	if(log_34 & 2)printf ("Timer TEI T201 %ld\n", talk->NR);
	if (talk->state & ST_T201) {
		talk->state &= ~ST_T201;
		if (!(talk->state & ST_up)) {
			talk->state |= ST_want_T202;
			return;
		}
		if (talk->NR >= N201 && tei_send_id (talk->card, TEI_BROADCAST) != -EAGAIN) {
			tei_enquiry(talk);
		} else {
			talk->NR++;
			tei_verify (talk);
		}
	}
}


static void
tei_T202 (isdn3_talk talk)
{
	if(log_34 & 2)printf ("Timer TEI T202 %ld\n", talk->NR);
	if (talk->state & ST_T202) {
		talk->state &= ~ST_T202;
		if (!(talk->state & ST_up)) {
			talk->state |= ST_want_T202;
			return;
		}
		if (talk->NR >= N202) {
			if (tei_send_id (talk->card, TEI_BROADCAST) == -EAGAIN) {
				talk->state |= ST_T202;
				talk->state &=~ ST_want_T202;
#ifdef NEW_TIMEOUT
				talk->timer = 
#endif
				timeout (&tei_T202, talk, T202 * HZ);
			}
		} else {
			talk->NR++;
			tei_enquiry (talk);
		}
	}
}


static int
tei_chstate (isdn3_talk talk, uchar_t ind, short add)
{
	if(0)printf ("TEI state for card %d says %s:%o\n", talk->card->nr, conv_ind(ind), add);
	switch (ind) {
	case PH_ACTIVATE_IND:
	case PH_ACTIVATE_CONF:
		talk->state |= ST_up;
		if(talk->state & ST_want_T202) {
			talk->state |= ST_T202;
			talk->state &=~ ST_want_T202;
#ifdef NEW_TIMEOUT
			talk->timer = 
#endif
			timeout (tei_T202, talk, T202 * HZ);
		}
		break;
	case PH_DEACTIVATE_IND:
	case PH_DEACTIVATE_CONF:
		talk->state &= ~ST_up;
		if (talk->state & ST_T202) {
			talk->state &=~ ST_T202;
#ifdef NEW_TIMEOUT
			untimeout(talk->timer);
#else
			untimeout (tei_T202, talk);
#endif
		} else if (talk->state & ST_T201) {
			talk->state &=~ ST_T201;
#ifdef NEW_TIMEOUT
			untimeout(talk->timer);
#else
			untimeout (tei_T201, talk);
#endif
		}
		/* Try again? */
		if(talk->card->TEI == TEI_BROADCAST)
			tei_enquiry(talk);
		break;
	case PH_DISCONNECT_IND:
		talk->state &= ~ST_inited; /* Leave this bit on, if and when */
		if (talk->state & ST_T202) {
#ifdef NEW_TIMEOUT
			untimeout(talk->timer);
#else
			untimeout (tei_T202, talk);
#endif
		} else if (talk->state & ST_T201) {
#ifdef NEW_TIMEOUT
			untimeout(talk->timer);
#else
			untimeout (tei_T201, talk);
#endif
		}
		break;
	case MDL_ERROR_IND:
		if(talk->state & ST_up)
			tei_verify(talk);
		else 
			talk->state |= ST_want_T202;
		break;
	}
	return 0;
}

static int
tei_recv (isdn3_talk talk, char isUI, mblk_t * data)
{
	ushort_t id;
	uchar_t key;
	mblk_t *mb;
	uchar_t TEI;
	long flags = isdn3_flags(talk->card->info,SAPI_TEI,-1);

	if (talk->state == 0) {
		tei_init (talk);
	}
	if (!(isUI & 2)) {			  /* got to be a broadcast */
		printf("TEI RejNoBroadcast\n");
		return -EINVAL;
	}
	mb = pullupm (data, 4);
	if (mb == NULL)
		return -ENOMEM;
	/* Do not return nonzero after this point */

	if (*mb->b_rptr++ != 0x0F) {
		freemsg (mb);
		return 0;
	}
	id = *((ushort_t *) mb->b_rptr)++;
	key = *mb->b_rptr++;
	mb = pullupm (mb, 0);
	TEI = *mb->b_rptr;
	if (!(TEI & 1))
		return 0;
	TEI >>= 1;
printf("R Key %x for %d\n",TEI, key);
	switch (key) {
	case TEI_IDDENY:
		if (id != (talk->TEI_id & 0xFFFF))
			break;
		switch(flags & FL_POINTMASK) {
		case FL_POINTOPOINT:
			TEI = 0;
			goto cfix;
		case FL_MULTIPOINT3:
			TEI = TEI_FIXED;
		cfix:
printf("Fixed TEI %d\n",TEI);
			if (talk->state & ST_T202) {
				talk->state &=~ ST_T202;
#ifdef NEW_TIMEOUT
				untimeout(talk->timer);
#else
				untimeout (tei_T202, talk);
#endif
			} else if (talk->state & ST_T201) {
				talk->state &=~ ST_T201;
#ifdef NEW_TIMEOUT
				untimeout(talk->timer);
#else
				untimeout (tei_T201, talk);
#endif
			}
			tei_send_id (talk->card, TEI);
			break;
		default:
printf("NonFixed TEI\n");
			tei_send_id (talk->card, TEI_BROADCAST);
			break;
		}
		tei_init (talk);
		break;
	case TEI_IDASS:
		if (id != (talk->TEI_id & 0xFFFF))
			break;
		if (talk->state & ST_T202) {
			talk->state &=~ ST_T202;
#ifdef NEW_TIMEOUT
			untimeout(talk->timer);
#else
			untimeout (tei_T202, talk);
#endif
			if (talk->state & ST_up) {
				tei_send_id (talk->card, TEI);
				tei_init (talk);
			} else
				printf ("TEI assign of %d without req\n", TEI);
		} else if (talk->state & ST_T201) {
			talk->state &=~ ST_T201;
#ifdef NEW_TIMEOUT
			untimeout(talk->timer);
#else
			untimeout (tei_T201, talk);
#endif
		}
		break;
	case TEI_IDCHECK_REQ:
		if (TEI != TEI_BROADCAST && TEI != talk->card->TEI) {
			if (talk->NR > 0)
				talk->NR--;
			break;
		}
		if (talk->card->TEI == TEI_BROADCAST)
			break;
		{
			mblk_t *mp;
			int err;

			if ((mp = allocb (5, BPRI_LO)) == NULL) {
				printf ("No Mem TEIchkReq\n");
			} else {
				*mp->b_wptr++ = 0x0F;
				*((ushort_t *) mp->b_wptr)++ = talk->TEI_id;
				*mp->b_wptr++ = TEI_IDCHECK_RESP;
				*mp->b_wptr++ = (talk->card->TEI << 1) | 1;;
				if ((err = isdn3_send (talk, AS_UIBROADCAST, mp)) != 0) {
					freemsg (mp);
				}
			}
			if(talk->state & ST_T202) {
				talk->state &=~ ST_T202;
#ifdef NEW_TIMEOUT
				untimeout(talk->timer);
#else
				untimeout(tei_T202, talk);
#endif
			} else if(talk->state & ST_T201) {
				talk->state &=~ ST_T201;
#ifdef NEW_TIMEOUT
				untimeout(talk->timer);
#else
				untimeout(tei_T201, talk);
#endif
			}
		}
		break;
	case TEI_IDREMOVE:
		if (TEI != talk->card->TEI)
			break;
		if(talk->state & ST_T202) {
			talk->state &=~ ST_T202;
#ifdef NEW_TIMEOUT
			untimeout(talk->timer);
#else
			untimeout(tei_T202, talk);
#endif
		} else if(talk->state & ST_T201) {
			talk->state &=~ ST_T201;
#ifdef NEW_TIMEOUT
			untimeout(talk->timer);
#else
			untimeout(tei_T201, talk);
#endif
		}
		tei_send_id (talk->card, TEI_BROADCAST);
		break;
	}
	if (mb != NULL)
		freemsg (mb);
	return 0;
}

static int
tei_sendcmd (isdn3_conn conn, ushort_t id, mblk_t * data)
{
	printf("TEI SendCmd, config problem!\n");
	return -EINVAL;
}

static int
tei_send (isdn3_conn conn, mblk_t * data)
{
	return -EINVAL;
}

static void
tei_kill (isdn3_talk talk, char force)
{
	if (talk->state & ST_T202) {
		talk->state &=~ ST_T202;
#ifdef NEW_TIMEOUT
		untimeout(talk->timer);
#else
		untimeout (tei_T202, talk);
#endif
	} else if (talk->state & ST_T201) {
		talk->state &=~ ST_T201;
#ifdef NEW_TIMEOUT
		untimeout(talk->timer);
#else
		untimeout (tei_T201, talk);
#endif
	}
	if (talk->state & ST_inited) {
		isdn3_chstate (talk, 0, 0, CH_CLOSEPROT);
		talk->state &=~ ST_inited;
	}
}

static void
tei_newcard (isdn3_card card)
{
	long flags = isdn3_flags(card->info,SAPI_TEI,-1);
	(void) isdn3_findtalk (card, &TEI_hndl, NULL, 1);

	if(flags & FL_TEI_IMMED)
		tei_getid (card);
}

int
tei_getid (isdn3_card card)
{
	isdn3_hndl hndl;
	isdn3_talk talk;

	if ((hndl = isdn3_findhndl (SAPI_TEI)) == NULL)
		return -ENXIO;
	if ((talk = isdn3_findtalk (card, hndl, NULL, 0)) == NULL)
		return -ENXIO;

	if (!talk->state & ST_inited)
		tei_init (talk);
	if (talk->state & ST_T202)
		return 0;
	talk->NR = 0;
	if(0)printf ("TEI GetId\n");
	tei_enquiry (talk);
	return 0;
}


struct _isdn3_hndl TEI_hndl =
{
		NULL, SAPI_TEI,1,
		NULL, &tei_newcard, &tei_chstate, NULL, &tei_recv,
		&tei_send, &tei_sendcmd, &tei_kill, NULL, NULL, NULL,
};
