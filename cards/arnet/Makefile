SHELL	= /bin/sh
MAKE	= make
##  =()<P	= @<P>@>()=
P	= 

##  =()<CC	= @<CC>@>()=
CC	= gcc
##  =()<DEFS    = @<DEFCARDS>@ @<KERNEL>@ -I../../include >()=
DEFS    = -I../../streams -I../../compat -fomit-frame-pointer -funroll-loops -DKERNEL -D__KERNEL__ -DMODULE -DCONFIG_MODVERSIONS -I../../include 
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

load:
	insmod arnet.o

ifeq (.depend,$(wildcard .depend))
include .depend
endif

