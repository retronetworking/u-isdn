#ifndef _ALAW_H
#define _ALAW_H

#include "primitives.h"

#define NALAW 16

#define ALAW_LAW CHAR2('l','a')	  /* use u-law if zero */

#define ALAW_RVCO_ON CHAR2('r','o')		/* VCO on (input) threshold (0..127,.
										 * zero disables). */
#define ALAW_RVCO_OFF CHAR2('r','x')	/* VCO off (input) threshold. */
#define ALAW_RVCO_CNT CHAR2('r','c')	/* Number of values below threshold
										 * before VCO turns sound off. */

#define ALAW_XVCO_ON CHAR2('x','o')		/* VCO on (input) threshold (0..127,.
										 * zero disables). */
#define ALAW_XVCO_OFF CHAR2('x','x')	/* VCO off (input) threshold. */
#define ALAW_XVCO_CNT CHAR2('x','c')	/* Number of values below threshold
										 * before VCO turns sound off. */

#endif							/* _ALAW_H */
