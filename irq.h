#ifndef _irq_h
#define _irq_h

#include "cpumask.h"
#include "cpu.h"

struct irq_s {
	unsigned int irq; /* IRQ's ID */
	char *type; /* IRQ type from /proc/interrupts like PCI-MSI-edge */
	char *desc; /* IRQ text description - device list */
	int refresh; /* Refresh flag. It !=0 if irq was found while populate */
	cpumask_t local_cpus; /* Local CPUs for this IRQs */
	cpumask_t affinity; /* Real current affinity form /proc/irq/.../smp_affinity */
	unsigned long long intr; /* Current number of interrupts */
	unsigned long long old_intr; /* Previous total number of interrupts. */
	cpu_t *cpu; /* Current IRQ affinity. Reference to correspondent CPU */
	int weight; /* Flag to don't move current IRQ anyway */
	int blacklisted; /* IRQ can be blacklisted when can't change affinity */
};
typedef struct irq_s irq_t;

#define SYSFS_PCI_PATH "/sys/bus/pci/devices"
#define PROC_INTERRUPTS "/proc/interrupts"
#define PROC_IRQ "/proc/irq"

/* Compare function for global IRQ list */
int irq_list_compare(const void *first, const void *second);

/* IRQ list functions */
int scan_irqs(lub_list_t *irqs, lub_list_t *balance_irqs, lub_list_t *pxms);
int irq_list_free(lub_list_t *irqs);
int irq_list_show(lub_list_t *irqs);
irq_t * irq_list_search(lub_list_t *irqs, unsigned int num);
int irq_get_affinity(irq_t *irq);

#endif
