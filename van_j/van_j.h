#ifndef __VAN_J
#define __VAN_J

/* #include "../ppp/ppp.h" */

#define PPP_PROTO_IP			0x0021
#define PPP_PROTO_VJC_COMP		0x002D
#define PPP_PROTO_VJC_UNCOMP	0x002F

/* End non-include */

#define VAN_J_PPP 	  01
#define VAN_J_ACTIVE  02
#define VAN_J_PASSIVE 04
#define VAN_J_CONN    010

#if defined(linux) && !defined(CONFIG_INET_BSD)
#define struct_ip struct iphdr
#else
#define struct_ip struct ip
#endif

#endif							/* __VAN_J */
