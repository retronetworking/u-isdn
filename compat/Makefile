all::	

depend::
	rm -f syscompat.h linux
	ln -s compat.h syscompat.h
	ln -s . linux

KERNEL=1

TOPDIR=..
include $(TOPDIR)/Make.rules

clean::
	rm -f linux
