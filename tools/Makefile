TOPDIR=..
include $(TOPDIR)/Make.config
PROGRAMS = cardname catfone
NOCLEAN = catfone

all:: 

install::
	@if test ! -f /etc/isdn.conf; then				 		\
		echo install isdn.conf /etc ;				 		\
		install isdn.conf /etc ;					 		\
	fi
	@if test -d /sbin/init.d/scripts ; then					\
		echo "*** /sbin/init.d/scripts/uisdn ***";			\
		install rc.isdn /sbin/init.d/scripts/uisdn ;		\
		install rc.isdnmon /sbin/init.d/scripts/uisdnmon ;	\
	elif test -d /sbin/init.d; then					 		\
		echo "*** /sbin/init.d/isdn ***";		 			\
		install rc.isdn /sbin/init.d/isdn ;		 			\
		install rc.isdnmon /sbin/init.d/isdnmon ;	 		\
	elif test -d /etc/rc.d ; then					 		\
		echo "*** /etc/rc.d/rc.isdn ***";		 			\
		install rc.isdn /etc/rc.d;				 			\
		install rc.isdnmon /etc/rc.d;				 		\
	else											 		\
		echo "*** /etc/rc.isdn ***";		 				\
		install rc.isdn /etc;					 			\
		install rc.isdnmon /etc;				 			\
	fi

include $(TOPDIR)/Make.rules

