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

static irq_t * irqs_add(lub_list_t *irqs, int num)
{
	irq_t *new;

	if (!(new = irq_new(num)))
		return NULL;
	lub_list_add(irqs, new);

	return new;
}

static int irqs_populate_pci(lub_list_t *irqs)
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
				irqs_add(irqs, num);
				printf("MSI: %d\n", num);
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
		irqs_add(irqs, num);
		printf("IRQ: %d\n", num);
	}
	closedir(dir);

	return 0;
}

int irqs_populate(lub_list_t *irqs)
{
	irqs_populate_pci(irqs);
//	irqs_populate_proc(irqs);

	return 0;
}
