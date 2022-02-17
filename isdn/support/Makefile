SHELL	= /bin/sh
MAKE	= make
##  =()<P	= @<P>@>()=
P	= 

##  =()<CC	= @<CCU>@>()=
CC	= gcc-elf -g 
##  =()<DEFS    = @<DEFS>@ -I../include>()=
DEFS    = -I../streams -I../compat -I/usr/src/linux-orig/include -I/usr/include/bsd -I../include
##  =()<CFLAGS	= @<CFLAGS>@ $(DEFS)>()=
CFLAGS	= -O2 -Wall  $(DEFS)
##  =()<RANLIB	= @<RANLIB>@>()=
RANLIB	= ranlib

##  =()<SYS	= @<SYS>@>()=
SYS	= linux

HEADERS = count.h log.h qinfo.h

SOURCES	= x75lib.c q_data.c streamlib.c dump.c sioctl.c timeout.c streams.c \
	kernel.c smallq.c log.c logh.c wildmat.c count.c  \
	qinfo.c libc.c vectcmp.c 

OBJ	= x75lib.o q_data.o streamlib.o dump.o sioctl.o timeout.o streams.o \
	kernel.o smallq.o log.o logh.o wildmat.o count.o  \
	qinfo.o libc.o vectcmp.o 

ALL	= ../support.a

all prog:	$(OBJ) $(ALL)
lib: all

../support.a:	$(OBJ)
	ar r ../support.a $?
	@ $(RANLIB) ../support.a

indent:
	../indent.sh $(HEADERS) $(SOURCES)

clean:
	rm -f $(OBJ) $(ALL) ../support.a

install update: all

depend:		Makefile $(SOURCES)
	$(CC) -M $(DEFS) $(SOURCES) > .depend

load:	all

ifeq (.depend,$(wildcard .depend))
include .depend
endif

