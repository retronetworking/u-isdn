#ifndef _PORT_M
#define _PORT_M

#include <sys/types.h>
#include <sys/param.h>
#include "isdn_limits.h"
#include "smallq.h"
#include "msgtype.h"
#include "primitives.h"

#define PORT_MODULE CHAR2('p','M') /* minor */
#define PORT_DRIVER CHAR2('p','D') /* minor */
#define PORT_MODDRV CHAR2('p','X') /* minor */
#define PORT_SEQ CHAR2('p','S') /* longint */
#define PORT_LINK CHAR2('p','L') /* ID : */
#define PORT_DATA CHAR2('p','>') /* ID : */
#define PORT_SETUP CHAR2('p','!')
#define PORT_IOCTL CHAR2('p','I')
#define PORT_PASS CHAR2('p','P') /* longint */
#define PORT_BAUD CHAR2('p','B') /* 1..15 */
#define PORT_FLAGS CHAR2('p','F') /* 0..0xFF */
# define PORT_F_PASS 01
			/* 1 data written to the module are passed through */
			/* 0 data written to the module are reported */

#define PORT_OPEN CHAR2('p','O') /* ID : */
#define PORT_CLOSED CHAR2('p','C') /* ID : */
#define PORT_WANTCLOSED CHAR2('p','W') /* ID : */

#define PASS_ONEMSG 0x7650DEAD

#define NAME_PORT "/dev/portm"
#define NAME_PORTMAN "/dev/portman"
#define NAME_ATINTR "./atcmd"

#endif							/* _PORT_M */
