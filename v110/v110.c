/**
 ** Streams V.110 module.
 **/

/*
 * Simple implementation. Handles 38400 bps _only_.
 */

#include "f_module.h"
#include "primitives.h"
#include <sys/types.h>
#include <sys/time.h>
#include "f_signal.h"
#include "f_malloc.h"
#include <sys/param.h>
#include <sys/sysmacros.h>
#include "streams.h"
#include "stropts.h"
#ifdef DONT_ADDERROR
#include "f_user.h"
#endif
#include <sys/errno.h>
#include "streamlib.h"
#include "v110.h"
#include "isdn_proto.h"

extern void log_printmsg (void *log, const char *, mblk_t *, const char*);

/*
 * Standard Streams stuff.
 */
static struct module_info v110_minfo =
{
		0, "v110", 0, INFPSZ, 200,100
};

static qf_open v110_open;
static qf_close v110_close;
static qf_srv v110_rsrv,v110_wsrv;
static qf_put v110_rput,v110_wput;

static struct qinit v110_rinit =
{
		v110_rput, v110_rsrv, v110_open, v110_close, NULL, &v110_minfo, NULL
};

static struct qinit v110_winit =
{
		v110_wput, v110_wsrv, NULL, NULL, NULL, &v110_minfo, NULL
};

struct streamtab v110info =
{&v110_rinit, &v110_winit, NULL, NULL};

struct v110_ {
	queue_t *q;
	char flags;
#define V110_INUSE 01
	char state;
	char bits;
	char oldc;
	char ctab[80];
	short ctablen;
	short ctabsafe;
};

/*
 * Initialize with reasonable default values.
 */
static int
v110__init (struct v110_ *v110)
{
	v110->state = 0;
	v110->bits = 0;
	return 0;
}



static void
v110_proto (queue_t * q, mblk_t * mp)
{
	/* we don't do anything here ... yet? */

	putnext (q, mp);
}


/* Streams code to open the driver. */
static int
v110_open (queue_t * q, dev_t dev, int flag, int sflag ERR_DECL)
{
	struct v110_ *v110;

	if (q->q_ptr) {
		return 0;
	}
	v110 = malloc(sizeof(*v110));
	if(v110 == NULL)
		ERR_RETURN(-ENOMEM);
	memset(v110,0,sizeof(*v110));
	WR (q)->q_ptr = (char *) v110;
	q->q_ptr = (char *) v110;

	v110->q = q;
	v110->flags = V110_INUSE;
	v110__init (v110);

	MORE_USE;
	return 0;
}

/* Streams code to close the driver. */
static void
v110_close (queue_t * q, int dummy)
{
	register struct v110_ *v110;

	v110 = (struct v110_ *) q->q_ptr;

	flushq (q, FLUSHALL);
	flushq (WR (q), FLUSHALL);

	free(v110);
	LESS_USE;
	return;
}


/* Streams code to write data. */
static void
v110_wput (queue_t * q, mblk_t * mp)
{
	switch (DATA_TYPE(mp)) {
	case MSG_PROTO:
	case CASE_DATA:
		putq (q, mp);
		break;
	case M_FLUSH:
		if (*mp->b_rptr & FLUSHW)
			flushq (q, FLUSHDATA);
		putnext (q, mp);
		break;
	default:{
			putnext (q, mp);
			break;
		}
	}
	return;
}

