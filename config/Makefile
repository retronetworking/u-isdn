SHELL	= /bin/sh
MAKE	= make

FILE	= files.list
LIST	= `grep -v '^;' <$(FILE)`

##  If you use the script version of subst, and your version of sed chokes
##  then get FSF sed and edit the next line as appropriate.
#SED	= -e /usr/gnu/bin/sed


all prog lib install update depend::	substitutions

indent load:

quiet:
	@$(MAKE) 'FILE=$(FILE)' substitutions | grep -v unchanged

clobber: clean
	rm -f config.data
	rm -f tags lint substitutions

clean:
	rm -f subst.tmp config.new subst
	rm -f tags lint substitutions

substitutions:		subst files.list config.data
	-@find .. -name substtmp.new -o -name substtmp.old | xargs rm -f ../.not.there
	@if test -f config.new ; then echo "*** Check config.new and rename it to config.data, then re-make!"; exit 1 ; fi
	@sh ./checkconf.sh
	cat config.data >subst.tmp
	#@awk 'NR==1 {printf "PATCHLEVEL\t%s\n", $$2;}' <../PATCHLOG >>subst.tmp
	-chmod +w $(LIST)
	./subst $(SED) -f subst.tmp $(LIST)
	@rm subst.tmp
	-date > substitutions

config.data:		subst config.dist
	@if test -f config.new; then echo "*** Check config.new and rename it to config.data, then re-make!" ; exit 1 ; fi
	-@rm -f subst.tmp
	@if test -f config.data; then cat config.data config.dist ; else cat config.dist ; fi > subst.tmp
	cp config.dist config.new
	./subst $(SED) -f subst.tmp config.new
	-@rm -f subst.tmp
	mv config.new config.data

subst:		subst.c subst.sh
	-make c || make sh || rm -f subst
	@if test ! -f subst; then echo Cannot make subst ; exit 1; fi
c:
	@rm -f subst
	$(CC) -o subst subst.c
sh:
	@rm -f subst
	cp subst.sh subst
	chmod +x subst

.PRECIOUS:	config.data files.list

