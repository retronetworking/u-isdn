
/* Streams library */

#include "f_module.h"
#include "primitives.h"
#include "kernel.h"
#include "streamlib.h"
#ifndef __KERNEL__
#include <ctype.h>
#endif
#include "isdn_proto.h"
#include "isdn_limits.h"
#include "msgtype.h"

#define ISNULL(x) ((x)==NULL || WR(x)==NULL || RD(x)==NULL)

#ifdef KERNEL
#ifdef isspace
#undef isspace
#endif
int
isspace (int x)
{
	return (x == ' ' || x == '\t' || x == '\r' || x == '\n');
}

#else
#include "kernel.h"
#endif

#ifdef DO_NEED_MEMCPY
/* GCC */
void
memcpy (uchar_t * a, uchar_t * b, int c)
{
	bcopy (b, a, c);
}
#endif

#ifdef DO_NEED_SPL
short
spl(short x)
{
	switch(x) {
	case 0: return spl0();
	case 1: return spl1();
	case 2: return spl2();
	case 3: return spl3();
	case 4: return spl4();
	case 5: return spl5();
	case 6: return spl6();
	case 7: return spl7();
	default:
		printf("SPL %x ?? \n",x);
		return 0;
	}
}
#endif


#if defined(M_UNIX) && defined(KERNEL)
void strlibinit(void)
{
	printcfg("StreamLib", 0, 0, 0, -1, "Add-On Streams Library");
	{
		struct fmodsw *fmswp;
		int i;

        printf("Known Streams modules:");
		for(i=0,fmswp=fmodsw;i < fmodcnt; i++,fmswp++) {
			if(fmswp->f_str != NULL)
				printf(" %s",fmswp->f_name);
		}
	    printf("\n");
	}
}
#endif

void
#ifdef AUX
streamlibinit (void)
#endif
#ifdef M_UNIX
strlibpminit(void)
#endif
#ifdef linux
streamlibinit (void)
#endif
{
#if defined(M_UNIX) && defined(KERNEL)
#else
	printf ("StreamLib Init\n");
#endif
#if !defined(M_UNIX)
	{
		struct fmodsw *fmswp;
		int i;

        printf("Modules:");
#ifdef linux
		for(i=0,fmswp=fmod_sw;i < fmodcnt; i++,fmswp++)
#else
		for(i=0,fmswp=fmodsw;i < fmodcnt; i++,fmswp++)
#endif
		{
			if(fmswp->f_str != NULL)
				printf(" %s",fmswp->f_name);
		}
	    printf("\n");
	}
#endif
}


int
strnamecmp (queue_t * q, mblk_t * mb)
{
	streamchar *n = (streamchar *)q->q_qinfo->qi_minfo->mi_idname;
	streamchar *x;
	streamchar *origmb = mb->b_rptr;
	ushort_t id = 0;


#if 1							  /* new system */
	while (m_getsx (mb, &id) == 0 && id != PROTO_MODULE) ;
	if (id != PROTO_MODULE) {
		mb->b_rptr = origmb;
		return 0;
	}
	m_getskip (mb);
#endif
	x = mb->b_rptr;
	while (*n != '\0' && x < mb->b_wptr && *x == *n)
		n++, x++;
	if (*n != '\0')
		return 0;
	if (x == mb->b_wptr || *x <= ' ') {
		mb->b_rptr = origmb;
		return 1;
	}
	mb->b_rptr = origmb;
	return 0;
}

int
dsize (mblk_t * mp)
{
	int size = 0;

	for (; mp != NULL; mp = mp->b_cont)
		size += mp->b_wptr - mp->b_rptr;

	return size;
}


/**
 * pullupm
 *
 * Concatenate the first n bytes of a message.
 *
 * This code returns NULL if the length is zero and the message is empty.
 */
