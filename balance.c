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

/* Drop the dont_move flag on all IRQs for specified CPU */
static int drop_dont_move_flag(cpu_t *cpu)
{
	lub_list_node_t *iter;

	if (!cpu)
		return -1;

	for (iter = lub_list_iterator_init(cpu->irqs); iter;
		iter = lub_list_iterator_next(iter)) {
		irq_t *irq;
		irq = (irq_t *)lub_list_node__get_data(iter);
		irq->dont_move = 0;
	}

	return 0;
}

/* Move IRQ to specified CPU. Remove IRQ from the IRQ list
   of old CPU. */
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
		drop_dont_move_flag(old_cpu);
	}
	drop_dont_move_flag(cpu);
	irq->cpu = cpu;
	lub_list_add(cpu->irqs, irq);

	return 0;
}

/* Search for the best CPU. Best CPU is a CPU with minimal load.
   If several CPUs have the same load then the best CPU is a CPU
   with minimal number of assigned IRQs */
static cpu_t *choose_cpu(lub_list_t *cpus, cpumask_t cpumask, float threshold)
{
	lub_list_node_t *iter;
	lub_list_t * min_cpus = NULL;
	float min_load = 100.00;
	lub_list_node_t *node;
	cpu_t *cpu = NULL;

	for (iter = lub_list_iterator_init(cpus); iter;
		iter = lub_list_iterator_next(iter)) {
		cpu = (cpu_t *)lub_list_node__get_data(iter);
		if (!cpu_isset(cpu->id, cpumask))
			continue;
		if (cpu->load >= threshold)
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

/* Find best CPUs for IRQs need to be balanced. */
int balance(lub_list_t *cpus, lub_list_t *balance_irqs, float threshold)
{
	lub_list_node_t *iter;

	for (iter = lub_list_iterator_init(balance_irqs); iter;
		iter = lub_list_iterator_next(iter)) {
		irq_t *irq;
		cpu_t *cpu;
		irq = (irq_t *)lub_list_node__get_data(iter);
		/* Try to find local CPU to move IRQ to.
		   The local CPU is CPU with native NUMA node. */
		cpu = choose_cpu(cpus, irq->local_cpus, threshold);
		/* If local CPU is not found then try to use
		   CPU from another NUMA node. It's better then
		   overloaded CPUs. */
		if (!cpu) {
			cpumask_t complement;
			cpus_complement(complement, irq->local_cpus);
			cpu = choose_cpu(cpus, complement, threshold);
		}
		if (cpu) {
			if (irq->cpu)
				printf("Move IRQ %u from CPU%u to CPU%u\n",
					irq->irq, irq->cpu->id, cpu->id);
			else
				printf("Move IRQ %u to CPU%u\n", irq->irq, cpu->id);
			move_irq_to_cpu(irq, cpu);
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

/* Search for the overloaded CPUs and then choose best IRQ for moving to
   another CPU. The best IRQ is IRQ with maximum number of interrupts.
   The IRQs with small number of interrupts have very low load or very
   high load (in a case of NAPI). */
int choose_irqs_to_move(lub_list_t *cpus, lub_list_t *balance_irqs, float threshold)
{
	lub_list_node_t *iter;
	cpu_t *overloaded_cpu = NULL;
	irq_t *irq_to_move = NULL;
	float max_load = 0.0;
	unsigned long long max_intr = 0;

	/* Search for the most overloaded CPU.
	   The load must be greater than threshold. */
	for (iter = lub_list_iterator_init(cpus); iter;
		iter = lub_list_iterator_next(iter)) {
		cpu_t *cpu = (cpu_t *)lub_list_node__get_data(iter);
		if (cpu->load < threshold)
			continue;
		if (cpu->load > max_load) {
			max_load = cpu->load;
			overloaded_cpu = cpu;
		}
	}
	/* Can't find overloaded CPUs */
	if (!overloaded_cpu)
		return 0;

	/* Search for the IRQ (owned by overloaded CPU) with
	   maximum number of interrupts. */
	if (lub_list_len(overloaded_cpu->irqs) <= 1)
		return 0;
	for (iter = lub_list_iterator_init(overloaded_cpu->irqs); iter;
		iter = lub_list_iterator_next(iter)) {
		irq_t *irq = (irq_t *)lub_list_node__get_data(iter);
		if (irq->dont_move)
			continue;
		if (irq->intr >= max_intr) {
			max_intr = irq->intr;
			irq_to_move = irq;
		}
	}

	if (irq_to_move) {
		/* Don't move this IRQ while next iteration. */
		irq_to_move->dont_move = 1;
		lub_list_add(balance_irqs, irq_to_move);
	}

	return 0;
}
