#include <sys/stream.h>
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
	streamchar *mask, unsigned short mlen);
