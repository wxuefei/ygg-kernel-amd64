include sys/amd64/compiler.mk

all:
### Kernel build
DEFINES+=-DARCH_AMD64
OBJS+=$(O)/sys/amd64/kernel.o \
	  $(O)/sys/amd64/mm/pool.o \
	  $(O)/sys/amd64/mm/mm.o \
	  $(O)/sys/amd64/hw/rs232.o \
	  $(O)/sys/amd64/hw/gdt.o \
	  $(O)/sys/amd64/hw/gdt_s.o \
	  $(O)/sys/amd64/hw/idt.o \
	  $(O)/sys/amd64/hw/ints.o \
	  $(O)/sys/amd64/hw/exc.o \
	  $(O)/sys/amd64/hw/regs.o \
	  $(O)/sys/amd64/hw/irq0.o \
	  $(O)/sys/amd64/hw/pic8259.o \
	  $(O)/sys/amd64/hw/irqs.o \
	  $(O)/sys/amd64/mm/map.o \
	  $(O)/sys/amd64/hw/timer.o \
	  $(O)/sys/amd64/acpi/tables.o \
	  $(O)/sys/amd64/acpi/hpet.o \
	  $(O)/sys/amd64/mm/phys.o \
	  $(O)/sys/amd64/mm/heap.o \
	  $(O)/sys/amd64/mm/vmalloc.o \
	  $(O)/sys/amd64/sys/thread.o \
	  $(O)/sys/amd64/sys/syscall.o \
	  $(O)/sys/amd64/sys/syscall_s.o \
	  $(O)/sys/amd64/sys/kidle.o
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
DIRS+=$(O)/sys/amd64/mm \
	  $(O)/sys/amd64/hw \
	  $(O)/sys/amd64/acpi \
	  $(O)/sys/amd64/sys
# add .inc includes for asm
HEADERS+=$(shell find include -name "*.inc")

$(O)/sys/amd64/kernel.elf: $(kernel_OBJS) $(kernel_LINKER)
	@printf " LD\t%s\n" $(@:$(O)/%=%)
	@$(CC64) $(kernel_LDFLAGS)  -o $@ $(kernel_OBJS)

$(O)/sys/%.o: sys/%.S $(HEADERS)
	@printf " AS\t%s\n" $(@:$(O)/%=%)
	@$(CC64) $(kernel_CFLAGS) -c -o $@ $<

$(O)/sys/%.o: sys/%.c $(HEADERS)
	@printf " CC\t%s\n" $(@:$(O)/%=%)
	@$(CC64) $(kernel_CFLAGS) -c -o $@ $<

### Kernel loader build
TARGETS+=$(O)/sys/amd64/loader.elf $(O)/sys/amd64/kernel.elf
DIRS+=$(O)/sys/amd64/loader
loader_OBJS+=$(O)/sys/amd64/loader/boot.o \
			 $(O)/sys/amd64/loader/loader.o \
			 $(O)/sys/amd64/loader/util.o
loader_LINKER=sys/amd64/loader/link.ld
loader_CFLAGS=-ffreestanding \
			  -nostdlib \
			  -I. \
			  -Iinclude \
			  -Wall \
			  -Wextra \
			  -Wpedantic \
			  -Wno-unused-argument \
			  -Werror \
			  -Wno-language-extension-token
loader_LDFLAGS=-nostdlib -T$(loader_LINKER)

$(O)/sys/amd64/loader.elf: $(loader_OBJS) $(loader_LINKER)
	@printf " LD\t%s\n" $(@:$(O)/%=%)
	@$(CC86) $(loader_LDFLAGS) -o $@ $(loader_OBJS)

$(O)/sys/amd64/loader/%.o: sys/amd64/loader/%.S $(HEADERS)
	@printf " AS\t%s\n" $(@:$(O)/%=%)
	@$(CC86) $(loader_CFLAGS) -c -o $@ $<

$(O)/sys/amd64/loader/%.o: sys/amd64/loader/%.c $(HEADERS)
	@printf " CC\t%s\n" $(@:$(O)/%=%)
	@$(CC86) $(loader_CFLAGS) -c -o $@ $<

### Debugging and emulation
QEMU_BIN?=qemu-system-x86_64
QEMU_OPTS?=-nographic \
		   -serial mon:stdio \
		   -m 512

qemu: all
	@$(QEMU_BIN) \
		-kernel $(O)/sys/amd64/loader.elf \
		-initrd $(O)/sys/amd64/kernel.elf $(QEMU_OPTS)
