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
##  =()<LD	= @<LD>@>()=
LD	= ld

##  =()<SYS	= @<SYS>@>()=
SYS	= linux

SOURCES	= van_j.c compress.c

OBJ	= van_j.o compress.o
ALL = vanj.o

all:	$(OBJ)	$(ALL)

lib:

vanj.o:	$(OBJ)
	$(LD) -r -o $@ $(OBJ)

install: all
	@sh ../iftrue.sh "-d $(SYS)" "cd $(SYS); make"

update: all
	@sh ../iftrue.sh "-d $(SYS)" "cd $(SYS); make update"

clean:
	rm -f $(OBJ) $(ALL)
	@sh ../iftrue.sh "-d $(SYS)" "cd $(SYS); make clean"

indent:
	../indent.sh $(SOURCES)

depend:		Makefile $(SOURCES)
	$(CC) -M $(DEFS) $(SOURCES) > .depend

load:	
	insmod vanj.o

ifeq (.depend,$(wildcard .depend))
include .depend
endif

