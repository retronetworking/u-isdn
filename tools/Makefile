SHELL	= /bin/sh
MAKE	= make
##  =()<P	= @<P>@>()=
P	= 

##  =()<CCU	= @<CCU>@>()=
CCU	= gcc 
##  =()<CC	= @<CC>@>()=
CC	= gcc
##  =()<DEFS	= @<DEFKERNEL>@ @<KERNEL>@ -I../include >()=
DEFS	= -I../streams -I../compat -I/usr/src/linux/include -fomit-frame-pointer -DKERNEL -D__KERNEL__ -DMODULE -DCONFIG_MODVERSIONS -I../include 
##  =()<CFLAGS	= @<CFLAGS>@ $(DEFS)>()=
CFLAGS	= -O2 -Wall  $(DEFS)
##  =()<DEFSN	= @<DEFS>@ -I../include >()=
DEFSN	= -I../streams -I../compat -I/usr/src/linux/include -I/usr/include/bsd -I../include 
##  =()<NCFLAGS	= @<CFLAGS>@ $(DEFSN)>()=
NCFLAGS	= -O2 -Wall  $(DEFSN)
##  =()<LIBS	= @<LIBS>@>()=
LIBS	= -lbsd
##  =()<LDFLAGS	= @<LDFLAGS>@>()=
LDFLAGS	= -v

##  =()<SYS	= @<SYS>@>()=
SYS	= linux

SOURCES	= cardname.c
OBJ	= cardname.o
ALL =	cardname

all:	

prog:	cardname

cardname:	cardname.o
	${CCU} ${LDFLAGS} -o $@ cardname.o ${LIBS}
cardname.o:	cardname.c
	${CCU} ${NCFLAGS} -c cardname.c

lib:

install: all

update: all

indent:
	../indent.sh $(SOURCES)

clean:
	rm -f $(ALL) $(OBJ)
	@sh ../iftrue.sh "-d $(SYS)" "cd $(SYS); make clean"

depend:		Makefile $(SOURCES)
	$(CC) -M $(DEFS) $(SOURCES) > .depend

load: 

ifeq (.depend,$(wildcard .depend))
include .depend
endif

