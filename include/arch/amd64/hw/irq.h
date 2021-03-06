#pragma once
#include "sys/types.h"

#define IRQ_LEG_KEYBOARD        1
#define IRQ_LEG_COM1_3          4
#define IRQ_LEG_MOUSE           12

#define IRQ_HANDLED             ((uint32_t) 0)
#define IRQ_UNHANDLED           ((uint32_t) -1)

typedef uint32_t (*irq_handler_func_t) (void *);
struct pci_device;

struct irq_handler {
    irq_handler_func_t func;
    void *ctx;
};

int irq_add_handler(uint8_t gsi, irq_handler_func_t handler, void *ctx);
int irq_add_leg_handler(uint8_t leg_irq, irq_handler_func_t handler, void *ctx);
int irq_add_msi_handler(irq_handler_func_t handler, void *ctx, uint8_t *vector);
int irq_add_pci_handler(struct pci_device *dev, uint8_t pin, irq_handler_func_t handler, void *ctx);

int irq_has_handler(uint8_t gsi);

void irq_enable_ioapic_mode(void);
void irq_init(int cpu);
