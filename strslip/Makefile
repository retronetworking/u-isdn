SHELL	= /bin/sh
MAKE	= make
##  =()<P	= @<P>@>()=
P	= 

##  =()<CC	= @<CC>@>()=
CC	= gcc
##  =()<DEFS	= @<DEFKERNEL>@ @<KERNEL>@ -I../include >()=
DEFS	= -I../streams -I../compat -I/usr/src/linux/include -fomit-frame-pointer -DKERNEL -D__KERNEL__ -DMODULE -DCONFIG_MODVERSIONS -I../include 
##  =()<CFLAGS	= @<CFLAGS>@ $(DEFS)>()=
CFLAGS	= -O2 -Wall  $(DEFS)
##  =()<LDFLAGS	= @<LDFLAGS>@>()=
LDFLAGS	= -v

##  =()<LIBS	= @<LIBS>@>()=
LIBS	= -lbsd

##  =()<SYS	= @<SYS>@>()=
SYS	= linux

SOURCES	= strslip.c

OBJ	= strslip.o
ALL =

all:		$(OBJ)

lib:

install: all
	@cd $(SYS) ; make  install

clean:
	rm -f $(ALL) $(OBJ)

indent:
	../indent.sh $(SOURCES)
	@sh ../iftrue.sh "-d $(SYS)" "cd $(SYS); make clean"

depend:		Makefile $(SOURCES)
	$(CC) -M $(DEFS) $(SOURCES) > .depend

load:	
	insmod strslip.o

ifeq (.depend,$(wildcard .depend))
include .depend
endif

