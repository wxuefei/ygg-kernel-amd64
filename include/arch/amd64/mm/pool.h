#pragma once
#include <stddef.h>
#include <stdint.h>

// 8192 page tables
#define MM_POOL_SIZE        (8192 * 0x1000)

struct amd64_pool_stat {
    size_t pages_free;
    size_t pages_used;
};

/// Initialize the paging structure allocation pool
void amd64_mm_pool_init(uintptr_t base, size_t size);

/// Allocate a paging structure (returns virtual addresses)
uint64_t *amd64_mm_pool_alloc(void);

/// Free a paging structure (obj is a virtual pointer)
void amd64_mm_pool_free(uint64_t *obj);

void amd64_mm_pool_stat(struct amd64_pool_stat *st);
