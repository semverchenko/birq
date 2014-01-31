#ifndef _balance_h
#define _balance_h

#include "lub/list.h"
#include "irq.h"
#include "cpu.h"

int balance(lub_list_t *cpus, lub_list_t *balance_irqs);
//int move_irq_to_cpu(irq_t *irq, cpu_t *cpu);

#endif
