#!/bin/sh

if [ "$1" = "2" ] ; then
	echo ../isdn_3.a ../isdn_2.a ../support.a
elif [ "$1" = "3" ] ; then
	echo ../isdn_3.a ../support.a
else 
	echo ../support.a
fi

