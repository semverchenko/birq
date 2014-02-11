#ifndef _cpu_h
#define _cpu_h

#include "lub/list.h"
#include "cpumask.h"

struct cpu_s {
	unsigned int id; /* Logical processor ID */
	unsigned int package_id;
	unsigned int core_id;
	cpumask_t cpumask; /* Mask with one bit set - current CPU. */
	unsigned long long old_load_all; /* Previous whole load from /proc/stat */
	unsigned long long old_load_irq; /* Previous IRQ, softIRQ load */
	float old_load; /* Previous CPU load in percents. */
	float load; /* Current CPU load in percents. */
	lub_list_t *irqs; /* List of IRQs belong to this CPU. */
};
typedef struct cpu_s cpu_t;

/* System CPU info */
#define SYSFS_CPU_PATH "/sys/devices/system/cpu"

/* CPU IDs compare function */
int cpu_list_compare(const void *first, const void *second);
int cpu_list_compare_len(const void *first, const void *second);

/* CPU list functions */
int cpu_list_free(lub_list_t *cpus);
int scan_cpus(lub_list_t *cpus, int ht);
int show_cpus(lub_list_t *cpus);
cpu_t * cpu_list_search(lub_list_t *cpus, unsigned int id);

#endif