#ifdef CONFIG_DEBUG_STREAMS
mblk_t *deb_pullupm(const char *deb_file, unsigned int deb_line, mblk_t *p_msg, short length)
#else
mblk_t *pullupm(mblk_t *p_msg, short length)
#endif
{
	mblk_t *p_newmsg;
	short offset = 0;

	if(p_msg == NULL)
		return NULL;

#ifdef CONFIG_DEBUG_STREAMS
	if(deb_msgdsize(deb_file,deb_line,p_msg) < 0)
		return NULL;
#endif
	while(p_msg != NULL && p_msg->b_rptr >= p_msg->b_wptr && p_msg->b_cont != NULL) {
		mblk_t *p_temp = p_msg->b_cont;
		freeb(p_msg);
		p_msg = p_temp;
#ifdef CONFIG_DEBUG_STREAMS
		if(msgdsize(p_msg) < 0)
			return NULL;
#endif
	}
	if((length == 0) && (p_msg->b_cont == NULL)) {
		if (p_msg->b_rptr >= p_msg->b_wptr) {
			freeb(p_msg);
			return NULL;
		}
		return p_msg;
	}
	if(length < 0) {
		offset = -length;
		length = msgsize(p_msg);
	}
	if ((p_msg->b_wptr - p_msg->b_rptr >= length) && (DATA_START(p_msg)+offset <= p_msg->b_rptr))
		return p_msg;

	if ((p_newmsg = allocb(offset+length, BPRI_MED)) == NULL)
		return NULL;
	
	DATA_TYPE(p_newmsg) = DATA_TYPE(p_msg);
	p_newmsg->b_rptr += offset;
	p_newmsg->b_wptr = p_newmsg->b_rptr;

	/*
	 * Copy the data.
	 */ 
	while (length > 0 && p_msg != NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		if(msgdsize(p_msg) < 0)
			return NULL;
#endif
		if(p_msg->b_wptr > p_msg->b_rptr) {
			short n = min(p_msg->b_wptr - p_msg->b_rptr, length);
			memcpy(p_newmsg->b_wptr, p_msg->b_rptr, n);
			p_newmsg->b_wptr += n;
			p_msg->b_rptr += n;
			length -= n;
			if (p_msg->b_rptr != p_msg->b_wptr)
				break;
		}
		{	mblk_t *p_cont;
			p_cont = p_msg->b_cont;
			freeb(p_msg);
			p_msg = p_cont;
		}
	}

	if (p_msg == NULL || p_msg->b_rptr < p_msg->b_wptr)
		p_newmsg->b_cont = p_msg;
	else {
		p_newmsg->b_cont = p_msg->b_cont;
		freeb(p_msg);
	}

	return p_newmsg;
}



/**
 * embedm
 *
 * Concatenate the first n bytes of a message.
 *
 * This code returns NULL if the length is zero and the message is empty.
 */
#ifdef CONFIG_DEBUG_STREAMS
mblk_t *deb_embedm(const char *deb_file, unsigned int deb_line, mblk_t *p_msg, short offstart, short offend)
#else
mblk_t *embedm(mblk_t *p_msg, short offstart, short offend)
#endif
{
	mblk_t *p_newmsg;
	int length;

	if(p_msg == NULL)
		return NULL;

#ifdef CONFIG_DEBUG_STREAMS
	if(deb_msgdsize(deb_file,deb_line,p_msg) < 0)
		return NULL;
#endif
	while(p_msg != NULL && p_msg->b_rptr >= p_msg->b_wptr && p_msg->b_cont != NULL) {
		mblk_t *p_temp = p_msg->b_cont;
		freeb(p_msg);
		p_msg = p_temp;
#ifdef CONFIG_DEBUG_STREAMS
		if(msgdsize(p_msg) < 0)
			return NULL;
#endif
	}
	length = msgsize(p_msg);

	if ((p_newmsg = allocb(offstart+length+offend, BPRI_MED)) == NULL)
		return NULL;
	
	DATA_TYPE(p_newmsg) = DATA_TYPE(p_msg);
	p_newmsg->b_rptr += offstart;
	p_newmsg->b_wptr = p_newmsg->b_rptr;

	/*
	 * Copy the data.
	 */ 
	while (length > 0 && p_msg != NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		if(msgdsize(p_msg) < 0)
			return NULL;
#endif
		if(p_msg->b_wptr > p_msg->b_rptr) {
			short n = min(p_msg->b_wptr - p_msg->b_rptr, length);
			memcpy(p_newmsg->b_wptr, p_msg->b_rptr, n);
			p_newmsg->b_wptr += n;
			p_msg->b_rptr += n;
			length -= n;
			if (p_msg->b_rptr != p_msg->b_wptr)
				break;
		}
		{	mblk_t *p_cont;
			p_cont = p_msg->b_cont;
			freeb(p_msg);
			p_msg = p_cont;
		}
	}
	freeb(p_msg);
	return p_newmsg;
}



