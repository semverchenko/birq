#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.
m4_define([MAJOR_VERSION], 1)
m4_define([MINOR_VERSION], 1)
m4_define([MICRO_VERSION], 3)

AC_PREREQ(2.59)
AC_INIT([birq],
        [MAJOR_VERSION.MINOR_VERSION.MICRO_VERSION],
        [serj.kalichev at gmail dot com])

AC_CONFIG_AUX_DIR([.])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE(subdir-objects)

# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_RANLIB

# needed to handle 64-bit architecture
AC_CHECK_SIZEOF(int)
AC_CHECK_SIZEOF(long)
AC_CHECK_SIZEOF(size_t)

################################
# Deal with debugging options
################################
AC_ARG_ENABLE(debug,
              [AS_HELP_STRING([--enable-debug],
                              [Turn on debugging including asserts [default=no]])],
              [],
              [enable_debug=no])
AM_CONDITIONAL(DEBUG,test x$enable_debug = xyes)

################################
# Check for getopt_long()
################################
AC_CHECK_HEADERS(getopt.h, [],
    AC_MSG_WARN([getopt.h not found: only short parameters can be used on command line]))

################################
# Check for locale.h
################################
AC_CHECK_HEADERS(locale.h, [],
    AC_MSG_WARN([locale.h not found: the locales is not supported]))

AC_CONFIG_FILES(Makefile)
AC_OUTPUT
