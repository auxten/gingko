SUBDIRS = .
INCLUDES = -I$(srcdir)/src \
    -I$(srcdir)/../../../../../third-64/gtest/include \
    -I$(srcdir)/../../../../../../third-64/gtest/include \
    -I$(srcdir)/../../../../../../../third-64/gtest/include \
    -I$(srcdir)/../../lib/libev/include 
#    -I$(srcdir)/../../../../../../third-64/libev/include 
#    -I$(srcdir)/../../../../../../../third-64/libev/include 
export INCLUDES

noinst_LIBRARIES=libxor_hash.a

DEFS+=-O0 -D_GNU_SOURCE -ggdb -funsigned-char
libxor_hash_a_SOURCES=xor_hash.cpp
libxor_hash_a_LDFLAGS=-O0 -D_GNU_SOURCE -ggdb -funsigned-char
