SUBDIRS = .
INCLUDES = -I$(srcdir)/src \
    -I$(srcdir)/../../lib/libev/include 
#    -I$(srcdir)/../../../../../../third-64/libev/include 
#    -I$(srcdir)/../../../../../../../third-64/libev/include 
#export INCLUDES

noinst_LIBRARIES=libxor_hash.a libmd5.a libgko_zip.a

DEFS+=-O0 -D_GNU_SOURCE -ggdb -funsigned-char
libxor_hash_a_SOURCES=xor_hash.cpp
libxor_hash_a_LDFLAGS=-O0 -D_GNU_SOURCE -ggdb -funsigned-char

DEFS+=-O2 -D_GNU_SOURCE -ggdb
libmd5_a_SOURCES=md5.cpp
libmd5_a_LDFLAGS=-O2 -D_GNU_SOURCE -ggdb

DEFS+=-D_GNU_SOURCE -ggdb -O3 -I. -Wall -W
libgko_zip_a_SOURCES=gko_zip.cpp lz4.cpp
libgko_zip_a_LDFLAGS=-D_GNU_SOURCE -ggdb -O3 -I. -Wall -W