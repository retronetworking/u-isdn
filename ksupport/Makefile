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

##  =()<SYS	= @<SYS>@>()=
SYS	= linux

SOURCES	= count.c dump.c log.c logh.c proto.c qinfo.c smallq.c streamlib.c x75lib.c
OBJ	= count.o dump.o log.o logh.o proto.o qinfo.o smallq.o streamlib.o x75lib.o
ALL =

all:		$(OBJ) $(ALL) 

lib:

install: all
	@sh ../iftrue.sh "-d $(SYS)" "cd $(SYS); make"

update: all
	@sh ../iftrue.sh "-d $(SYS)" "cd $(SYS); make update"

indent:
	../indent.sh $(SOURCES)

clean:
	rm -f $(ALL) $(OBJ)
	@sh ../iftrue.sh "-d $(SYS)" "cd $(SYS); make clean"

depend:		Makefile $(SOURCES)
	$(CC) -M $(DEFS) $(SOURCES) > .depend

load:	
	insmod streamlib.o
	insmod smallq.o
	insmod dump.o
	insmod log.o
	insmod logh.o
	insmod count.o
	insmod proto.o
	insmod qinfo.o
	insmod x75lib.o

ifeq (.depend,$(wildcard .depend))
include .depend
endif

