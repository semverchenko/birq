/* numa.c
 * Parse NUMA-related files.
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
#include "numa.h"

int numa_list_compare(const void *first, const void *second)
{
	const numa_t *f = (const numa_t *)first;
	const numa_t *s = (const numa_t *)second;
	return (f->id - s->id);
}

static numa_t * numa_new(unsigned int id)
{
	numa_t *new;

	if (!(new = malloc(sizeof(*new))))
		return NULL;
	new->id = id;
	cpus_setall(new->cpumap);

	return new;
}

static void numa_free(numa_t *numa)
{
	free(numa);
}

numa_t * numa_list_search(lub_list_t *numas, unsigned int id)
{
	lub_list_node_t *node;
	numa_t search;

	search.id = id;
	node = lub_list_search(numas, &search);
	if (!node)
		return NULL;
	return (numa_t *)lub_list_node__get_data(node);
}

static numa_t * numa_list_add(lub_list_t *numas, numa_t *numa)
{
	numa_t *old = numa_list_search(numas, numa->id);

	if (old) /* NUMA already exists. May be renew some fields later */
		return old;
	lub_list_add(numas, numa);

	return numa;
}

int numa_list_free(lub_list_t *numas)
{
	lub_list_node_t *iter;
	while ((iter = lub_list__get_head(numas))) {
		numa_t *numa;
		numa = (numa_t *)lub_list_node__get_data(iter);
		numa_free(numa);
		lub_list_del(numas, iter);
		lub_list_node_free(iter);
	}
	lub_list_free(numas);
	return 0;
}

/* Show NUMA information */
static void show_numa_info(numa_t *numa)
{
	char buf[NR_CPUS + 1];
	cpumask_scnprintf(buf, sizeof(buf), numa->cpumap);
	buf[sizeof(buf) - 1] = '\0';
	printf("NUMA node %d cpumap %s\n", numa->id, buf);
}

/* Show NUMA list */
int show_numas(lub_list_t *numas)
{
	lub_list_node_t *iter;
	for (iter = lub_list_iterator_init(numas); iter;
		iter = lub_list_iterator_next(iter)) {
		numa_t *numa;
		numa = (numa_t *)lub_list_node__get_data(iter);
		show_numa_info(numa);
	}
	return 0;
}

/* Search for NUMA nodes */
int scan_numas(lub_list_t *numas)
{
	FILE *fd;
	char path[PATH_MAX];
	unsigned int id;
	numa_t *numa;
	char *str = NULL;
	size_t sz;
	cpumask_t cpumap;

	for (id = 0; id < NR_NUMA_NODES; id++) {
		snprintf(path, sizeof(path),
			"%s/node%d", SYSFS_NUMA_PATH, id);
		path[sizeof(path) - 1] = '\0';
		if (access(path, F_OK))
			break;

		if (!(numa = numa_list_search(numas, id))) {
			numa = numa_new(id);
			numa_list_add(numas, numa);
		}

		/* Get NUMA node cpumap */
		snprintf(path, sizeof(path),
			"%s/node%d/cpumap", SYSFS_NUMA_PATH, id);
		path[sizeof(path) - 1] = '\0';
		if ((fd = fopen(path, "r"))) {
			if (getline(&str, &sz, fd) >= 0)
				cpumask_parse_user(str, strlen(str), cpumap);
			fclose(fd);
			cpus_and(numa->cpumap, numa->cpumap, cpumap);
		}
	}
	free(str);

	return 0;
}
