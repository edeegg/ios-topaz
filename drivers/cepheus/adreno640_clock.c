/* SPDX-License-Identifier: GPL-2.0-only
 * A640 clock sequence adapted from the Linux Foundation's adreno_a6xx.c
 * (copyright2017-2021), pinned in adreno640_registers.h.
 */
#include "adreno640_clock.h"
#include "adreno640_registers.h"

static enum a640_status validate(const struct a640_io *io, uint32_t chip,
                                 unsigned ready)
{
    if (!io || !io->read || !io->write) return A640_INVALID;
    /* Only the actual chip revision queried on this Mi9 is qualified here. */
    if (chip != 0x06040001u) return A640_UNSUPPORTED;
    if ((ready & 15u) != 15u) return A640_NOT_READY;
    return A640_OK;
}

static int rmw(const struct a640_io *io, enum a640_domain domain,
               uint32_t reg, uint32_t clear, uint32_t set)
{
    uint32_t value;
    if (io->read(io->cookie, domain, reg, &value)) return -1;
    return io->write(io->cookie, domain, reg, (value & ~clear) | set);
}

enum a640_status a640_set_hwcg(const struct a640_io *io, uint32_t chip,
                              unsigned ready, int enabled)
{
    enum a640_status status = validate(io, chip, ready);
    uint32_t current;
    const uint32_t target = enabled ? 0x8aa8aa82u : 0;
    if (status != A640_OK) return status;
    if (enabled != 0 && enabled != 1) return A640_INVALID;
    if (io->write(io->cookie, A640_GMU,
                  A6XX_GPU_GMU_AO_GMU_CGC_MODE_CNTL, enabled ? 0x20202u : 0) ||
        io->write(io->cookie, A640_GMU,
                  A6XX_GPU_GMU_AO_GMU_CGC_DELAY_CNTL, enabled ? 0x10111u : 0) ||
        io->write(io->cookie, A640_GMU,
                  A6XX_GPU_GMU_AO_GMU_CGC_HYST_CNTL, enabled ? 0x5555u : 0))
        return A640_IO_ERROR;
    if (io->read(io->cookie, A640_GPU, A6XX_RBBM_CLOCK_CNTL, &current))
        return A640_IO_ERROR;
    if (current == target) return A640_OK;
    if (rmw(io, A640_GMU, A6XX_GPU_GMU_GX_SPTPRAC_CLOCK_CONTROL, 1u, 0))
        return A640_IO_ERROR;
    for (unsigned i = 0; i < sizeof(a640_hwcg)/sizeof(a640_hwcg[0]); ++i)
        if (io->write(io->cookie, A640_GPU, a640_hwcg[i].reg,
                      enabled ? a640_hwcg[i].value : 0))
            return A640_IO_ERROR;
    if (rmw(io, A640_GMU, A6XX_GPU_GMU_GX_SPTPRAC_CLOCK_CONTROL, 0, 1u))
        return A640_IO_ERROR;
    if (io->write(io->cookie, A640_GPU, A6XX_RBBM_CLOCK_CNTL, target))
        return A640_IO_ERROR;
    return A640_OK;
}

enum a640_status a640_set_gbif(const struct a640_io *io, uint32_t chip,
                              unsigned ready)
{
    enum a640_status status = validate(io, chip, ready);
    if (status != A640_OK) return status;
    for (unsigned i = 0; i < sizeof(a640_gbif)/sizeof(a640_gbif[0]); ++i)
        if (io->write(io->cookie, A640_GPU, a640_gbif[i].reg, a640_gbif[i].value))
            return A640_IO_ERROR;
    return A640_OK;
}
