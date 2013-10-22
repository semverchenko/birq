#ifndef _irq_h
#define _irq_h

struct irq_s {
	int irq;
	char *desc; /* IRQ text description */
};
typedef struct irq_s irq_t;

#define SYSFS_PCI_PATH "/sys/bus/pci/devices"

/* Compare function for global IRQ list */
int irq_list_compare(const void *first, const void *second);

/* IRQ list functions */
int irq_list_populate(lub_list_t *irqs);

#endif
