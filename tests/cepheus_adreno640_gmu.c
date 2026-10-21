/* SPDX-License-Identifier: GPL-2.0-only */
#include "../drivers/cepheus/adreno640_gmu.h"
static unsigned writes, barriers, fail;
static struct { unsigned window, reg, value; } operations[64];
static int write_reg(void *ctx, enum a640_gmu_window window, uint32_t reg, uint32_t value)
{
    (void)ctx;
    if (writes >= 64) return -1;
    operations[writes].window = window;
    operations[writes].reg = reg;
    operations[writes].value = value;
    ++writes;
    return fail && writes == fail ? -1 : 0;
}
static int barrier(void *ctx) { (void)ctx; ++barriers; return 0; }
int test(void)
{
    const struct a640_gmu_io io = {0, write_reg, barrier};
    for (unsigned mask = 0; mask < 15; ++mask)
        if (a640_gmu_program_sequences(&io, 0x06040001, mask, 0x30010, 0x30000, 0x30090) != -2 || writes) return 1;
    if (a640_gmu_program_sequences(&io, 0x07040000, 15, 0x30010, 0x30000, 0x30090) != -2 || writes) return 2;
    if (a640_gmu_program_sequences(&io, 0x06040001, 15, 0x30010, 0x30000, 0x30080) != -2 || writes) return 3;
    if (a640_gmu_program_sequences(&io, 0x06040001, 15, 0x30010, 0x30000, 0x30090) || writes != 48 || barriers != 1) return 4;
    if (operations[0].reg != 0x23404 || operations[0].value != (1u << 24) ||
        operations[16].reg != 0x23584 || operations[16].value != 0x0020e8a8 ||
        operations[17].window != A640_PDC_SEQUENCE || operations[17].value != 0xfebea1e1 ||
        operations[47].reg != 0x1140 || operations[47].value != 0x80000001) return 5;
    for (unsigned i = 1; i <= 48; ++i) {
        writes = barriers = 0; fail = i;
        if (a640_gmu_program_sequences(&io, 0x06040001, 15, 0x30010, 0x30000, 0x30090) != -3 || writes != i || barriers) return 6;
    }
    return 0;
}
