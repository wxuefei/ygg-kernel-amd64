#pragma once
#include "arch/amd64/hw/irq.h"
#include "sys/types.h"

#define PCI_PORT_CONFIG_ADDR        0xCF8
#define PCI_PORT_CONFIG_DATA        0xCFC

#define PCI_CONFIG_ID               0x00
#define PCI_CONFIG_CMD              0x04
#define PCI_CONFIG_CLASS            0x08
#define PCI_CONFIG_INFO             0x0C
#define PCI_CONFIG_BAR(n)           (0x10 + (n) * 4)
#define PCI_CONFIG_SUBSYSTEM        0x2C
#define PCI_CONFIG_CAPABILITIES     0x34
#define PCI_CONFIG_IRQ              0x3C

#define PCI_CONFIG_BRIDGE           0x18

#define PCI_ID(vnd, dev)            (((uint32_t) (vnd)) | ((uint32_t) (dev) << 16))

struct pci_device;
typedef void (*pci_driver_func_t)(struct pci_device *dev);

void pci_get_device_address(const struct pci_device *dev, uint8_t *b, uint8_t *d, uint8_t *f);

void pci_init(void);
void pci_add_root_bus(uint8_t n);

uint32_t pci_config_read_dword_legacy(uint8_t bus, uint8_t dev, uint8_t func, uint32_t off);
uint32_t pci_config_read_dword(struct pci_device *dev, uint16_t off);
void pci_config_write_dword_legacy(uint8_t bus, uint8_t dev, uint8_t func, uint32_t off, uint32_t val);
void pci_config_write_dword(struct pci_device *dev, uint16_t off, uint32_t val);
void pci_add_irq(struct pci_device *dev, irq_handler_func_t handler, void *ctx);

void pci_add_class_driver(uint32_t full_class, pci_driver_func_t func, const char *name);
void pci_add_device_driver(uint32_t id, pci_driver_func_t func, const char *name);

// pcidb.c
const char *pci_class_string(uint16_t full_class);
