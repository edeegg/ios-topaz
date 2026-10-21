/* SPDX-License-Identifier: MIT */
#ifndef CEPHEUS_ADRENO640_MMIO_H
#define CEPHEUS_ADRENO640_MMIO_H
#include <stddef.h>
#include "adreno640_clock.h"
#include "adreno640_gmu.h"
/* Device mappings owned by platform, never ordinary cached RAM on hardware.
 * Full mapped lengths below must be present, caller serialized and powered.
 * Caller must install a real device barrier; no mapping/allocation here.
 */
struct a640_mmio {
    volatile uint32_t *gpu, *gmu, *pdc_config, *pdc_sequence;
    size_t gpu_bytes, gmu_bytes, pdc_config_bytes, pdc_sequence_bytes;
    void *barrier_cookie;
    int (*barrier)(void *);
};
int a640_mmio_read(void *, enum a640_domain, uint32_t, uint32_t *);
int a640_mmio_write(void *, enum a640_domain, uint32_t, uint32_t);
int a640_mmio_gmu_write(void *, enum a640_gmu_window, uint32_t, uint32_t);
int a640_mmio_barrier(void *);
#endif
