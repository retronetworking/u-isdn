SHELL	= /bin/sh
MAKE	= make
##  =()<P	= @<P>@>()=
P	= 

##  =()<CC	= @<CC>@>()=
CC	= gcc
##  =()<LDFLAGS	= @<LDFLAGS>@>()=
LDFLAGS	= -v
##  =()<LIBS	= @<LIBS>@>()=
LIBS	= -lbsd
##  =()<DEFS    = @<DEFS>@ @<DEFKERNEL>@ @<KERNEL>@ >()=
DEFS    = -I../include -I$(TOPDIR)/include -I/usr/include/bsd -I../include -I$(TOPDIR)/include -fomit-frame-pointer -DKERNEL -D__KERNEL__ 
##  =()<CFLAGS	= @<CFLAGS>@ $(DEFS)>()=
CFLAGS	= -g -O2 -W -Wreturn-type -Wshadow -Wcomment  $(DEFS)
##  =()<DEFSN    = @<DEFS>@ >()=
DEFSN    = -I../include -I$(TOPDIR)/include -I/usr/include/bsd 
##  =()<CFLAGNS	= @<CFLAGS>@ $(DEFSN)>()=
CFLAGNS	= -g -O2 -W -Wreturn-type -Wshadow -Wcomment  $(DEFSN)
##  =()<RANLIB	= @<RANLIB>@>()=
RANLIB	= ranlib

##  =()<SYS	= @<SYS>@>()=
SYS	= linux

SOURCES	= port_m.c

OBJ	= port_m.o 
ALL = # porttest

all:		$(OBJ) $(ALL)

porttest:        porttest.o
		$(CC) $(LDFLAGS) -o porttest porttest.o $(LIBS)
porttest.o:      porttest.c
		$(CC) $(CFLAGSN) -c porttest.c

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

load:	all
	insmod port_m.o

ifeq (.depend,$(wildcard .depend))
include .depend
endif

