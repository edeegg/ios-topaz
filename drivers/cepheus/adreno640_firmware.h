/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef CEPHEUS_ADRENO640_FIRMWARE_H
#define CEPHEUS_ADRENO640_FIRMWARE_H
#include "adreno640_gmu.h"
#include "third_party/AdrenoFirmware/AdrenoFirmware.h"
enum a640_fw_region { A640_ITCM, A640_ICACHE, A640_DTCM, A640_DCACHE };
struct a640_firmware_io {
    void *cookie;
    /* TCM: global GPU DWORD index; cache: GMU IOVA, backed by owned RAM.
     * Cache callback must translate to its allocation; never CPU-dereference
     * GMU IOVAs. Caller ensures mapped ownership and device visibility.
     */
    int (*tcm_word)(void *, uint32_t, uint32_t);
    int (*cache_block)(void *, enum a640_fw_region, uint32_t, const uint8_t *, size_t);
    int (*barrier)(void *);
};
/* No writes before complete structural validation. Firmware and regions must
 * stay immutable and disjoint from scratch. All hardware prerequisites use
 * a640_gmu_ready; RESET_HELD is required throughout upload. Stop after I/O
 * failure. Does not authenticate firmware, allocate/map caches, release reset
 * or start HFI. Only caller-verified stock firmware may reach the live path.
 * 0 success, -1 argument, -2 chip/state, -3 malformed/unmapped, -4 I/O.
 */
int a640_gmu_upload(const struct a640_firmware_io *, uint32_t chip, unsigned ready,
    const uint8_t *firmware, size_t bytes, AdrenoFirmwareBlock *scratch, size_t capacity);
#endif
