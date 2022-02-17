/*
 * Routines to compress and uncompess tcp packets (for transmission over low
 * speed serial lines.
 * 
 * Copyright (c) 1989 Regents of the University of California. All rights
 * reserved.
 * 
 * Redistribution and use in source and binary forms are permitted provided that
 * the above copyright notice and this paragraph are duplicated in all such
 * forms and that any documentation, advertising materials, and other materials
 * related to such distribution and use acknowledge that the software was
 * developed by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived from this
 * software without specific prior written permission. THIS SOFTWARE IS
 * PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
 * WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE.
 * 
 * Van Jacobson (van@helios.ee.lbl.gov), Dec 31, 1989: - Initial distribution.
 */

#define F_NOCODE
#include "f_module.h"
#include "primitives.h"
#include "f_ip.h"
#include "compress.h"
#include "streamlib.h"
#include "vanj.h"

#ifndef SL_NO_STATS
#define INCR(counter) do { ++comp->counter } while(0)
#else
#define INCR(counter)
#endif

#ifdef SYSV
#define BCMP(p1,p2,n) memcmp((char *)(p1), (char *)(p2), (int)(n))
#else
#define BCMP(p1, p2, n) bcmp((char *)(p1), (char *)(p2), (int)(n))
#endif
#define BCOPY(p1, p2, n) bcopy((char *)(p1), (char *)(p2), (int)(n))
#ifndef KERNEL
#define ovbcopy bcopy
#endif

void
compress_init (struct compress *comp)
{
	uint_t i;
	struct cstate *tstate = comp->tstate;

	bzero ((char *) comp, sizeof (*comp));
	for (i = MAX_STATES - 1; i > 0; --i) {
		tstate[i].cs_id = i;
		tstate[i].cs_next = &tstate[i - 1];
	}
	tstate[0].cs_next = &tstate[MAX_STATES - 1];
	tstate[0].cs_id = 0;
	comp->last_cs = &tstate[0];
	comp->last_recv = 255;
	comp->last_xmit = 255;
	comp->flags = SLF_TOSS;
}


/*
 * ENCODE encodes a number that is known to be non-zero.  ENCODEZ checks for
 * zero (since zero has to be encoded in the long, 3 byte form).
 */
#define ENCODE(n) do { \
	if ((ushort_t)(n) >= 256) { \
		*cp++ = 0; \
		cp[1] = (n); \
		cp[0] = (n) >> 8; \
		cp += 2; \
	} else { \
		*cp++ = (n); \
	} \
} while(0)

#define ENCODEZ(n) do { \
	if ((ushort_t)(n) >= 256 || (ushort_t)(n) == 0) { \
		*cp++ = 0; \
		cp[1] = (n); \
		cp[0] = (n) >> 8; \
		cp += 2; \
	} else { \
		*cp++ = (n); \
	} \
} while(0)

#define DECODEL(f) do { \
	if (*cp == 0) {\
		(f) = htonl(ntohl(f) + ((cp[1] << 8) | cp[2])); \
		cp += 3; \
	} else { \
		(f) = htonl(ntohl(f) + (ulong_t)*cp++); \
	} \
} while(0)

#define DECODES(f) do { \
	if (*cp == 0) {\
		(f) = htons(ntohs(f) + ((cp[1] << 8) | cp[2])); \
		cp += 3; \
	} else { \
		(f) = htons(ntohs(f) + (ulong_t)*cp++); \
	} \
} while(0)

#define DECODEU(f) do { \
	if (*cp == 0) {\
		(f) = htons((cp[1] << 8) | cp[2]); \
		cp += 3; \
	} else { \
		(f) = htons((ulong_t)*cp++); \
	} \
} while(0)


