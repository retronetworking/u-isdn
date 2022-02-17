/**
 ** Streams Audio module
 **/

/*
 * Process sound. Handle basic conversion from signed 8-bit values to and from
 * A-law (but _not_ expansion of the 8 bit values).
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
#include <sys/stropts.h>
/* #include <sys/user.h> */
#include <sys/errno.h>
#include "streamlib.h"
#include "alaw.h"
#include "isdn_proto.h"


/*
 * Standard Streams stuff.
 */
static struct module_info alaw_minfo_w =
{
		0, "alaw", 0, INFPSZ, 4000, 1000
};

static struct module_info alaw_minfo_r =
{
		0, "alaw", 0, INFPSZ, 10000, 2000
};

static qf_open alaw_open;
static qf_close alaw_close;
static qf_put alaw_rput, alaw_wput;
static qf_srv alaw_rsrv, alaw_wsrv;

static struct qinit alaw_rinit =
{
		alaw_rput, alaw_rsrv, alaw_open, alaw_close, NULL, &alaw_minfo_r, NULL
};

static struct qinit alaw_winit =
{
		alaw_wput, alaw_wsrv, NULL, NULL, NULL, &alaw_minfo_w, NULL
};

struct streamtab alawinfo =
{&alaw_rinit, &alaw_winit, NULL, NULL};

struct _vco {
	short count;			  /* VCO turn-ogff counter. */
	short run;				  /* Number of characters below threshold. Zero
							   * = mute. */
	char on, off;			  /* Thresholds. */
};

struct alaw_ {
	queue_t *q;
	char connmode;
	struct _vco r, w;
};

/*
 * Initialize with reasonable default values.
 */
static int
alaw__init (struct alaw_ *alaw)
{
	alaw->r.count = 0;
	alaw->r.run = 1;
	alaw->r.on = alaw->r.off = 0;
	alaw->w.count = 0;
	alaw->w.run = 1;
	alaw->w.on = alaw->w.off = 0;
	return 0;
}



static void
alaw_proto (queue_t * q, mblk_t * mp)
{
	struct alaw_ *alaw = (struct alaw_ *) q->q_ptr;
	streamchar *origmp = mp->b_rptr;
	ushort_t id;

	/* In case we want to printf("%s") this... */
	if (mp->b_wptr < mp->b_datap->db_lim)
		*mp->b_wptr = '\0';
	if (m_getid (mp, &id) != 0) {
		mp->b_rptr = origmp;
		putnext (q, mp);
		return;
	}
	switch (id) {
	case PROTO_MODULE:
		if (strnamecmp (q, mp)) { /* Config information for me. */
			long z;

			while (mp != NULL && m_getsx (mp, &id) == 0)
				switch (id) {
				  err:
					printf (" :Err %s\n", mp->b_rptr);
					mp->b_rptr = origmp;
					freemsg (mp);
					mp = NULL;
				default:
					break;
				case ALAW_LAW:
					printf ("ALAW: Only A-Law supported\n");
					goto err;
				case ALAW_RVCO_ON:
					if (m_geti (mp, &z) != 0)
						goto err;
					if (z < 0 || z > 127)
						goto err;
					alaw->r.on = z;
					break;
				case ALAW_RVCO_OFF:
					if (m_geti (mp, &z) != 0)
						goto err;
					if (z < 0 || z > 127)
						goto err;
					alaw->r.off = z;
					break;
				case ALAW_RVCO_CNT:
					if (m_geti (mp, &z) != 0)
						goto err;
					if (z < 50 || z > 65535)
						goto err;
					alaw->r.count = z;
					break;
				case ALAW_XVCO_ON:
					if (m_geti (mp, &z) != 0)
						goto err;
					if (z < 0 || z > 127)
						goto err;
					alaw->w.on = z;
					break;
				case ALAW_XVCO_OFF:
					if (m_geti (mp, &z) != 0)
						goto err;
					if (z < 0 || z > 127)
						goto err;
					alaw->w.off = z;
					break;
				case ALAW_XVCO_CNT:
					if (m_geti (mp, &z) != 0)
						goto err;
					if (z < 50 || z > 65535)
						goto err;
					alaw->w.count = z;
					break;
				}
			if (mp != NULL) {
				freemsg (mp);
				mp = NULL;
			}
		}
	default:
		break;
	}
	if (mp != NULL) {
		if (origmp != NULL)
			mp->b_rptr = origmp;
		putnext (q, mp);
	}
}


/*
 * Muting code.
 */
