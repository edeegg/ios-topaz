/* SPDX-License-Identifier: MIT
 * Mi9 GPUCC observation only. Addresses verified against live DT and
 * gpucc-sm8150.c at aa587668f86bd02ff1d7f825f5a89a886a84c8d8.
 * No register writes, GPU-core access, reset, RPMh vote or DMA activation.
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <kern/clock.h>
#include <kern/thread.h>
#include <kern/sched_prim.h>
#include <machine/machine_routines.h>
#include <pexpert/pexpert.h>
#include "gpu_memory_xnu.h"

extern vm_offset_t ml_io_map(vm_offset_t, vm_size_t);
extern kern_return_t thread_terminate(thread_t);
void cepheus_gpu_probe_init(void);

static int gpu_probe_sleep;
static bool gpu_probe_started;

static uint32_t gpucc_read(vm_offset_t base, unsigned offset)
{
    uint32_t value = *(volatile uint32_t *)(base + offset);
    __asm__ volatile("dmb oshld" ::: "memory");
    return value;
}

static void gpu_probe_thread(void *arg, wait_result_t result)
{
    (void)arg; (void)result;
    uint64_t deadline;
    clock_interval_to_deadline(3000, NSEC_PER_MSEC, &deadline);
    (void)assert_wait_deadline(&gpu_probe_sleep, THREAD_UNINT, deadline);
    thread_block(THREAD_CONTINUE_NULL);

    /* One 4K range, rounded by this kernel's16K mapper. Never map full GPU.
     * Both GDSCRs and the documented clock/reset control words live here.
     */
    printf("CEPHEUS GPU PROBE: begin GPUCC read-only, no GPU-core access\n");
    vm_offset_t base = ml_io_map(0x02c91000u, 0x1000);
    if (!base) {
        printf("CEPHEUS GPU PROBE: GPUCC map failed\n");
        goto out;
    }
    uint32_t ahb = gpucc_read(base, 0x78);
    if (ahb == 0xffffffffu) {
        printf("CEPHEUS GPU PROBE: invalid AHB read, stopping\n");
        goto out;
    }
    printf("CEPHEUS GPU PROBE: cx-gdscr=%08x gx-gdscr=%08x ahb=%08x\n",
        gpucc_read(base, 0x6c), gpucc_read(base, 0x0c), ahb);
    printf("CEPHEUS GPU PROBE: cx-gmu=%08x gx-gmu=%08x cx-apb=%08x\n",
        gpucc_read(base, 0x98), gpucc_read(base, 0x64), gpucc_read(base, 0x88));
    printf("CEPHEUS GPU PROBE: cxo-aon=%08x cxo=%08x\n",
        gpucc_read(base, 0x04), gpucc_read(base, 0x9c));
    printf("CEPHEUS GPU PROBE: cx-reset=%08x gx-reset=%08x gmu-reset=%08x\n",
        gpucc_read(base, 0x68), gpucc_read(base, 0x08), gpucc_read(base, 0x11c));
    printf("CEPHEUS GPU PROBE: gx-domain=%08x cx-hwctrl=%08x\n",
        gpucc_read(base, 0x508), gpucc_read(base, 0x540));
    printf("CEPHEUS GPU PROBE: done, no hardware state changed\n");
out:
    {
        int memory_test = 0;
        if (PE_parse_boot_argn("cepheusgpu_memtest", &memory_test, sizeof(memory_test)) && memory_test == 1)
            cepheus_gpu_memory_probe();
    }
    (void)thread_terminate(current_thread());
    thread_block(THREAD_CONTINUE_NULL);
}

void cepheus_gpu_probe_init(void)
{
    int enabled = 0;
    if (gpu_probe_started ||
        !PE_parse_boot_argn("cepheusgpu_probe", &enabled, sizeof(enabled)) || enabled != 1)
        return;
    gpu_probe_started = true;
    thread_t thread = THREAD_NULL;
    if (kernel_thread_start(gpu_probe_thread, NULL, &thread) == KERN_SUCCESS)
        thread_deallocate(thread);
    else
        printf("CEPHEUS GPU PROBE: worker creation failed\n");
}
