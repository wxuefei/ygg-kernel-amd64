### This file contains all things for a generic amd64-build

### Kernel build
DEFINES+=-DARCH_AMD64

OBJS+=$(O)/sys/amd64/hw/rs232.o \
	  $(O)/sys/amd64/kernel.o \
	  $(O)/sys/amd64/hw/gdt.o \
	  $(O)/sys/amd64/hw/gdt_s.o \
	  $(O)/sys/amd64/hw/acpi.o \
	  $(O)/sys/amd64/mm/mm.o \
	  $(O)/sys/amd64/hw/apic.o \
	  $(O)/sys/amd64/hw/idt.o \
	  $(O)/sys/amd64/hw/exc_s.o \
	  $(O)/sys/amd64/hw/irq0.o \
	  $(O)/sys/amd64/hw/con.o \
	  $(O)/sys/amd64/hw/timer.o \
	  $(O)/sys/amd64/hw/ioapic.o \
	  $(O)/sys/amd64/hw/irqs_s.o \
	  $(O)/sys/amd64/sys/spin_s.o

kernel_OBJS=$(O)/sys/amd64/entry.o \
			$(OBJS)
kernel_LINKER=sys/amd64/link.ld
kernel_LDFLAGS=-nostdlib \
			   -fPIE \
			   -fno-plt \
			   -fno-pic \
			   -static \
			   -Wl,--build-id=none \
			   -z max-page-size=0x1000 \
			   -ggdb \
			   -T$(kernel_LINKER)
kernel_CFLAGS=-ffreestanding \
			  -I. \
			  $(DEFINES) \
			  $(CFLAGS) \
			  -fPIE \
			  -fno-plt \
			  -fno-pic \
			  -static \
			  -fno-asynchronous-unwind-tables \
			  -mcmodel=large \
			  -mno-red-zone \
			  -mno-mmx \
			  -mno-sse \
			  -mno-sse2 \
			  -z max-page-size=0x1000
DIRS+=$(O)/sys/amd64/image/boot/grub \
	  $(O)/sys/amd64/hw \
	  $(O)/sys/amd64/sys \
	  $(O)/sys/amd64/mm
