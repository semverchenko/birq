/* pxm.c
 * Parse manual proximity config.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <dirent.h>
#include <limits.h>
#include <ctype.h>

#include "lub/list.h"
#include "numa.h"
#include "pxm.h"

static pxm_t * pxm_new(const char *addr)
{
	pxm_t *new;

	if (!(new = malloc(sizeof(*new))))
		return NULL;
	new->addr = strdup(addr);
	cpus_clear(new->cpumask);

	return new;
}

static void pxm_free(pxm_t *pxm)
{
	if (!pxm)
		return;
	if (pxm->addr)
		free(pxm->addr);
	free(pxm);
}

static pxm_t * pxm_list_add(lub_list_t *pxms, pxm_t *pxm)
{
	lub_list_add(pxms, pxm);
	return pxm;
}

int pxm_list_free(lub_list_t *pxms)
{
	lub_list_node_t *iter;
	while ((iter = lub_list__get_head(pxms))) {
		pxm_t *pxm;
		pxm = (pxm_t *)lub_list_node__get_data(iter);
		pxm_free(pxm);
		lub_list_del(pxms, iter);
		lub_list_node_free(iter);
	}
	lub_list_free(pxms);
	return 0;
}

/* Show proximity information */
static void show_pxm_info(pxm_t *pxm)
{
	char buf[NR_CPUS + 1];
	if (cpus_full(pxm->cpumask))
		snprintf(buf, sizeof(buf), "*");
	else
		cpumask_scnprintf(buf, sizeof(buf), pxm->cpumask);
	buf[sizeof(buf) - 1] = '\0';
	printf("PXM: %s cpumask %s\n", pxm->addr, buf);
}

/* Show PXM list */
int show_pxms(lub_list_t *pxms)
{
	lub_list_node_t *iter;
	for (iter = lub_list_iterator_init(pxms); iter;
		iter = lub_list_iterator_next(iter)) {
		pxm_t *pxm;
		pxm = (pxm_t *)lub_list_node__get_data(iter);
		show_pxm_info(pxm);
	}
	return 0;
}

int pxm_search(lub_list_t *pxms, const char *addr, cpumask_t *cpumask)
{
	lub_list_node_t *iter;
	size_t maxaddr = 0;

	for (iter = lub_list_iterator_init(pxms); iter;
		iter = lub_list_iterator_next(iter)) {
		pxm_t *pxm;
		char *tmp = NULL;
		size_t len;

		pxm = (pxm_t *)lub_list_node__get_data(iter);
		tmp = strstr(addr, pxm->addr);
		if (!tmp)
			continue;
		len = strlen(pxm->addr);
		if (maxaddr >= len)
			continue;
		maxaddr = len;
		*cpumask = pxm->cpumask;
	}

	if (!maxaddr)
		return -1;
	return 0;
}

int parse_pxm_config(const char *fname, lub_list_t *pxms, lub_list_t *numas)
{
	FILE *file;
	char *line = NULL;
	size_t size = 0;
	char *saveptr = NULL;
	unsigned int ln = 0; /* Line number */
	pxm_t *pxm;

	if (!fname)
		return -1;
	file = fopen(fname, "r");
	if (!file)
		return -1;

	while (!feof(file)) {
		char *str = NULL;
		char *pci_addr = NULL;
		char *pxm_cmd = NULL;
		char *pxm_pxm = NULL;
		cpumask_t cpumask;

		ln++; /* Next line */
		if (getline(&line, &size, file) <= 0)
			continue;
		/* Find comments */
		str = strchr(line, '#');
		if (str)
			*str = '\0';
		/* Find \n */
		str = strchr(line, '\n');
		if (str)
			*str = '\0';
		/* Get PCI address */
		pci_addr = strtok_r(line, " ", &saveptr);
		if (!pci_addr)
			continue;
		/* Get PXM command */
		pxm_cmd = strtok_r(NULL, " ", &saveptr);
		if (!pxm_cmd) {
			fprintf(stderr, "Warning: Illegal line %u in %s\n",
				ln, fname);
			continue;
		}
		/* Get PXM string (mask or node) */
		pxm_pxm = strtok_r(NULL, " ", &saveptr);
		if (!pxm_pxm) {
			fprintf(stderr, "Warning: Illegal line %u in %s\n",
				ln, fname);
			continue;
		}

		if (!strcasecmp(pxm_cmd, "cpumask")) {
			cpumask_parse_user(pxm_pxm, strlen(pxm_pxm),
				cpumask);
		} else if (!strcasecmp(pxm_cmd, "node")) {
			char *endptr;
			int noden = -1;
			noden = strtol(pxm_pxm, &endptr, 10);
			if (endptr == pxm_pxm) {
				fprintf(stderr, "Warning: Wrong NUMA node in "
					"line %u in %s\n", ln, fname);
				continue;
			}
			if (noden == -1) /* Non-NUMA = all CPUs */
				cpus_setall(cpumask);
			else {
				numa_t *numa;
				numa = numa_list_search(numas, noden);
				if (!numa) {
					fprintf(stderr, "Warning: Wrong NUMA node. Line %u in %s\n",
						ln, fname);
					continue;
				}
				cpus_clear(cpumask);
				cpus_or(cpumask, cpumask, numa->cpumap);
			}
		} else {
			fprintf(stderr, "Warning: Illegal command %u in %s\n",
				ln, fname);
			continue;
		}

		/* Add new entry to PXM list */
		pxm = pxm_new(pci_addr);
		cpus_clear(pxm->cpumask);
		cpus_or(pxm->cpumask, pxm->cpumask, cpumask);
		pxm_list_add(pxms, pxm);
	}

	fclose(file);
	free(line);

	return 0;
}
