# config and include are first, tools is next
SUBDIRS  = config include tools compat streams support isdn_3 isdn_4 \
		   ksupport isdn_2 str_if cards x75 alaw tools van_j strslip \
		   v110 pr_on strslip fakeh t70 rate timer reconnect ip_mon

.PHONY: depend

all::	.diddepend
dep: depend

TOPDIR=.
include Make.rules

.diddepend:: Makefile
	$(MAKE) depend
	touch .diddepend

clean::
	rm -f .toldem .diddepend
