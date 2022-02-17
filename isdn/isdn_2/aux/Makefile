PROG = isdn_2

all:	install

$(PROG):	../$(PROG).o
		ld -r -x -o $@ $<

../$(PROG).o:
	cd ..; make

install update:	install.$(PROG)

install.$(PROG):	/etc/boot.d/$(PROG) /etc/master.d/$(PROG) /etc/init.d/$(PROG)

/etc/master.d/$(PROG):	$(PROG).master
		super cp $(PROG).master /etc/master.d/$(PROG)
/etc/init.d/$(PROG):	$(PROG).init
		super cp $(PROG).init /etc/init.d/$(PROG)
/etc/boot.d/$(PROG): $(PROG)
		super cp $(PROG) /etc/boot.d/$(PROG)

clean:
		rm -f $(PROG)


