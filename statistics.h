#ifndef _statistics_h
#define _statistics_h

#include "lub/list.h"

void parse_proc_stat(lub_list_t *cpus, lub_list_t *irqs);
void show_statistics(lub_list_t *cpus);

#endif
