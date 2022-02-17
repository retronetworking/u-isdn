## Makefile for cStreams support

SHELL	= /bin/sh
MAKE	= make
##  =()<P	= @<P>@>()=
P	= 

##  =()<CC	= @<CC>@>()=
CC	= gcc
##  =()<DEFS	= @<DEFKERNEL>@ @<KERNEL>@ -I../include >()=
DEFS	= -I../streams -I../compat -I/usr/src/linux-orig/include -fomit-frame-pointer -DKERNEL -D__KERNEL__ -DMODULE -DCONFIG_MODVERSIONS -I../include 
##  =()<CFLAGS	= @<CFLAGS>@ $(DEFS)>()=
CFLAGS	= -O2 -Wall  $(DEFS)
##  =()<LD	= @<LD>@>()=
LD	= ld

##  =()<SYS	= @<SYS>@>()=
SYS	= linux

SOURCES	= streams_sys.c streams_io.c
OBJ	= streams_sys.o streams_io.o
ALL =	streams.o

all:	${ALL}

lib:

install: all
	# @sh ../iftrue.sh "-d $(SYS)" "cd $(SYS); make"

update: all
	# @sh ../iftrue.sh "-d $(SYS)" "cd $(SYS); make update"

indent:
	../indent.sh $(SOURCES)

clean:
	rm -f $(ALL) $(OBJ)
	# @sh ../iftrue.sh "-d $(SYS)" "cd $(SYS); make clean"

depend:		Makefile $(SOURCES)
	rm -f sys linux
	ln -s . linux
	ln -s . sys
	$(CC) -M $(DEFS) $(SOURCES) > .depend

load: all
	insmod streams.o

streams.o:	$(OBJ)
	$(LD) -r -o streams.o $(OBJ)

ifeq (.depend,$(wildcard .depend))
include .depend
endif
