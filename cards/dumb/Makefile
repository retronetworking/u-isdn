
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
##  =()<LD	= @<LD>@>()=
LD	= ld
##  =()<DUMBCARDS	= @<DUMBCARDS>@>()=
DUMBCARDS	= teles

##  =()<SYS	= @<SYS>@>()=
SYS	= linux

SOURCES	= avm_io.c bsc_io.c teles_io.c ncp_io.c ncp16_io.c shell.c insert.c

OBJ	= 
ALL =   teles.o   avm.o   bsc.o   ncp.o   ncp16.o \
      d_teles.o d_avm.o d_bsc.o d_ncp.o d_ncp16.o 

all:	$(foreach c,${DUMBCARDS},$c.o) $(foreach c,${DUMBCARDS},d_$c.o)

load:	$(foreach c,${DUMBCARDS},ins-$c)
	

avm.o:	insert.c
	$(CC) $(CFLAGS) -DCARDTYPE=avm -o avm.o -c insert.c
bsc.o:	insert.c
	$(CC) $(CFLAGS) -DCARDTYPE=bsc -o bsc.o -c insert.c
ncp16.o:	insert.c
	$(CC) $(CFLAGS) -DCARDTYPE=ncp16 -o ncp16.o -c insert.c
ncp.o:	insert.c
	$(CC) $(CFLAGS) -DCARDTYPE=ncp -o ncp.o -c insert.c
teles.o:	insert.c
	$(CC) $(CFLAGS) -DCARDTYPE=teles -o teles.o -c insert.c

d_avm.o:	shell.c avm_io.c
	$(CC) $(CFLAGS) -D_avm_ -o d_avm.o -c shell.c
d_bsc.o:	shell.c bsc_io.c
	$(CC) $(CFLAGS) -D_bsc_ -o d_bsc.o -c shell.c
d_ncp16.o:	shell.c ncp16_io.c
	$(CC) $(CFLAGS) -D_ncp16_ -DWIDE -o d_ncp16.o -c shell.c
d_ncp.o:	shell.c ncp_io.c
	$(CC) $(CFLAGS) -D_ncp_ -o d_ncp.o -c shell.c
d_teles.o:	shell.c teles_io.c
	$(CC) $(CFLAGS) -D_teles_ -o d_teles.o -c shell.c

ins-avm: 
	insmod d_avm.o
	insmod avm.o
ins-bsc: 
	insmod d_bsc.o
	insmod bsc.o
ins-teles: 
	insmod d_teles.o
	insmod teles.o
ins-ncp: 
	insmod d_ncp.o
	insmod ncp.o
ins-ncp16: 
	insmod d_ncp16.o
	insmod ncp16.o

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