#ifdef CONFIG_DEBUG_STREAMS
void
deb_putbqff(const char *deb_file,unsigned int deb_line, queue_t * q, mblk_t * mp)
#else
void
putbqff(queue_t * q, mblk_t * mp)
#endif
{
	int qflag;

	/*
	 * like putbqf but is not an error...
	 */
	int ms = splstr ();

	qflag = q->q_flag;
	q->q_flag |= QENAB;
#ifdef CONFIG_DEBUG_STREAMS
	deb_putbq (deb_file,deb_line, q, mp);
#else
	putbq (q, mp);
#endif
	if (!(qflag & QENAB))
		q->q_flag &= ~QENAB;
	q->q_flag |= QWANTR;
	if(!(qflag & QENAB))
		qretry(q);
	splx (ms);
}



#ifdef CONFIG_DEBUG_STREAMS
void
deb_putbqf (const char *deb_file,unsigned int deb_line, queue_t * q, mblk_t * mp)
#else
void
putbqf (queue_t * q, mblk_t * mp)
#endif
{
#ifdef CONFIG_DEBUG_STREAMS
	/* This is KERN_EMERG message because it just shouldn't happen --
	   putbqf is for emergencies, putbq is for normal scheduling. */
	printf("%sPutBQF %p:%p at %s:%d\n",KERN_EMERG,q,mp,deb_file,deb_line);
	deb_putbqff(deb_file,deb_line, q,mp);
#else
	putbqff(q,mp);
#endif
}

int
#ifdef CONFIG_DEBUG_STREAMS
deb_putctlx (const char *deb_file,unsigned int deb_line, queue_t * q, char type)
#else
putctlx (queue_t * q, char type)
#endif
{
	register mblk_t *bp;

	if (ISNULL (q)) {
		printf ("\n* ERR PutCtlX: %p\n", q);
		return (0);
	}
	if (q->q_next == NULL) {
		printf (" X1\n");
		return (0);
	}
#if defined(CONFIG_DEBUG_STREAMS) && defined(linux)
	if (!(bp = deb_allocb (deb_file,deb_line, 0, BPRI_HI)))
#else
	if (!(bp = allocb (0, BPRI_HI)))
#endif
	{
		return (0);
	}
	DATA_TYPE(bp) = type;
#if defined(CONFIG_DEBUG_STREAMS) && defined(linux) && 0
	deb_putnext (deb_file,deb_line, q, bp);
#else
	putnext (q, bp);
#endif
	return (1);
}

int
#ifdef CONFIG_DEBUG_STREAMS
deb_putctlx1 (const char *deb_file, unsigned int deb_line, queue_t * q, char type, streamchar param)
#else
putctlx1 (queue_t * q, char type, streamchar param)
#endif
{
	register mblk_t *bp;

	if (ISNULL (q)) {
#if defined(CONFIG_DEBUG_STREAMS) && defined(linux)
		printf ("\n* ERR PutCtlX1: %s:%d\n", deb_file,deb_line);
#else
		printf ("\n* ERR PutCtlX1: %p\n", q);
#endif
		return (0);
	}
	if (q->q_next == NULL) {
		printf (" X1\n");
		return (0);
	}
#if defined(CONFIG_DEBUG_STREAMS) && defined(linux) && 0
	if (!(bp = deb_allocb (deb_file, deb_line, 1, BPRI_HI)))
#else
	if (!(bp = allocb (1, BPRI_HI)))
#endif
	{
		return (0);
	}
	DATA_TYPE(bp) = type;
	*bp->b_wptr++ = param;
#if defined(CONFIG_DEBUG_STREAMS) && defined(linux) && 0
	deb_putnext (deb_file, deb_line, q, bp);
#else
	putnext (q, bp);
#endif
	return (1);
}


