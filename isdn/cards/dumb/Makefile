
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

SOURCES	= avm_io.c bsc_io.c teles_io.c ncp_io.c shell.c 

OBJ	= Xteles.o Xbsc.o Xavm.o Cavm.o Cbsc.o Cteles.o Xncp.o Cncp.o
ALL = teles.o avm.o bsc.o ncp.o

all:	teles.o

load:	all
	insmod teles.o

Cavm.o:	Config.c
	$(CC) $(CFLAGS) -D_avm_ -o Cavm.o -c Config.c
Cbsc.o:	Config.c
	$(CC) $(CFLAGS) -D_bsc_ -o Cbsc.o -c Config.c
Cncp.o:	Config.c
	$(CC) $(CFLAGS) -D_ncp_ -o Cncp.o -c Config.c
Cteles.o:	Config.c
	$(CC) $(CFLAGS) -D_teles_ -o Cteles.o -c Config.c

Xavm.o:	shell.c avm_io.c
	$(CC) $(CFLAGS) -D_avm_ -o Xavm.o -c shell.c
Xbsc.o:	shell.c bsc_io.c
	$(CC) $(CFLAGS) -D_bsc_ -o Xbsc.o -c shell.c
Xncp.o:	shell.c ncp_io.c
	$(CC) $(CFLAGS) -D_ncp_ -o Xncp.o -c shell.c
Xteles.o:	shell.c teles_io.c
	$(CC) $(CFLAGS) -D_teles_ -o Xteles.o -c shell.c

avm.o:	Xavm.o Cavm.o
	ld -r -o avm.o Xavm.o Cavm.o
bsc.o:	Xbsc.o Cbsc.o
	ld -r -o bsc.o Xbsc.o Cbsc.o
ncp.o:	Xncp.o Cncp.o
	ld -r -o ncp.o Xncp.o Cncp.o
teles.o:	Xteles.o Cteles.o
	ld -r -o teles.o Xteles.o Cteles.o

lib:

indent:
	../../indent.sh $(SOURCES)

depend:		Makefile $(SOURCES)
	cp /dev/null .depend
	for i in avm ncp teles bsc ; do $(CC) -D_$${i}_ -M $(DEFS) shell.c |\
		sed -e "s/shell\.o/X$${i}.o/" >> .depend ; done

clean:
	rm -f $(OBJ) $(ALL)

install: all
	@sh ../../iftrue.sh "-d $(SYS)" "cd $(SYS); make"

update: all
	@sh ../../iftrue.sh "-d $(SYS)" "cd $(SYS); make update"


ifeq (.depend,$(wildcard .depend))
include .depend
endif

