/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef CEPHEUS_ADRENO640_CLOCK_H
#define CEPHEUS_ADRENO640_CLOCK_H
#include <stdint.h>

enum a640_domain { A640_GPU, A640_GMU };
enum a640_status { A640_OK, A640_INVALID, A640_UNSUPPORTED, A640_NOT_READY,
                   A640_IO_ERROR };
enum a640_prerequisite {
    A640_CX_POWERED = 1, A640_GX_POWERED = 2,
    A640_GMU_READY = 4, A640_QUIESCED = 8
};
struct a640_io {
    void *cookie;
    int (*read)(void *, enum a640_domain, uint32_t, uint32_t *);
    int (*write)(void *, enum a640_domain, uint32_t, uint32_t);
};

/* Callbacks take DWORD register indices in distinct GPU/GMU domains.
 * The platform must translate and bound them against its actual MMIO windows,
 * serialize calls and establish all prerequisites from real hardware state.
 * This is a clock-programming component, NOT a power/GMU boot implementation.
 * On I/O error partial programming is possible: stop, do not blindly retry.
 * There is intentionally no live MMIO adapter or published GPU service yet.
 */
enum a640_status a640_set_hwcg(const struct a640_io *, uint32_t chip_id,
                             unsigned prerequisites, int enabled);
enum a640_status a640_set_gbif(const struct a640_io *, uint32_t chip_id,
                             unsigned prerequisites);
#endif
