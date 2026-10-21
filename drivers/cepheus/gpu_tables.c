/* SPDX-License-Identifier: MIT */
#include "gpu_tables.h"

ArmSmmu4KStatus cepheus_gpu_tables(void *pool, uint64_t pool_physical,
    size_t pool_bytes, unsigned output_bits, size_t leaf_budget,
    const ArmSmmu4KRange *ranges, size_t count, ArmSmmu4KResult *result)
{
    if (!result) return ArmSmmu4KArgument;
    result->root_physical = result->mair = 0;
    result->table_pages = result->leaf_pages = result->start_level = 0;
    result->upper_region = 0;
    if (!pool || ((uintptr_t)pool & 16383) || (pool_physical & 16383) ||
        !pool_bytes || (pool_bytes & 16383) || !ranges || !count ||
        count > SIZE_MAX / sizeof(*ranges)) return ArmSmmu4KArgument;
    for (size_t i = 0; i < count; ++i)
        if ((ranges[i].physical & 16383) || (ranges[i].bytes & 16383))
            return ArmSmmu4KBadRange;
    const ArmSmmu4KConfig config = {32, output_bits, pool_physical, leaf_budget};
    return ArmSmmu4KBuild(&config, ranges, count, pool, pool_bytes / 4096, result);
}
