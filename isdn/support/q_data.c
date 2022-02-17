#include "f_module.h"
#include "primitives.h"
#include "q_data.h"
#include "f_malloc.h"
#include "streams.h"

struct scan_qd {				  /* Progress information for scanning. */
	uchar_t *data;
	int len;
	uchar_t dict;				  /* current dictionary */
};

void *
qd_scan_init (uchar_t * data, int len)
{
	struct scan_qd *sc = (struct scan_qd *)malloc (sizeof (struct scan_qd));

	if (sc == NULL)
		return NULL;
	MORE_USE;
	sc->data = data;
	sc->len = len;
	sc->dict = 0;
	return sc;
}

uchar_t *
qd_scan (void *xinfo, uchar_t * dict, uchar_t * key, int *nlen)
{
	struct scan_qd *sc = (struct scan_qd *)xinfo;
	uchar_t ckey;
	uchar_t *cdata;

	*dict = sc->dict;
	while (1) {
		if (sc->len == 0)
			return NULL;
		ckey = *sc->data;
		/* Special case: switch to another dictionary */
		if ((ckey & 0xF0) == 0x90) {
			*dict = ckey & 0x07;
			sc->data++, sc->len--;
			if ((ckey & 0x08) == 0)
				sc->dict = *dict;
			continue;
		}
		*key = ckey;
		if (ckey & 0x80) {
			cdata = sc->data++;
			sc->len--;
			*nlen = 1;
			return cdata;
		} else {
			int xlen = sc->data[1];

			cdata = sc->data + 2;
			sc->data += xlen + 2;
			sc->len -= xlen + 2;
			if (sc->len < 0)
				return NULL;
			*nlen = xlen;
			return cdata;
		}
	}
}
void
qd_scan_exit (void *scan)
{
	LESS_USE;
	free (scan);
}


uchar_t *
qd_find (uchar_t * data, int len, uchar_t dict, uchar_t key, int *nlen)
{
	uchar_t thisdict = 0, tempdict = 0;

	*nlen = 0;

	while (len > 0) {
		tempdict = thisdict;
		while ((len > 0) && ((*data & 0xF0) == 0x90)) {
			tempdict = *data & 0x07;
			if ((*data & 0x08) == 0)
				thisdict = tempdict;
			data++, len--;
		}
		if (dict == tempdict) {
			if (key & 0x80) {
				if (((*data ^ key) & 0xF0) == 0) {		/* Alternate way of
														 * saying "if ((x &
														 * 0xF0) == (y & 0xF0))" */
					*nlen = 1;
					return data;
				}
			} else {
				int xlen;

				if (*data == key) {
					xlen = data[1];
					data += 2;
					if (len < xlen + 2)
						return NULL;
					*nlen = xlen;
					return data;
				}
			}
		}
		if (*data & 0x80)
			data++, len--;
		else if (len < 2)
			return NULL;
		else {
			short xlen = data[1] + 2;

			if (xlen > len)
				return NULL;
			data += xlen;
			len -= xlen;
		}
	}
	return NULL;
}


uchar_t *
qd_insert (uchar_t * data, int *len, uchar_t dict, uchar_t key, int nlen, char duplicate)
{
	uchar_t thisdict = 0, tempdict = 0, newdict = 0;
	int clen = *len;

	while (clen > 0) {
		thisdict = tempdict;
		tempdict = newdict;
		if ((*data & 0xF0) == 0x90) {
			tempdict = *data & 0x07;
			if ((*data & 0x08) == 0) {
				newdict = tempdict;
				if (newdict > dict)
					break;
			}
			data++, len--;
		} else if (dict != thisdict) {
		  skip:
			if (*data & 0x80)
				data++, clen--;
			else if (clen < 2)
				return NULL;
			else {
				short xlen = data[1] + 2;

				data += xlen;
				clen -= xlen;
			}

		} else if (key & 0x80) {
			if (((*data ^ key) & 0xF0) != 0) {
				if (*data > key)
					break;
				else
					goto skip;
			}
			break;
		} else {
			if (*data != key) {
				if (*data > key)
					break;
				else
					goto skip;
			}
			break;
		}
	}
	if (thisdict == dict && clen > 0 && !duplicate) {	/* Replace existing data */
		if (key & 0x80) {
			if ((key & 0xF0) == (*data & 0xF0)) {
				*data = key;
				return data;
			}
		} else {
			if (key == *data) {
				if (data[1] == nlen) {
					return data + 2;
				} else {
					uchar_t *src = data + data[1] + 2;
					uchar_t *dest = data + nlen + 2;
					int slen = clen - data[1] - 2;

					if (data[1] > nlen) {
						while (slen--)
							*dest++ = *src++;
					} else {
						dest += slen;
						src += slen;
						while (slen--)
							*--dest = *--src;
					}
					*len += nlen - data[1];
					data[1] = nlen;
					return data + 2;
				}
			}
		}
	}
	if (clen > 0) {				  /* Insert new data here */
		uchar_t *src = data;
		uchar_t *dest;
		int slen;

		if (key & 0x80)
			slen = 1;
		else
			slen = nlen + 2;
		if (thisdict != dict)
			slen++;
		dest = src + slen;
		src += clen;
		dest += clen;
		while (clen--)
			*--dest = *--src;
	}
	if (thisdict != dict) {		  /* Add new dictionary record if necessary */
		(*len)++;
		*data++ = dict | 0x90;
	}
	/* Now setup the data */
	if (key & 0x80) {
		*len += 1;
		*data = key;
		return data;
	} else {
		*len += nlen + 2;
		data[0] = key;
		data[1] = nlen;
		return data + 2;
	}
}


#ifdef TEST
#include "dump.h"

main ()
{
#define p printf
#define d(x) do { if(s == NULL) { printf("Error: %s\n",x); exit(1); }} while(0)
	uchar_t xx[1024];
	uchar_t *s;
	int len, slen;

	len = 0;
	s = qd_insert (xx, &len, 0, 0x12, 3);
	d ("Bad1");
	strncpy ((char *) s, "123", 3);
	dumpblock ("1", xx, len);
	s = qd_insert (xx, &len, 1, 0x23, 4);
	d ("Bad2");
	strncpy ((char *) s, "1234", 4);
	dumpblock ("2", xx, len);
	s = qd_insert (xx, &len, 0, 0x81, 1);
	d ("Bad3");
	dumpblock ("3", xx, len);
	s = qd_insert (xx, &len, 0, 0x01, 2);
	d ("Bad4");
	strncpy ((char *) s, "12", 2);
	dumpblock ("4", xx, len);
	s = qd_insert (xx, &len, 0, 0x23, 4);
	d ("Bad5");
	strncpy ((char *) s, "+234", 4);
	dumpblock ("5", xx, len);
	s = qd_insert (xx, &len, 0, 0x12, 4);
	d ("Bad6");
	strncpy ((char *) s, "+012", 4);
	dumpblock ("6", xx, len);

	s = qd_find (xx, len, 0, 0x23, &slen);
	d ("bad1");
	printf ("0 23 ");
	dumpascii (s, slen);
	printf ("\n");

}

#endif


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
