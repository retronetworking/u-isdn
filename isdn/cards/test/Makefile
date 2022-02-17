
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

##  =()<SYS	= @<SYS>@>()=
SYS	= linux

SOURCES	= test.c

OBJ	= 
ALL = test.o

all:	test.o

load:	all
	insmod test.o

test.o:	test.c
	$(CC) $(CFLAGS) -o test.o -c test.c

lib:

indent:
	../../indent.sh $(SOURCES)

depend:		Makefile $(SOURCES)
	$(CC) -M $(DEFS) test.c > .depend ; done

clean:
	rm -f $(OBJ) $(ALL)

install: all
	@sh ../../iftrue.sh "-d $(SYS)" "cd $(SYS); make"

update: all
	@sh ../../iftrue.sh "-d $(SYS)" "cd $(SYS); make update"


ifeq (.depend,$(wildcard .depend))
include .depend
endif

