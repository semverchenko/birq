/* stat_parse.c
 * Parse statistics files.
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
#include "balance.h"

/* The setting of smp affinity is not reliable due to problems with some
 * APIC hw/driver. So we need to relink IRQs to CPUs on each iteration.
 * The linkage is based on current smp affinity value.
 */
void link_irqs_to_cpus(lub_list_t *cpus, lub_list_t *irqs)
{
	lub_list_node_t *iter;

	/* Clear all CPU's irq lists. These lists are probably out of date. */
	for (iter = lub_list_iterator_init(cpus); iter;
		iter = lub_list_iterator_next(iter)) {
		cpu_t *cpu = (cpu_t *)lub_list_node__get_data(iter);
		lub_list_node_t *node;
		while ((node = lub_list__get_tail(cpu->irqs))) {
			lub_list_del(cpu->irqs, node);
			lub_list_node_free(node);
		}
	}

	/* Iterate through IRQ list */
	for (iter = lub_list_iterator_init(irqs); iter;
		iter = lub_list_iterator_next(iter)) {
		irq_t *irq = (irq_t *)lub_list_node__get_data(iter);
		int cpu_num;
		cpu_t *cpu;

		/* Ignore blacklisted IRQs */
		if (irq->blacklisted)
			continue;
		/* Ignore IRQs with multi-affinity. */
		if (cpus_weight(irq->affinity) > 1)
			continue;

		cpu_num = first_cpu(irq->affinity);
		if (NR_CPUS == cpu_num) /* Something went wrong. No bits set. */
			continue;
		if (!(cpu = cpu_list_search(cpus, cpu_num)))
			continue;
		move_irq_to_cpu(irq, cpu);
	}
}

/* Gather load statistics for CPUs and number of interrupts
 * for current iteration.
 */
void gather_statistics(lub_list_t *cpus, lub_list_t *irqs)
{
	FILE *file;
	char *line = NULL;
	size_t size = 0;
	int cpunr, rc, cpucount;
	unsigned long long l_user;
	unsigned long long l_nice;
	unsigned long long l_system;
	unsigned long long l_idle;
	unsigned long long l_iowait;
	unsigned long long l_irq;
	unsigned long long l_softirq;
	unsigned long long l_steal;
	unsigned long long l_guest;
	unsigned long long l_guest_nice;
	unsigned long long load_irq, load_all;
	char *intr_str;
	char *saveptr = NULL;
	unsigned int inum = 0;

	file = fopen("/proc/stat", "r");
	if (!file) {
		fprintf(stderr, "Warning: Can't open /proc/stat. Balacing is broken.\n");
		return;
	}

	/* Get statistics for CPUs */
	/* First line is the header. */
	if (getline(&line, &size, file) == 0) {
		free(line);
		fprintf(stderr, "Warning: Can't read /proc/stat. Balancing is broken.\n");
		fclose(file);
		return;
	}

	cpucount = 0;
	while (!feof(file)) {
		cpu_t *cpu;
		if (getline(&line, &size, file)==0)
			break;
		if (!strstr(line, "cpu"))
			break;
		cpunr = strtoul(&line[3], NULL, 10);

		cpu = cpu_list_search(cpus, cpunr);
		if (!cpu)
			continue;

		rc = sscanf(line, "%*s %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
			&l_user, &l_nice, &l_system, &l_idle, &l_iowait,
			&l_irq, &l_softirq, &l_steal, &l_guest, &l_guest_nice);
		if (rc < 2)
			break;
		cpucount++;

		load_all = l_user + l_nice + l_system + l_idle + l_iowait +
			l_irq + l_softirq + l_steal + l_guest + l_guest_nice;
		load_irq = l_irq + l_softirq;

		cpu->old_load = cpu->load;
		if (cpu->old_load_all == 0) {
			/* When old_load_all = 0 - it's first iteration */
			cpu->load = 0;
		} else {
			float d_all = (float)(load_all - cpu->old_load_all);
			float d_irq = (float)(load_irq - cpu->old_load_irq);
			cpu->load = d_irq * 100 / d_all;
		}

		cpu->old_load_all = load_all;
		cpu->old_load_irq = load_irq;
	}

	/* Parse "intr" line. Get number of interrupts. */
	strtok_r(line, " ", &saveptr); /* String "intr" */
	strtok_r(NULL, " ", &saveptr); /* Total number of interrupts */
	for (intr_str = strtok_r(NULL, " ", &saveptr);
		intr_str; intr_str = strtok_r(NULL, " ", &saveptr)) {
		unsigned long long intr = 0;
		char *endptr;
		irq_t *irq;
		
		irq = irq_list_search(irqs, inum);
		inum++;
		if (!irq)
			continue;
		intr = strtoull(intr_str, &endptr, 10);
		if (endptr == intr_str)
			intr = 0;
		if (irq->old_intr == 0)
			irq->intr = 0;
		else
			irq->intr = intr - irq->old_intr;
		irq->old_intr = intr;
	}

	fclose(file);
	free(line);
}

void show_statistics(lub_list_t *cpus, int verbose)
{
	lub_list_node_t *iter;

	for (iter = lub_list_iterator_init(cpus); iter;
		iter = lub_list_iterator_next(iter)) {
		cpu_t *cpu;
		lub_list_node_t *irq_iter;

		cpu = (cpu_t *)lub_list_node__get_data(iter);
		printf("CPU%u package %u, core %u, irqs %d, old %.2f%%, load %.2f%%\n",
			cpu->id, cpu->package_id, cpu->core_id,
			lub_list_len(cpu->irqs), cpu->old_load, cpu->load);

		if (!verbose)
			continue;
		for (irq_iter = lub_list_iterator_init(cpu->irqs); irq_iter;
		irq_iter = lub_list_iterator_next(irq_iter)) {
			char buf[NR_CPUS + 1];
			irq_t *irq = (irq_t *)lub_list_node__get_data(irq_iter);
			if (cpus_full(irq->affinity))
				snprintf(buf, sizeof(buf), "*");
			else
				cpumask_scnprintf(buf, sizeof(buf), irq->affinity);
			buf[sizeof(buf) - 1] = '\0';
			printf("    IRQ %3u, [%s], weight %d, intr %llu, %s\n", irq->irq, buf, irq->weight, irq->intr, irq->desc);
		}
	}
}