/* Streams code to scan the write queue. */
static void
v110_wsrv (queue_t * q)
{
	mblk_t *mp;

	while ((mp = getq (q)) != NULL) {
		switch (DATA_TYPE(mp)) {
		case MSG_PROTO:
			v110_proto (q, mp);
			break;
		case CASE_DATA:
			{
				mblk_t *mr = NULL, *mrs = NULL;

				log_printmsg (NULL, "Vor", mp,KERN_DEBUG);
				while (mp != NULL) {
					streamchar *xcur = mp->b_rptr;
					streamchar *xlim = mp->b_wptr;
					streamchar *w;
					unsigned char c1 = 0, c2;
					char s, t;

					if (xcur < xlim) {
						if ((xlim - xcur) > 1630)
							xlim = xcur + 1630;
						{
							/*
							 * Magic numbers below:
							 * 
							 * First step: We have bytes. Get required # of bits
							 * (10); data (6 bits) is carried by 8 bytes within
							 * a V.110 frame.
							 * 
							 * Second step: Allocate room for the necessary number
							 * of (incidentally, 10-byte) frames.
							 */
							int blocks = (((xlim - xcur) * 10) / 6 / 8) + 1;

							if (mrs == NULL) {
								mr = mrs = allocb (blocks * 10, BPRI_LO);
							} else {
								mr->b_cont = allocb (blocks * 10, BPRI_LO);
								mr = mr->b_cont;
							}
						}
						if (mr == NULL)
							break;
						w = mr->b_wptr;
						s = 5;	  /* How many dibits of c1 are "done" */
						t = 0;	  /* Position in block */
						c2 = *xcur++;
						while (xcur < xlim) {
							c1 = c2;
							c2 = *xcur++;
						  another:
							switch (t++ % 10) {
							case 0:
								*w++ = 0x00;
								break;
							case 5:
								*w++ = 0x01;
								break;
							default:
								switch (s) {
								case 5:
									*w++ = ((c1 << 2) & 0x7C) | 0x01;
									s = 3;
									goto another;
								case 1:
									*w++ = (c1 & 0x7E) | 0x01;
									s = 4;
									goto another;
								case 2:
									*w++ = (c1 >> 2) | 0x41;
									s = 5;
									break;
								case 3:
									*w++ = (c1 >> 4) | ((c2 << 6) & 0x40) | 0x11;
									s = 1;
									break;
								case 4:
									*w++ = (c1 >> 6) | ((c2 << 4) & 0x70) | 0x05;
									s = 2;
									break;
								}
							}
						}
						while (s != 0) {
							switch (t++ % 0x10) {
							case 0:
								*w++ = 0x00;
								break;
							case 5:
								*w++ = 0x01;
								break;
							default:
								switch (s) {
								case 5:
									*w++ = ((c1 << 2) & 0x7C) | 0x01;
									s = 3;
									break;
								case 1:
									*w++ = (c1 & 0x7E) | 0x01;
									s = 4;
									break;
								case 2:
									*w++ = (c1 >> 2) | 0x41;
									s = 0;
									break;
								case 3:
									*w++ = (c1 >> 4) | 0x71;
									s = 0;
									break;
								case 4:
									*w++ = (c1 >> 6) | 0x7D;
									s = 0;
									break;
								}
							}
						}
						switch (t % 10) {
						case 1:
							*w++ = 0x7F;
						case 2:
							*w++ = 0x7F;
						case 3:
							*w++ = 0x7F;
						case 4:
							*w++ = 0x7F;
						case 5:
							*w++ = 0x01;
						case 6:
							*w++ = 0x7F;
						case 7:
							*w++ = 0x7F;
						case 8:
							*w++ = 0x7F;
						case 9:
							*w++ = 0x7F;
						}
						mr->b_wptr = w;
					}
					if (xcur < xlim)	/* break */
						;
					else if (xlim == mp->b_wptr) {		/* did all */
						mblk_t *mp2 = unlinkb (mp);

						freeb (mp);
						mp = mp2;
					} else {
						mp->b_rptr = xlim;
					}
				}
				if (mrs != NULL) {
					log_printmsg (NULL, "Nach", mrs, KERN_DEBUG);
					putnext (q, mrs);
				}
				if (mp != NULL) {
					log_printmsg (NULL, "Back", mrs, KERN_DEBUG);
					putbq (q, mp);
					return;
				}
			}
			break;
		case M_FLUSH:
			if (*mp->b_rptr & FLUSHW)
				flushq (q, FLUSHDATA);
			/* FALL THRU */
		default:
			if (DATA_TYPE(mp) > QPCTL || canput (q->q_next)) {
				putnext (q, mp);
				continue;
			} else {
				putbq (q, mp);
				return;
			}
		}
	}
	return;
}

