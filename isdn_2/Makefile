SHELL	= /bin/sh
MAKE	= make
##  =()<P	= @<P>@>()=
P	= 

##  =()<CC	= @<CC>@>()=
CC	= gcc
##  =()<DEFS    = @<DEFKERNEL>@ @<KERNEL>@ -I../include >()=
DEFS    = -I../streams -I../compat -I/usr/src/linux/include -fomit-frame-pointer -DKERNEL -D__KERNEL__ -DMODULE -DCONFIG_MODVERSIONS -I../include 
##  =()<CFLAGS	= @<CFLAGS>@ $(DEFS)>()=
CFLAGS	= -O2 -Wall  $(DEFS)
##  =()<RANLIB	= @<RANLIB>@>()=
RANLIB	= ranlib

##  =()<SYS	= @<SYS>@>()=
SYS	= linux

SOURCES	= isdn_2.c

OBJ	= isdn_2.o
ALL =	../isdn_2.a

all:		$(OBJ) $(ALL)

install: all
	@sh ../iftrue.sh "-d $(SYS)" "cd $(SYS); make"

../isdn_2.a:	$(OBJ)
	ar r ../isdn_2.a $(OBJ)
	@$(RANLIB) ../isdn_2.a

clean:
	rm -f $(OBJ) $(ALL)
	@sh ../iftrue.sh "-d $(SYS)" "cd $(SYS); make clean"

indent:
	../indent.sh $(SOURCES)

depend:		Makefile $(SOURCES)
	$(CC) -M $(DEFS) $(SOURCES) > .depend

load:	
	insmod isdn_2.o

ifeq (.depend,$(wildcard .depend))
include .depend
endif

