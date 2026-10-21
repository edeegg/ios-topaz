/* SPDX-License-Identifier: MIT */
#ifndef CEPHEUS_GPU_MEMORY_XNU_H
#define CEPHEUS_GPU_MEMORY_XNU_H
#include <sys/param.h>
struct cepheus_gpu_memory {
    vm_offset_t address;
    vm_size_t bytes;
    uint64_t physical;
};
/* Kernel-owned, CPU16K, physically contiguous normal RAM. No device mappings.
 * Caller initializes the handle to zero and serializes its whole lifetime.
 * Buffers are at most1MiB each and must fit the candidate36-bit GPU range.
 * This does NOT establish hardware OAS, SMMU mappings or DMA authorization.
 */
int cepheus_gpu_memory_alloc(struct cepheus_gpu_memory *, vm_size_t);
int cepheus_gpu_memory_sync(const struct cepheus_gpu_memory *);
/* ONLY before activation, or after reset/quiescence AND removing all device
 * mappings/references. The caller owns that proof; never free a live BO.
 */
void cepheus_gpu_memory_free(struct cepheus_gpu_memory *);
void cepheus_gpu_memory_probe(void);
#endif