ushort_t
compress_tcp (struct compress *comp, mblk_t ** mp)
{
	struct cstate *cs = comp->last_cs->cs_next;
	struct ip *p_ip;
	uint_t hlen;
	struct tcphdr *oth;
	struct tcphdr *th;
	uint_t deltaS, deltaA;
	uchar_t changes = 0;
	uchar_t new_seq[16];
	uchar_t *cp = new_seq;

	mblk_t *mq = *mp;

	/* This might already have happened, but we make sure */

	mq = pullupm (mq, 128);
	if (mq == NULL)
		return (htons (PPP_PROTO_IP));

	*mp = mq;
	p_ip = (struct ip *) mq->b_rptr;
	hlen = p_ip->ip_hl;


	/*
	 * Bail if this is an IP fragment or if the TCP packet isn't `compressible'
	 * (i.e., ACK isn't set or some other control bit is set).  (We do NOT
	 * assume that the caller has already made sure the packet is IP proto
	 * TCP).
	 */
	if (p_ip->ip_p != IPPROTO_TCP
			|| (p_ip->ip_off & htons (0x3fff)) || mq->b_wptr - mq->b_rptr < 40)
		return (htons (PPP_PROTO_IP));

	th = (struct tcphdr *) & ((uchar_t *) p_ip)[hlen << 2];
	if ((THFLAG(th) & (TH_SYN | TH_FIN | TH_RST | TH_ACK)) != TH_ACK)
		return (htons (PPP_PROTO_IP));
	/*
	 * Packet is compressible -- we're going to send either a COMPRESSED_TCP or
	 * UNCOMPRESSED_TCP packet.  Either way we need to locate (or create) the
	 * connection state.  Special case the most recently used connection since
	 * it's most likely to be used again & we don't have to do any reordering
	 * if it's used.
	 */
	INCR (sls_packets);
	if (ADR(p_ip->ip_src) != ADR(cs->cs_ip.ip_src) ||
			ADR(p_ip->ip_dst) != ADR(cs->cs_ip.ip_dst) ||
			*(ulong_t *) th != *(ulong_t *) & (((uchar_t *) & cs->cs_ip)[cs->cs_ip.ip_hl << 2])) {
		/*
		 * Wasn't the first -- search for it.
		 * 
		 * States are kept in a circularly linked list with last_cs pointing to
		 * the end of the list.  The list is kept in lru order by moving a
		 * state to the head of the list whenever it is referenced.  Since the
		 * list is short and, empirically, the connection we want is almost
		 * always near the front, we locate states via linear search.  If we
		 * don't find a state for the datagram, the oldest state is (re-)used.
		 */
		struct cstate *lcs;
		struct cstate *lastcs = comp->last_cs;

		do {
			lcs = cs;
			cs = cs->cs_next;
			INCR (sls_searches);
			if (ADR(p_ip->ip_src) == ADR(cs->cs_ip.ip_src)
					&& ADR(p_ip->ip_dst) == ADR(cs->cs_ip.ip_dst)
					&& *(ulong_t *) th == *(ulong_t *) & (((uchar_t *) & cs->cs_ip)[cs->cs_ip.ip_hl]))
				goto found;
		} while (cs != lastcs);

		/*
		 * Didn't find it -- re-use oldest cstate.  Send an uncompressed packet
		 * that tells the other side what connection number we're using for
		 * this conversation. Note that since the state list is circular, the
		 * oldest state points to the newest and we only need to set last_cs to
		 * update the lru linkage.
		 */
		INCR (sls_misses);
		comp->last_cs = lcs;
		hlen += th->th_off;
		hlen <<= 2;
		goto uncompressed;

	  found:
		/*
		 * Found it -- move to the front on the connection list.
		 */
		if (cs == lastcs)
			comp->last_cs = lcs;
		else {
			lcs->cs_next = cs->cs_next;
			cs->cs_next = lastcs->cs_next;
			lastcs->cs_next = cs;
		}
	}
	/*
	 * Make sure that only what we expect to change changed. The first line of
	 * the `if' checks the IP protocol version, header length & type of
	 * service.  The 2nd line checks the "Don't fragment" bit. The 3rd line
	 * checks the time-to-live and protocol (the protocol check is unnecessary
	 * but costless).  The 4th line checks the TCP header length.  The 5th line
	 * checks IP options, if any.  The 6th line checks TCP options, if any.  If
	 * any of these things are different between the previous & current
	 * datagram, we send the current datagram `uncompressed'.
	 */
	oth = (struct tcphdr *) & ((ulong_t *) & cs->cs_ip)[hlen];
	deltaS = hlen;
	hlen += th->th_off;
	hlen <<= 2;

	if (((ushort_t *) p_ip)[0] != ((ushort_t *) & cs->cs_ip)[0] ||
			((ushort_t *) p_ip)[3] != ((ushort_t *) & cs->cs_ip)[3] ||
			((ushort_t *) p_ip)[4] != ((ushort_t *) & cs->cs_ip)[4] ||
			th->th_off != oth->th_off ||
			(deltaS > 5 &&
					BCMP (p_ip + 1, &cs->cs_ip + 1, (deltaS - 5) << 2)) ||
			(th->th_off > 5 &&
					BCMP (th + 1, oth + 1, (th->th_off - 5) << 2)))
		goto uncompressed;

	/*
	 * Figure out which of the changing fields changed.  The receiver expects
	 * changes in the order: urgent, window, ack, seq (the order minimizes the
	 * number of temporaries needed in this section of code).
	 */
	if (THFLAG(th) & TH_URG) {
		deltaS = ntohs (th->th_urp);
		ENCODEZ (deltaS);
		changes |= NEW_U;
	} else if (th->th_urp != oth->th_urp)
		/*
		 * argh! URG not set but urp changed -- a sensible implementation
		 * should never do this but RFC793 doesn't prohibit the change so we
		 * have to deal with it.
		 */
		goto uncompressed;

	if ((deltaS = (ushort_t) (ntohs (th->th_win) - ntohs (oth->th_win))) != 0) {
		ENCODE (deltaS);
		changes |= NEW_W;
	}
	if ((deltaA = ntohl (th->th_ack) - ntohl (oth->th_ack)) != 0) {
		if (deltaA > 0xffff)
			goto uncompressed;
		ENCODE (deltaA);
		changes |= NEW_A;
	}
	if ((deltaS = ntohl (th->th_seq) - ntohl (oth->th_seq)) != 0) {
		if (deltaS > 0xffff)
			goto uncompressed;
		ENCODE (deltaS);
		changes |= NEW_S;
	}
	switch (changes) {

	case 0:
		/*
		 * Nothing changed. If this packet contains data and the last one
		 * didn't, this is probably a data packet following an ack (normal on
		 * an interactive connection) and we send it compressed.  Otherwise
		 * it's probably a retransmit, retransmitted ack or window probe.  Send
		 * it uncompressed in case the other side missed the compressed
		 * version.
		 */
		if (p_ip->ip_len != cs->cs_ip.ip_len &&
				ntohs (cs->cs_ip.ip_len) == hlen)
			break;

		/* (fall through) */

	case SPECIAL_I:
	case SPECIAL_D:
		/*
		 * actual changes match one of our special case encodings -- send
		 * packet uncompressed.
		 */
		goto uncompressed;

	case NEW_S | NEW_A:
		if (deltaS == deltaA &&
				deltaS == ntohs (cs->cs_ip.ip_len) - hlen) {
			/* special case for echoed terminal traffic */
			changes = SPECIAL_I;
			cp = new_seq;
		}
		break;

	case NEW_S:
		if (deltaS == ntohs (cs->cs_ip.ip_len) - hlen) {
			/* special case for data xfer */
			changes = SPECIAL_D;
			cp = new_seq;
		}
		break;
	}

	deltaS = ntohs (p_ip->ip_id) - ntohs (cs->cs_ip.ip_id);
	if (deltaS != 1) {
		ENCODEZ (deltaS);
		changes |= NEW_I;
	}
	if (THFLAG(th) & TH_PUSH)
		changes |= TCP_PUSH_BIT;
	/*
	 * Grab the cksum before we overwrite it below.  Then update our state with
	 * this packet's header.
	 */
	deltaA = ntohs (th->th_sum);
	BCOPY (p_ip, &cs->cs_ip, hlen);

	/*
	 * We want to use the original packet as our compressed packet. (cp -
	 * new_seq) is the number of bytes we need for compressed sequence numbers.
	 * In addition we need one byte for the change mask, one for the connection
	 * id and two for the tcp checksum. So, (cp - new_seq) + 4 bytes of header
	 * are needed.  hlen is how many bytes of the original packet to toss so
	 * subtract the two to get the new packet size.
	 */
	deltaS = cp - new_seq;
	cp = (uchar_t *) p_ip;
	if (comp->last_xmit != cs->cs_id) {
		comp->last_xmit = cs->cs_id;
		hlen -= deltaS + 4;
		cp += hlen;
		(uchar_t *) mq->b_rptr = cp;
		*cp++ = changes | NEW_C;
		*cp++ = cs->cs_id;
	} else {
		hlen -= deltaS + 3;
		cp += hlen;
		(uchar_t *) mq->b_rptr = cp;
		*cp++ = changes;
	}
	*cp++ = deltaA >> 8;
	*cp++ = deltaA;
	BCOPY (new_seq, cp, deltaS);
	INCR (sls_compressed);
	return (htons (PPP_PROTO_VJC_COMP));

	/*
	 * Update connection state cs & send uncompressed packet ('uncompressed'
	 * means a regular ip/tcp packet but with the 'conversation id' we hope to
	 * use on future compressed packets in the protocol field).
	 */
  uncompressed:
	BCOPY (p_ip, &cs->cs_ip, hlen);
	p_ip->ip_p = cs->cs_id;
	comp->last_xmit = cs->cs_id;
	return (htons (PPP_PROTO_VJC_UNCOMP));
}


