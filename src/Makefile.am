SUBDIRS = .
INCLUDES = -I$(srcdir)/src -I$(srcdir)/../lib/libevent \
    -I$(srcdir)/../lib/libevent/include -I$(srcdir)/../../lib/libevent \
    -I$(srcdir)/../../lib/libevent/include
export INCLUDES

noinst_PROGRAMS=gingko_serv gingko_clnt #gingko_test_clnt

gingko_serv_SOURCES=async_threads.cc async_conn.cc gingko_serv.cc \
    gingko_base.cc recurse_dir.cc md5.cc fnv_hash.cc path.cc
gingko_serv_LDADD=$(srcdir)/../lib/libevent/.libs/libevent.a
gingko_serv_LDFLAGS=-D_GNU_SOURCE -ggdb -funsigned-char

gingko_clnt_SOURCES=async_threads.cc async_conn.cc gingko_clnt.cc \
    gingko_base.cc  md5.cc fnv_hash.cc path.cc route.cc
gingko_clnt_LDADD=$(srcdir)/../lib/libevent/.libs/libevent.a
gingko_clnt_LDFLAGS=-D_GNU_SOURCE -ggdb -funsigned-char

#gingko_test_clnt_SOURCES=async_threads.cc async_conn.cc gingko_test_clnt.cc \
#    gingko_base.cc  md5.cc fnv_hash.cc path.cc
#gingko_test_clnt_LDADD=$(srcdir)/../lib/libevent/.libs/libevent.a
#gingko_test_clnt_LDFLAGS=-D_GNU_SOURCE -ggdb -funsigned-char

DEFS+=-D_GNU_SOURCE