#ifndef _numa_h
#define _numa_h

#include "lub/list.h"
#include "cpumask.h"

struct numa_s {
	unsigned int id; /* NUMA ID */
	cpumask_t cpumap;
};
typedef struct numa_s numa_t;

#define NR_NUMA_NODES 256
/* System NUMA info */
#define SYSFS_NUMA_PATH "/sys/devices/system/node"

int numa_list_compare(const void *first, const void *second);
int numa_list_free(lub_list_t *numas);
int scan_numas(lub_list_t *numas);
int show_numas(lub_list_t *numas);
numa_t * numa_list_search(lub_list_t *numas, unsigned int id);

#endif
