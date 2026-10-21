/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef CEPHEUS_ADRENO640_QMP_H
#define CEPHEUS_ADRENO640_QMP_H
#include <stddef.h>
#include <stdint.h>
enum a640_qmp_result { A640_QMP_OK, A640_QMP_ARGUMENT, A640_QMP_DESCRIPTOR,
    A640_QMP_BUSY, A640_QMP_TIMEOUT, A640_QMP_IO, A640_QMP_DISCONNECTED };
struct a640_qmp_io {
    void *cookie;
    /* Byte offsets inside the verified4KiB AOP msgram mapping. */
    int (*read)(void *, uint32_t, uint32_t *);
    int (*write)(void *, uint32_t, uint32_t);
    int (*barrier)(void *);
    int (*notify)(void *);
    int (*wait_us)(void *, unsigned);
};
struct a640_qmp {
    struct a640_qmp_io io;
    unsigned exclusive, connected;
    uint32_t tx_offset, tx_bytes, rx_offset, rx_bytes;
};
/* Caller owns the APSS link exclusively; no concurrent firmware/Linux client.
 * Validate actual descriptor and negotiate existing firmware-owned mailboxes.
 * No allocation, resource votes or power-on operation. Timeout leaves partial
 * handshake state; requalify ownership/state before reuse. No forced clearing
 * of stale messages. Callbacks must order real device accesses and wait.
 */
enum a640_qmp_result a640_qmp_connect(struct a640_qmp *, unsigned timeout_us);
/* Payload includes padding/terminator and must be a multiple of4 bytes.
 * On success AOP consumed it; this is NOT proof a power/voltage vote succeeded.
 * Resource-specific prerequisites and resulting hardware checks are required.
 * I/O failure or timeout invalidates connection; never blindly resubmit.
 */
enum a640_qmp_result a640_qmp_send(struct a640_qmp *, const uint8_t *, size_t,
    unsigned timeout_us);
#endif
