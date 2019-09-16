HEADERS=$(shell find $(S) -name "*.h")

CFLAGS+=-Wall \
		-Wextra \
		-Werror \
		-Wpedantic \
		-Wno-unused-parameter \
		-Iinclude

ifdef KERNEL_TEST_MODE
CFLAGS+=-DKERNEL_TEST_MODE
endif

DIRS+=$(O)/sys
OBJS+=$(O)/sys/mem.o \
	  $(O)/sys/string.o \
	  $(O)/sys/debug.o \
	  $(O)/sys/panic.o
