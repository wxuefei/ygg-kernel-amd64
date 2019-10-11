#include "sys/amd64/cpu.h"
#include "sys/amd64/mm/mm.h"
#include "sys/thread.h"
#include "sys/debug.h"
#include "sys/spin.h"

static struct thread *sched_queue_heads[AMD64_MAX_SMP] = { 0 };
static struct thread *sched_queue_tails[AMD64_MAX_SMP] = { 0 };
static spin_t sched_lock = 0;

#define TEST_STACK  65536

// Testing
static char t_stack[TEST_STACK * AMD64_MAX_SMP] = {0};
static struct thread t_idle[AMD64_MAX_SMP] = {0};

void idle_func(uintptr_t id) {
    while (1) {
        kdebug("%d\n", id);
        asm volatile ("sti; hlt");
    }
}

static void make_idle_task(int cpu) {
    struct thread *t = &t_idle[cpu];

    uintptr_t stack_base = (uintptr_t) t_stack + cpu * TEST_STACK;

    t->data.stack0_base = stack_base;
    t->data.stack0_size = TEST_STACK;
    t->data.rsp0 = stack_base + TEST_STACK - sizeof(struct cpu_context);

    // Setup context
    struct cpu_context *ctx = (struct cpu_context *) t->data.rsp0;

    ctx->rip = (uintptr_t) idle_func;
    ctx->rsp = t->data.rsp0;
    ctx->cs = 0x08;
    ctx->ss = 0x10;
    ctx->rflags = 0x248;

    ctx->rax = 0;
    ctx->rcx = 0;
    ctx->rdx = 0;
    ctx->rdx = 0;
    ctx->rbp = 0;
    ctx->rsi = 0;

    // For testing
    ctx->rdi = cpu;

    ctx->cr3 = (uintptr_t) mm_kernel - 0xFFFFFF0000000000;

    ctx->r8 = 0;
    ctx->r9 = 0;
    ctx->r10 = 0;
    ctx->r11 = 0;
    ctx->r12 = 0;
    ctx->r13 = 0;
    ctx->r14 = 0;
    ctx->r15 = 0;

    ctx->ds = 0x10;
    ctx->es = 0x10;
    ctx->fs = 0;

    ctx->__canary = AMD64_STACK_CTX_CANARY;
}

void sched_add_to(int cpu, struct thread *t) {
    t->next = NULL;

    spin_lock(&sched_lock);
    if (sched_queue_tails[cpu] != NULL) {
        sched_queue_tails[cpu]->next = t;
    } else {
        sched_queue_heads[cpu] = t;
    }
    sched_queue_tails[cpu] = t;
    spin_release(&sched_lock);
}

void sched_init(void) {
    for (int i = 0; i < AMD64_MAX_SMP; ++i) {
        make_idle_task(i);
        sched_add_to(i, &t_idle[i]);
    }
}

int sched(void) {
    struct thread *from = get_cpu()->thread;
    struct thread *to;

    spin_lock(&sched_lock);
    if (from && from->next) {
        to = from->next;
    } else {
        to = sched_queue_heads[get_cpu()->processor_id];
    }
    spin_release(&sched_lock);

    // Empty scheduler queue
    if (!to) {
        return -1;
    }

    get_cpu()->thread = to;

    return 0;
}