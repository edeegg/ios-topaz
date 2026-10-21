/* SPDX-License-Identifier: MIT */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <vm/pmap.h>
#include <vm/vm_kern.h>
#include <arm/caches_internal.h>
#include "gpu_memory_xnu.h"
#include "gpu_tables.h"

extern ppnum_t pmap_find_phys(pmap_t, addr64_t);

int cepheus_gpu_memory_alloc(struct cepheus_gpu_memory *memory, vm_size_t bytes)
{
    if (!memory || memory->address || memory->bytes || memory->physical ||
        PAGE_SIZE != 16384 || !bytes || bytes > 0x100000 || (bytes & PAGE_MASK))
        return EINVAL;
    vm_offset_t address = 0;
    if (kmem_alloc_contig(kernel_map, &address, bytes, PAGE_MASK, 0, 0,
        KMA_DATA | KMA_ZERO, VM_KERN_MEMORY_IOKIT) != KERN_SUCCESS) return ENOMEM;
    uint64_t physical = ptoa_64(pmap_find_phys(kernel_pmap, (addr64_t)address));
    if (!physical || (physical & PAGE_MASK) || physical >= (1ULL << 36) ||
        bytes > (1ULL << 36) - physical) goto invalid;
    for (vm_size_t offset = 0; offset < bytes; offset += PAGE_SIZE)
        if (ptoa_64(pmap_find_phys(kernel_pmap, (addr64_t)(address + offset))) !=
            physical + offset) goto invalid;
    memory->address = address;
    memory->physical = physical;
    memory->bytes = bytes;
    return 0;
invalid:
    kmem_free(kernel_map, address, bytes);
    return EFAULT;
}

int cepheus_gpu_memory_sync(const struct cepheus_gpu_memory *memory)
{
    if (!memory || !memory->address || !memory->physical || !memory->bytes ||
        memory->bytes > 0x100000 || (memory->bytes & PAGE_MASK) ||
        (memory->physical & PAGE_MASK)) return EINVAL;
    flush_dcache64((addr64_t)memory->physical, (unsigned)memory->bytes, TRUE);
    __asm__ volatile("dsb osh" ::: "memory");
    return 0;
}

void cepheus_gpu_memory_free(struct cepheus_gpu_memory *memory)
{
    if (!memory || !memory->address) return;
    kmem_free(kernel_map, memory->address, memory->bytes);
    memory->address = 0;
    memory->physical = 0;
    memory->bytes = 0;
}

void cepheus_gpu_memory_probe(void)
{
    struct cepheus_gpu_memory pool = {0}, buffer = {0};
    int error = cepheus_gpu_memory_alloc(&pool, 16384);
    if (!error) error = cepheus_gpu_memory_alloc(&buffer, 16384);
    if (error) {
        printf("CEPHEUS GPU MEMORY: allocation failed %d\n", error);
        goto out;
    }
    ArmSmmu4KRange range;
    range.iova = 0x20000000;
    range.physical = buffer.physical;
    range.bytes = buffer.bytes;
    range.permissions = ArmSmmu4KRead | ArmSmmu4KWrite;
    range.writeback = 0;
    range.shareability = 2;
    ArmSmmu4KResult tables;
    ArmSmmu4KStatus status = cepheus_gpu_tables((void *)pool.address,
        pool.physical, pool.bytes, 36, 4, &range, 1, &tables);
    if (status != ArmSmmu4KSuccess || tables.table_pages != 3 || tables.leaf_pages != 4) {
        printf("CEPHEUS GPU MEMORY: inactive table build failed %u\n", (unsigned)status);
        goto out;
    }
    uint64_t *entries = (uint64_t *)pool.address;
    const uint64_t mask = 0x0000fffffffff000ULL;
    for (unsigned page = 0; page < 4; ++page) {
        uint64_t iova = range.iova + page * 4096;
        size_t table = 0;
        for (unsigned level = 1; level < 3; ++level) {
            uint64_t descriptor = entries[table * 512 + ((iova >> (39 - 9 * level)) & 511)];
            uint64_t address = descriptor & mask;
            if ((descriptor & 3) != 3 || address < pool.physical ||
                address >= pool.physical + tables.table_pages * 4096) goto bad_walk;
            table = (size_t)((address - pool.physical) / 4096);
        }
        uint64_t descriptor = entries[table * 512 + ((iova >> 12) & 511)];
        if ((descriptor & 3) != 3 || (descriptor & mask) != buffer.physical + page * 4096)
            goto bad_walk;
    }
    if (cepheus_gpu_memory_sync(&pool) || cepheus_gpu_memory_sync(&buffer)) goto bad_walk;
    printf("CEPHEUS GPU MEMORY: owned pool=%llx buffer=%llx CPU16K GPU4K leaves=4 verified\n",
        pool.physical, buffer.physical);
    printf("CEPHEUS GPU MEMORY: inactive only, no SMMU activation or GPU execution\n");
    goto out;
bad_walk:
    printf("CEPHEUS GPU MEMORY: inactive table walk/cache preparation failed\n");
out:
    /* These allocations have NEVER been exposed to a device context. */
    cepheus_gpu_memory_free(&buffer);
    cepheus_gpu_memory_free(&pool);
}
