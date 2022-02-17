
all: isdn

isdn: dummy
	make -C isdn

depend dep:
	make -C isdn depend

load:
	make -C isdn load

clean:
	make -C isdn clean
	-rm .toldem

dummy:
