/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef CEPHEUS_ADRENO640_GMU_H
#define CEPHEUS_ADRENO640_GMU_H
#include <stdint.h>
enum a640_gmu_window { A640_RSC, A640_PDC_CONFIG, A640_PDC_SEQUENCE };
enum a640_gmu_ready {
    A640_GMU_CX_ON = 1, A640_GMU_CLOCKS_ON = 2,
    A640_GMU_RESET_HELD = 4, A640_GMU_EXCLUSIVE = 8
};
struct a640_gmu_io {
    void *cookie;
    /* RSC uses global GPU DWORD indices; PDC windows use local DWORDs. */
    int (*write)(void *, enum a640_gmu_window, uint32_t, uint32_t);
    int (*barrier)(void *);
};
/* Power-sequencer programming only. Does not release CM3 reset, upload
 * firmware, start HFI or establish RPMh resource votes. On I/O failure the
 * caller must stop bring-up; partial writes cannot be rolled back here.
 * Prerequisites must come from verified platform state, not constants.
 * Returns 0, -1 invalid, -2 unqualified/not ready, -3 I/O failure.
 */
int a640_gmu_program_sequences(const struct a640_gmu_io *, uint32_t chip,
    unsigned ready, uint32_t mx, uint32_t cx, uint32_t xo);
#endif
