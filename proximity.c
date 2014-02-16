/* proximity.c
 * Parse manual proximity config.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <limits.h>
#include <ctype.h>

#include "lub/list.h"
#include "proximity.h"

#define STR(str) ( str ? str : "" )

int parse_pxm_config(const char *fname)
{
	FILE *file;
	char *line = NULL;
	size_t size = 0;
	char *saveptr;

	if (!fname)
		return -1;
	file = fopen(fname, "r");
	if (!file)
		return -1;

	while (!feof(file)) {
		if (getline(&line, &size, file) == 0)
			break;
printf("%s\n", line);
/*		if (!strstr(line, "cpu"))
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
			cpu->load = 0;
		} else {
			float d_all = (float)(load_all - cpu->old_load_all);
			float d_irq = (float)(load_irq - cpu->old_load_irq);
			cpu->load = d_irq * 100 / d_all;
		}

		cpu->old_load_all = load_all;
		cpu->old_load_irq = load_irq;
*/
	}

	/* Parse "intr" line. Get number of interrupts. */
#if 0
	strtok_r(line, " ", &saveptr);
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
#endif

	fclose(file);
	free(line);

	return 0;
}
