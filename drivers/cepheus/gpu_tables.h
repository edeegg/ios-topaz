/* SPDX-License-Identifier: MIT */
#ifndef CEPHEUS_GPU_TABLES_H
#define CEPHEUS_GPU_TABLES_H
#include "third_party/ArmSmmu4K/ArmSmmu4K.h"

/* Build inactive GPU 4K tables inside a caller-owned CPU 16K allocation.
 * Physical buffer starts and lengths must cover whole owned CPU pages.
 * IOVAs remain 4K aligned. Inputs must be immutable and disjoint from pool
 * and result storage. Caller establishes physical ownership and attributes.
 * No allocation, MMIO, cache maintenance, TLB invalidation or activation.
 * Output width is a caller-verified hardware property, not a default.
 */
ArmSmmu4KStatus cepheus_gpu_tables(void *pool, uint64_t pool_physical,
    size_t pool_bytes, unsigned output_bits, size_t leaf_budget,
    const ArmSmmu4KRange *ranges, size_t count, ArmSmmu4KResult *result);
#endif
