/* balance.c
 * Balance IRQs.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <limits.h>
#include <ctype.h>

#include "statistics.h"
#include "cpu.h"
#include "irq.h"

static int move_irq_to_cpu(irq_t *irq, cpu_t *cpu)
{
	if (!irq || !cpu)
		return -1;

	if (irq->cpu) {
		cpu_t *old_cpu = irq->cpu;
		lub_list_node_t *node;
		node = lub_list_search(old_cpu->irqs, irq);
		if (node) {
			lub_list_del(old_cpu->irqs, node);
			lub_list_node_free(node);
		}
	}
	lub_list_add(cpu->irqs, irq);
	irq->cpu = cpu;
	return 0;
}

static cpu_t *choose_cpu(lub_list_t *cpus, irq_t *irq)
{
	lub_list_node_t *iter;
	lub_list_t * min_cpus = NULL;
	float min_load = 100.00;
	lub_list_node_t *node;
	cpu_t *cpu = NULL;

	for (iter = lub_list_iterator_init(cpus); iter;
		iter = lub_list_iterator_next(iter)) {
		cpu = (cpu_t *)lub_list_node__get_data(iter);
		if (!cpu_isset(cpu->id, irq->local_cpus))
			continue;
		if ((!min_cpus) || (cpu->load < min_load)) {
			min_load = cpu->load;
			if (!min_cpus)
				min_cpus = lub_list_new(cpu_list_compare_len);
			while ((node = lub_list__get_tail(min_cpus))) {
				lub_list_del(min_cpus, node);
				lub_list_node_free(node);
			}
			lub_list_add(min_cpus, cpu);
		}
		if (cpu->load == min_load)
			lub_list_add(min_cpus, cpu);
	}
	if (!min_cpus)
		return NULL;
	node = lub_list__get_head(min_cpus);
	cpu = (cpu_t *)lub_list_node__get_data(node);
	while ((node = lub_list__get_tail(min_cpus))) {
		lub_list_del(min_cpus, node);
		lub_list_node_free(node);
	}
	lub_list_free(min_cpus);

	return cpu;
}

int balance(lub_list_t *cpus, lub_list_t *balance_irqs)
{
	lub_list_node_t *iter;

	for (iter = lub_list_iterator_init(balance_irqs); iter;
		iter = lub_list_iterator_next(iter)) {
		irq_t *irq;
		cpu_t *cpu;
		irq = (irq_t *)lub_list_node__get_data(iter);
		cpu = choose_cpu(cpus, irq);
		if (cpu) {
			move_irq_to_cpu(irq, cpu);
			printf("Move IRQ %u to CPU%u\n", irq->irq, cpu->id);
		}
	}
	return 0;
}

int apply_affinity(lub_list_t *balance_irqs)
{
	lub_list_node_t *iter;

	for (iter = lub_list_iterator_init(balance_irqs); iter;
		iter = lub_list_iterator_next(iter)) {
		irq_t *irq;
		irq = (irq_t *)lub_list_node__get_data(iter);
		if (!irq->cpu)
			continue;
		irq_set_affinity(irq, irq->cpu->cpumask);
	}
	return 0;
}
