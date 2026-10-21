/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef CEPHEUS_ADRENO640_RPMH_H
#define CEPHEUS_ADRENO640_RPMH_H
#include <stdint.h>
enum a640_rpmh_result { A640_RPMH_OK, A640_RPMH_ARGUMENT, A640_RPMH_CAPABILITY,
    A640_RPMH_BUSY, A640_RPMH_IO, A640_RPMH_TIMEOUT };
struct a640_rpmh_io {
    void *cookie;
    /* Domain0: APPS DRV config, domain1: verified TCS register window.
     * Offsets are bytes, never RPMh resource addresses.
     */
    int (*read)(void *,unsigned,uint32_t,uint32_t *);
    int (*write)(void *,unsigned,uint32_t,uint32_t);
    int (*barrier)(void *);
    int (*wait_us)(void *,unsigned);
};
struct a640_rpmh {
    struct a640_rpmh_io io;
    unsigned exclusive, poisoned, voltage_policy_validated;
};
/* SM8150 APPS DRV2, active TCS0/1 only, caller owns the APSS driver exclusively.
 * Qualified requests ONLY: MX index2, CX index3, XO index3, from this Mi9's CMD
 * DB and OEM GMU wake sequence. Before setting voltage_policy_validated the
 * platform MUST establish aggregate/inherited rail votes and ensure these
 * requests cannot lower a rail required by CPU/display/storage. Positive
 * indices alone do not prove an upward transition. No automatic live caller
 * or voltage policy is provided by this transport.
 * Completion requires a fresh TCS IRQ and issued+completed command status.
 * I/O failure/timeout poisons the owner: do not reuse or blindly roll back.
 * No automatic power-up: clocks, GDSC, AOP and readback are separate steps.
 */
enum a640_rpmh_result a640_rpmh_vote(struct a640_rpmh *,uint32_t resource,uint32_t index);
#endif
