PROGRAM = master
EXTRA_LIBS=../isdn_3.a ../support.a 
EXTRA_CFLAGS=$(addsuffix _,$(addprefix -D_,$(PROTOCOLS) $(SUBPROTOCOLS)))
#EXTRA_LDFLAGS=-static

TOPDIR=..
include $(TOPDIR)/Make.rules
