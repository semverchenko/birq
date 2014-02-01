#ifndef _balance_h
#define _balance_h

#include "lub/list.h"
#include "irq.h"
#include "cpu.h"

int balance(lub_list_t *cpus, lub_list_t *balance_irqs, float threshold);
int apply_affinity(lub_list_t *balance_irqs);
int choose_irqs_to_move(lub_list_t *cpus, lub_list_t *balance_irqs, float threshold);

#endif
