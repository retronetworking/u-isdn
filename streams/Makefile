
all::

depend::
	rm -f sys linux
	ln -s . linux
	ln -s . sys

KERNEL=1
OBJ = streams.o

TOPDIR=..
include $(TOPDIR)/Make.rules

clean::
	rm -f sys linux
