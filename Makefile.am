## Process this file with automake to generate Makefile.in

if DEBUG
  DEBUG_CFLAGS = -DDEBUG
endif

AUTOMAKE_OPTIONS = foreign nostdinc
ACLOCAL_AMFLAGS = -I m4
AM_CPPFLAGS = -I. -I$(top_srcdir)
AM_LD = $(CC)
AM_CFLAGS = -Wall $(DEBUG_CFLAGS)

sbin_PROGRAMS = birq
lib_LIBRARIES =

noinst_HEADERS = \
	irq.h \
	nl.h \
	cpumask.h

birq_SOURCES = \
	birq.c \
	nl.c \
	irq_parse.c

birq_LDADD = liblub.a
birq_DEPENDENCIES = liblub.a

EXTRA_DIST = \
	lub/module.am \
	LICENCE \
	README

include $(top_srcdir)/lub/module.am
