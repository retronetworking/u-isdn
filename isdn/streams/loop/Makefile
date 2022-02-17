#
# Makefile for the loop Streams driver.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#

.c.s:
	$(CC) $(CFLAGS) -S $<
.s.o:
	$(AS) -c -o $*.o $<
.c.o:
	$(CC) $(CFLAGS) -c $<

SUBDIRS= 

OBJS  = loop.o

all:	loop.a

loop.a:	$(OBJS)
	@$(LD) -r -o loop.a $(OBJS)

#subdirs: dummy
#	@for i in $(SUBDIRS); do (cd $$i && echo $$i && $(MAKE)) || exit; done

clean:
	rm -f core *.o *.a *.s
#	for i in $(SUBDIRS); do (cd $$i && $(MAKE) clean) || exit; done

dep:
	$(CPP) -M $(KEYBOARD) *.c > .depend
#	for i in $(SUBDIRS); do (cd $$i && $(MAKE) dep) || exit; done

dummy:

#
# include a dependency file if one exists
#
ifeq (.depend,$(wildcard .depend))
include .depend
endif
