#ifndef _balance_h
#define _balance_h

#include "lub/list.h"
#include "irq.h"
#include "cpu.h"

typedef enum {
	BIRQ_CHOOSE_MAX,
	BIRQ_CHOOSE_MIN,
	BIRQ_CHOOSE_RND
} birq_choose_strategy_e;

int remove_irq_from_cpu(irq_t *irq, cpu_t *cpu);
int move_irq_to_cpu(irq_t *irq, cpu_t *cpu);
int balance(lub_list_t *cpus, lub_list_t *balance_irqs, float threshold);
int apply_affinity(lub_list_t *balance_irqs);
int choose_irqs_to_move(lub_list_t *cpus, lub_list_t *balance_irqs,
	float threshold, birq_choose_strategy_e strategy);

#endif
