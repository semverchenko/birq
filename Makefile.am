## Process this file with automake to generate Makefile.in

if DEBUG
  DEBUG_CFLAGS = -DDEBUG
endif

AUTOMAKE_OPTIONS = foreign nostdinc
ACLOCAL_AMFLAGS = -I m4
AM_CPPFLAGS = -I. -I$(top_srcdir)
AM_LD = $(CC)
AM_CFLAGS = -Wall -D_GNU_SOURCE $(DEBUG_CFLAGS)

sbin_PROGRAMS = birq
lib_LIBRARIES =

noinst_HEADERS = \
	birq.h \
	bitmap.h \
	non-atomic.h \
	cpumask.h \
	irq.h \
	cpu.h \
	statistics.h \
	balance.h

birq_SOURCES = \
	birq.c \
	bitmap.c \
	irq_parse.c \
	cpu_parse.c \
	statistics.c \
	balance.c

birq_LDADD = liblub.a
birq_DEPENDENCIES = liblub.a

EXTRA_DIST = \
	lub/module.am \
	LICENCE \
	README

include $(top_srcdir)/lub/module.am
