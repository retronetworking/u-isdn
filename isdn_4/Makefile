SHELL	= /bin/sh
MAKE	= make
##  =()<P	= @<P>@>()=
P	= 

##  =()<CC	= @<CCU>@>()=
CC	= gcc 
##  =()<DEFS	= @<CARDDEFS>@ @<DEFS>@ -DLEVEL=@<MLEVEL>@ -I../include >()=
DEFS	= -DDUMB -I../streams -I../compat -I/usr/src/linux/include -I/usr/include/bsd -DLEVEL=3 -I../include 
##  =()<CFLAGS	= @<CFLAGS>@ $(DEFS)>()=
CFLAGS	= -O2 -Wall  $(DEFS)
##  =()<LDFLAGS	= -static @<LDFLAGS>@>()=
LDFLAGS	= -static -v
##  =()<LEVEL	= @<MLEVEL>@>()=
LEVEL	= 3
##  =()<LIBS	= @<LIBS>@>()=
LIBS	= -lbsd

SOURCES	= cf.c conn.c devs.c info.c malldebug.c master.c match.c nr.c \
			stream.c texts.c util.c work.c loader.c
OBJ     = cf.o conn.o devs.o info.o malldebug.o master.o match.o nr.o \
			stream.o texts.o util.o work.o loader.o
ALL	= master

all prog:		$(OBJ) $(ALL)

install update: all

master:		$(P) $(OBJ) ../support.a ../isdn_3.a
	$(CC) $(LDFLAGS) -o $@ $(OBJ) `./libs.sh $(LEVEL)` $(LIBS)
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