static mblk_t *
alaw_mute (mblk_t * mb, struct _vco *vco)
{
	mblk_t *mx = NULL, *mxs = NULL;

	if (vco->on == 0)
		return mb;
	while (mb != NULL) {
		uchar_t *xcur = mb->b_rptr;
		uchar_t *xend = mb->b_wptr;
		uchar_t *blkend = NULL;

		while (xcur < xend) {
			if (vco->run == 0) {
				for (; xcur < xend; xcur++) {
					if (((*xcur > 0) ? *xcur : -*xcur) > vco->on) {
						vco->run = 1;
						break;
					}
				}
				if (xcur < xend) {/* Start a new segment: Clone the old one. */
					if (blkend != NULL) {		/* Copy a previous segment */
						mblk_t *mz = allocb (blkend - (uchar_t *) mb->b_rptr, BPRI_LO);

						if (mz != NULL) {
							short sz;
							mz->b_datap->db_type = mb->b_datap->db_type;

							bcopy (mb->b_rptr, mz->b_wptr, (sz = blkend - (uchar_t *) mb->b_rptr));
							mz->b_wptr += sz;
							if (mx != NULL) {
								mx->b_cont = mz;
								mx = mz;
							} else
								mx = mxs = mz;
						}
					}
					mb->b_rptr = xcur;
					/* blkend is set to xend in the next pass */
				}
			} else {
				short x = vco->run;

				blkend = xend;
				for (; xcur < xend; xcur++) {
					if (((*xcur > 0) ? *xcur : -*xcur) > vco->off)
						x = 1;
					else {
						if (++x >= vco->count) {
							if ((uchar_t *) mb->b_rptr > (blkend = xcur - x))
								blkend = NULL;
							xcur++;
							x = 0;
							break;
						}
					}
				}
				vco->run = x;
			}
		}
		if (blkend != NULL) {
			mb->b_wptr = blkend;
			if (mx != NULL) {
				mx->b_cont = mb;
				mx = mb;
			} else
				mx = mxs = mb;
			mb = unlinkb (mx);
		} else {
			mblk_t *my = mb;

			mb = unlinkb (mb);
			freeb (my);
		}
	}
	if (mx != NULL)
		mx->b_cont = NULL;
	return mxs;
}


static char cswap[256] =
{								  /* Mirror bits */
		0x00, 0x80, 0x40, 0xC0, 0x20, 0xA0, 0x60, 0xE0, 0x10, 0x90, 0x50, 0xD0, 0x30, 0xB0, 0x70, 0xF0,
		0x08, 0x88, 0x48, 0xC8, 0x28, 0xA8, 0x68, 0xE8, 0x18, 0x98, 0x58, 0xD8, 0x38, 0xB8, 0x78, 0xF8,
		0x04, 0x84, 0x44, 0xC4, 0x24, 0xA4, 0x64, 0xE4, 0x14, 0x94, 0x54, 0xD4, 0x34, 0xB4, 0x74, 0xF4,
		0x0C, 0x8C, 0x4C, 0xCC, 0x2C, 0xAC, 0x6C, 0xEC, 0x1C, 0x9C, 0x5C, 0xDC, 0x3C, 0xBC, 0x7C, 0xFC,
		0x02, 0x82, 0x42, 0xC2, 0x22, 0xA2, 0x62, 0xE2, 0x12, 0x92, 0x52, 0xD2, 0x32, 0xB2, 0x72, 0xF2,
		0x0A, 0x8A, 0x4A, 0xCA, 0x2A, 0xAA, 0x6A, 0xEA, 0x1A, 0x9A, 0x5A, 0xDA, 0x3A, 0xBA, 0x7A, 0xFA,
		0x06, 0x86, 0x46, 0xC6, 0x26, 0xA6, 0x66, 0xE6, 0x16, 0x96, 0x56, 0xD6, 0x36, 0xB6, 0x76, 0xF6,
		0x0E, 0x8E, 0x4E, 0xCE, 0x2E, 0xAE, 0x6E, 0xEE, 0x1E, 0x9E, 0x5E, 0xDE, 0x3E, 0xBE, 0x7E, 0xFE,
		0x01, 0x81, 0x41, 0xC1, 0x21, 0xA1, 0x61, 0xE1, 0x11, 0x91, 0x51, 0xD1, 0x31, 0xB1, 0x71, 0xF1,
		0x09, 0x89, 0x49, 0xC9, 0x29, 0xA9, 0x69, 0xE9, 0x19, 0x99, 0x59, 0xD9, 0x39, 0xB9, 0x79, 0xF9,
		0x05, 0x85, 0x45, 0xC5, 0x25, 0xA5, 0x65, 0xE5, 0x15, 0x95, 0x55, 0xD5, 0x35, 0xB5, 0x75, 0xF5,
		0x0D, 0x8D, 0x4D, 0xCD, 0x2D, 0xAD, 0x6D, 0xED, 0x1D, 0x9D, 0x5D, 0xDD, 0x3D, 0xBD, 0x7D, 0xFD,
		0x03, 0x83, 0x43, 0xC3, 0x23, 0xA3, 0x63, 0xE3, 0x13, 0x93, 0x53, 0xD3, 0x33, 0xB3, 0x73, 0xF3,
		0x0B, 0x8B, 0x4B, 0xCB, 0x2B, 0xAB, 0x6B, 0xEB, 0x1B, 0x9B, 0x5B, 0xDB, 0x3B, 0xBB, 0x7B, 0xFB,
		0x07, 0x87, 0x47, 0xC7, 0x27, 0xA7, 0x67, 0xE7, 0x17, 0x97, 0x57, 0xD7, 0x37, 0xB7, 0x77, 0xF7,
		0x0F, 0x8F, 0x4F, 0xCF, 0x2F, 0xAF, 0x6F, 0xEF, 0x1F, 0x9F, 0x5F, 0xDF, 0x3F, 0xBF, 0x7F, 0xFF,
};

