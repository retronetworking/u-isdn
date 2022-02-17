##  =()<CARDS	= @<CARDS>@>()=
CARDS	= dumb

all lib clean depend install update indent load:
	@for i in $(CARDS) ; do echo "make $@ in cards/$$i:" && cd $$i && make $@ && cd .. || exit 1 ; done
