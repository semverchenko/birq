## Process this file with automake to generate Makefile.in
AUTOMAKE_OPTIONS = foreign nostdinc
ACLOCAL_AMFLAGS = -I m4

AM_CPPFLAGS = -I. -I$(top_srcdir)
AM_LD = $(CC)

if DEBUG
  DEBUG_CFLAGS = -DDEBUG
endif

AM_CFLAGS = -Wall $(DEBUG_CFLAGS)
#AM_CFLAGS = -ansi -pedantic -Werror -Wall -D_POSIX_C_SOURCE=199309 -DVERSION=$(VERSION) $(DEBUG_CFLAGS)

sbin_PROGRAMS = birq
lib_LTLIBRARIES =
noinst_LTLIBRARIES =
lib_LIBRARIES =
nobase_include_HEADERS =

birq_SOURCES = \
	birq.c

birq_LDADD = liblub.la
birq_DEPENDENCIES = liblub.la

EXTRA_DIST = \
	lub/module.am \
	LICENCE \
	README

include $(top_srcdir)/lub/module.am
