#!/bin/sh
if [ -x "`which autoreconf 2>/dev/null`" ] ; then
   exec autoreconf --verbose --force -i
fi

LIBTOOLIZE=libtoolize
SYSNAME=`uname`
if [ "x$SYSNAME" = "xDarwin" ] ; then
  LIBTOOLIZE=glibtoolize
fi
aclocal && \
	autoheader && \
	autoconf && \
	automake --add-missing --force-missing --copy
