HEADERS	= $(wildcard *.h)

TOPDIR=..
include $(TOPDIR)/Make.rules

all:: $(KERNELSRC)/include/linux/autoconf.h $(KERNELSRC)/include/linux/version.h

depend::
	rm -f symtables
	cp /dev/null symtables

$(KERNELSRC)/include/linux/autoconf.h $(KERNELSRC)/include/linux/version.h:
	@echo "Die Kernelsourcen sind entweder irgendwo, nur nicht in "
	@echo "$(KERNELSRC), oder jemand hat dort 'make mrproper' "
	@echo "aufgerufen. -> Gehe nach $(KERNELSRC),"
	@echo "make config; make dep; make clean; make; installiere das Teil,"
	@echo "dann komm wieder und mache hier weiter."
	@exit 1

install::
	-mkdir -p $${MODDIR:-/lib/modules}/$(shell ../tools/getversion)/isdn
	-mkdir -p $(DESTDIR)/bin-$(shell ../tools/getversion) 
	cp /dev/null ../modules.isdn

clean::
	rm -f ../modules.isdn
