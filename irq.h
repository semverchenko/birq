#ifndef _irq_h
#define _irq_h

#include "cpumask.h"

struct irq_s {
	unsigned int irq;
	char *type; /* IRQ type from /proc/interrupts like PCI-MSI-edge */
	char *desc; /* IRQ text description - device list */
	int refresh; /* Refresh flag. It !=0 if irq was found while populate */
	cpumask_t local_cpus;
};
typedef struct irq_s irq_t;

#define SYSFS_PCI_PATH "/sys/bus/pci/devices"
#define PROC_INTERRUPTS "/proc/interrupts"
#define PROC_IRQ "/proc/irq"

/* Compare function for global IRQ list */
int irq_list_compare(const void *first, const void *second);

/* IRQ list functions */
int irq_list_populate(lub_list_t *irqs);
int irq_list_free(lub_list_t *irqs);

#endif
