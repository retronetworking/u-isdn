#include "vectcmp.h"

/*
 * Compare two feature vectors, mask off certain bits.
 * Return..:
 * 10 if the second vector is malformed.
 * 8 if the first vector is bytewise-numerically smaller.
 * 3 if the first vector is longer.
 * 1 if the first vector has more information.
 * 0 if the vectors are equal.
 * -1...-10 ditto.
 */
int vectcmp(
	streamchar *str1, unsigned short len1,
	streamchar *str2, unsigned short len2,
	streamchar *mask, unsigned short mlen)
{
	int diff=0, maskskip=0;

	while(len1>0 && len2>0) {
		if (!maskskip && ((mlen==0)
				? ((*str1 ^ *str2) & 0x7F)
				: ((*str1 ^ *str2) & 0x7F & *mask))) {
			if ((mlen == 0)
					? ((*str1&0x7F) > (*str2&0x7F))
					: ((*str1&0x7F&*mask) > (*str2&0x7F&*mask)))
				return -8;
			else
				return 8;
		}
		if((*str1 ^ *str2) & 0x80) {
			if (*str1&0x80) {
				if (diff == 0)
					diff=-1;
				while((len2>0) && !(*str2&0x80))
					str2++, len2--;
				if (len2==0)
					return -10;
			} else {
				if (diff == 0)
					diff=1;
				while((len1>0) && !(*str1&0x80))
					str1++, len1--;
				if (len1==0)
					return 10;
			}
			if(mlen>0) {
				while((mlen > 0) && !(*mask&0x80))
					mask++,mlen--;
				maskskip=(mlen==0);
			}
		}
		if(mlen>0) {
			if ((*str1&0x80)) {
				while((mlen>0) && !(*mask&0x80))
					mask++,mlen--;
				if(mlen>0)
					mask++,mlen--;
				maskskip=(mlen==0);
			} else if ((*mask&0x80)) {
				maskskip=1;
			}
		}
		if (--len1)
			str1++;
		if (--len2)
			str2++;
	}
	if (len1>0)
		return 3;
	else if(len2>0)
		return -3;
	else
		return diff;
}
