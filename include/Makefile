HEADERS	= $(wildcard *.h)

TOPDIR=..
include $(TOPDIR)/Make.rules

install::
	-mkdir -p /lib/modules/$(shell ../tools/getversion)/isdn
	-mkdir -p $(DESTDIR)/bin-$(shell ../tools/getversion) 
	cp /dev/null /lib/modules/modules.isdn.all

