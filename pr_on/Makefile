SHELL	= /bin/sh
MAKE	= make
##  =()<P	= @<P>@>()=
P	= 

##  =()<CC	= @<CC>@>()=
CC	= gcc
##  =()<DEFS    = @<DEFKERNEL>@ @<KERNEL>@ -I../include >()=
DEFS    = -I../streams -I../compat -I/usr/src/linux/include -fomit-frame-pointer -DKERNEL -D__KERNEL__ -DMODULE -DCONFIG_MODVERSIONS -I../include 
##  =()<CFLAGS	= @<CFLAGS>@ @<DEFKERNEL>@ @<KERNEL>@ -I../include >()=
CFLAGS	= -O2 -Wall  -I../streams -I../compat -I/usr/src/linux/include -fomit-frame-pointer -DKERNEL -D__KERNEL__ -DMODULE -DCONFIG_MODVERSIONS -I../include 

##  =()<SYS	= @<SYS>@>()=
SYS	= linux

SOURCES	= pr_on.c

OBJ	= pr_on.o
ALL =

all:	$(OBJ)

lib:

install: all
	@sh ../iftrue.sh "-d $(SYS)" "cd $(SYS); make"

update: all
	@sh ../iftrue.sh "-d $(SYS)" "cd $(SYS); make update"

indent:
	../indent.sh $(SOURCES)

depend:		Makefile $(SOURCES)
	$(CC) -M $(DEFS) $(SOURCES) > .depend

clean:
	rm -f $(OBJ) $(ALL)
	@sh ../iftrue.sh "-d $(SYS)" "cd $(SYS); make clean"

load:	
	insmod pr_on.o

ifeq (.depend,$(wildcard .depend))
include .depend
endif

