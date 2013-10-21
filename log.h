#ifndef _log_h
#define _log_h

#include <syslog.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

int parse_log_facility(const char *str, int *facility);

#endif