int
#ifdef CONFIG_DEBUG_STREAMS
deb_putctlerr (const char *deb_file, unsigned int deb_line, queue_t * q, int err)
#else
putctlerr (queue_t * q, int err)
#endif
{
	register mblk_t *bp;

	if (ISNULL (q)) {
#if defined(CONFIG_DEBUG_STREAMS) && defined(linux)
		printf ("\n* ERR PutCtlErr: %s:%d\n", deb_file,deb_line);
#else
		printf ("\n* ERR PutCtlErr: %p\n", q);
#endif
		return (0);
	}
	if (q->q_next == NULL) {
		printf (" Err\n");
		return (0);
	}
#if defined(CONFIG_DEBUG_STREAMS) && defined(linux) && 0
	if (!(bp = deb_allocb (deb_file, deb_line, 1, BPRI_HI)))
#else
	if (!(bp = allocb (1, BPRI_HI)))
#endif
	{
		return (0);
	}
	DATA_TYPE(bp) = M_ERROR;
	*bp->b_wptr++ = ((err > 0) ? err : -err);
#if defined(CONFIG_DEBUG_STREAMS) && defined(linux) && 0
	deb_putnext (deb_file, deb_line, q, bp);
#else
	putnext (q, bp);
#endif
	return (1);
}



void
m_putid (mblk_t * mb, ushort_t id)
{
	if (mb == NULL)
		return;
	if (mb->b_wptr + 2 > DATA_END(mb))
		return;
#ifdef ALIGNED_ONLY
	*mb->b_wptr++ = id >> 8;
	*mb->b_wptr++ = id & 0xFF;
#else
	*((ushort_t *)mb->b_wptr)++ = id;
#endif
}

void
m_putdelim (mblk_t * mb)
{
	if (mb == NULL)
		return;
	if (mb->b_wptr + 3 > DATA_END(mb))
		return;
	*mb->b_wptr++ = ' ';
	*mb->b_wptr++ = ':';
	*mb->b_wptr++ = ':';
}


void
m_putsx (mblk_t * mb, ushort_t id)
{
	if (mb == NULL)
		return;
	if (mb->b_wptr + 4 > DATA_END(mb))
		return;
	*mb->b_wptr++ = ' ';
	*mb->b_wptr++ = ':';
#ifdef ALIGNED_ONLY
	*mb->b_wptr++ = id >> 8;
	*mb->b_wptr++ = id & 0xFF;
#else
	*((ushort_t *)mb->b_wptr)++ = id;
#endif
}

void
m_putsx2 (mblk_t * mb, ushort_t id)
{
	if (mb == NULL)
		return;
	if (mb->b_wptr + 3 > DATA_END(mb))
		return;
	*mb->b_wptr++ = ' ';
#ifdef ALIGNED_ONLY
	*mb->b_wptr++ = id >> 8;
	*mb->b_wptr++ = id & 0xFF;
#else
	*((ushort_t *)mb->b_wptr)++ = id;
#endif
}

void
m_putc (mblk_t * mb, char what)
{
	if (mb == NULL)
		return;
	if (mb->b_wptr + 2 > DATA_END(mb))
		return;
	*mb->b_wptr++ = ' ';
	*mb->b_wptr++ = what;
}

void
m_putsc (mblk_t * mb, uchar_t id)
{
	if (mb == NULL)
		return;
	if (mb->b_wptr + 2 > DATA_END(mb))
		return;
	*mb->b_wptr++ = ' ';
	*mb->b_wptr++ = id;
}

