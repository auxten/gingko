noinst_PROGRAMS=cts
cts_SOURCES=cts.cc cons.cc threads.cc
cts_LDADD=$(top_srcdir)/../../lib/libevent/.libs/libevent.a
cts_LDFLAGS=-D_GNU_SOURCE
DEFS+=-D_GNU_SOURCE
INCLUDES=-I$(srcdir)/../../../lib/libevent -I$(srcdir)/../../../lib/libevent/include