/* Streams code to read data. */
static void
v110_rput (queue_t * q, mblk_t * mp)
{
	switch (DATA_TYPE(mp)) {

	case M_FLUSH:
		if (*mp->b_rptr & FLUSHR) {
			flushq (q, FLUSHDATA);
		}
		putnext (q, mp);		  /* send it along too */
		break;
	case MSG_PROTO:
	case CASE_DATA:
		putq (q, mp);			  /* queue it for my service routine */
		break;

	case M_ERROR:
	case M_HANGUP:
	default:
		putnext (q, mp);
	}
	return;
}

/* Streams code to scan the read queue. */
static void
v110_rsrv (queue_t * q)
{
	mblk_t *mp;

#ifdef FORCE
	mblk_t *m_off = NULL;

#endif
	register struct v110_ *v110 = (struct v110_ *) q->q_ptr;

	while ((mp = getq (q)) != NULL) {
		switch (DATA_TYPE(mp)) {
		case MSG_PROTO:
			v110_proto (q, mp);
			break;
		case CASE_DATA:
			{
				char state = v110->state;
				char bits = v110->bits;
				char oldc = v110->oldc;
				short ctlen = v110->ctablen;
				mblk_t *mm;

				if (!canput (q->q_next)) {
					putbq (q, mp);
#ifdef FORCE
					if (m_off != NULL)
						putbq (q, m_off);
#endif
					return;
				}
#ifdef FORCE
				if (m_off != NULL) {
					linkb (m_off, mp);
					mp = m_off;
					m_off = NULL;
				}
#endif
				while (mp != NULL) {
					streamchar *bpos = mp->b_rptr;
					streamchar *blim = mp->b_wptr;

					while (bpos < blim) {
						char chr;

						switch (state) {
						case 0:
							if (ctlen > sizeof (v110->ctab) - 6) {
								if ((mm = allocb (ctlen, BPRI_MED)) != NULL) {
									bcopy (v110->ctab, mm->b_wptr, ctlen);
									mm->b_wptr += ctlen;
									putnext (q, mm);
								}
								v110->ctabsafe = ctlen = 0;
							} else
								v110->ctabsafe = ctlen;
						case 10:
							if (*bpos++ != 0x00) {
								bits = 0;
								ctlen = v110->ctabsafe;
								continue;
							}
						case 11:
							state = 1;
							if (bpos + 9 >= blim)
								break;
							if (bpos[9] != 0x00) {
								state = bits = 0;
								break;
							}
							break;
						case 5:
							if (!(chr = *bpos++) & 0x01) {
								bpos--;
								state = bits = 0;
								ctlen = v110->ctabsafe;
								break;
							}
							state = 6;
							break;
						case 1:
						case 2:
						case 3:
						case 4:
						case 6:
						case 7:
						case 8:
						case 9:
							if (!(chr = *bpos++) & 0x01) {
								bpos--;
								state = bits = 0;
								ctlen = v110->ctabsafe;
								break;
							}
							state++;
							switch (bits) {
							case 1:
								oldc = (chr & 0x7E) >> 1;
								bits = 7;
								break;
							case 2:
								oldc |= (chr & 0x7E);
								bits = 8;
								break;
							case 3:
								oldc |= (chr << 1) & 0xFC;
								bits = 9;
								break;
							case 4:
								if (chr & 0x40)
									v110->ctab[ctlen++] = oldc | ((chr << 2) & 0xF8);
								chr |= 0x7E;
								goto findnextp;
							case 5:
								if (chr & 0x20)
									v110->ctab[ctlen++] = oldc | ((chr << 3) & 0xF0);
								chr |= 0x3E;
								goto findnextp;
							case 6:
								if (chr & 0x10)
									v110->ctab[ctlen++] = oldc | ((chr << 4) & 0xE0);
								chr |= 0x1E;
								goto findnextp;
							case 7:
								if (chr & 0x08)
									v110->ctab[ctlen++] = oldc | ((chr << 5) & 0xC0);
								chr |= 0x0E;
								goto findnextp;
							case 8:
								if (chr & 0x04)
									v110->ctab[ctlen++] = oldc | ((chr << 6) & 0x80);
								chr |= 0x06;
								goto findnextp;
							case 9:
								if ((chr & 0x02) != 0)
									v110->ctab[ctlen++] = oldc;
								else
									chr |= 0x02;
								goto findnextp;
							case 0:
							  findnextp:
								if ((chr & 0x7E) == 0x7E) {
									bits = 0;
									continue;
								}
								if ((chr & 0x0E) == 0x0E) {	/* upper 3 */
									if (chr & 0x10) {
										if (chr & 0x20) {		/* 0x40 == 0 */
											bits = 1;
											oldc = 0;
										} else {		/* 0x20 == 0 */
											bits = 2;
											oldc = (chr >> 6) & 0x01;
										}
									} else {	/* 0x10 == 0 */
										bits = 3;
										oldc = (chr >> 5) & 0x03;
									}
								} else {		/* lower 3 */
									if (chr & 0x02) {
										if (chr & 0x04) {		/* 0x08 == 0 */
											bits = 4;
											oldc = (chr >> 4) & 0x07;
										} else {		/* 0x04 == 0 */
											bits = 5;
											oldc = (chr >> 3) & 0x0F;
										}
									} else {	/* 0x02 == 0 */
										bits = 6;
										oldc = (chr >> 2) & 0x1F;
									}
								}
							}
						}
					}
					{			  /* Try to pull up more data, if necessary */
						mm = mp;
						mp->b_rptr = bpos;
#if FORCE
						if (bpos + 1 == blim && *bpos == 0x00 && state == 10) {
							bpos++;
							state = 11;
						}
						if (bpos == blim)
#endif
						{
							mp = unlinkb (mp);
							freeb (mm);
						}
#if FORCE
						else if (mp->b_cont != NULL) {
							mp = pullupm (mp, 11);
							if (mp == NULL)
								m_off = mm;
						} else {
							m_off = mp;
							mp = NULL;
						}
#endif
					}
				}
				v110->state = state;
				v110->oldc = oldc;
				v110->bits = bits;
				v110->ctablen = ctlen;
				if ((ctlen = v110->ctabsafe) > 0) {
					if ((mm = allocb (ctlen, BPRI_MED)) != NULL) {
						bcopy (v110->ctab, mm->b_wptr, ctlen);
						mm->b_wptr += ctlen;
						putnext (q, mm);
					}
					if ((ctlen = v110->ctablen - ctlen) > 0) {
						bcopy (v110->ctab + v110->ctabsafe, v110->ctab, ctlen);
						v110->ctablen = ctlen;
						v110->ctabsafe = 0;
					} else {
						v110->ctablen = v110->ctabsafe = 0;
					}
				}
			}
			break;
		case M_HANGUP:
		case M_ERROR:
			/* FALL THRU */
		default:
			if (DATA_TYPE(mp) > QPCTL || canput (q->q_next)) {
				putnext (q, mp);
				continue;
			} else {
				putbq (q, mp);
				return;
			}
		}
	}
#ifdef FORCE
	if (m_off != NULL) {
		q->q_flag &= ~QWANTR;
		putbq (q, m_off);
		q->q_flag |= QWANTR;
	}
#endif
	return;
}


#ifdef MODULE
static int do_init_module(void)
{
	return register_strmod(&v110info);
}

static int do_exit_module(void)
{
	return unregister_strmod(&v110info);
}
#endif
