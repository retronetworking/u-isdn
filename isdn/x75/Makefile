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

##  =()<SYS	= @<SYS>@>()=
SYS	= linux

SOURCES	= x75.c

OBJ	= x75.o
ALL =

all:	$(OBJ)	$(ALL)

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
	rm -f test_x75 test_x75.o
	@sh ../iftrue.sh "-d $(SYS)" "cd $(SYS); make clean"

test: test_x75

test_x75: test_x75.o x75.o ../support.a
		$(CC) $(CFLAGS) -o $@ $<

load:	all
	insmod x75.o

ifeq (.depend,$(wildcard .depend))
include .depend
endif

