## Process this file with automake to generate Makefile.in
AUTOMAKE_OPTIONS = foreign nostdinc
ACLOCAL_AMFLAGS = -I m4

AM_CPPFLAGS = -I. -I$(top_srcdir)
AM_LD = $(CC)

if DEBUG
  DEBUG_CFLAGS = -DDEBUG
endif

AM_CFLAGS = -Wall $(DEBUG_CFLAGS)

sbin_PROGRAMS = birq
lib_LIBRARIES =
nobase_include_HEADERS =

birq_SOURCES = \
	birq.c

birq_LDADD = liblub.a
birq_DEPENDENCIES = liblub.a

EXTRA_DIST = \
	lub/module.am \
	LICENCE \
	README

include $(top_srcdir)/lub/module.am
