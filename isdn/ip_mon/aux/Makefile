PROG = ip_mon

all:	install

$(PROG):	../$(PROG).o
		ld -r -x -o $@ $<

$(PROG)2:	
		cp /dev/null $(PROG)2.c
		cc -c $(PROG)2.c
		rm $(PROG)2.c
		ld -r -x -o $@ $(PROG)2.o
		rm $(PROG)2.o

../$(PROG).o:
	cd ..; make

install update:	install.$(PROG) install.$(PROG)2
	super /etc/newunix $(PROG)

install.$(PROG):	/etc/install.d/boot.d/$(PROG) /etc/install.d/master.d/$(PROG)
install.$(PROG)2:	/etc/boot.d/$(PROG)2 /etc/master.d/$(PROG)2 /etc/init.d/$(PROG)2

/etc/install.d/master.d/$(PROG):	$(PROG).master
		super cp $(PROG).master /etc/install.d/master.d/$(PROG)
/etc/install.d/boot.d/$(PROG):	$(PROG)
		super cp $(PROG) /etc/install.d/boot.d/$(PROG)
/etc/master.d/$(PROG)2:	$(PROG)2.master
		super cp $(PROG)2.master /etc/master.d/$(PROG)2
/etc/boot.d/$(PROG)2: $(PROG)2
		super cp $(PROG)2 /etc/boot.d/$(PROG)2
/etc/init.d/$(PROG)2: $(PROG)2.init
		super cp $(PROG)2.init /etc/init.d/$(PROG)2

clean:
		rm -f $(PROG)


