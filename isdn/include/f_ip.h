#ifndef _F_IP
#define _F_IP

#define INET
#include "primitives.h"

#if defined(linux) && !defined(CONFIG_INET_BSD)

/* If somebody could please reboot the person responsible for this.
   Start over. It avoids a big whole lot of hassle for us all. Thanks. */
#define iphdr ip
#define ip_hl ihl
#define ip_v version
#define ip_tos tos
#define ip_len tot_len
#define ip_id id
#define ip_off frag_off
#define ip_ttl ttl
#define ip_p protocol
#define ip_sum check
#define ip_src saddr
#define ip_dst daddr

#define tcphdr tcp
#define th_sport source
#define th_dport dest
#define th_seq seq
#define th_ack ack_seq
#define THFLAG(a) *((unsigned char *)(&a)+13)
#define TH_FIN  0x01
#define TH_SYN  0x02
#define TH_RST  0x04
#define TH_PUSH 0x08
#define TH_ACK  0x10
#define TH_URG  0x20

#define th_off doff
#define th_win window
#define th_sum check
#define th_urp urg_ptr

#define uh_sport source
#define uh_dport dest
#define uh_ulen len
#define uh_sum check

#define icmphdr icmp
#define icmp_type type
#define icmp_code code
#define icmp_cksum checksum


#include <linux/in.h>
#include <linux/ip.h>
#include <linux/icmp.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#define ADR(x) x /* in struct ip */
#define _F_DONE
#endif


#ifndef _F_DONE
#define ADR(x) x.s_addr

#include <net/if.h>
#ifdef AUX
#include <net/netisr.h>
#include <sys/protosw.h>
#endif
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/ip_icmp.h>
#define THFLAG(a) (a)->th_flags
#define SOCK_HAS_LEN	/* socket has sa_len field */
#endif
#endif /* _F_IP */
