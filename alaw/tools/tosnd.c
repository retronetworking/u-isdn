#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#ifdef linux
#include <compat.h>
#endif

short mult[] = {2,2,4,8,16,32,64,128};
short base[] = {1,33,66,132,264,528,1056,2112,4032};
uchar_t val[256];
static int clientsock(char *host, int port);

int main(int argc, char *argv[])
{

	short shift;
	short i,j,k,v;
	short fi = 0;
	
	if(argc < 2) {
		fprintf(stderr,"Usage: %s <shift> < a-law-file > MacSample\n",*argv);
		exit(2);
	}
	shift = atoi(*++argv);
	if(argc > 2) {
		char *host = *++argv;
		ushort num = 65430;
		if(argc > 3)
			num = atoi(*++argv);
		fi = clientsock(host,num); if(fi < 0) { perror("Open"); exit(1); }
	}

	for (i=0;i<=7;i++) {
		for(j=0;j<=15;j++) {
			k=(i<<4)+j;
			if(shift >= 0)
				v=((base[i]+(mult[i]*j)) >>shift);
			else
				v=k;
			if(v>127) v=127;
			val[k] = 128+v;
			val[255-k]=127-v;
		}
	}

	{
		uchar_t arr[10240];
		int len1;
		while((len1=read(fileno(stdin),arr,sizeof arr))>0) {
			uchar_t *arp = arr+len1;
			int len2 = 0;
			while(--arp >= arr) 
				*arp = val[*arp];
			arp++;
			while(len1>0 && (len2=write(fi?fi:fileno(stdout),arp,len1))>0) {
				arp += len2;
				len1 -= len2;
			}
			if(len2<0) {
				perror("Write");
				exit(1);
			} else if(len2 == 0)
				exit(0);

		}
		if(len1<0) {
			perror("Read");
			exit(1);
		}
	}
	exit(0);
}


#ifndef FD_SET		/* for 4.2BSD */
#define FD_SETSIZE      (sizeof(fd_set) * 8)
#define FD_SET(n, p)    (((fd_set *) (p))->fds_bits[0] |= (1 << ((n) % 32)))
#define FD_CLR(n, p)    (((fd_set *) (p))->fds_bits[0] &= ~(1 << ((n) % 32)))
#define FD_ISSET(n, p)  (((fd_set *) (p))->fds_bits[0] & (1 << ((n) % 32)))
#define FD_ZERO(p)      bzero((char *)(p), sizeof(*(p)))
#endif

static int clientsock(char *host, int port)
{
	int	sock;
	struct	sockaddr_in server;
	struct	hostent *hp, *gethostbyname();

	bzero(&server, sizeof(server));
	server.sin_family = AF_INET;
	server.sin_port = htons(port);

	if (isdigit(host[0]))
		server.sin_addr.s_addr = inet_addr(host);
	else
	{
		hp = gethostbyname(host);
		if (hp == NULL)
			return -9999;
		bcopy(hp->h_addr, &server.sin_addr, hp->h_length);
	}

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0)
		return -errno;

	if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0)
	{
		close(sock);
		return -errno;
	}

	return sock;
}