void
m_putsl (mblk_t * mb, ushort_t id)
{
	if (mb == NULL)
		return;
	if (mb->b_wptr + 5 > DATA_END(mb))
		return;
	*mb->b_wptr++ = ' ';
#ifdef ALIGNED_ONLY
	*mb->b_wptr++ = id >> 24;
	*mb->b_wptr++ = (id >> 16) & 0xFF;
	*mb->b_wptr++ = (id >> 8) & 0xFF;
	*mb->b_wptr++ = id & 0xFF;
#else
	*((ulong_t *)mb->b_wptr)++ = id;
#endif
}

void
m_putlx (mblk_t * mb, ulong_t id)
{
	if (mb == NULL)
		return;
	if (mb->b_wptr + 5 > DATA_END(mb))
		return;
	*mb->b_wptr++ = ' ';
#ifdef ALIGNED_ONLY
	*mb->b_wptr++ = id >> 24;
	*mb->b_wptr++ = (id >> 16) & 0xFF;
	*mb->b_wptr++ = (id >> 8) & 0xFF;
	*mb->b_wptr++ = id & 0xFF;
#else
	*((ulong_t *)mb->b_wptr)++ = id;
#endif
}

void
m_puti (mblk_t * mb, long id)
{
	char xx[10];
	char *xptr;

	if (mb == NULL)
		return;
	if (id > 0 && id < 10) {
		if (mb->b_wptr + 2 > DATA_END(mb))
			return;
	} else if (id > -10 && id < 100) {
		if (mb->b_wptr + 3 > DATA_END(mb))
			return;
	} else if (id > -100 && id < 1000) {
		if (mb->b_wptr + 4 > DATA_END(mb))
			return;
	} else if (id > -1000 && id < 10000) {
		if (mb->b_wptr + 5 > DATA_END(mb))
			return;
	} else {
		if (mb->b_wptr + 15 > DATA_END(mb))
			return;
	}

	*mb->b_wptr++ = ' ';
	if (id < 0) {
		id = -id;
		*mb->b_wptr++ = '-';
	}
	xptr = &xx[10];
	*--xptr = '\0';
	do {
		*--xptr = (id % 10) + '0';
		id /= 10;
	} while (id > 0);
	while (*xptr)
		*mb->b_wptr++ = *xptr++;
}

void
m_puts (mblk_t * mb, uchar_t * data, int len)
{
	if (mb == NULL)
		return;
	if (mb->b_wptr + 2 >= DATA_END(mb))
		return;
	*mb->b_wptr++ = ' ';
	while (len-- > 0) {
		if (mb->b_wptr >= DATA_END(mb))
			return;
		*mb->b_wptr++ = *data++;
	}
}

void
m_putsz (mblk_t * mb, uchar_t * data)
{
	if (mb == NULL)
		return;
	if (mb->b_wptr + 2 >= DATA_END(mb))
		return;
	*mb->b_wptr++ = ' ';
	while (*data != '\0') {
		if (mb->b_wptr >= DATA_END(mb))
			return;
		*mb->b_wptr++ = *data++;
	}
}

void m_puthex(mblk_t *mb, uchar_t *id, int len)
{
	uchar_t ch;

	if (mb == NULL)
		return;
	if (len < 0)
		return;
	if (len == 0) {
		static uchar_t cx = 0;
		len = 1;
		id = &cx;
	}
	if (mb->b_wptr+2*len+1 > DATA_END(mb))
		return;
	*mb->b_wptr++ = ' ';

	if(len == 0) {
		*mb->b_wptr++ = '0';
		*mb->b_wptr++ = '0';
	} else while(--len >= 0) {
		ch = *id >> 4;
		if(ch > 9)
			*mb->b_wptr++ = ch+'A'-10;
		else
			*mb->b_wptr++ = ch+'0';

		ch = *id++ & 0x0F;
		if(ch > 9)
			*mb->b_wptr++ = ch+'A'-10;
		else
			*mb->b_wptr++ = ch+'0';
	}
}