/* Streams code to open the driver. */
static int
alaw_open (queue_t * q, dev_t dev, int flag, int sflag
#ifdef DO_ADDERROR
		,int *err
#endif
)
{
	register struct alaw_ *alaw;

	alaw = malloc(sizeof(*alaw));
	if(alaw == NULL)
		return OPENFAIL;
	memset(alaw,0,sizeof(*alaw));

	WR (q)->q_ptr = (char *) alaw;
	q->q_ptr = (char *) alaw;

	alaw->q = q;
	alaw__init (alaw);

	if (0)
		printf ("ALAW pseudo-driver %d opened.\n", dev);

	MORE_USE;
	return 0;
}

/* Streams code to close the driver. */
static void
alaw_close (queue_t * q, int dummy)
{
	register struct alaw_ *alaw;

	alaw = (struct alaw_ *) q->q_ptr;

	flushq (q, FLUSHALL);
	flushq (WR (q), FLUSHALL);
	if (0)
		printf ("ALAW driver closed.\n");
	free(alaw);
	LESS_USE;
	return;
}


/* Streams code to write data. */
static void
alaw_wput (queue_t * q, mblk_t * mp)
{
	switch (mp->b_datap->db_type) {
	case MSG_PROTO:
	CASE_DATA
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
alaw_wsrv (queue_t * q)
{
	register struct alaw_ *alaw = (struct alaw_ *) q->q_ptr;
	mblk_t *mp;

	while ((mp = getq (q)) != NULL) {
		switch (mp->b_datap->db_type) {
		case MSG_PROTO:
			alaw_proto (q, mp);
			break;
		CASE_DATA {
				mblk_t *mr;

				if (!canput (q->q_next)) {
					putbq (q, mp);
					return;
				}
				mr = alaw_mute (mp, &alaw->w);
				if (mr != NULL) {
					for (mp = mr; mp != NULL; mp = mp->b_cont) {
						streamchar *xcur = mp->b_rptr;
						streamchar *xlim = mp->b_wptr;

						for (; xcur < xlim; xcur++) {
							char val = *xcur;

							*xcur = (cswap[(val > 0) ? val : -val]
									| ((val < 0) ? 0 : 1)) ^ 0xAA;
						}
					}
					putnext (q, mr);
				}
			} break;
		case M_FLUSH:
			if (*mp->b_rptr & FLUSHW)
				flushq (q, FLUSHDATA);
			/* FALL THRU */
		default:
			if (mp->b_datap->db_type > QPCTL || canput (q->q_next)) {
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
alaw_rput (queue_t * q, mblk_t * mp)
{
	switch (mp->b_datap->db_type) {

	case M_FLUSH:
		if (*mp->b_rptr & FLUSHR) {
			flushq (q, FLUSHDATA);
		}
		putnext (q, mp);		  /* send it along too */
		break;
	case MSG_PROTO:
	CASE_DATA
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
alaw_rsrv (queue_t * q)
{
	mblk_t *mp;
	register struct alaw_ *alaw = (struct alaw_ *) q->q_ptr;

	while ((mp = getq (q)) != NULL) {
		switch (mp->b_datap->db_type) {
		case MSG_PROTO:
			alaw_proto (q, mp);
			break;
		CASE_DATA {
				mblk_t *mr;

				if (!canput (q->q_next)) {
					putbq (q, mp);
					return;
				}
				for (mr = mp; mr != NULL; mr = mr->b_cont) {
					streamchar *xcur = mr->b_rptr;
					streamchar *xlim = mr->b_wptr;

					for (; xcur < xlim; xcur++) {
						unsigned char val = *xcur ^ 0xAA;

						if (val & 0x01)
							*xcur = cswap[val] & 0x7F;	/* high bit is set */
						else
							*xcur = -cswap[val];		/* high bit is clear */
					}
				}
				if ((mr = alaw_mute (mp, &alaw->r)) != NULL)
					putnext (q, mr);
			} break;
		case M_HANGUP:
		case M_ERROR:
			/* FALL THRU */
		default:
			if (mp->b_datap->db_type > QPCTL || canput (q->q_next)) {
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


#ifdef MODULE
static int do_init_module(void)
{
	return register_strmod(&alawinfo);
}

static int do_exit_module(void)
{
	return unregister_strmod(&alawinfo);
}
#endif
