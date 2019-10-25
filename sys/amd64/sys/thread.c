#include "sys/amd64/mm/mm.h"
#include "sys/amd64/cpu.h"
#include "sys/assert.h"
#include "sys/thread.h"
#include "sys/heap.h"
#include "sys/vmalloc.h"
#include "sys/debug.h"
#include "sys/mm.h"

int thread_init(
        struct thread *t,
        mm_space_t space,
        uintptr_t entry,
        uintptr_t rsp0_base,
        size_t rsp0_size,
        uintptr_t rsp3_base,
        size_t rsp3_size,
        uint32_t flags,
        void *arg) {
    if (!rsp0_base) {
        void *kstack = kmalloc(32768);
        _assert(kstack);

        rsp0_base = (uintptr_t) kstack;
        rsp0_size = 32768;
    }

    if (!(flags & THREAD_KERNEL) && !rsp3_base) {
        uintptr_t ustack = vmalloc(space, 0x100000, 0xF0000000, 4, VM_ALLOC_USER | VM_ALLOC_WRITE);
        _assert(ustack != MM_NADDR);

        rsp3_base = ustack;
        rsp3_size = 4 * 0x1000;

        kdebug("Allocated user stack: %p\n", ustack);
    }

    t->data.stack0_base = rsp0_base;
    t->data.stack0_size = rsp0_size;
    t->data.rsp0 = t->data.stack0_base + t->data.stack0_size - sizeof(struct cpu_context);

    if (!(flags & THREAD_KERNEL)) {
        t->data.stack3_base = rsp3_base;
        t->data.stack3_size = rsp3_size;
    }

    // Setup context
    struct cpu_context *ctx = (struct cpu_context *) t->data.rsp0;

    ctx->rip = entry;
    ctx->rflags = 0x248;

    if (flags & THREAD_KERNEL) {
        ctx->rsp = t->data.stack0_base + t->data.stack0_size;
        ctx->cs = 0x08;
        ctx->ss = 0x10;

        ctx->ds = 0x10;
        ctx->es = 0x10;
        ctx->fs = 0;
    } else {
        ctx->rsp = t->data.stack3_base + t->data.stack3_size;
        ctx->cs = 0x23;
        ctx->ss = 0x1B;

        ctx->ds = 0x1B;
        ctx->es = 0x1B;
        ctx->fs = 0;
    }

    ctx->rax = 0;
    ctx->rcx = 0;
    ctx->rdx = 0;
    ctx->rdx = 0;
    ctx->rbp = 0;
    ctx->rsi = 0;
    ctx->rdi = (uintptr_t) arg;

    ctx->cr3 = ({ _assert((uintptr_t) space >= 0xFFFFFF0000000000); (uintptr_t) space - 0xFFFFFF0000000000; });

    ctx->r8 = 0;
    ctx->r9 = 0;
    ctx->r10 = 0;
    ctx->r11 = 0;
    ctx->r12 = 0;
    ctx->r13 = 0;
    ctx->r14 = 0;
    ctx->r15 = 0;

    ctx->__canary = AMD64_STACK_CTX_CANARY;

    t->space = space;
    t->flags = flags;
    t->next = NULL;

    return 0;
}

void amd64_thread_set_ip(struct thread *t, uintptr_t ip) {
    struct cpu_context *ctx = (struct cpu_context *) t->data.rsp0;

    ctx->rip = ip;
    ctx->rflags = 0x248;
}
