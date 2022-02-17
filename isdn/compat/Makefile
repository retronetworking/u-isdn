## Makefile for compatibility stuff

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

##  =()<SYS	= @<SYS>@>()=
SYS	= linux

SOURCES	= compat.c
OBJ	= compat.o
ALL =	

all:	compat.o

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
	rm -f syscompat.h linux
	ln -s compat.h syscompat.h
	ln -s . linux
	$(CC) -M $(DEFS) $(SOURCES) > .depend

load: all
	insmod compat.o

ifeq (.depend,$(wildcard .depend))
include .depend
endif

