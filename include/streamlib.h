#ifndef _STREAMLIB_H
#define _STREAMLIB_H

/**
 ** Streams library -- provides some useful functions for Streams.
 **/

/*
 * Message format: See streamlib.h. keyID [:paramID [ arg.. ]].. [ ::### ]...
 * All key and parameter IDs consist of two characters separated by whitespace.
 * Parameter IDs are preceded by one colon. Two colons signify the end of the
 * argumnent list. "###" means "anything". Often this is another
 * key-parameter-argument list for a nested command.
 */

#include <sys/types.h>
#include <sys/time.h>
#include "f_signal.h"
#include <sys/param.h>
#include <sys/sysmacros.h>
#include "stropts.h"

#include "streams.h"
#include "config.h"

#ifndef linux
short spl(short);
#endif

/*
 * Like putbq, but doesn't schedule the queue just because it's a priority
 * message we're putting back. This is necessary for requeuing in case of
 * resource shortage.
 */
void putbqf (queue_t * q, mblk_t * mp);
void putbqff(queue_t * q, mblk_t * mp);

/*
 * dsize()
 * 
 * A no-nonsense version of msgdsize().
 */
int dsize (mblk_t * mp);

/*
 * strnamecmp()
 * 
 * Returns 1 and skips if the first string in the mblock is this module's name.
 * Else return 0. Used in module configuration to quickly identify if the
 * config information applies to a module.
 */
int strnamecmp (queue_t * q, mblk_t * mb);

/*
 * pullupm()
 * 
 * An improved version of pullupmsg() which doesn't have to play around with
 * mblk_t buffers. If it returns NULL, the original argument is left alone,
 * else the original is freed or integrated into the returned mblock.
 * 
 * As a special case, a "what" value of zero means to drop leading empty blocks
 * and is used as a quick call to strip a leading data block if it's empty. In
 * this case the argument is always either freed or returned, thus it's safe to
 * say "mb = pullup(mb,0);".
 */
mblk_t *pullupm (mblk_t * mb, short length);


/*
 * putctlx(), putctlx1()
 * 
 * Special versions of putctl() and putctl1() which use putnext instead of putq.
 */
int putctlx (queue_t * q, char type);
int putctlx1 (queue_t * q, char type, streamchar msg);


/* Debugging stuff. */

#ifdef CONFIG_DEBUG_STREAMS

#define pullupm(q,x) deb_pullupm(__FILE__,__LINE__,(q),(x))
mblk_t *deb_pullupm (const char *deb_file, unsigned int deb_line, mblk_t * mb, short what);

#define putctlx(q,t) deb_putctlx(__FILE__,__LINE__,(q),(t))
#define putctlx1(q,t,p) deb_putctlx1(__FILE__,__LINE__,(q),(t),(p))
int deb_putctlx (const char *deb_file, unsigned int deb_line, queue_t * q, char type);
int deb_putctlx1 (const char *deb_file, unsigned int deb_line, queue_t * q, char type, streamchar msg);

#define putbqf(q,m) deb_putbqf(__FILE__,__LINE__,(q),(m))
#define putbqff(q,m) deb_putbqff(__FILE__,__LINE__,(q),(m))
void deb_putbqf (const char *deb_file, unsigned int deb_line, queue_t * q, mblk_t *mb);
void deb_putbqff(const char *deb_file, unsigned int deb_line, queue_t * q, mblk_t *mb);

#endif							/* CONFIG_DEBUG_STREAMS */


void m_putid (mblk_t * mb, ushort_t id);
void m_putdelim (mblk_t * mb);
void m_putsx (mblk_t * mb, ushort_t id);
void m_putsx2 (mblk_t * mb, ushort_t id);
void m_putc (mblk_t * mb, char what);
void m_putsc (mblk_t * mb, uchar_t id);
void m_putsl (mblk_t * mb, ushort_t id);
void m_putlx (mblk_t * mb, ulong_t id);
void m_puti (mblk_t * mb, long id);
void m_puts (mblk_t * mb, uchar_t * data, int len);
void m_putsz (mblk_t * mb, uchar_t * data);
void m_putx (mblk_t * mb, ulong_t id);
void m_puthex (mblk_t *mb, uchar_t *data, int len);
void m_getskip (mblk_t * mb);
int m_getid (mblk_t * mb, ushort_t * id);
int m_getip (mblk_t * mb, ulong_t * id);
int m_getsx (mblk_t * mb, ushort_t * id);
int m_getlx (mblk_t * mb, ulong_t * id);
int m_geti (mblk_t * mb, long *id);
int m_getx (mblk_t * mb, ulong_t * id);
int m_getstr (mblk_t * mb, char *str, int maxlen);
int m_getstrlen (mblk_t * mb);
int m_getc (mblk_t * mb, char *c);
int m_gethex (mblk_t *mb, uchar_t *data, int len);
int m_gethexlen (mblk_t *mb);


#ifdef CONFIG_DEBUG_STREAMS
void deb_m_reply (const char*, unsigned int, queue_t * q, mblk_t * mb, int err);
#define m_reply(a,b,c) deb_m_reply(__FILE__,__LINE__,(a),(b),(c))
void deb_md_reply (const char*, unsigned int, queue_t * q, mblk_t * mb, int err);
#define md_reply(a,b,c) deb_md_reply(__FILE__,__LINE__,(a),(b),(c))
mblk_t * deb_make_reply (const char*, unsigned int, int err);
#define make_reply(a) deb_make_reply(__FILE__,__LINE__,(a))
#else
void m_reply (queue_t * q, mblk_t * mb, int err);
void md_reply (queue_t * q, mblk_t * mb, int err);
mblk_t * make_reply (int err);
#endif

#endif							/* _STREAMLIB_H */
