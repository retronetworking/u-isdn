SHELL	= /bin/sh
MAKE	= make
##  =()<P	= @<P>@>()=
P	= 

##  =()<CC	= @<CC>@>()=
CC	= gcc
##  =()<CCU	= @<CCU>@>()=
CCU	= gcc-elf -g 
##  =()<DEFS    = @<DEFKERNEL>@ @<KERNEL>@ -I../include>()=
DEFS    = -I../streams -I../compat -I/usr/src/linux-orig/include -fomit-frame-pointer -DKERNEL -D__KERNEL__ -DMODULE -DCONFIG_MODVERSIONS -I../include
##  =()<CFLAGS	= @<CFLAGS>@ $(DEFS)>()=
CFLAGS	= -O2 -Wall  $(DEFS)
##  =()<DEFSN    = @<DEFS>@ -I../include>()=
DEFSN    = -I../streams -I../compat -I/usr/src/linux-orig/include -I/usr/include/bsd -I../include
##  =()<CFLAGSN	= @<CFLAGS>@ $(DEFSN)>()=
CFLAGSN	= -O2 -Wall  $(DEFSN)
##  =()<LDFLAGS	= @<LDFLAGS>@>()=
LDFLAGS	= -v
##  =()<STR_IN	= str_@<STR_IN>@>()=
STR_IN	= str_linux
##  =()<LIBS	= @<LIBS>@>()=
LIBS	= -lbsd

##  =()<SYS	= @<SYS>@>()=
SYS	= linux

SOURCES	= ${STR_IN}.c slipto.c

OBJ	= str_if.o ${STR_IN}.o
ALL	= slipto

all:  str_if.o
prog: slipto

slipto: slipto.o ../support.a
	$(CCU) $(LDFLAGS) -o $@ $^ ${LIBS}

../support.a:
	cd ../support; make lib

slipto.o: slipto.c
	$(CCU) $(CFLAGSN) -c slipto.c

lib:

install: all
	@sh ../iftrue.sh "-d $(SYS)" "cd $(SYS); make"

update: all
	@sh ../iftrue.sh "-d $(SYS)" "cd $(SYS); make update"

clean:
	rm -f $(ALL) $(OBJ) slipto.old
	@sh ../iftrue.sh "-d $(SYS)" "cd $(SYS); make clean"

indent:
	../indent.sh $(SOURCES)

depend:		Makefile $(SOURCES)
	$(CC) -M $(DEFS) $(SOURCES) > .depend

str_if.o: ${STR_IN}.o
	@-rm str_if.o 2>/dev/null
	@ln ${STR_IN}.o str_if.o

load:	all
	insmod str_if.o

ifeq (.depend,$(wildcard .depend))
include .depend
endif

