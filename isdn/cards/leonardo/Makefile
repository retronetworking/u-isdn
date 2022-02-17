
SHELL	= /bin/sh
MAKE	= make
##  =()<P	= @<P>@>()=
P	= 

##  =()<CC	= @<CC>@>()=
CC	= gcc
##  =()<DEFS    = @<DEFCARDS>@ @<KERNEL>@ >()=
DEFS    = -I../../streams -I../../compat -fomit-frame-pointer -funroll-loops -DKERNEL -D__KERNEL__ -DMODULE -DCONFIG_MODVERSIONS 
##  =()<CFLAGS	= @<CFLAGS>@ $(DEFS)>()=
CFLAGS	= -O2 -Wall  $(DEFS)

##  =()<SYS	= @<SYS>@>()=
SYS	= linux

SOURCES	= leo.c

OBJ	= leo.o
ALL = leo

all:	$(OBJ)	$(ALL)

lib:

indent:
	../../indent.sh $(SOURCES)

depend:		Makefile $(SOURCES)
	$(CC) -M $(DEFS) $(SOURCES) > .depend

clean:
	rm -f $(OBJ) $(ALL)

leo:	leo.o # trace.o
		ld -o $@ -x -r $< # /usr/lib/unshared.ld

install update:	install.leo

install.leo:	/etc/init.d/leonardo /etc/boot.d/leonardo /etc/master.d/leonardo

/etc/init.d/leonardo:	leo.init
		super cp leo.init /etc/init.d/leonardo
/etc/master.d/leonardo:	leo.master
		super cp leo.master /etc/master.d/leonardo
/etc/boot.d/leonardo: leo
		super cp leo /etc/boot.d/leonardo

ifeq (.depend,$(wildcard .depend))
include .depend
endif

