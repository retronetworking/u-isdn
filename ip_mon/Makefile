SHELL	= /bin/sh
MAKE	= make
##  =()<P	= @<P>@>()=
P	= 

##  =()<CCU	= @<CCU>@>()=
CCU	= gcc 
##  =()<CC	= @<CC>@>()=
CC	= gcc
##  =()<DEFS    = @<DEFKERNEL>@ @<KERNEL>@ -I../include >()=
DEFS    = -I../streams -I../compat -I/usr/src/linux/include -fomit-frame-pointer -DKERNEL -D__KERNEL__ -DMODULE -DCONFIG_MODVERSIONS -I../include 
##  =()<CFLAGS	= @<CFLAGS>@ $(DEFS)>()=
CFLAGS	= -O2 -Wall  $(DEFS)
##  =()<DEFSN    = @<DEFS>@ -I../include >()=
DEFSN    = -I../streams -I../compat -I/usr/src/linux/include -I/usr/include/bsd -I../include 
##  =()<CFLAGSN	= @<CFLAGS>@ $(DEFSN)>()=
CFLAGSN	= -O2 -Wall  $(DEFSN)
##  =()<LIBS	= @<LIBS>@>()=
LIBS	= -lbsd
##  =()<LDFLAGS	= @<LDFLAGS>@>()=
LDFLAGS	= -v

##  =()<SYS	= @<SYS>@>()=
SYS	= linux

SOURCES	= ip_mon.c monitor.c

OBJ	= ip_mon.o monitor.o
ALL = monitor

all:	ip_mon.o
prog:	monitor

monitor:	monitor.o
	$(CCU) $(LDFLAGS) -o monitor monitor.o $(LIBS)
monitor.o:	monitor.c
	$(CCU) $(CFLAGSN) -c monitor.c

lib:

install: all
	@sh ../iftrue.sh "-d $(SYS)" "cd $(SYS); make"

update: all
	@sh ../iftrue.sh "-d $(SYS)" "cd $(SYS); make update"

clean:
	rm -f $(OBJ) $(ALL) monitor.old
	@sh ../iftrue.sh "-d $(SYS)" "cd $(SYS); make clean"

indent:
	../indent.sh $(SOURCES)

depend:		Makefile $(SOURCES)
	$(CC) -M $(DEFS) $(SOURCES) > .depend

load:	
	insmod ip_mon.o

ifeq (.depend,$(wildcard .depend))
include .depend
endif

