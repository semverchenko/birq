#ifndef _cpu_h
#define _cpu_h

#include "lub/list.h"
#include "cpumask.h"

struct cpu_s {
	unsigned int id; /* Logical processor ID */
	unsigned int package_id;
	unsigned int core_id;
	cpumask_t cpumask;
	unsigned long long old_load_all;
	unsigned long long old_load_irq;
	float load;
	lub_list_t *irqs;
};
typedef struct cpu_s cpu_t;

#define SYSFS_CPU_PATH "/sys/devices/system/cpu"

/* CPU IDs compare function */
int cpu_list_compare(const void *first, const void *second);
int cpu_list_compare_len(const void *first, const void *second);

/* CPU list functions */
int cpu_list_populate(lub_list_t *cpus);
int cpu_list_free(lub_list_t *cpus);
int scan_cpus(lub_list_t *cpus);
int show_cpus(lub_list_t *cpus);
cpu_t * cpu_list_search(lub_list_t *cpus, unsigned int id);

#endif
