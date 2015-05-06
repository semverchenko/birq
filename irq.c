/* irq.c
 * Parse IRQ-related files.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <limits.h>
#include <ctype.h>

#include "lub/list.h"
#include "irq.h"
#include "pxm.h"

#define STR(str) ( str ? str : "" )

int irq_list_compare(const void *first, const void *second)
{
	const irq_t *f = (const irq_t *)first;
	const irq_t *s = (const irq_t *)second;
	return (f->irq - s->irq);
}

static irq_t * irq_new(int num)
{
	irq_t *new;

	if (!(new = malloc(sizeof(*new))))
		return NULL;
	new->irq = num;
	new->type = NULL;
	new->desc = NULL;
	new->refresh = 1;
	new->old_intr = 0;
	new->intr = 0;
	new->cpu = NULL;
	new->weight = 0;
	cpus_setall(new->local_cpus);
	cpus_clear(new->affinity);
	new->blacklisted = 0;

	return new;
}

static void irq_free(irq_t *irq)
{
	free(irq->type);
	free(irq->desc);
	free(irq);
}

irq_t * irq_list_search(lub_list_t *irqs, unsigned int num)
{
	lub_list_node_t *node;
	irq_t search;

	search.irq = num;
	node = lub_list_search(irqs, &search);
	if (!node)
		return NULL;
	return (irq_t *)lub_list_node__get_data(node);
}

static irq_t * irq_list_add(lub_list_t *irqs, unsigned int num)
{
	lub_list_node_t *node;
	irq_t *new;
	irq_t search;

	search.irq = num;
	node = lub_list_search(irqs, &search);
	if (node) /* IRQ already exists. May be renew some fields later */
		return (irq_t *)lub_list_node__get_data(node);
	if (!(new = irq_new(num)))
		return NULL;
	lub_list_add(irqs, new);

	return new;
}

int irq_list_free(lub_list_t *irqs)
{
	lub_list_node_t *iter;
	while ((iter = lub_list__get_head(irqs))) {
		irq_t *irq;
		irq = (irq_t *)lub_list_node__get_data(iter);
		irq_free(irq);
		lub_list_del(irqs, iter);
		lub_list_node_free(iter);
	}
	lub_list_free(irqs);
	return 0;
}

/* Show IRQ information */
static void irq_show(irq_t *irq)
{
	char buf[NR_CPUS + 1];

	if (cpus_full(irq->local_cpus))
		snprintf(buf, sizeof(buf), "*");
	else
		cpumask_scnprintf(buf, sizeof(buf), irq->local_cpus);
	buf[sizeof(buf) - 1] = '\0';
	printf("IRQ %3d %s [%s] %s\n", irq->irq, buf, STR(irq->type), STR(irq->desc));
}

/* Show IRQ list */
int irq_list_show(lub_list_t *irqs)
{
	lub_list_node_t *iter;
	for (iter = lub_list_iterator_init(irqs); iter;
		iter = lub_list_iterator_next(iter)) {
		irq_t *irq;
		irq = (irq_t *)lub_list_node__get_data(iter);
		irq_show(irq);
	}
	return 0;
}

static int parse_local_cpus(lub_list_t *irqs, const char *sysfs_path,
	unsigned int num, lub_list_t *pxms)
{
	char path[PATH_MAX];
	FILE *fd;
	char *str = NULL;
	size_t sz;
	cpumask_t local_cpus;
	irq_t *irq = NULL;
	cpumask_t cpumask;

	irq = irq_list_search(irqs, num);
	if (!irq)
		return -1;

	/* Find proximity in config file. */
	if (!pxm_search(pxms, sysfs_path, &cpumask)) {
		irq->local_cpus = cpumask;
		return 0;
	}

	snprintf(path, sizeof(path),
		"%s/%s/local_cpus", SYSFS_PCI_PATH, sysfs_path);
	path[sizeof(path) - 1] = '\0';
	if (!(fd = fopen(path, "r")))
		return -1;
	if (getline(&str, &sz, fd) < 0) {
		fclose(fd);
		return -1;
	}
	fclose(fd);
	cpumask_parse_user(str, strlen(str), local_cpus);
	cpus_and(irq->local_cpus, irq->local_cpus, local_cpus);
	free(str);

	return 0;
}