void
m_putx (mblk_t * mb, ulong_t id)
{
	char xx[10];
	char *xptr;

	if (mb == NULL)
		return;
	if (mb->b_wptr + 10 > DATA_END(mb))
		return;

	*mb->b_wptr++ = ' ';
	xptr = &xx[10];
	*--xptr = '\0';
	do {
		int ch = id & 0x0F;

		if (ch < 10)
			*--xptr = ch + '0';
		else
			*--xptr = ch - 10 + 'A';
		id >>= 4;
	} while (id != 0);
	while (*xptr)
		*mb->b_wptr++ = *xptr++;
}



void
m_getnskip (mblk_t * mb)
{
	if (mb == NULL)
		return;
	while ((mb->b_rptr < mb->b_wptr) && !isspace (*mb->b_rptr))
		mb->b_rptr++;
}

void
m_getskip (mblk_t * mb)
{
	if (mb == NULL)
		return;
	while ((mb->b_rptr < mb->b_wptr) && isspace (*mb->b_rptr))
		mb->b_rptr++;
}

int
m_getid (mblk_t * mb, ushort_t * id)
{
#ifdef ALIGNED_ONLY
	ushort_t xid = 0;
	int i;
#endif

	if (mb == NULL)
		return -EINVAL;
	m_getskip (mb);
	while (mb->b_rptr < mb->b_wptr && *mb->b_rptr == ':')
		mb->b_rptr++;
	if (mb->b_rptr + sizeof(ushort_t) > mb->b_wptr)
		return -ESRCH;
#ifdef ALIGNED_ONLY
	for (i = 0; i < sizeof(ushort_t); i++)
		xid = xid << 8 | (*mb->b_rptr++ & 0xFF);
	*id = xid;
#else
	*id = *((ushort_t *)mb->b_rptr)++;
#endif
	return 0;
}

int
m_getsx (mblk_t * mb, ushort_t * id)
{
#ifdef ALIGNED_ONLY
	ushort_t xid = 0;
	int i;
#endif

	if (mb == NULL)
		return -EINVAL;
	while (1) {
		m_getskip (mb);
		if (mb->b_rptr >= mb->b_wptr)
			return -ESRCH;
		if (*mb->b_rptr == ':') {
			mb->b_rptr++;
			break;
		}
		m_getnskip(mb);
	}
	if (*mb->b_rptr == ':') {
		mb->b_rptr++;
		return -EAGAIN;
	}
	if (mb->b_rptr + sizeof(ushort_t) > mb->b_wptr)
		return -ESRCH;
#ifdef ALIGNED_ONLY
	for (i = 0; i < sizeof(ushort_t); i++) 
		xid = (xid << 8) | (*mb->b_rptr++ & 0xFF);
	*id = xid;
#else
	*id = *((ushort_t *)mb->b_rptr)++;
#endif
	return 0;
}

int
m_getlx (mblk_t * mb, ulong_t * id)
{
#ifdef ALIGNED_ONLY
	ulong_t xid = 0;
	int i;
#endif

	if (mb == NULL)
		return -EINVAL;
	m_getskip (mb);
	if (*mb->b_rptr == ':')
		return -ENOENT;
	if (mb->b_rptr + sizeof(ulong_t) > mb->b_wptr)
		return -ESRCH;
#ifdef ALIGNED_ONLY
	for (i = 0; i < sizeof(ulong_t); i++)
		xid = (xid << 8) | (*mb->b_rptr++ & 0xFF);
	*id = xid;
#else
	*id = *((ulong_t *)mb->b_rptr)++;
#endif
	return 0;
}

