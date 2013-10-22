#ifndef _irq_h
#define _irq_h

struct irq_t {
	int irq;
	char *desc; /* IRQ text description */
};

#define SYSFS_PCI_PATH "/sys/bus/pci/devices"

/* IRQ list functions */
int irqs_populate(lub_list_t *irqs);

#endif
