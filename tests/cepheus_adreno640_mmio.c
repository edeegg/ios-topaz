/* SPDX-License-Identifier: MIT */
#include "../drivers/cepheus/adreno640_mmio.h"
static uint32_t gpu[0x10000], gmu[0xc000], cfg[0x4000], seq[0x4000];
static unsigned barriers, fail;
static int barrier(void *ctx)
{ (void)ctx; ++barriers; return fail && barriers == fail ? -1 : 0; }
int test(void)
{
    static struct a640_mmio io = {gpu, gmu, cfg, seq,
                          sizeof(gpu), sizeof(gmu), sizeof(cfg), sizeof(seq), 0, barrier};
    uint32_t result = 0xfeed;
    if (a640_mmio_write(&io, A640_GMU, 0x1a880, 0x1234) || gmu[0x80] != 0x1234) return 1;
    if (a640_mmio_gmu_write(&io, A640_RSC, 0x23404, 0x5678) || gmu[0x8c04] != 0x5678) return 2;
    if (a640_mmio_gmu_write(&io, A640_PDC_CONFIG, 0x1572, 7) || cfg[0x1572] != 7 || seq[0x1572]) return 3;
    if (a640_mmio_read(&io, A640_GMU, 0x1a880, &result) || result != 0x1234) return 4;
    unsigned before = barriers;
    if (!a640_mmio_write(&io, A640_GPU, 0x10000, 1) ||
        !a640_mmio_write(&io, A640_GMU, 0x1a7ff, 1) ||
        !a640_mmio_write(&io, A640_GMU, 0x26800, 1) ||
        !a640_mmio_gmu_write(&io, A640_PDC_CONFIG, 0x4000, 1) ||
        !a640_mmio_write(&io, (enum a640_domain)5, 0, 1) || barriers != before) return 5;
    io.gpu_bytes = 0x3ffff;
    if (!a640_mmio_write(&io, A640_GPU, 0, 1) || barriers != before) return 6;
    io.gpu_bytes = sizeof(gpu);
    barriers = 0; fail = 1;
    if (!a640_mmio_write(&io, A640_GPU, 0, 1) || gpu[0]) return 7;
    barriers = 0; fail = 2; result = 0xfeed;
    if (!a640_mmio_read(&io, A640_GMU, 0x1a880, &result) || result != 0xfeed) return 8;
    return 0;
}
