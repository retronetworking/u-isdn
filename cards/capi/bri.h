/************************************************************************/
/*									*/
/*	(C) 1993 BinTec Computersysteme					*/
/*	All Rights Reserved						*/
/*									*/
/************************************************************************/

#ifdef __linux__
#define far
#endif

/************************************************************************


	==================================================
	=  BIANCA-BRI, BRI-4	ISDN adapter for AT-BUS  =
	==================================================

	memory layout, 

	all shorts are motorola format, and have to be swapped to intel format

	16 KByte shared memory

	+---------------+
	| 0x0008	|	8 KByte rcv buffer 
	~		~
	|---------------|
	| 0x2000	|	8 KByte snd buffer
	~		~
	|---------------|
	| 0x3ff0	|	size of rcv buffer
	| 0x3ff2	|	write index rcv buffer
	| 0x3ff4	|	read index rcv buffer
	|---------------|
	| 0x3ff6	|	size of snd buffer
	| 0x3ff8	|	write index snd buffer
	| 0x3ffa	|	read index snd buffer
	|---------------|
	| 0x3ffc	|	state register
	| 0x3ffd	|	debug register
	| 0x3ffe	|	control register
	+---------------+



	state register (BRI, BRI-4)
	+-------+--------------------+----------------------------------+
	|  BIT  |	READ         |		WRITE			|
	+-------+--------------------+----------------------------------+
	|   0   |                    |  1 -> gen interrupt if 680x0 CPU |
	|	|                    | 	     has filled rcv buffer with |
	|	|                    | 	     data                       |
	+-------+--------------------+----------------------------------+
	|   1   |                    |  1 -> gen interrupt if 680x0 CPU |
	|	|                    | 	     has read data from snd     |
	|	|                    | 	     buffer                     |
	+-------+--------------------+----------------------------------+
	|   7   | 1 -> hardware      |					|
	|       |      failure	     |					|
	+-------+--------------------+----------------------------------+

	control register (BRI, BRI-4)
	+-------+--------------------+----------------------------------+
	|  BIT  |	READ         |		WRITE			|
	+-------+--------------------+----------------------------------+
	|   0   |  1 -> interrupt to |  0 -> disable interrupts 	|
	|	|       host pending | 	1 -> enable interrupts		|
	+-------+--------------------+----------------------------------+
	|   1   |  0 -> CPU in reset |	0 -> reset CPU			|
	+-------+--------------------+----------------------------------+
	|   2   |  0 -> CPU in halt  |	0 -> halt CPU			|
	|	|  1 -> CPU run      |	1 -> run CPU			|
	+-------+--------------------+----------------------------------+
	|   3   |	   	     |	1 -> enable cpu cache		|
	+-------+--------------------+----------------------------------+
	|   4   |  	             |	1 -> interrupt to 680x0 to read	|
	|	|	             |	     shared memory		|
	+-------+--------------------+----------------------------------+
	|  7-5  |  0 -> BRI-4        |					|
	|	|  1 -> X21	     |					|
	|	|  2 -> BRI	     |					|
	+-------+-------------------------------------------------------+





	==================================================
	=  BIANCA-PMX	S2M ISDN adapter for AT-BUS  	 =
	==================================================

	memory layout,

	all shorts are motorola format, and have to be swapped to intel format

	64 KByte shared memory

	+---------------+
	| 0x0008	|	8 KByte rcv buffer
	~		~
	|---------------|
	| 0x2000	|	8 KByte snd buffer
	~		~
	|---------------|
	| 0x3ff0	|	size of rcv buffer
	| 0x3ff2	|	write index rcv buffer
	| 0x3ff4	|	read index rcv buffer
	|---------------|
	| 0x3ff6	|	size of snd buffer
	| 0x3ff8	|	write index snd buffer
	| 0x3ffa	|	read index snd buffer
	|---------------|
	| 0x3ffc	|	state register
	| 0x3ffd	|	debug register
	~		~
	~		~
	| 0xfffe	|	control register
	+---------------+



	state register
	+-------+--------------------+----------------------------------+
	|  BIT  |	READ         |		WRITE			|
	+-------+--------------------+----------------------------------+
	|   0   |                    |  1 -> gen interrupt if 68040 CPU |
	|	|                    | 	     has filled rcv buffer with |
	|	|                    | 	     data                       |
	+-------+--------------------+----------------------------------+
	|   1   |                    |  1 -> gen interrupt if 68040 CPU |
	|	|                    | 	     has read data from snd     |
	|	|                    | 	     buffer                     |
	+-------+--------------------+----------------------------------+
	|   7   | 1 -> hardware      |					|
	|       |      failure	     |					|
	+-------+--------------------+----------------------------------+

	control register
	+-------+--------------------+----------------------------------+
	|  BIT  |	READ         |		WRITE			|
	+-------+--------------------+----------------------------------+
	|   0   |                    |  				|
	|	|                    |  1 -> run CPU			|
	+-------+--------------------+----------------------------------+
	|   1   |  		     |					|
	|	|                    |	1 -> run CPU			|
	+-------+--------------------+----------------------------------+

*************************************************************************/

