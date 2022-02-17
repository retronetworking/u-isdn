SHELL	= /bin/sh
MAKE	= make
##  =()<P	= @<P>@>()=
P	= 

##  =()<CC	= @<CCU>@>()=
CC	= gcc-elf -g 
##  =()<DEFS	= @<CARDDEFS>@ @<DEFS>@ -DLEVEL=@<MLEVEL>@ -I../include >()=
DEFS	= -DDUMB -I../streams -I../compat -I/usr/src/linux-orig/include -I/usr/include/bsd -DLEVEL=3 -I../include 
##  =()<CFLAGS	= @<CFLAGS>@ $(DEFS)>()=
CFLAGS	= -O2 -Wall  $(DEFS)
##  =()<LDFLAGS	= -static @<LDFLAGS>@>()=
LDFLAGS	= -static -v
##  =()<LEVEL	= @<MLEVEL>@>()=
LEVEL	= 3
##  =()<LIBS	= @<LIBS>@>()=
LIBS	= -lbsd

SOURCES	= master.c
OBJ = master.o
ALL	= master

all prog:		$(OBJ) $(ALL)

install update: all

master:		$(P) master.o ../support.a ../isdn_3.a
	$(CC) $(LDFLAGS) -o $@ master.o `./libs.sh $(LEVEL)` $(LIBS)
lib:

clean:
	rm -f $(OBJ) $(ALL) master.old

indent:
	../indent.sh $(SOURCES)

depend:		Makefile $(SOURCES)
	$(CC) -M $(DEFS) $(SOURCES) > .depend

load:

ifeq (.depend,$(wildcard .depend))
include .depend
endif

