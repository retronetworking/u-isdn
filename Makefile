
# config and include are first, final is last
DIRS  = config compat streams include bin support isdn_3 isdn_4 \
		ksupport isdn_2 str_if cards x75 alaw \
		v110 pr_on strslip fakeh t70 rate timer reconnect ip_mon final
ADIRS = config include bin support isdn_3 isdn_4 str_if ip_mon alaw tools
KDIRS = config compat streams include ksupport isdn_2 str_if cards x75 \
		alaw v110 pr_on strslip fakeh t70 rate timer reconnect ip_mon final
#	dumbmgr portman port_m 

SYSTEMS = linux
#SYSTEMS = aux sco svr4 linux

all prog:: .depend

.depend:
	@$(MAKE) $(MAKEFLAGS) depend
	@touch .depend

all update load::
	set -e;for i in $(KDIRS);do echo -n "$$i: "; $(MAKE) $(MAKEFLAGS) -C $$i $@ ;  done

prog::
	set -e;for i in $(ADIRS);do echo -n "$$i: "; $(MAKE) $(MAKEFLAGS) -C $$i $@ ;  done

depend clean indent::
	set -e;for i in $(DIRS);do echo -n "$$i: "; $(MAKE) $(MAKEFLAGS) -C $$i $@ ;  done

install: all
	$(MAKE) $(MAKEFLAGS) -C final doinstall

conf:
	cd config ; $(MAKE) $(MAKEFLAGS)

clean::
	for i in $(ALLDIRS) ; do ( cd $$i; \
		for j in $(SYSTEMS) ; do echo $$i/$$j ; \
		sh ../iftrue.sh "-d $$j" "cd $$j ; $(MAKE) $(MAKEFLAGS) clean" ; done \
	) ; done
	find . -name .depend -print|xargs rm -f
	rm -f .toldem

iprog:
	cd final && $(MAKE) $(MAKEFLAGS) install.bin


dep: depend