int
m_geti (mblk_t * mb, long *id)
{
	int x = 0;
	int neg = 0;
	streamchar *oldp;

	if (mb == NULL)
		return -EINVAL;
	m_getskip (mb);
	if (mb->b_rptr >= mb->b_wptr)
		return -ESRCH;
	if (*mb->b_rptr == '-') {
		neg = 1;
		if (++mb->b_rptr >= mb->b_wptr)
			return -ESRCH;
	}
	oldp = mb->b_rptr;
	if (*oldp == ':')
		return -ENOENT;
	while ((mb->b_rptr < mb->b_wptr) && (*mb->b_rptr >= '0' && *mb->b_rptr <= '9'))
		x = x * 10 + *mb->b_rptr++ - '0';
	if (oldp == mb->b_rptr)
		return -ESRCH;
	if (neg)
		*id = -x;
	else
		*id = x;
	return 0;
}

int
m_getip(mblk_t * mb, ulong_t * ipa)
{
	unsigned long id;
	long xx; streamchar x;
	int err;

	err = m_geti(mb,&xx);
	if(err < 0)
		return err;
	id = xx<<24;

	err = m_getc(mb,&x);
	if(err < 0)
		return err;
	if(x != '.')
		return -EINVAL;

	err = m_geti(mb,&xx);
	if(err < 0)
		return err;
	id |= xx<<16;

	err = m_getc(mb,&x);
	if(err < 0)
		return err;
	if(x != '.')
		return -EINVAL;

	err = m_geti(mb,&xx);
	if(err < 0)
		return err;
	id |= xx<<8;

	err = m_getc(mb,&x);
	if(err < 0)
		return err;
	if(x != '.')
		return -EINVAL;

	err = m_geti(mb,&xx);
	if(err < 0)
		return err;
	id |= xx;
	*ipa = id;

	return 0;
}

int
m_getx (mblk_t * mb, ulong_t * id)
{
	int x = 0;
	streamchar *oldp;
	char ch;

	if (mb == NULL)
		return -EINVAL;
	m_getskip (mb);
	if (mb->b_rptr >= mb->b_wptr)
		return -ESRCH;

	oldp = mb->b_rptr;
	if (*oldp == ':')
		return -ENOENT;
	while (ch = *mb->b_rptr, ((ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'F') || (ch >= 'a' && ch <= 'f'))) {
		if (ch <= '9')
			ch -= '0';
		else if (ch >= 'a')
			ch -= 'a' - 10;
		else
			ch -= 'A' - 10;
		x = (x << 4) + ch;
		mb->b_rptr++;
		if (mb->b_rptr >= mb->b_wptr)
			break;
	}
	if (oldp == mb->b_rptr)
		return -ESRCH;
	*id = x;
	return 0;
}

int
m_gethex (mblk_t * mb, uchar_t * id, int len)
{
	uchar_t x = 0; /* shut up GCC */
	uchar_t ch;
	char upper = 1;

	if (mb == NULL)
		return -EINVAL;
	m_getskip (mb);
	if (mb->b_rptr >= mb->b_wptr)
		return 0;

	if (*mb->b_rptr == ':')
		return -ENOENT;
	while (len > 0) {
		if (mb->b_rptr >= mb->b_wptr)
			return -EINVAL;
		ch = *mb->b_rptr++;
		switch(ch)
		{
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			ch -= '0';
			break;
		case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
			ch -= 'A'-10;
			break;
		case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
			ch -= 'a'-10;
			break;
		default:
			return -EINVAL;
		}
		if (upper) {
			x = ch << 4;
			upper = 0;
		} else {
			*id++ = x | ch;
			len--;
			upper = 1;
		}
	}
	return 0;
}

int
m_gethexlen (mblk_t * mb)
{
	int len = 0;
	uchar_t ch;
	streamchar *oldp;

	if (mb == NULL)
		return -EINVAL;
	m_getskip (mb);
	if (mb->b_rptr >= mb->b_wptr)
		return -ESRCH;

	oldp = mb->b_rptr;
	if (*oldp == ':')
		return -ENOENT;
	while (mb->b_rptr < mb->b_wptr) {
		ch = *mb->b_rptr++;
		switch(ch)
		{
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
		case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
		case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
			len++;
			break;
		default:
			goto ex;
		}
	}
  ex:
	mb->b_rptr = oldp;
	if (len & 1)
		return 0;
	return len >> 1;
}

