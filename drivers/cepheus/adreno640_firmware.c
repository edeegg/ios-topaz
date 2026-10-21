/* SPDX-License-Identifier: GPL-2.0-only
 * A640 memory layout and TCM register data from Linux Foundation (2017-2020)
 * kgsl_gmu.c, adreno_a6xx_gmu.c and a6xx_reg.h at
 * aa587668f86bd02ff1d7f825f5a89a886a84c8d8.
 */
#include "adreno640_firmware.h"
static const AdrenoFirmwareRegion regions[] = {
    {0, 0x4000, A640_ITCM}, {0x4000, 0x3c000, A640_ICACHE},
    {0x40000, 0x4000, A640_DTCM}, {0x44000, 0x3c000, A640_DCACHE}
};
static uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 |
           (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
int a640_gmu_upload(const struct a640_firmware_io *io, uint32_t chip, unsigned ready,
    const uint8_t *firmware, size_t bytes, AdrenoFirmwareBlock *scratch, size_t capacity)
{
    size_t count = 0;
    if (!io || !io->tcm_word || !io->cache_block || !io->barrier) return -1;
    if (chip != 0x06040001 || (ready & 15) != 15) return -2;
    if (AdrenoFirmwarePlanGmu(firmware, bytes, regions, 4, scratch, capacity, &count)
        != AdrenoFirmwareSuccess) return -3;
    for (size_t i = 0; i < count; ++i) {
        const AdrenoFirmwareBlock *b = scratch + i;
        if (!b->bytes) continue; /* Metadata remains opaque, as in stock KGSL. */
        const uint8_t *data = firmware + b->payload_offset;
        if (b->region_identifier == A640_ITCM || b->region_identifier == A640_DTCM) {
            uint32_t reg = b->region_identifier == A640_ITCM ?
                0x1b400 + b->address / 4 : 0x1c400 + (b->address - 0x40000) / 4;
            for (uint32_t offset = 0; offset < b->bytes; offset += 4)
                if (io->tcm_word(io->cookie, reg + offset / 4, le32(data + offset))) return -4;
        } else if (io->cache_block(io->cookie, (enum a640_fw_region)b->region_identifier,
                                  b->address, data, b->bytes)) return -4;
    }
    return io->barrier(io->cookie) ? -4 : 0;
}
