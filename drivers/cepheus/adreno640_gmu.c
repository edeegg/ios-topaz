/* SPDX-License-Identifier: GPL-2.0-only
 * A640 path of _load_gmu_rpmh_ucode, Linux Foundation copyright 2017-2021.
 * android_kernel_xiaomi_cepheus aa587668f86bd02ff1d7f825f5a89a886a84c8d8,
 * drivers/gpu/msm/adreno_a6xx_gmu.c and a6xx_reg.h. DWORD indices.
 */
#include "adreno640_gmu.h"
struct seq_pair { uint32_t reg, value; };
static const struct seq_pair rsc[] = {
    {0x23404, 1u << 24}, {0x2340b, 1},
    {0x2340e, 0}, {0x2340d, 0}, {0x23410, 0}, {0x2340f, 0},
    {0x23412, 0x80000000}, {0x23411, 0},
    {0x23500, 0}, {0x23408, 0x4520}, {0x23409, 0x4510}, {0x2340a, 0x4514},
    {0x23580, 0xa7a506a0}, {0x23581, 0xa1e6a6e7},
    {0x23582, 0xa2e081e1}, {0x23583, 0xe9a982e2}, {0x23584, 0x0020e8a8}
};
static const uint32_t pdc[] = {
    0xfebea1e1, 0xa5a4a3a2, 0x8382a6e0, 0xbce3e284, 0x002081fc
};
int a640_gmu_program_sequences(const struct a640_gmu_io *io, uint32_t chip,
    unsigned ready, uint32_t mx, uint32_t cx, uint32_t xo)
{
    if (!io || !io->write || !io->barrier) return -1;
    /* These CMD DB addresses are verified on this Mi9; never reuse A740's. */
    if (chip != 0x06040001 || (ready & 15) != 15 ||
        mx != 0x30010 || cx != 0x30000 || xo != 0x30090) return -2;
    for (unsigned i = 0; i < sizeof(rsc)/sizeof(rsc[0]); ++i)
        if (io->write(io->cookie, A640_RSC, rsc[i].reg, rsc[i].value)) return -3;
    for (unsigned i = 0; i < 5; ++i)
        if (io->write(io->cookie, A640_PDC_SEQUENCE, i, pdc[i])) return -3;
    for (unsigned bank = 0; bank < 2; ++bank) {
        uint32_t control = bank ? 0x15d6 : 0x1572;
        if (io->write(io->cookie, A640_PDC_CONFIG, control + 1, 7) ||
            io->write(io->cookie, A640_PDC_CONFIG, control + 2, 0) ||
            io->write(io->cookie, A640_PDC_CONFIG, control, 0)) return -3;
        for (unsigned cmd = 0; cmd < 3; ++cmd) {
            uint32_t reg = control + 3 + cmd * 4;
            uint32_t addr = cmd == 0 ? mx : cmd == 1 ? cx : xo;
            uint32_t data = bank ? (cmd == 0 ? 2 : 3) : (cmd == 0 ? 1 : 0);
            if (io->write(io->cookie, A640_PDC_CONFIG, reg, 0x10108) ||
                io->write(io->cookie, A640_PDC_CONFIG, reg + 1, addr) ||
                io->write(io->cookie, A640_PDC_CONFIG, reg + 2, data)) return -3;
        }
    }
    if (io->write(io->cookie, A640_PDC_CONFIG, 0x1148, 0) ||
        io->write(io->cookie, A640_PDC_CONFIG, 0x1140, 0x80000001) ||
        io->barrier(io->cookie)) return -3;
    return 0;
}
