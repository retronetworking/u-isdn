SHELL	= /bin/sh
MAKE	= make
##  =()<P	= @<P>@>()=
P	= 

##  =()<CC	= @<CCU>@>()=
CC	= gcc 

## Change this to  @<CFLAGS>@ @<DEFKERNEL>@ @<KERNEL>@ -I../include
#### if you want to move isdn_3 into the kernel
#### Also swap the commented lines for all: and lib:

##  =()<DEFS    = @<DEFS>@ -I../include >()=
DEFS    = -I../streams -I../compat -I/usr/src/linux/include -I/usr/include/bsd -I../include 
##  =()<CFLAGS	= @<CFLAGS>@ $(DEFS)>()=
CFLAGS	= -O2 -Wall  $(DEFS)

##  =()<RANLIB  = @<RANLIB>@>()=
RANLIB  = ranlib

HEADERS = phone.h capi.h prot_1TR6_0.h prot_1TR6_1.h tei.h fixed.h

SOURCES	= isdn_3.c tei.c fixed.c capi.c phone.c prot_1TR6_0.c prot_1TR6_1.c prot_1TR6_common.c prot_ETS.c

OBJ	= isdn_3.o tei.o fixed.o capi.o phone.o prot_1TR6_0.o prot_1TR6_1.o prot_1TR6_common.o prot_ETS.o

ALL = ../isdn_3.a

all prog:	$(OBJ)	$(ALL)

../isdn_3.a:	$(OBJ)
	ar r ../isdn_3.a $?
	@ $(RANLIB) ../isdn_3.a

indent:
	../indent.sh $(HEADERS) $(SOURCES)

install update: all

clean:
	rm -f $(ALL) $(OBJ) 

depend:		Makefile $(SOURCES)
	$(CC) -M $(DEFS) $(SOURCES) > .depend

load:

ifeq (.depend,$(wildcard .depend))
include .depend
endif

