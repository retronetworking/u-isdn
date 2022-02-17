#ifndef _LEO_H
#define _LEO_H

#define LEO_ACTIVEOPEN 0x80

#define LEO_MAXMSG 4000
#define LEO_MAXINCORE 16000
#define LEO_NRLEN 32
#define LEO_MINTIME 20
typedef char leo_nr[LEO_NRLEN];

#define CONN_NONE 0
#define CONN_CALLING 1
#define CONN_LISTEN 2
#define CONN_OPEN 3
#define CONN_CLOSING 4

struct leo_stats {
	int pkt_in;
	int pkt_out;
	int byte_in;
	int byte_out;
	int conn_cost;
	char state;
	char lastmsg;
	leo_nr telnr;
	char version[32];
};

#define LEO_ASYNC   _IO ('*',0)
#define LEO_SYNC    _IO ('*',1)
#define LEO_STATE   _IOR('*',2,struct leo_stats)
#define LEO_DIAL    _IOW('*',3,leo_nr)
#define LEO_WAIT    _IO ('*',4)
#define LEO_DISC    _IO ('*',5)
#define LEO_EAZ     _IOW('*',6,char)
#define LEO_WAITING _IO ('*',7)

/* DA-Codes sind mit Vorsicht zu geniessen */
#define LEO_DA_ON   _IO ('*',8)
#define LEO_DA_OFF  _IO ('*',9)

/* superuser use only */
#define LEO_RESET   _IO ('*',10)

#define LEO_INFO _IO('*',11)
#define LEO_KICK _IO('*',12)
#define LEO_UNKICK _IO('*',13)

#define LEO_SETMEM  _IO ('*',15)
#define LEO_EXEC  _IO ('*',16)

#endif
