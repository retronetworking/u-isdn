/*
 * This file is part of the ISDN master program.
 *
 * Copyright (C) 1995 Matthias Urlichs.
 * See the file COPYING for license details.
 */

#include "master.h"



void
read_info (void)
{
	streamchar x[MAXLINE];
	int len;

	if ((len = read (fileno (stdin), x, sizeof (x) - 1)) <= 0)
		xquit ("Read", "HL");
	x[len] = '\0';
	do_info (x, len);
}

void
read_data ()
{
	struct _isdn23_hdr hdr;
	int err;
	uchar_t dbuf[MAXLINE];
	int len = 0;
	int iovlen = 1;
	struct iovec io[2];

#define xREAD(what) 														\
	do { 																\
		int xlen = hdr.hdr_##what.len; 									\
		uchar_t *dbufp = dbuf; 											\
		if (xlen == 0) 													\
			break; 														\
		if (xlen >= sizeof(dbuf)) { 									\
			syslog(LOG_ERR,"Header %d: Len %d",hdr.key,hdr.hdr_##what.len);	\
			dumpaschex((u_char *)&hdr,sizeof(hdr));								\
			xquit(NULL,NULL); 											\
		} 																\
		while((err = read(fd_mon,dbufp,xlen)) > 0) { 					\
			len += err; 												\
			dbufp += err;												\
			xlen -= err; 												\
			if (xlen <= 0) 												\
				break; 													\
		} 																\
		if (err < 0) 													\
			xquit("Err Header Read",NULL); 								\
		else if (err == 0) 												\
			xquit("EOF Header Read",NULL); 								\
	} while(0);

	if ((err = read (fd_mon, &hdr, sizeof (hdr))) != sizeof (hdr)) {
		if (err < 0)
			xquit ("Read Header", NULL);
		syslog (LOG_ERR, "Header: len is %d, got %d", sizeof (hdr), err);
		xquit (NULL, NULL);
	}
	switch (hdr.key) {
	default:
		syslog (LOG_ERR, "Unknown header ID %d", hdr.key);
		xquit (NULL, NULL);
	case HDR_ATCMD:
		xREAD (atcmd);
		break;
	case HDR_PROTOCMD:
		xREAD (protocmd);
		break;
	case HDR_XDATA:
		xREAD (xdata);
		break;
	case HDR_DATA:
		xREAD (data);
		break;
	case HDR_UIDATA:
		xREAD (uidata);
		break;
	case HDR_RAWDATA:
		xREAD (rawdata);
		break;
	case HDR_OPEN:
		break;
	case HDR_CLOSE:
		break;
	case HDR_ATTACH:
		break;
	case HDR_DETACH:
		break;
	case HDR_CARD:
		break;
	case HDR_NOCARD:
		break;
	case HDR_OPENPROT:
		break;
	case HDR_CLOSEPROT:
		break;
	case HDR_NOTIFY:
		break;
	case HDR_TEI:
		break;
	case HDR_INVAL:
		if ((err = read (fd_mon, dbuf, sizeof (hdr))) != sizeof (hdr)) {
			if (err < 0)
				xquit ("Read InvHeader", NULL);
			syslog (LOG_ERR, "InvHeader: len is %d, got %d", sizeof (hdr), err);
			xquit (NULL, NULL);
		}
		len = sizeof (hdr);
		break;
	}

	/* dump_hdr(&hdr,"Read 2->3",dbuf); */

	io[0].iov_base = (caddr_t) & hdr;
	io[0].iov_len = sizeof (struct _isdn23_hdr);

	if (len > 0) {
		io[1].iov_base = (caddr_t) dbuf;
		io[1].iov_len = len;
		iovlen = 2;
	}
	strwritev (xs_mon, io, iovlen, 0);
}

static int backtimeout;
static void backtime(void *nix) {
	backtimeout = 1;
}

int
backrun (int fd, int timeo)
{
	int err;
	fd_set rdx;
	time_t tx = time(NULL);

	if(!testonly && !FD_ISSET(fd_mon,&rd)) 
		xquit("RD fd_set cleared",NULL);
	backtimeout = 0;
	if(timeo > 0)
		timeout(backtime,NULL,timeo);
	do {
		struct timeval now = {0, 0};
		rdx = rd;

		runqueues (); runqueues ();
		if(fd >= 0) FD_SET(fd,&rdx);
		err = select (FD_SETSIZE, &rdx, NULL, NULL, &now);
		if (err == 0 || (err < 0 && errno == EINTR)) {
			rdx = rd;
			if(fd >= 0) FD_SET(fd,&rdx);

			if(fd == -1) goto endit;
			if(fd < 0) callout_adjtimeout ();
			err = select (FD_SETSIZE, &rdx, NULL, NULL, (fd < 0 && callout_hastimer ())? &callout_time : NULL);
		}
		runqueues(); runqueues();
		if (err <= 0) {
			if (err < 0 && errno != EINTR)
				xquit ("Select", NULL);
			callout_alarm ();
			if(backtimeout)
				break;
			else
				continue;
		}
		if (!testonly && FD_ISSET (fd_mon, &rdx)) 
			read_data ();
		if (FD_ISSET (fileno (stdin), &rdx)) 
			read_info ();
		runqueues(); runqueues();
	} while((fd >= 0) ? !FD_ISSET(fd,&rdx) : has_progs());
  endit:
	if(timeo && !backtimeout)
		untimeout(backtime,NULL);
	runqueues(); runqueues();
	return backtimeout;
}


void
syspoll (void)
{
	FD_ZERO (&rd);
	if (!testonly)
		FD_SET (fd_mon, &rd);
	if (!igstdin)
		FD_SET (fileno (stdin), &rd);
	while (has_progs())
		backrun(-2,0);
}


void
do_h (queue_t * q)
{
	uchar_t data[MAXLINE];
	int len = sizeof (data) - 1;
	int err;

	while ((err = strread (xs_mon, (streamchar *) data, &len, 1)) == 0 && len > 0) {
		do_info (data, len);
		len = sizeof (data) - 1;
	}
	if (err != 0) {
		errno = err;
		syslog (LOG_ERR, "Read H: %m");
	} else
		q->q_flag |= QWANTR;
}

void
do_l (queue_t * q)
{
	struct _isdn23_hdr h;
	int err;
	uchar_t data[MAXLINE];

	while (1) {
		struct iovec io[3];
		int iovlen = 1;
		int len = sizeof (struct _isdn23_hdr);

		if ((err = strread (xs_mon, (streamchar *) &h, &len, 0)) == 0 && len == sizeof (struct _isdn23_hdr)) ;

		else
			break;

		io[0].iov_base = (caddr_t) & h;
		io[0].iov_len = sizeof (struct _isdn23_hdr);

		switch (h.key) {
		default:
			break;
		case HDR_ATCMD:
		case HDR_XDATA:
		case HDR_RAWDATA:
		case HDR_DATA:
		case HDR_UIDATA:
		case HDR_PROTOCMD:
			io[iovlen].iov_base = (caddr_t) data;
			io[iovlen].iov_len = len = h.hdr_atcmd.len;
			if ((err = strread (xs_mon, (streamchar *) data, &len, 0)) != 0 || len != h.hdr_atcmd.len) {
				syslog (LOG_ERR, "do_l: Fault, %d, %m", len);
				return;
			}
			io[iovlen].iov_base = (caddr_t) data;
			io[iovlen].iov_len = len;
			iovlen++;
			break;
		case HDR_INVAL:
			io[iovlen].iov_base = (caddr_t) data;
			io[iovlen].iov_len = len = sizeof (struct _isdn23_hdr);
			if ((err = strread (xs_mon, (streamchar *) data, &len, 0)) != 0 || len != sizeof (struct _isdn23_hdr)) {
				syslog (LOG_ERR, "do_l: Fault, %d, %m", len);
				return;
			}
			iovlen++;
			break;
		}

		/*
		 * dump_hdr(&h,testonly ? "Write <-3" : "Write 2<-3",(uchar_t
		 * *)io[1].iov_base);
		 */
		if (testonly)
			err = 0;
		else {
			if (((char *)(io[0].iov_base))[0] == 0x3F)
				abort ();
			err = writev (fd_mon, io, iovlen);
		}
	}
	if (err != 0) {
		errno = err;
		syslog (LOG_ERR, "Read L: %m");
	} else
		q->q_flag |= QWANTR;
}
