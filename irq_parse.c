/* irq_parse.c
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
	unsigned int num)
{
	char path[PATH_MAX];
	FILE *fd;
	char *str = NULL;
	size_t sz;
	irq_t *irq;

	irq = irq_list_search(irqs, num);
	if (!irq)
		return -1;

	sprintf(path, "%s/%s/local_cpus", SYSFS_PCI_PATH, sysfs_path);
	if (!(fd = fopen(path, "r")))
		return -1;
	if (getline(&str, &sz, fd) < 0) {
		fclose(fd);
		return -1;
	}
	fclose(fd);
	cpumask_parse_user(str, strlen(str), irq->local_cpus);
//	printf("%d %s %s\n", num, str, sysfs_path);
	free(str);

	return 0;
}

static int scan_sysfs(lub_list_t *irqs)
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
		sprintf(path, "%s/%s/msi_irqs", SYSFS_PCI_PATH, dent->d_name);
		if ((msi = opendir(path))) {
			while((ment = readdir(msi))) {
				if (!strcmp(ment->d_name, ".") ||
					!strcmp(ment->d_name, ".."))
					continue;
				num = strtol(ment->d_name, NULL, 10);
				if (!num)
					continue;
				parse_local_cpus(irqs, dent->d_name, num);
			}
			closedir(msi);
			continue;
		}

		/* Try to get IRQ number from irq file */
		sprintf(path, "%s/%s/irq", SYSFS_PCI_PATH, dent->d_name);
		if (!(fd = fopen(path, "r")))
			continue;
		if (fscanf(fd, "%d", &num) < 0) {
			fclose(fd);
			continue;
		}
		fclose(fd);
		if (!num)
			continue;
		parse_local_cpus(irqs, dent->d_name, num);
	}
	closedir(dir);

	return 0;
}

/* Parse /proc/interrupts to get actual IRQ list */
int irq_list_populate(lub_list_t *irqs, lub_list_t *balance_irqs)
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
		
		if (!(irq = irq_list_search(irqs, num))) {
			new = 1;
			irq = irq_list_add(irqs, num);
		}

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

		/* Set refresh flag because IRQ was found */
		irq->refresh = 1;

		/* By default all CPUs are local for IRQ */
		cpus_setall(irq->local_cpus);

		if (new) {
			lub_list_add(balance_irqs, irq);
			printf("Add IRQ %3d %s\n", irq->irq, STR(irq->desc));
		}
	}
	free(str);
	fclose(fd);

	/* Remove disapeared IRQs */
	iter = lub_list_iterator_init(irqs);
	while(iter) {
		irq_t *irq;
		lub_list_node_t *old_iter;
		irq = (irq_t *)lub_list_node__get_data(iter);
		old_iter = iter;
		iter = lub_list_iterator_next(iter);
		if (!irq->refresh) {
			lub_list_del(irqs, old_iter);
			irq_free(irq);
			printf("Remove IRQ %3d %s\n", irq->irq, STR(irq->desc));
		} else {
			/* Drop refresh flag for next iteration */
			irq->refresh = 0;
		}
	}

	/* Add IRQ info from sysfs */
	scan_sysfs(irqs);

	return 0;
}
