#ifndef _Q_DATA
#define _Q_DATA

#include <sys/types.h>
#include <sys/param.h>

/**
 ** This module handles the data structure of additional information elements,
 ** as defined in Q.931.
 ** Multiple elemnts are supported except for the impossibility to insert an appropriate marker.
 **/

/*
 * Find an element of type "key" in dictionary "dict" within the data area
 * defined by "data" and "len".
 * 
 * Return a pointer to the data area of the element; return the data length in
 * nlen. One-byte elements obviously get a pointer to the descriptor returned.
 */
uchar_t *qd_find (uchar_t * data, int len, uchar_t dict, uchar_t key, int *nlen);

/*
 * Insert an element of type "key" in dictionary "dict" within the data area
 * defined by "data" and "len". It is assumed that the area pointed to by
 * "data" has "len"+2 bytes free at its end.
 * 
 * Return a pointer to the data area of the element. If "duplicate" is zero and an
 * element of that type is found, adjust its size and return a pointer to it,
 * else allocate a new element.
 */
uchar_t *qd_insert (uchar_t * data, int *len, uchar_t dict, uchar_t key, int nlen, char duplicate);

/*
 * Basic code to scan an element list.
 */
void *qd_scan_init (uchar_t * data, int len);
uchar_t *qd_scan (void *info, uchar_t * dict, uchar_t * key, int *nlen);
void qd_scan_exit (void *scan);

/**
 * Useful macros for scanning. Usage:
 * {  // Declarations //
 *    QD_INIT (some_data, some_length) {
 *        // Handle failure and exit //
 *    }
 *    QD {
 *    QD_CASE (the_dict, the_key):
 *        // Data pointed to by qd_data, length qd_length;
 *        // these variables may be changed freely. //
 *    }
 *    QD_EXIT
 * }
 */
#define QD_INIT(_data,_len) \
	uchar_t *qd_data; int qd_len; void *_qd; uchar_t _qd_dict,_qd_key; \
	if ((_qd = qd_scan_init(_data,_len)) == NULL)
#define QD_EXIT qd_scan_exit(_qd)
#define QD while((qd_data = qd_scan(_qd,&_qd_dict,&_qd_key,&qd_len)) != NULL) \
		switch((_qd_dict << 8 )|_qd_key)
#define QD_CASE(dict,key) case (((dict)<<8)|(key))

#define QD_FIND(md,ml,di,ke) (qd_data = qd_find(md,ml,di,ke,&qd_len))
#endif							/* _Q_DATA */
