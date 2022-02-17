SHELL	= /bin/sh
MAKE	= make
##  =()<P	= @<P>@>()=
P	= 

##  =()<CC	= @<CC>@>()=
CC	= gcc
##  =()<DEFS    = @<DEFKERNEL>@ @<KERNEL>@  -I../include >()=
DEFS    = -I../streams -I../compat -I/usr/src/linux-orig/include -fomit-frame-pointer -DKERNEL -D__KERNEL__ -DMODULE -DCONFIG_MODVERSIONS  -I../include 
##  =()<CFLAGS	= @<CFLAGS>@ $(DEFS)>()=
CFLAGS	= -O2 -Wall  $(DEFS)
##  =()<DEFSN    = @<DEFS>@ >()=
DEFSN    = -I../streams -I../compat -I/usr/src/linux-orig/include -I/usr/include/bsd 
##  =()<CFLAGSN	= @<CFLAGS>@ $(DEFSN)>()=
CFLAGSN	= -O2 -Wall  $(DEFSN)

##  =()<SYS	= @<SYS>@>()=
SYS	= linux

SOURCES	= t70.c
TESTSOURCES = test_t70.c

OBJ	= t70.o
ALL = 
TESTALL = test_t70
TESTOBJ = test_t70.o t70t.o

all:	$(OBJ) $(ALL)

test:	$(TESTALL)

test_t70: test_t70.o t70t.o ../support.a
		$(CC) $(CFLAGS) -o $@ test_t70.o t70t.o ../support.a

test_t70.o: test_t70.c
	$(CC) $(CFLAGSN) -c $<
t70t.o:	t70.c
	$(CC) $(CFLAGSN) -c -o $@ $<

lib:

install: all
	@sh ../iftrue.sh "-d $(SYS)" "cd $(SYS); make"

update: all
	@sh ../iftrue.sh "-d $(SYS)" "cd $(SYS); make update"

indent:
	../indent.sh $(SOURCES) $(TESTSOURCES)

depend:		Makefile $(SOURCES) $(TESTSOURCES)
	$(CC) -M $(DEFS) $(SOURCES) $(TESTSOURCES) > .depend

clean:
	rm -f $(OBJ) $(ALL) $(TESTALL) $(TESTOBJ)
	@sh ../iftrue.sh "-d $(SYS)" "cd $(SYS); make clean"

load:	all
	insmod t70.o

ifeq (.depend,$(wildcard .depend))
include .depend
endif

