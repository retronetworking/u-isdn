TOPDIR=..
include $(TOPDIR)/Make.config

KERNEL=1
REALSOURCE=loader.c
OBJS=$(addsuffix .o,$(CARDS))
EXTRA_CFLAGS= -DCARDTYPE=$(basename $@ .o)

SUBDIRS=capi dumb arnet

include $(TOPDIR)/Make.rules

