PROG = slip

all:	install

$(PROG):	../$(PROG).o
		ld -r -x -o $@ $^

../$(PROG).o:
	cd ..; make

install update:	install.$(PROG)

install.$(PROG):	/etc/install.d/boot.d/$(PROG) /etc/install.d/master.d/$(PROG)
	super newunix $(PROG)

/etc/install.d/master.d/$(PROG):	$(PROG).master
		super cp $(PROG).master /etc/install.d/master.d/$(PROG)
/etc/install.d/boot.d/$(PROG): $(PROG)
		super cp $(PROG) /etc/install.d/boot.d/$(PROG)

clean:
		rm -f $(PROG)