void
uncompress_tcp (struct compress *comp, mblk_t ** mp, ushort_t protocol)
{
	uchar_t *cp;
	uint_t hlen;
	uchar_t changes;
	struct tcphdr *th;
	struct cstate *cs;
	struct ip *p_ip;

	mblk_t *mq = *mp;

	/* This might already have happened, but we make sure */

	switch (ntohs (protocol)) {

	case PPP_PROTO_VJC_UNCOMP:
		if(mq->b_wptr-mq->b_rptr < sizeof(struct ip)) {
			printf("VJ: Small packet1 %d\n",mq->b_wptr-mq->b_rptr);
			goto bad;
		}
			
		p_ip = (struct ip *) mq->b_rptr;
		if (p_ip->ip_p >= MAX_STATES) {
			printf("VJ: state table1 %d\n",p_ip->ip_p);
			goto bad;
		}
		cs = &comp->rstate[comp->last_recv = p_ip->ip_p];
		comp->flags &= ~SLF_TOSS;
		p_ip->ip_p = IPPROTO_TCP;
		hlen = p_ip->ip_hl;
		hlen += ((struct tcphdr *) & ((int *) p_ip)[hlen])->th_off;
		hlen <<= 2;
		if(mq->b_wptr-mq->b_rptr < hlen) {
			printf("VJ: Small packet2 %d %d\n",mq->b_wptr-mq->b_rptr, hlen);
			goto bad;
		}
			
		BCOPY (p_ip, &cs->cs_ip, hlen);
		cs->cs_ip.ip_sum = 0;
		cs->cs_hlen = hlen;
		INCR (sls_uncompressedin);
		return;

	default:
		printf("VJ: unknown type %x\n",ntohs(protocol));
		goto bad;

	case PPP_PROTO_VJC_COMP:
		break;
	}
	/* We've got a compressed packet. */

	INCR (sls_compressedin);
	cp = (uchar_t *) mq->b_rptr;
	changes = *cp++;
	if (changes & NEW_C) {
		/*
		 * Make sure the state index is in range, then grab the state. If we
		 * have a good state index, clear the 'discard' flag.
		 */
		if (*cp >= MAX_STATES) {
			printf("VJ: state table2 %d\n",*cp);
			goto bad;
		}

		comp->flags &= ~SLF_TOSS;
		comp->last_recv = *cp++;
	} else {
		/*
		 * this packet has an implicit state index.  If we've had a line error
		 * since the last time we got an explicit state index, we have to toss
		 * the packet.
		 */
		if (comp->flags & SLF_TOSS) {
			INCR (sls_tossed);
			printf("VJ: line error, toss\n");
			goto bad2;
		}
	}
	cs = &comp->rstate[comp->last_recv];
	hlen = cs->cs_ip.ip_hl << 2;
	th = (struct tcphdr *) & ((uchar_t *) & cs->cs_ip)[hlen];
	th->th_sum = htons ((*cp << 8) | cp[1]);
	cp += 2;
	if (changes & TCP_PUSH_BIT)
		THFLAG(th) |= TH_PUSH;
	else
		THFLAG(th) &= ~TH_PUSH;

	switch (changes & SPECIALS_MASK) {
	case SPECIAL_I:
		{
			uint_t i = ntohs (cs->cs_ip.ip_len) - cs->cs_hlen;

			th->th_ack = htonl (ntohl (th->th_ack) + i);
			th->th_seq = htonl (ntohl (th->th_seq) + i);
		}
		break;

	case SPECIAL_D:
		th->th_seq = htonl (ntohl (th->th_seq) + ntohs (cs->cs_ip.ip_len)
				- cs->cs_hlen);
		break;

	default:
		if (changes & NEW_U) {
			THFLAG(th) |= TH_URG;
			DECODEU (th->th_urp);
		} else
			THFLAG(th) &= ~TH_URG;
		if (changes & NEW_W)
			DECODES (th->th_win);
		if (changes & NEW_A)
			DECODEL (th->th_ack);
		if (changes & NEW_S)
			DECODEL (th->th_seq);
		break;
	}
	if (changes & NEW_I) {
		DECODES (cs->cs_ip.ip_id);
	} else
		cs->cs_ip.ip_id = htons (ntohs (cs->cs_ip.ip_id) + 1);

	/*
	 * If we overran our buffer, something is seriously bad.
	 * 
	 * We may have corrupted the TCP+IP data.  Drop them.
	 */
	if (cp > (uchar_t *) mq->b_wptr) {
		printf ("VAN_J: IP data corrupted.\n");
		ADR(cs->cs_ip.ip_src) = 0;	/* illegal, so this will never be found */
		goto bad;
	}
	/*
	 * At this point, cp points to the first byte of data in the packet.  Get
	 * the new mblk_t for the header, and finish things.
	 */

	(uchar_t *) mq->b_rptr = cp;

	{
		mblk_t *mp2 = allocb (cs->cs_hlen << 2, BPRI_MED);

		if (mp2 == NULL) {
			printf("VJ: No data %d\n",cs->cs_hlen << 2);
			goto bad;
		}
		cs->cs_ip.ip_len = htons(cs->cs_hlen + dsize(mq));
		BCOPY (&cs->cs_ip, mp2->b_wptr, cs->cs_hlen);
		mp2->b_wptr += cs->cs_hlen;
		mq = pullupm(mq,0);
		if(mq != NULL)
			linkb (mp2, mq);
		mq = *mp = mp2;
	}

	/* recompute the ip header checksum */
	{
		ushort_t *bp = (ushort_t *) mq->b_rptr;
		ulong_t chksum = 0;

		if(0)printf("ReCk(%x) ",((struct ip *) mq->b_rptr)->ip_sum);
		for (; hlen > 0; hlen -= 2) {
			if(0)printf("%x ",*bp);
			chksum += *bp++;
		}
		if(0)printf("= %lux",chksum);
		chksum = (chksum & 0xffff) + (chksum >> 16);
		chksum = (chksum & 0xffff) + (chksum >> 16);
		((struct ip *) mq->b_rptr)->ip_sum = ~chksum;
		if(0)printf("= %x",((struct ip *) mq->b_rptr)->ip_sum);
	}
	return;
  bad:
	comp->flags |= SLF_TOSS;
	INCR (sls_errorin);
  bad2:
	if (*mp != NULL) {
		freemsg (*mp);
		*mp = NULL;
	}
	return;
}
