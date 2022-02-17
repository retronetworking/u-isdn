##  $Revision: 1.20 $
SHELL	= /bin/sh
MAKE	= make
##  =()<P	= @<P>@>()=
P	= 

FILE	= files.list
LIST	= `grep -v '^;' <$(FILE)`

##  If you use the script version of subst, and your version of sed chokes
##  then get FSF sed and edit the next line as appropriate.
#SED	= -e /usr/gnu/bin/sed


all prog lib install update depend:	substitutions

indent load:

quiet:
	@$(MAKE) 'FILE=$(FILE)' substitutions | grep -v unchanged

clobber clean:
	rm -f subst.tmp config.new subst
	rm -f tags lint substitutions

substitutions:		subst files.list config.data
	-@rm -f ../*/substtmp.new ../*/substtmp.old
	@sh ./checkconf.sh
	cat config.data >subst.tmp
	#@awk 'NR==1 {printf "PATCHLEVEL\t%s\n", $$2;}' <../PATCHLOG >>subst.tmp
	-chmod +w $(LIST)
	./subst $(SED) -f subst.tmp $(LIST)
	@rm subst.tmp
	-date > substitutions

backup:
	@sh ../iftrue.sh '-d ORIG' 'echo ORIG exists! ; exit 1'
	mkdir ORIG
	@for I in $(LIST) ; do \
	    echo cp $$I ORIG ; \
	    cp $$I ORIG ; \
	done

restore:
	@sh ../iftrue.sh '! -d ORIG' 'echo No ORIG directory! ; exit 1'
	@for I in $(LIST) ; do \
	    echo cp ORIG/`basename $$I` $$I ; \
	    cp ORIG/`basename $$I` $$I ; \
	done

config.data:		config.dist
	@sh ../iftrue.sh '-f config.new' 'echo config.new exists! ; exit 1'
	@sh ../iftrue.sh '-f config.old' 'echo config.old exists! ; exit 1'
	@sh ../iftrue.sh '! -f config.data' '$(MAKE) CLOBBER=YES SCRATCH'
	cat config.data config.dist >subst.tmp
	cp config.dist config.new
	./subst $(SED) -f subst.tmp config.new
	-@rm -f subst.tmp
	@echo "Update config.old from config.new!"
	@sh ../iftrue.sh '-f config.data' \
	    'echo "Renamed config file to config.old"; mv config.data config.old'
	exit 1 ;
SCRATCH:
	@sh ../iftrue.sh '-z $(CLOBBER)' 'echo No way! ; exit 1'
	@echo 'You do not have a config.data file.'
	@echo 'Setting one up using the default file.'
	@echo 'You must edit this file, then remove config.new and config.old.'
	@echo 'Then restart the make.'
	cat config.dist >config.data

subst:		subst.c subst.sh
	-make c || make sh || rm -f subst
	@sh ../iftrue.sh '! -f subst' 'echo Cannot make subst ; exit 1'
c:
	@rm -f subst
	$(CC) -o subst subst.c
sh:
	@rm -f subst
	cp subst.sh subst
	chmod +x subst

.PRECIOUS:	config.data files.list

