TOPDIR=..
include $(TOPDIR)/Make.config
PROGRAMS = cardname catfone
NOCLEAN = catfone

all:: 

install::
	-mkdir -p /lib/modules/$(shell ./getversion)/isdn
	-mkdir -p $(DESTDIR)/bin-$(shell ./getversion) 
	cp /dev/null /lib/modules/modules.isdn.all
	@if test ! -f /etc/isdn.conf; then				 \
		echo install isdn.conf /etc ;				 \
		install isdn.conf /etc ;					 \
	fi
	@if test -d /etc/rc.d ; then					 \
		if test ! -f /etc/rc.d/rc.isdn ; then		 \
			echo install rc.isdn /etc/rc.d ;		 \
			install rc.isdn /etc/rc.d;				 \
		fi;											 \
	else											 \
		if test ! -f /etc/rc.isdn ; then			 \
			echo install rc.isdn /etc ;				 \
			install rc.isdn /etc;					 \
		fi;											 \
	fi

include $(TOPDIR)/Make.rules

