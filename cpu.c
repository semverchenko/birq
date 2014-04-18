/* cpu.c
 * Parse CPU-related files.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <limits.h>
#include <ctype.h>
#include <unistd.h>

#include "lub/list.h"
#include "cpumask.h"
#include "cpu.h"
#include "irq.h"

int cpu_list_compare(const void *first, const void *second)
{
	const cpu_t *f = (const cpu_t *)first;
	const cpu_t *s = (const cpu_t *)second;
	return (f->id - s->id);
}

int cpu_list_compare_len(const void *first, const void *second)
{
	const cpu_t *f = (const cpu_t *)first;
	const cpu_t *s = (const cpu_t *)second;
	return (lub_list_len(f->irqs) - lub_list_len(s->irqs));
}

static cpu_t * cpu_new(unsigned int id)
{
	cpu_t *new;

	if (!(new = malloc(sizeof(*new))))
		return NULL;
	new->id = id;
	new->old_load_all = 0;
	new->old_load_irq = 0;
	new->old_load = 0;
	new->load = 0;
	new->irqs = lub_list_new(irq_list_compare);
	cpus_clear(new->cpumask);
	cpu_set(new->id, new->cpumask);

	return new;
}

static void cpu_free(cpu_t *cpu)
{
	lub_list_node_t *node;

	while ((node = lub_list__get_tail(cpu->irqs))) {
		lub_list_del(cpu->irqs, node);
		lub_list_node_free(node);
	}
	lub_list_free(cpu->irqs);
	free(cpu);
}

/* Search for CPU with specified package and core IDs.
   The second CPU with the same IDs is a thread of Hyper Threading.
   We don't want to use HT for IRQ balancing. */
static cpu_t * cpu_list_search_ht(lub_list_t *cpus,
	unsigned int package_id, unsigned int core_id,
	cpumask_t *thread_siblings)
{
	lub_list_node_t *iter;

	/* Check if current CPU has thread siblings */
	/* The CPUs without thread siblings has no hyper
	   threading. For example some AMD processors has
	   two CPUs with the same package and core ids but
	   has no thread siblings. Don't consider such CPUs as
	   a hyper threading. */
	if (cpus_weight(*thread_siblings) < 2)
		return NULL;

	for (iter = lub_list_iterator_init(cpus); iter;
		iter = lub_list_iterator_next(iter)) {
		cpu_t *cpu;
		cpu = (cpu_t *)lub_list_node__get_data(iter);
		if (cpu->package_id != package_id)
			continue;
		if (cpu->core_id != core_id)
			continue;
		return cpu;
	}

	return NULL;
}

cpu_t * cpu_list_search(lub_list_t *cpus, unsigned int id)
{
	lub_list_node_t *node;
	cpu_t search;

	search.id = id;
	node = lub_list_search(cpus, &search);
	if (!node)
		return NULL;
	return (cpu_t *)lub_list_node__get_data(node);
}

static cpu_t * cpu_list_add(lub_list_t *cpus, cpu_t *cpu)
{
	cpu_t *old = cpu_list_search(cpus, cpu->id);

	if (old) /* CPU already exists. May be renew some fields later */
		return old;
	lub_list_add(cpus, cpu);

	return cpu;
}

int cpu_list_free(lub_list_t *cpus)
{
	lub_list_node_t *iter;
	while ((iter = lub_list__get_head(cpus))) {
		cpu_t *cpu;
		cpu = (cpu_t *)lub_list_node__get_data(iter);
		cpu_free(cpu);
		lub_list_del(cpus, iter);
		lub_list_node_free(iter);
	}
	lub_list_free(cpus);
	return 0;
}

/* Show CPU information */
static void show_cpu_info(cpu_t *cpu)
{
	char buf[NR_CPUS + 1];
	cpumask_scnprintf(buf, sizeof(buf), cpu->cpumask);
	buf[sizeof(buf) - 1] = '\0';
	printf("CPU %d package %d core %d mask %s\n", cpu->id, cpu->package_id, cpu->core_id, buf);
}

/* Show CPU list */
int show_cpus(lub_list_t *cpus)
{
	lub_list_node_t *iter;
	for (iter = lub_list_iterator_init(cpus); iter;
		iter = lub_list_iterator_next(iter)) {
		cpu_t *cpu;
		cpu = (cpu_t *)lub_list_node__get_data(iter);
		show_cpu_info(cpu);
	}
	return 0;
}

/* Search for CPUs */
int scan_cpus(lub_list_t *cpus, int ht)
{
	FILE *fd;
	char path[PATH_MAX];
	unsigned int id;
	unsigned int package_id;
	unsigned int core_id;
	cpu_t *new;
	char *str = NULL;
	size_t sz;
	cpumask_t thread_siblings;

	for (id = 0; id < NR_CPUS; id++) {
		snprintf(path, sizeof(path), "%s/cpu%d", SYSFS_CPU_PATH, id);
		path[sizeof(path) - 1] = '\0';
		if (access(path, F_OK))
			break;

		/* Try to get package_id */
		snprintf(path, sizeof(path),
			"%s/cpu%d/topology/physical_package_id",
			SYSFS_CPU_PATH, id);
		path[sizeof(path) - 1] = '\0';
		if (!(fd = fopen(path, "r")))
			continue;
		if (fscanf(fd, "%u", &package_id) < 0) {
			fclose(fd);
			continue;
		}
		fclose(fd);

		/* Try to get core_id */
		snprintf(path, sizeof(path), "%s/cpu%d/topology/core_id",
			SYSFS_CPU_PATH, id);
		path[sizeof(path) - 1] = '\0';
		if (!(fd = fopen(path, "r")))
			continue;
		if (fscanf(fd, "%u", &core_id) < 0) {
			fclose(fd);
			continue;
		}
		fclose(fd);

		/* Get thread siblings */
		cpus_clear(thread_siblings);
		cpu_set(id, thread_siblings);
		snprintf(path, sizeof(path), "%s/cpu%d/topology/thread_siblings",
			SYSFS_CPU_PATH, id);
		path[sizeof(path) - 1] = '\0';
		if ((fd = fopen(path, "r"))) {
			if (getline(&str, &sz, fd) >= 0)
				cpumask_parse_user(str, strlen(str), thread_siblings);
			fclose(fd);
		}

		/* Don't use second thread of Hyper Threading */
		if (!ht && cpu_list_search_ht(cpus, package_id, core_id,
			&thread_siblings))
			continue;

		new = cpu_new(id);
		new->package_id = package_id;
		new->core_id = core_id;
		cpu_list_add(cpus, new);
	}
	free(str);

	return 0;
}
