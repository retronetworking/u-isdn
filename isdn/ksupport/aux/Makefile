
all: install

WHAT = proto smallq streamlib x75lib
NWHAT = count strlog qinfo

count: ../count.o
		ld -r -x -o $@ $<
strlog: ../log.o
		ld -r -x -o $@ $<
qinfo: ../qinfo.o
		ld -r -x -o $@ $<
proto: ../proto.o
		ld -r -x -o $@ $<
smallq: ../smallq.o
		ld -r -x -o $@ $<
streamlib: ../streamlib.o
		ld -r -x -o $@ $<
x75lib: ../x75lib.o
		ld -r -x -o $@ $<

../$(PROG).o:
	cd ..; make

install update:
	@for i in $(WHAT) $(NWHAT); do make install.$$i ; done
	super /etc/newunix $(NWHAT)

install.count:	/etc/install.d/boot.d/count /etc/install.d/count /etc/uninstall.d/count
install.strlog:	/etc/install.d/boot.d/strlog /etc/install.d/strlog /etc/uninstall.d/strlog
install.qinfo:	/etc/install.d/boot.d/qinfo /etc/install.d/qinfo /etc/uninstall.d/qinfo
install.proto:	/etc/boot.d/proto /etc/master.d/proto
install.smallq:	/etc/boot.d/smallq /etc/master.d/smallq
install.streamlib:	/etc/boot.d/streamlib /etc/master.d/streamlib
install.x75lib:	/etc/boot.d/x75lib /etc/master.d/x75lib

/etc/install.d/count:	count.install
		super cp count.install /etc/install.d/count
/etc/uninstall.d/count:	count.uninstall
		super cp count.uninstall /etc/uninstall.d/count
/etc/install.d/boot.d/count: count
		super cp count /etc/install.d/boot.d/count

/etc/install.d/strlog:	strlog.install
		super cp strlog.install /etc/install.d/strlog
/etc/uninstall.d/strlog:	strlog.uninstall
		super cp strlog.uninstall /etc/uninstall.d/strlog
/etc/install.d/boot.d/strlog: strlog
		super cp strlog /etc/install.d/boot.d/strlog

/etc/install.d/qinfo:	qinfo.install
		super cp qinfo.install /etc/install.d/qinfo
/etc/uninstall.d/qinfo:	qinfo.uninstall
		super cp qinfo.uninstall /etc/uninstall.d/qinfo
/etc/install.d/boot.d/qinfo: qinfo
		super cp qinfo /etc/install.d/boot.d/qinfo

/etc/master.d/proto:	proto.master
		super cp proto.master /etc/master.d/proto
/etc/boot.d/proto: proto
		super cp proto /etc/boot.d/proto

/etc/master.d/smallq:	smallq.master
		super cp smallq.master /etc/master.d/smallq
/etc/boot.d/smallq: smallq
		super cp smallq /etc/boot.d/smallq

/etc/master.d/streamlib:	streamlib.master
		super cp streamlib.master /etc/master.d/streamlib
/etc/boot.d/streamlib: streamlib
		super cp streamlib /etc/boot.d/streamlib

/etc/master.d/x75lib:	x75lib.master
		super cp x75lib.master /etc/master.d/x75lib
/etc/boot.d/x75lib: x75lib
		super cp x75lib /etc/boot.d/x75lib

clean:
		rm -f $(WHAT) $(NWHAT)

