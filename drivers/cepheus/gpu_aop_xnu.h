/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef CEPHEUS_GPU_AOP_XNU_H
#define CEPHEUS_GPU_AOP_XNU_H
#include <sys/param.h>
/* Explicit cepheusgpu_aop=1 experiment: firmware-controlled CX request only.
 * Does not establish rail voltage policy, change clock rates or start GMU/GX.
 */
void cepheus_gpu_aop_start(vm_offset_t gpucc);
#endif
