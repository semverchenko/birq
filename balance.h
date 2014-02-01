#ifndef _balance_h
#define _balance_h

#include "lub/list.h"
#include "irq.h"
#include "cpu.h"

int balance(lub_list_t *cpus, lub_list_t *balance_irqs);
int apply_affinity(lub_list_t *balance_irqs);

#endif
