/* SPDX-License-Identifier: MIT */
#include "../drivers/cepheus/gpu_tables.h"
#define MASK UINT64_C(0x0000fffffffff000)
int test(void *storage)
{
    uint64_t *pool = storage;
    const uint64_t base = UINT64_C(0x148000000);
    ArmSmmu4KRange range;
    range.iova = 0x20000000;
    range.physical = UINT64_C(0x144000000);
    range.bytes = 16384;
    range.permissions = ArmSmmu4KRead | ArmSmmu4KWrite;
    range.writeback = 0;
    range.shareability = 2;
    ArmSmmu4KResult result;
    if (cepheus_gpu_tables(pool, base, 16384, 36, 4, &range, 1, &result)) return 1;
    if (result.table_pages != 3 || result.leaf_pages != 4 ||
        result.start_level != 1 || result.root_physical != base) return 2;
    for (unsigned page = 0; page < 4; ++page) {
        uint64_t va = range.iova + page * 4096;
        size_t table = 0;
        for (unsigned level = 1; level < 3; ++level) {
            uint64_t entry = pool[table * 512 + ((va >> (39-9*level)) & 511)];
            if ((entry & 3) != 3 || (entry & MASK) < base ||
                (entry & MASK) >= base + 12288) return 3;
            table = (size_t)(((entry & MASK) - base) / 4096);
        }
        uint64_t leaf = pool[table * 512 + ((va >> 12) & 511)];
        if ((leaf & MASK) != range.physical + page * 4096 ||
            (leaf & 3) != 3 || (leaf & (UINT64_C(3) << 53)) != (UINT64_C(3) << 53)) return 4;
    }
    pool[0] = UINT64_C(0xfeed);
    range.physical += 4096;
    if (cepheus_gpu_tables(pool, base, 16384, 36, 4, &range, 1, &result) !=
        ArmSmmu4KBadRange || pool[0] != 0xfeed || result.table_pages) return 5;
    range.physical -= 4096;
    if (cepheus_gpu_tables(pool, base, 16384, 36, 3, &range, 1, &result) !=
        ArmSmmu4KBudget || pool[0] != 0xfeed) return 6;
    range.physical = base;
    if (cepheus_gpu_tables(pool, base, 16384, 36, 4, &range, 1, &result) !=
        ArmSmmu4KOverlap || pool[0] != 0xfeed) return 7;
    if (cepheus_gpu_tables((char *)pool + 8, base, 16384, 36, 4, &range, 1,
        &result) != ArmSmmu4KArgument || pool[0] != 0xfeed) return 8;
    return 0;
}
