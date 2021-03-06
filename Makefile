# config and include are first, then libraries, tools, modules+programs
SUBDIRS  = config include  compat streams support  isdn_3 isdn_4  tools  \
		   ksupport isdn_2 str_if cards x75 alaw tools van_j strslip \
		   v110 pr_on fakeh fakecept t70 buffer rate timer reconnect ip_mon

.PHONY: depend

all::	.diddepend

config::
	$(MAKE) -C config

dep: depend

TOPDIR=.
include Make.rules

.diddepend: Makefile config/config.data config/files.list
	$(MAKE) depend
	touch .diddepend

config/config.data: config

clean::
	rm -f .toldem .diddepend

master:
	set -e; for i in config support isdn_3 isdn_4 ; do $(MAKE) -C $$i all ; done

