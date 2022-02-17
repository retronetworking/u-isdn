SHELL	= /bin/sh
MAKE	= make
##  =()<P	= @<P>@>()=
P	= 

##  =()<CCU	= @<CCU>@>()=
CCU	= gcc-elf -g 
##  =()<CC	= @<CC>@>()=
CC	= gcc
##  =()<DEFS	= @<DEFKERNEL>@ @<KERNEL>@ -I../include >()=
DEFS	= -I../streams -I../compat -I/usr/src/linux-orig/include -fomit-frame-pointer -DKERNEL -D__KERNEL__ -DMODULE -DCONFIG_MODVERSIONS -I../include 
##  =()<CFLAGS	= @<CFLAGS>@ $(DEFS)>()=
CFLAGS	= -O2 -Wall  $(DEFS)
##  =()<DEFSN	= @<DEFS>@ >()=
DEFSN	= -I../streams -I../compat -I/usr/src/linux-orig/include -I/usr/include/bsd 
##  =()<NCFLAGS	= @<CFLAGS>@ $(DEFSN)>()=
NCFLAGS	= -O2 -Wall  $(DEFSN)
##  =()<LIBS	= @<LIBS>@>()=
LIBS	= -lbsd
##  =()<LDFLAGS	= @<LDFLAGS>@>()=
LDFLAGS	= -v

##  =()<SYS	= @<SYS>@>()=
SYS	= linux

SOURCES	= alaw.c
OBJ	= alaw.o tosnd.o toalaw.o
ALL =	tosnd toalaw

all:	alaw.o
prog:	tosnd toalaw

toalaw:	toalaw.o
	${CCU} ${LDFLAGS} -o $@ toalaw.o ${LIBS}
tosnd:	tosnd.o
	${CCU} ${LDFLAGS} -o $@ tosnd.o ${LIBS}
toalaw.o:	toalaw.c
	${CCU} ${NCFLAGS} -c toalaw.c
tosnd.o:	tosnd.c
	${CCU} ${NCFLAGS} -c tosnd.c

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

load: all
	insmod alaw.o

ifeq (.depend,$(wildcard .depend))
include .depend
endif

