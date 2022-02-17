SHELL	= /bin/sh
MAKE	= make
##  =()<SYS	= @<SYS>@>()=
SYS	= linux

##  =()<CC	= @<CC>@>()=
CC	= gcc
##  =()<DEFS	= @<DEFKERNEL>@ @<KERNEL>@ >()=
DEFS	= -I../streams -I../compat -I/usr/src/linux-orig/include -fomit-frame-pointer -DKERNEL -D__KERNEL__ -DMODULE -DCONFIG_MODVERSIONS 
##  =()<CFLAGS	= @<CFLAGS>@ $(DEFS)>()=
CFLAGS	= -O2 -Wall  $(DEFS)

##  =()<SYS	= @<SYS>@>()=
SYS	= linux
##  =()<DESTDIR	= @<DESTDIR>@>()=
DESTDIR	= /usr/local/isdn

SOURCES	= x75.c

doinstall:
	@cd .. && make install

install.sys:
	@sh ../iftrue.sh "-f $(SYS)/Makefile" "cd $(SYS) && make install \
		CFLAGS=\"$(CFLAGS)\" CC=\"$(CC)\""
	if test ! -d ${DESTDIR} ; then mkdir ${DESTDIR} ; else true ; fi
	if test ! -d ${DESTDIR}/bin ; then mkdir ${DESTDIR}/bin ; else true ; fi
	if test ! -f /etc/isdn.conf; then cp ../cf.tcp /etc/isdn.conf ; else true ; fi

install.bin:
	DIR=${DESTDIR}/bin-$$(uname -r) ; \
	if test -d $$DIR ; then true; else mkdir -p $$DIR; fi ; \
	for i in `/bin/ls ../bin` ; do 				\
		test -f $$DIR/$$i && \
			mv -f $$DIR/$$i $$DIR/$$i.old	; \
		cp ../bin/$$i $$DIR/$$i ;  \
	done

# "install" does _not_ depend on all, because all calls install indirectly
#install: install.sys install.bin
install: install.sys

clean:
	@sh ../iftrue.sh "-f $(SYS)/Makefile" "cd $(SYS) && make clean"

run:
	@sh ../iftrue.sh "-f $(SYS)/Makefile" "cd $(SYS) && make run"

lib:

depend:

load:

all:

