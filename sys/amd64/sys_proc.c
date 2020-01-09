#include "sys/amd64/sys_proc.h"
#include "sys/amd64/syscall.h"
#include "sys/amd64/cpu.h"
#include "sys/assert.h"
#include "sys/sched.h"
#include "sys/thread.h"
#include "sys/debug.h"
#include "sys/errno.h"
#include "sys/panic.h"

void sys_exit(int status) {
    struct thread *thr = get_cpu()->thread;
    _assert(thr);
    kdebug("%u exited with code %d\n", thr->pid, status);
    thr->exit_code = status;
    thr->flags |= THREAD_STOPPED;
    amd64_syscall_yield_stopped();
}

int sys_kill(int pid, int signum) {
    asm volatile ("cli");
    struct thread *cur_thread = get_cpu()->thread;

    // Find destination thread
    struct thread *dst_thread = sched_find(pid);

    if (!dst_thread) {
        return -ESRCH;
    }

    // Push an item to signal queue
    thread_signal(dst_thread, signum);

    if (cur_thread == dst_thread) {
        // Suicide signal, just hang on and wait
        // until scheduler decides it's our time
        asm volatile ("sti; hlt; cli");

        kdebug("Returned from shit\n");
    }

    return 0;
}

void sys_signal(uintptr_t entry) {
    struct thread *thr = get_cpu()->thread;
    _assert(thr);
    kdebug("%u set its signal handler to %p\n", thr->pid, entry);
    thr->sigentry = entry;
}

void sys_sigret(void) {
    struct thread *thr = get_cpu()->thread;
    _assert(thr);

    thr->flags |= THREAD_SIGRET;

    asm volatile ("sti; hlt; cli");

    panic("Fuck\n");
}

int sys_waitpid(int pid, int *status) {
    struct thread *thr = get_cpu()->thread;
    _assert(thr);
    // Find child with such PID
    struct thread *waited = NULL;
    int wait_good = 0;

    for (struct thread *child = thr->child; child; child = child->next_child) {
        if (!(child->flags & THREAD_KERNEL) && (int) child->pid == pid) {
            waited = child;
            break;
        }
    }

    if (!waited) {
        return -ESRCH;
    }

    while (1) {
        if (waited->flags & THREAD_STOPPED) {
            kdebug("%d is done waiting for %d\n", thr->pid, waited->pid);
            wait_good = 1;
            break;
        }
        asm volatile ("sti; hlt; cli");
    }

    if (wait_good) {
        if (status) {
            *status = waited->exit_code;
        }
        waited->flags |= THREAD_DONE_WAITING;

        thread_terminate(waited);
    }

    return 0;
}

int sys_getpid(void) {
    struct thread *thr = get_cpu()->thread;
    _assert(thr);
    return thr->pid;
}