#ifndef __BRI_H
#define __BRI_H
/* static char _sccsid_bri_h[] = "@(#)bri.h	1.20"; */

/*----------------------------------------------------------------------*/
/*	MACRO-DEFINES							*/
/*----------------------------------------------------------------------*/
#define CTRL_ENABLE(bp)		if ((bp)->type != BOARD_ID_PMX) 	\
				    (*(bp)->ctrl = (bp)->cflag = 0xf)
#define CTRL_DISABLE(bp)	if ((bp)->type != BOARD_ID_PMX) 	\
				    (*(bp)->ctrl = (bp)->cflag = 0xe)
#define	CTRL_SET(bp,val)	(*(bp)->ctrl = (bp)->cflag = (val))
#define	STATE_SET(bp,val)	(*(bp)->state = (val))
#define CTRL_RESET(bp)		reset_card(bp)



/* #define BOARD_TYPE(bp)		(*(bp)->ctrl >> 5) */
#define BOARD_TYPE(bp)		((bp)->type)

#define BOARD_ID_BRI4		0	/* Quattro 4 S0 */
#define BOARD_ID_X21		1 	/* X21 adapter */
#define BOARD_ID_BRI		2       /* BRI */
#define BOARD_ID_PMX		7       /* S2M */

#define BOARD_BOOTLOADER	"BOOT.68K"
#define BOARD_SYS_BRI4		"BRI_Q.68K"
#define BOARD_SYS_BRI		"BRI.68K"
#define BOARD_SYS_X21		"X21.68K"
#define BOARD_SYS_PMX		"PMX.68K"


/*----------------------------------------------------------------------*/
/*	GLOBAL DEFINES  						*/
/*----------------------------------------------------------------------*/
#define IO_8259A_1         	0x20	/* IO addr of first 8259A 	*/
#define IO_8259A_2         	0xa0	/* IO addr of second 8259A  	*/
#define OCW2_8259_EOI      	0x20	/* end of interrupt command	*/

#define APIINIT_IDENT		"Here is what I'am searching for"
#define API_BOSSCOM_ID		1


/*----------------------------------------------------------------------*/
/*	DATA STRUCTURES 						*/
/*----------------------------------------------------------------------*/
typedef struct {
    unsigned short sz;		/*  size of buffer	*/
    unsigned short wi;		/*  write index		*/
    unsigned short ri;		/*  read index		*/
} icinfo_t;

typedef struct {
    volatile unsigned char far *p; /*  pointer to buffer  */
    volatile icinfo_t far *d;      /*  buffer information */
} intercom_t;

#if 0

typedef struct {
    unsigned 		addr;	/*  address of shared memory	   */
    unsigned char 	hwInt;	/*  intr 			   */

    volatile unsigned char far *base;	/*  base address of shared memory  */
    volatile unsigned char far *state;	/*  state flag			   */
    volatile unsigned char far *debug;	/*  debug output		   */
    volatile unsigned char far *ctrl;	/*  address of control register	   */

    intercom_t rcv;			/*  incoming intercom buffer	   */
    intercom_t snd;			/*  outgoing intercom buffer	   */
    int cflag;				/*  control flag		   */

    unsigned long ctrlmask;		/*  controller bitmask		   */
    unsigned char 	type;   	/*  board type			   */
} board_t;

typedef struct {
    char 	section[512];
    char 	drvpath[512];
    char        ctrl;
    char 	type;
} bd_t;

typedef struct {
    unsigned short	board;		/*  board			    */
    unsigned short	ctrl;		/*  controller number		    */

    char  		profile[32];	/*  profile to load 		    */
    char        	bindaddr[32];	/*  login ident 		    */
    unsigned short	tei;		/*  static TEI, or -1 if dynamic    */
    unsigned short	disc_d;         /*  time to hold d-channel layer 2  */
    unsigned short	permlink;	/*  permanent hold d-channel lay 2  */
    unsigned long 	flags;
    char		spid1[20];	/*  service point identifier (NI-1) */
    char		spid2[20];	/*  service point identifier (NI-1) */
    char		telno1[32];	/*  own telno			    */
    char		telno2[32];	/*  own telno			    */
} bdcfg_t;
#endif

#endif
