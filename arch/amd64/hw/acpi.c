#include "arch/amd64/hw/acpi.h"
#include "arch/amd64/hw/apic.h"
#include "arch/amd64/hw/rtc.h"
#include "sys/char/ring.h"
#include "sys/char/line.h"
#include "sys/char/chr.h"
#include "sys/string.h"
#include "sys/debug.h"
#include "sys/panic.h"
#include "user/acpi.h"
#include "sys/dev.h"
#include "acpi.h"

static uintptr_t early_rsdp, early_rsdp_v2;

struct acpi_rsdp_ext *acpi_rsdp = NULL;
struct acpi_madt *acpi_madt = NULL;
struct acpi_fadt *acpi_fadt = NULL;
struct acpi_mcfg *acpi_mcfg = NULL;

static struct chrdev _acpi_event_dev = {
    .dev_data = NULL,
    .write = NULL,
    .read = simple_line_read,
    .ioctl = NULL
};

static uint8_t checksum(const void *ptr, size_t size) {
    uint8_t v = 0;
    for (size_t i = 0; i < size; ++i) {
        v += ((const uint8_t *) ptr)[i];
    }
    return v;
}

static uint32_t acpi_power_button_handler(void *ctx) {
    ring_putc(NULL, &_acpi_event_dev.buffer, ACEV_POWER_BUTTON, 0);
    return 0;
}

static ACPI_STATUS acpica_init(void) {
    ACPI_STATUS ret;
    ACPI_OBJECT arg1;
    ACPI_OBJECT_LIST args;

    /*
    * 0 = PIC
    * 1 = APIC
    * 2 = SAPIC ?
    */
    arg1.Type = ACPI_TYPE_INTEGER;
    arg1.Integer.Value = 1;
    args.Count = 1;
    args.Pointer = &arg1;

    if (ACPI_FAILURE(ret = AcpiInitializeSubsystem())) {
        kerror("AcpiInitializeSubsystem: %s\n", AcpiFormatException(ret));
        return ret;
    }

    if (ACPI_FAILURE(ret = AcpiInitializeTables(NULL, 0, FALSE))) {
        kerror("AcpiInitializeTables: %s\n", AcpiFormatException(ret));
        return ret;
    }

    if (ACPI_FAILURE(ret = AcpiLoadTables())) {
        kerror("ACPI INIT failure %s\n", AcpiFormatException(ret));
        return ret;
    }

    if (ACPI_FAILURE(ret = AcpiEvaluateObject(NULL, "\\_PIC", &args, NULL))) {
        if (ret != AE_NOT_FOUND) {
            kerror("ACPI INIT failure %s\n", AcpiFormatException(ret));
            return ret;
        }
        kwarn("\\_PIC = AE_NOT_FOUND\n");
        // Guess that's ok?
    }

    if (ACPI_FAILURE(ret = AcpiEnableSubsystem(ACPI_FULL_INITIALIZATION))) {
        kerror("ACPI INIT failure %s\n", AcpiFormatException(ret));
        return ret;
    }

    if (ACPI_FAILURE(ret = AcpiInitializeObjects(ACPI_FULL_INITIALIZATION))) {
        kerror("ACPI INIT failure %s\n", AcpiFormatException(ret));
        return ret;
    }

    if (ACPI_FAILURE(ret = AcpiInstallFixedEventHandler(ACPI_EVENT_POWER_BUTTON,
                                                        acpi_power_button_handler,
                                                        NULL))) {
        kwarn("Failed to install ACPI power button handler: %s\n", AcpiFormatException(ret));
    }

    return AE_OK;
}

void amd64_acpi_set_rsdp(uintptr_t addr) {
    early_rsdp = addr;
}

void amd64_acpi_set_rsdp2(uintptr_t addr) {
    early_rsdp_v2 = addr;
}

static uintptr_t locate_rsdp(void) {
    // TODO: support rsdpv2
    if (early_rsdp) {
        if (checksum((void *) early_rsdp, sizeof(struct acpi_rsdp)) != 0) {
            kwarn("Provided RSDP has invalid checksum, falling back to scan method\n");
        } else {
            return early_rsdp;
        }
    }

    for (size_t i = 0xFFFFFF0000000000 + 0x000E0000; i < 0xFFFFFF0000000000 + 0x000FFFFF - 8; ++i) {
        if (!strncmp((const char *) i, "RSD PTR ", 8)) {
            return i;
        }
    }

    return 0;
}

void amd64_acpi_init(void) {
    struct acpi_rsdt *rsdt;
    acpi_madt = NULL;

    acpi_rsdp = (struct acpi_rsdp_ext *) locate_rsdp();

    if (!acpi_rsdp) {
        panic("Failed to find RSDP\n");
    }

    kdebug("Found RSDP: %p\n", acpi_rsdp);
    if (checksum(acpi_rsdp, sizeof(struct acpi_rsdp)) != 0) {
        kdebug("RSDP is invalid\n");
        while (1);
    }

    kdebug("RSDP revision %d\n", acpi_rsdp->rsdp.rev);
    rsdt = (struct acpi_rsdt *) (0xFFFFFF0000000000 + acpi_rsdp->rsdp.rsdt_address);
    kdebug("RSDT: %p\n", rsdt);

    if (checksum(rsdt, rsdt->hdr.length) != 0) {
        kdebug("RSDT is invalid\n");
        while (1);
    }

    for (size_t i = 0; i < (rsdt->hdr.length - sizeof(struct acpi_header)) / sizeof(uint32_t); ++i) {
        uintptr_t entry_addr = 0xFFFFFF0000000000 + rsdt->entry[i];
        struct acpi_header *hdr = (struct acpi_header *) entry_addr;

        if (!strncmp((const char *) hdr, "APIC", 4)) {
            kdebug("Found MADT = %p\n", entry_addr);
            if (checksum(hdr, hdr->length) != 0) {
                kdebug("Entry is invalid\n");
                while (1);
            }

            acpi_madt = (struct acpi_madt *) hdr;
        } else if (!strncmp((const char *) hdr, "FACP", 4)) {
            kdebug("Found FADT = %p\n", entry_addr);
            if (checksum(hdr, hdr->length) != 0) {
                kdebug("Entry is invalid\n");
                while (1);
            }

            acpi_fadt = (struct acpi_fadt *) hdr;
        } else if (!strncmp((const char *) hdr, "HPET", 4)) {
            kdebug("Found HPET = %p\n", entry_addr);
            if (checksum(hdr, hdr->length) != 0) {
                kdebug("Entry is invalid\n");
                while (1);
            }
        } else if (!strncmp((const char *) hdr, "MCFG", 4)) {
            kdebug("Found MCFG = %p\n", entry_addr);
            if (checksum(hdr, hdr->length) != 0) {
                panic("Entry is invalid\n");
            }

            acpi_mcfg = (struct acpi_mcfg *) hdr;
        }
    }

    if (acpi_fadt) {
        rtc_set_century(acpi_fadt->century);
    }

    if (!acpi_madt) {
        panic("ACPI: No MADT present\n");
    }

    if (ACPI_FAILURE(acpica_init())) {
        panic("Failed to initialize ACPICA\n");
    }

    ring_init(&_acpi_event_dev.buffer, 16);
    dev_add(DEV_CLASS_CHAR, 0, &_acpi_event_dev, "acpi");
}