static int parse_sysfs(lub_list_t *irqs, lub_list_t *pxms)
{
	DIR *dir;
	DIR *msi;
	struct dirent *dent;
	struct dirent *ment;
	FILE *fd;
	char path[PATH_MAX];
	int num;

	/* Now we can parse PCI devices only */
	/* Get info from /sys/bus/pci/devices */
	dir = opendir(SYSFS_PCI_PATH);
	if (!dir)
		return -1;
	while((dent = readdir(dir))) {
		if (!strcmp(dent->d_name, ".") ||
			!strcmp(dent->d_name, ".."))
			continue;

		/* Search for MSI IRQs. Since linux-3.2 */
		snprintf(path, sizeof(path),
			"%s/%s/msi_irqs", SYSFS_PCI_PATH, dent->d_name);
		path[sizeof(path) - 1] = '\0';
		if ((msi = opendir(path))) {
			while((ment = readdir(msi))) {
				if (!strcmp(ment->d_name, ".") ||
					!strcmp(ment->d_name, ".."))
					continue;
				num = strtol(ment->d_name, NULL, 10);
				if (!num)
					continue;
				parse_local_cpus(irqs, dent->d_name, num, pxms);
			}
			closedir(msi);
			continue;
		}

		/* Try to get IRQ number from irq file */
		snprintf(path, sizeof(path),
			"%s/%s/irq", SYSFS_PCI_PATH, dent->d_name);
		path[sizeof(path) - 1] = '\0';
		if (!(fd = fopen(path, "r")))
			continue;
		if (fscanf(fd, "%d", &num) < 0) {
			fclose(fd);
			continue;
		}
		fclose(fd);
		if (!num)
			continue;

		parse_local_cpus(irqs, dent->d_name, num, pxms);
	}
	closedir(dir);

	return 0;
}

int irq_get_affinity(irq_t *irq)
{
	char path[PATH_MAX];
	FILE *fd;
	char *str = NULL;
	size_t sz;

	if (!irq)
		return -1;

	snprintf(path, sizeof(path),
		"%s/%u/smp_affinity", PROC_IRQ, irq->irq);
	path[sizeof(path) - 1] = '\0';
	if (!(fd = fopen(path, "r")))
		return -1;
	if (getline(&str, &sz, fd) < 0) {
		fclose(fd);
		return -1;
	}
	fclose(fd);
	cpumask_parse_user(str, strlen(str), irq->affinity);
	free(str);

	return 0;
}

/* Parse /proc/interrupts to get actual IRQ list */
int scan_irqs(lub_list_t *irqs, lub_list_t *balance_irqs, lub_list_t *pxms)
{
	FILE *fd;
	unsigned int num;
	char *str = NULL;
	size_t sz;
	irq_t *irq;
	lub_list_node_t *iter;

	if (!(fd = fopen(PROC_INTERRUPTS, "r")))
		return -1;
	while(getline(&str, &sz, fd) >= 0) {
		char *endptr, *tok;
		int new = 0;
		num = strtoul(str, &endptr, 10);
		if (endptr == str)
			continue;

		/* Search for IRQ within list of known IRQs */
		if (!(irq = irq_list_search(irqs, num))) {
			new = 1;
			irq = irq_list_add(irqs, num);
		}

		/* Set refresh flag because IRQ was found.
		 * It's used to find out disappeared IRQs.
		 */
		irq->refresh = 1;

		/* Doesn't refresh info for blacklisted IRQs */
		if (irq->blacklisted)
			continue;
	
		/* Find IRQ type - first non-digital and non-space */
		while (*endptr && !isalpha(*endptr))
			endptr++;
		tok = endptr; /* It will be IRQ type */
		while (*endptr && !isblank(*endptr))
			endptr++;
		free(irq->type);
		irq->type = strndup(tok, endptr - tok);

		/* Find IRQ devices list */
		while (*endptr && !isalpha(*endptr))
			endptr++;
		tok = endptr; /* It will be device list */
		while (*endptr && !iscntrl(*endptr))
			endptr++;
		free(irq->desc);
		irq->desc = strndup(tok, endptr - tok);

		/* Always get current smp affinity. It's necessary due to
		 * problems with arch/driver. The affinity can be old (didn't
		 * switched to new state).
		 */
		irq_get_affinity(irq);
		/* If affinity uses more than one CPU then consider IRQ as new one.
		 * It's not normal state for really non-new IRQs.
		 */
		if (cpus_weight(irq->affinity) > 1)
			new = 1;

		/* Add new IRQs to list of IRQs to balance. */
		if (new) {
			/* By default all CPUs are local for IRQ. Real local
			 * CPUs will be find while sysfs scan.
			 */
			cpus_setall(irq->local_cpus);
			/* Don't consider old affinity for new IRQs */
			cpus_setall(irq->affinity);
			lub_list_add(balance_irqs, irq);
			printf("Add IRQ %3d %s\n", irq->irq, STR(irq->desc));
		}
	}
	free(str);
	fclose(fd);

	/* Remove disappeared IRQs */
	iter = lub_list_iterator_init(irqs);
	while(iter) {
		irq_t *irq;
		lub_list_node_t *old_iter;
		irq = (irq_t *)lub_list_node__get_data(iter);
		old_iter = iter;
		iter = lub_list_iterator_next(iter);
		if (!irq->refresh) {
			lub_list_del(irqs, old_iter);
			printf("Remove IRQ %3d %s\n", irq->irq, STR(irq->desc));
			irq_free(irq);
		} else {
			/* Drop refresh flag for next iteration */
			irq->refresh = 0;
		}
	}

	/* No new IRQs were found. It doesn't need to scan sysfs. */
	if (lub_list_len(balance_irqs) == 0)
		return 0;
	/* Add IRQ info from sysfs */
	parse_sysfs(irqs, pxms);

	return 0;
}