int
m_getstr (mblk_t * mb, char *str, int maxlen)
{
	streamchar *p;
	streamchar *lim;

	if (mb == NULL)
		return -EINVAL;
	m_getskip (mb);
	if (mb->b_rptr >= mb->b_wptr)
		return -ESRCH;

	p = mb->b_rptr;
	if (*p == ':')
		return -ENOENT;
	lim = mb->b_wptr;
	while (!isspace (*p)) {
		if (maxlen == 0 || p == lim)
			break;
		*str++ = *p++;
		maxlen--;
	}
	mb->b_rptr = p;
	*str = '\0';
	return 0;
}


int
m_getstrlen (mblk_t * mb)
{
	streamchar *p;
	streamchar *lim;
	int len = 0;

	if (mb == NULL)
		return -EINVAL;
	m_getskip (mb);
	if (mb->b_rptr >= mb->b_wptr)
		return -ESRCH;

	p = mb->b_rptr;
	if (*p == ':')
		return -ENOENT;
	lim = mb->b_wptr;
	while (!isspace (*p)) {
		if (p == lim)
			break;
		len++;
		p++;
	}
	return len;
}


int
m_getc (mblk_t * mb, char *c)
{
	if (mb == NULL)
		return -EINVAL;
	m_getskip (mb);
	if (mb->b_rptr >= mb->b_wptr)
		return -ESRCH;

	if (*mb->b_rptr == ':')
		return -ENOENT;
	*c = *mb->b_rptr++;
	return 0;
}


mblk_t *
#ifdef CONFIG_DEBUG_STREAMS
deb_make_reply (const char *deb_file, unsigned int deb_line, int err)
#else
make_reply (int err)
#endif
{
#ifdef CONFIG_DEBUG_STREAMS
	mblk_t *mq = deb_allocb (deb_file,deb_line,err ? 16 : 8, BPRI_HI);
#else
	mblk_t *mq = allocb (err ? 16 : 8, BPRI_HI);
#endif

	if (mq == NULL) {
#ifdef CONFIG_DEBUG_STREAMS
		printf("* NoMem make_reply %s:%d %d\n",deb_file,deb_line,err);
#else
		printf("* NoMem make_reply %d\n",err);
#endif
		return NULL;
	}
	if (err == 0) {
		m_putid (mq, PROTO_NOERROR);
	} else {
		m_putid (mq, PROTO_ERROR);
		m_putsx (mq, PROTO_ERROR);
		m_puti (mq, (err > 0) ? err : -err);
	}
	m_putdelim (mq);
	return mq;
}

void
#ifdef CONFIG_DEBUG_STREAMS
deb_md_reply (const char *deb_file, unsigned int deb_line,queue_t * q, mblk_t * mb, int err)
#else
md_reply (queue_t * q, mblk_t * mb, int err)
#endif
{
#ifdef CONFIG_DEBUG_STREAMS
	mblk_t *mq = deb_make_reply(deb_file,deb_line,err);
#else
	mblk_t *mq = make_reply(err);
#endif

	if (mq == NULL) {
		freemsg (mb);
		return;
	}
	linkb (mq, mb);
	putnext (OTHERQ (q), mq);
}


void
#ifdef CONFIG_DEBUG_STREAMS
deb_m_reply (const char *deb_file, unsigned int deb_line,queue_t * q, mblk_t * mb, int err)
#else
m_reply (queue_t * q, mblk_t * mb, int err)
#endif
{
#ifdef CONFIG_DEBUG_STREAMS
	mblk_t *mq = deb_make_reply(deb_file,deb_line,err);
#else
	mblk_t *mq = make_reply(err);
#endif

	if (mq == NULL) {
		freemsg (mb);
		return;
	}
	linkb (mq, mb);
	DATA_TYPE(mq) = MSG_PROTO;
	putnext (OTHERQ (q), mq);
}


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
