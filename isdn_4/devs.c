/*
 * This file is part of the ISDN master program.
 *
 * Copyright (C) 1995 Matthias Urlichs.
 * See the file COPYING for license details.
 */

#include "master.h"



/* Device names. 's'=short, 'm'=medium, ''=fullname; 'i'=non-terminal version */

char *
sdevname (short minor)
{
	static char dev[20];

	sprintf (dev, "i%02d", minor & 0xFF);
	return dev;
}

char *
mdevname (short minor)
{
	static char dev[20];

	sprintf (dev, "tty%s", sdevname (minor));
	return dev;
}

char *
devname (short minor)
{
	static char dev[20];

	sprintf (dev, "/dev/%s", mdevname (minor));
	return dev;
}

char *
isdevname (short minor)
{
	static char dev[20];

	sprintf (dev, "isdn%d", minor & 0xFF);
	return dev;
}

char *
idevname (short minor)
{
	static char dev[20];

	sprintf (dev, "/dev/isdn/%s", isdevname (minor));
	return dev;
}


/* Check a lock file. */
void
checkdev(int dev)
{
	char permtt1[sizeof(LOCKNAME)+15];
	char permtt2[sizeof(LOCKNAME)+15];
	int f, len, pid;
	char sbuf[10];

	sprintf(permtt1,LOCKNAME,mdevname(dev));
	sprintf(permtt2,LOCKNAME,isdevname(dev));

	if((f = open(permtt1,O_RDWR)) < 0)  {
		if(0)syslog(LOG_WARNING,"Checking %s: unopenable, deleted, %m",permtt1);
		unlink(permtt1);
	} else {
		len=read(f,sbuf,sizeof(sbuf)-1);
		if(len<=0) {
			if(0)syslog(LOG_WARNING,"Checking %s: unreadable, deleted, %m",permtt1);
			unlink(permtt1);
		} else {
			if(sbuf[len-1]=='\n')
				sbuf[len-1]='\0';
			else
				sbuf[len]='\0';
			pid = atoi(sbuf);
			if(pid <= 0 || (kill(pid,0) == -1 && errno == -ESRCH)) {
				if(0)syslog(LOG_WARNING,"Checking %s: unkillable, pid %d, deleted, %m",permtt1, pid);
				unlink(permtt1);
			}
		}
		close(f);
	}
	if((f = open(permtt2,O_RDWR)) < 0) {
		if(0)syslog(LOG_WARNING,"Checking %s: unopenable, deleted, %m",permtt2);
		unlink(permtt2);
	} else {
		len=read(f,sbuf,sizeof(sbuf)-1);
		if(len<=0) {
			if(0)syslog(LOG_WARNING,"Checking %s: unreadable, deleted, %m",permtt2);
			unlink(permtt2);
		} else {
			if(sbuf[len-1]=='\n')
				sbuf[len-1]='\0';
			else
				sbuf[len]='\0';
			pid = atoi(sbuf);
			if(pid <= 0 || (kill(pid,0) == -1 && errno == -ESRCH)) {
				if(0)syslog(LOG_WARNING,"Checking %s: unkillable, pid %d, deleted, %m",permtt2, pid);
				unlink(permtt2);
			}
		}
		close(f);
	}
}


/* Lock a device. One failure, for externally-opened devices (cu), may be tolerated. */
int
lockdev(int dev, char onefailok)
{
	char permtt1[sizeof(LOCKNAME)+15];
	char permtt2[sizeof(LOCKNAME)+15];
	char vartt[sizeof(LOCKNAME)+15];
	char pidnum[7];
	int f, err, len;

	len = sprintf(pidnum,"%d\n",getpid());
	sprintf(vartt,LOCKNAME,pidnum);
	sprintf(permtt1,LOCKNAME,mdevname(dev));
	sprintf(permtt2,LOCKNAME,isdevname(dev));

	unlink(vartt);
	f=open(vartt,O_WRONLY|O_CREAT,0644);
	if(f < 0) {
		syslog(LOG_WARNING,"Locktemp %s: unopenable, %m",vartt);
		return -1;
	}
	if((err = write(f,pidnum,len)) != len) {
		syslog(LOG_WARNING,"Locktemp %s: unwriteable, %m",vartt);
		close(f);
		return -1;
	}
	close(f);
	chmod(vartt,S_IRUSR|S_IRGRP|S_IROTH);

	if((err = link(vartt,permtt1)) < 0) {
		if(onefailok == 0) {
			if(0) syslog(LOG_WARNING,"Lock %s: unlinkable, %m",permtt1);
			checkdev(dev);
			unlink(vartt);
			return -1;
		}
		--onefailok;
		if(0) syslog(LOG_INFO,"Lock %s: unlinkable(ignored), %m",permtt1);
	}
	if((err = link(vartt,permtt2)) < 0) {
		if (onefailok == 0) {
			if(0) syslog(LOG_WARNING,"Lock %s: unlinkable, %m",permtt2);
			checkdev(dev);
			unlink(vartt);
			unlink(permtt1);
			return -1;
		}
		--onefailok;
		if(0) syslog(LOG_INFO,"Lock %s: unlinkable(ignored), %m",permtt2);
	}
	unlink(vartt);
	if(0) syslog(LOG_DEBUG,"Locked %s and %s",permtt1,permtt2);
	return 0;
}


/* ... and unlock it. */
void
unlockdev(int dev)
{
	char permtt1[sizeof(LOCKNAME)+15];
	char permtt2[sizeof(LOCKNAME)+15];

	sprintf(permtt1,LOCKNAME,mdevname(dev));
	sprintf(permtt2,LOCKNAME,isdevname(dev));

	unlink(permtt1);
	unlink(permtt2);
	if(0) syslog(LOG_DEBUG,"Unlocked %s and %s",permtt1,permtt2);
}

