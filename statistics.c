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

#define STR(str) ( str ? str : "" )

void parse_proc_stat(lub_list_t *cpus)
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

	file = fopen("/proc/stat", "r");
	if (!file) {
		fprintf(stderr, "Warning: Can't open /proc/stat. Balacing is broken.\n");
		return;
	}

	/* first line is the header we don't need; nuke it */
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

		if (cpu->old_load_all == 0) {
			cpu->load = 0;
		} else {
			float d_all = (float)(load_all - cpu->old_load_all);
			float d_irq = (float)(load_irq - cpu->old_load_irq);
			cpu->load = d_irq * 100 / d_all;
		}

		cpu->old_load_all = load_all;
		cpu->old_load_irq = load_irq;

printf("CPU %u %.2f\%\n", cpunr, cpu->load);


		/*
 		 * For each cpu add the irq and softirq load and propagate that
 		 * all the way up the device tree
 		 */
//		if (cycle_count) {
//			cpu->load = (irq_load + softirq_load) - (cpu->last_load);
			/*
			 * the [soft]irq_load values are in jiffies, with
			 * HZ jiffies per second.  Convert the load to nanoseconds
			 * to get a better integer resolution of nanoseconds per
			 * interrupt.
			 */
//			cpu->load *= NSEC_PER_SEC/HZ;
//		}
//		cpu->last_load = (irq_load + softirq_load);
	}

	fclose(file);
	free(line);
//	if (cpucount != get_cpu_count()) {
//		fprintf(stderr, "Warning: Can't collect load info for all cpus, balancing is broken\n");
//		return;
//	}

	/*
 	 * Reset the load values for all objects above cpus
 	 */
//	for_each_object(cache_domains, reset_load, NULL);

	/*
 	 * Now that we have load for each cpu attribute a fair share of the load
 	 * to each irq on that cpu
 	 */
//	for_each_object(cpus, compute_irq_branch_load_share, NULL);
//	for_each_object(cache_domains, compute_irq_branch_load_share, NULL);
//	for_each_object(packages, compute_irq_branch_load_share, NULL);
//	for_each_object(numa_nodes, compute_irq_branch_load_share, NULL);

}
