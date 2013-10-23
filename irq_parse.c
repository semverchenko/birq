/* irq_parse.c
 * Parse IRQ-related files.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <limits.h>

#include "lub/list.h"
#include "irq.h"

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
	new->desc = NULL;
	return new;
}

static void irq_free(irq_t *irq)
{
	if (irq->desc)
		free(irq->desc);
	free(irq);
}

static irq_t * irq_list_add(lub_list_t *irqs, int num)
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
	while (iter = lub_list__get_head(irqs)) {
		irq_t *irq;
		irq = (irq_t *)lub_list_node__get_data(iter);
		irq_free(irq);
		lub_list_del(irqs, iter);
		lub_list_node_free(iter);
	}
	lub_list_free(irqs);
	return 0;
}

static int irq_list_show(lub_list_t *irqs)
{
	lub_list_node_t *iter;
	for (iter = lub_list_iterator_init(irqs); iter;
		iter = lub_list_iterator_next(iter)) {
		irq_t *irq;
		irq = (irq_t *)lub_list_node__get_data(iter);
		printf("IRQ %3d %s\n", irq->irq, irq->desc ? irq->desc : "");
	}
	return 0;
}

static int irq_list_populate_pci(lub_list_t *irqs)
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

		/* Get enable flag */
		sprintf(path, "%s/%s/enable", SYSFS_PCI_PATH, dent->d_name);
		if ((fd = fopen(path, "r"))) {
			if (fscanf(fd, "%d", &num) > 0) {
				if (0 == num) {
					fclose(fd);
					continue;
				}
			}
			fclose(fd);
		}

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
				irq_list_add(irqs, num);
				printf("MSI: %3d %s\n", num, dent->d_name);
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
		irq_list_add(irqs, num);
		printf("IRQ: %3d %s\n", num, dent->d_name);
	}
	closedir(dir);

	irq_list_show(irqs);

	return 0;
}

int irq_list_populate(lub_list_t *irqs)
{
	irq_list_populate_pci(irqs);
//	irq_list_populate_proc(irqs);

	return 0;
}

