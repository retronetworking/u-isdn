SHELL	= /bin/sh
MAKE	= make
##  =()<P	= @<P>@>()=
P	= 

##  =()<CC	= @<CC>@>()=
CC	= gcc
##  =()<DEFS    = @<DEFKERNEL>@ @<KERNEL>@ -I../include >()=
DEFS    = -I../streams -I../compat -I/usr/src/linux-orig/include -fomit-frame-pointer -DKERNEL -D__KERNEL__ -DMODULE -DCONFIG_MODVERSIONS -I../include 
##  =()<CFLAGS	= @<CFLAGS>@ $(DEFS)>()=
CFLAGS	= -O2 -Wall  $(DEFS)
##  =()<RANLIB	= @<RANLIB>@>()=
RANLIB	= ranlib

##  =()<SYS	= @<SYS>@>()=
SYS	= linux

SOURCES	= arnet.c

OBJ	= arnet.o

all:		$(OBJ)

install: all
	@sh ../iftrue.sh "-d $(SYS)" "cd $(SYS); make"

clean:
	rm -f $(OBJ) $(ALL)
	@sh ../iftrue.sh "-d $(SYS)" "cd $(SYS); make clean"

indent:
	../indent.sh $(SOURCES)

depend:		Makefile $(SOURCES)
	$(CC) -M $(DEFS) $(SOURCES) > .depend

load:	all
	insmod arnet.o

ifeq (.depend,$(wildcard .depend))
include .depend
endif

