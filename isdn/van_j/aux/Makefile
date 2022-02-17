PROG = van_j

all:	install

$(PROG):	../$(PROG).o ../compress.o
		ld -r -x -o $@ $^

../$(PROG).o ../compress.o:
	cd ..; make

install update:	install.$(PROG)

install.$(PROG):	/etc/boot.d/$(PROG) /etc/master.d/$(PROG)

/etc/master.d/$(PROG):	$(PROG).master
		super cp $(PROG).master /etc/master.d/$(PROG)
/etc/boot.d/$(PROG): $(PROG)
		super cp $(PROG) /etc/boot.d/$(PROG)

clean:
		rm -f $(PROG)


