SHELL	= /bin/sh
MAKE	= make
##  =()<P	= @<P>@>()=
P	= 

##  =()<CC	= @<CC>@>()=
CC	= gcc
##  =()<DEFS    = @<DEFKERNEL>@ @<KERNEL>@ -I../include  >()=
DEFS    = -I../streams -I../compat -I/usr/src/linux/include -fomit-frame-pointer -DKERNEL -D__KERNEL__ -DMODULE -DCONFIG_MODVERSIONS -I../include  
##  =()<CFLAGS	= @<CFLAGS>@ $(DEFS)>()=
CFLAGS	= -O2 -Wall  $(DEFS)
##  =()<DEFSN    = @<DEFS>@ >()=
DEFSN    = -I../streams -I../compat -I/usr/src/linux/include -I/usr/include/bsd 
##  =()<CFLAGSN	= @<CFLAGS>@ $(DEFSN)>()=
CFLAGSN	= -O2 -Wall  $(DEFSN)

##  =()<SYS	= @<SYS>@>()=
SYS	= linux

SOURCES	= fakeh.c

OBJ	= fakeh.o
ALL = 

all:	$(OBJ) $(ALL)

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


load:	
	insmod fakeh.o

ifeq (.depend,$(wildcard .depend))
include .depend
endif

