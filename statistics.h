#ifndef _statistics_h
#define _statistics_h

#include "lub/list.h"

void link_irqs_to_cpus(lub_list_t *cpus, lub_list_t *irqs);
void gather_statistics(lub_list_t *cpus, lub_list_t *irqs);
void show_statistics(lub_list_t *cpus, int verbose);

#endif
