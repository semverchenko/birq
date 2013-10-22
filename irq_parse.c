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

int irqs_populate(lub_list_t *irqs)
{
	DIR *dir;
	struct dirent *dent;
	char path[PATH_MAX];

	/* Now we can parse PCI devices only */
	dir = opendir(SYSFS_PCI_PATH);
	if (!dir)
		return -1;
	while((dent = readdir(dir))) {
		if (!strcmp(dent->d_name, ".") ||
			!strcmp(dent->d_name, ".."))
			continue;
		printf("entry: %s\n", dent->d_name);
	}

	return 0;
}

