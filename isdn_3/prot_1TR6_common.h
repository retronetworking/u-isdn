#ifndef _1TR6_COMMON
#define _1TR6_COMMON
#include "kernel.h"
#include "streams.h"
#include "phone_1TR6.h"
#include "q_data.h"
#include "primitives.h"

void report_addnsf    (mblk_t * mb, uchar_t * data, ushort_t len);
void report_addisplay (mblk_t * mb, uchar_t * data, int len);
void report_adddate   (mblk_t * mb, uchar_t * data, int len);
void report_addcost   (mblk_t * mb, uchar_t * data, int len);
void report_addstatus (mblk_t * mb, uchar_t * data, int len);

uchar_t n1_idtocause(ushort_t id);
ushort_t n1_causetoid(uchar_t id);

uchar_t n1_idtofac(ushort_t id);
ushort_t n1_factoid(uchar_t id);
uchar_t n1_idtofacsub(ushort_t id);
ushort_t n1_facsubtoid(uchar_t id);

/* Returns the cause byte */
uchar_t report_addcause  (mblk_t * mb, uchar_t * data, int len);

#endif /* _1TR6_COMMON */
