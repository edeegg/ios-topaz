/* SPDX-License-Identifier: MIT */
#include "adreno640_mmio.h"

/* Global GPU DWORD address zero corresponds to PA02c00000. GMU's mapped
 * PA02c6a000 therefore begins at global DWORD1a800, NOT index zero.
 * RSC/AO live within this same GMU aperture. PDC windows are local indices.
 */
static volatile uint32_t *address(volatile uint32_t *base, size_t bytes,
    uint32_t reg, uint32_t first, size_t aperture)
{
    if (!base || ((uintptr_t)base & 3) || bytes < aperture || reg < first ||
        (uint64_t)(reg - first) * 4 >= aperture) return 0;
    return base + (reg - first);
}
static volatile uint32_t *clock_address(struct a640_mmio *io,
    enum a640_domain domain, uint32_t reg)
{
    if (!io || !io->barrier) return 0;
    switch (domain) {
    case A640_GPU: return address(io->gpu, io->gpu_bytes, reg, 0, 0x40000);
    case A640_GMU: return address(io->gmu, io->gmu_bytes, reg, 0x1a800, 0x30000);
    default: return 0;
    }
}
int a640_mmio_read(void *cookie, enum a640_domain domain, uint32_t reg, uint32_t *out)
{
    struct a640_mmio *io = cookie;
    volatile uint32_t *p = clock_address(io, domain, reg);
    if (!p || !out || io->barrier(io->barrier_cookie)) return -1;
    uint32_t value = *p;
    if (io->barrier(io->barrier_cookie)) return -1;
    *out = value;
    return 0;
}
static int write_address(struct a640_mmio *io, volatile uint32_t *p, uint32_t value)
{
    if (!io || !p || !io->barrier || io->barrier(io->barrier_cookie)) return -1;
    *p = value;
    return io->barrier(io->barrier_cookie) ? -1 : 0;
}
int a640_mmio_write(void *cookie, enum a640_domain domain, uint32_t reg, uint32_t value)
{
    return write_address(cookie, clock_address(cookie, domain, reg), value);
}
int a640_mmio_gmu_write(void *cookie, enum a640_gmu_window window, uint32_t reg, uint32_t value)
{
    struct a640_mmio *io = cookie;
    volatile uint32_t *p = 0;
    if (!io) return -1;
    switch (window) {
    case A640_RSC: p = clock_address(io, A640_GMU, reg); break;
    case A640_PDC_CONFIG:
        p = address(io->pdc_config, io->pdc_config_bytes, reg, 0, 0x10000); break;
    case A640_PDC_SEQUENCE:
        p = address(io->pdc_sequence, io->pdc_sequence_bytes, reg, 0, 0x10000); break;
    default: return -1;
    }
    return write_address(io, p, value);
}
int a640_mmio_barrier(void *cookie)
{
    struct a640_mmio *io = cookie;
    return io && io->barrier ? io->barrier(io->barrier_cookie) : -1;
}
