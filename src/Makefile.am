lib_LTLIBRARIES = libxrap.la

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libxrap.pc

include_HEADERS = \
    ../include/xrap.h \
    ../include/xrap_msg.h

libxrap_la_SOURCES = \
    platform.h \
    xrap_msg.c

EXTRA_DIST = xrap_msg.xml xrap_msg.bnf license.xml generate

AM_CFLAGS = -g
AM_CPPFLAGS = -I$(top_srcdir)/include
bin_PROGRAMS = xrap_selftest
xrap_selftest_LDADD = libxrap.la
xrap_selftest_SOURCES = xrap_selftest.c
libxrap_la_LDFLAGS = -version-info @LTVER@
TESTS = xrap_selftest
