all::	

XHEADERS = count.h log.h qinfo.h
HEADERS  = $(XHEADERS)

XSOURCES = count.c dump.c log.c logh.c qinfo.c smallq.c streamlib.c
YSOURCES = streams_sys.c
PSOURCES = q_data.c sioctl.c timeout.c streams.c kernel.c wildmat.c \
	libc.c vectcmp.c $(XSOURCES) $(YSOURCES)

LIB	= ../support.a

$(XHEADERS) $(XSOURCES):
	ln -sf ../ksupport/$@ $@

$(YSOURCES):
		ln -sf ../streams/$@ $@

clean::
	rm -f $(HEADERS) $(XSOURCES) $(YSOURCES)

TOPDIR=..
include $(TOPDIR)/Make.rules

