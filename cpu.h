#ifndef _cpu_h
#define _cpu_h

struct cpu_s {
	unsigned int id; /* Logical processor ID */
	unsigned int package_id;
	unsigned int core_id;
};
typedef struct cpu_s cpu_t;

#define SYSFS_CPU_PATH "/sys/devices/system/cpu"

/* CPU IDs compare function */
int cpu_list_compare(const void *first, const void *second);

/* CPU list functions */
int cpu_list_populate(lub_list_t *cpus);
int cpu_list_free(lub_list_t *cpus);
int scan_cpus(lub_list_t *cpus);
int show_cpus(lub_list_t *cpus);

#endif
