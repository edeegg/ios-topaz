/* SPDX-License-Identifier: GPL-2.0-only
 * Mi9 AOP resource and mailbox layout from live DT, message from
 * gdsc-regulator.c at aa587668f86bd02ff1d7f825f5a89a886a84c8d8.
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <kern/clock.h>
#include <kern/thread.h>
#include <kern/sched_prim.h>
#include <machine/machine_routines.h>
#include <pexpert/pexpert.h>
#include "adreno640_qmp.h"
#include "gpu_aop_xnu.h"
extern vm_offset_t ml_io_map(vm_offset_t, vm_size_t);
struct aop_context { vm_offset_t ram, irq; int sleep_channel; };
static struct aop_context aop;
static bool aop_started;
static int read32(void *cookie,uint32_t offset,uint32_t *value)
{
    struct aop_context *ctx=cookie;
    if(!ctx || !ctx->ram || !value || offset>=0x1000 || (offset&3)) return -1;
    *value=*(volatile uint32_t *)(ctx->ram+offset);
    __asm__ volatile("dmb oshld" ::: "memory");
    return 0;
}
static int write32(void *cookie,uint32_t offset,uint32_t value)
{
    struct aop_context *ctx=cookie;
    if(!ctx || !ctx->ram || offset>=0x1000 || (offset&3)) return -1;
    __asm__ volatile("dmb oshst" ::: "memory");
    *(volatile uint32_t *)(ctx->ram+offset)=value;
    __asm__ volatile("dsb osh" ::: "memory");
    return 0;
}
static int barrier(void *cookie)
{ (void)cookie; __asm__ volatile("dsb osh" ::: "memory");return 0; }
static int notify(void *cookie)
{
    struct aop_context *ctx=cookie;
    if(!ctx || !ctx->irq) return -1;
    __asm__ volatile("dmb oshst" ::: "memory");
    *(volatile uint32_t *)ctx->irq=1;
    __asm__ volatile("dsb osh" ::: "memory");
    return 0;
}
static int wait_us(void *cookie,unsigned us)
{
    struct aop_context *ctx=cookie;
    uint64_t deadline;
    if(!ctx || !us || us>1000) return -1;
    clock_interval_to_deadline(us,NSEC_PER_USEC,&deadline);
    (void)assert_wait_deadline(&ctx->sleep_channel,THREAD_UNINT,deadline);
    thread_block(THREAD_CONTINUE_NULL);
    return 0;
}
void cepheus_gpu_aop_start(vm_offset_t gpucc)
{
    int enabled=0;
    if(aop_started || !gpucc || !PE_parse_boot_argn("cepheusgpu_aop",&enabled,sizeof(enabled)) || enabled!=1)
        return;
    aop_started=true;
    /* All prior Linux/ABL APSS clients have stopped; this kernel has no other
     * AOP client. Never clear pending messages or claim an occupied mailbox.
     */
    aop.ram=ml_io_map(0x0c300000,0x1000);
    aop.irq=ml_io_map(0x17c0000c,4);
    if(!aop.ram || !aop.irq) {printf("CEPHEUS GPU AOP: mapping failed\n");return;}
    struct a640_qmp q={0};
    q.io.cookie=&aop;q.io.read=read32;q.io.write=write32;
    q.io.barrier=barrier;q.io.notify=notify;q.io.wait_us=wait_us;q.exclusive=1;
    enum a640_qmp_result result=a640_qmp_connect(&q,5000000);
    printf("CEPHEUS GPU AOP: link result=%u connected=%u\n",(unsigned)result,q.connected);
    if(result!=A640_QMP_OK) return;
    /* OEM GDSC mailbox request. Existing boot-firmware rail settings are
     * preserved: no RPMh voltage writes, clock-rate changes or GMU start.
     */
    static const uint8_t request[96]="{class: clock, res: gpu_noc_wa}";
    result=a640_qmp_send(&q,request,sizeof(request),1000000);
    printf("CEPHEUS GPU AOP: GPU NoC request result=%u\n",(unsigned)result);
    if(result!=A640_QMP_OK) return;
    uint32_t state=0;
    for(unsigned i=0;i<500;++i) {
        state=*(volatile uint32_t *)(gpucc+0x6c);
        __asm__ volatile("dmb oshld" ::: "memory");
        if((state&0x80000001u)==0x80000000u) {
            printf("CEPHEUS GPU AOP: CX power confirmed, gdscr=%08x; GMU/GX not started\n",state);
            return;
        }
        (void)wait_us(&aop,1000);
    }
    printf("CEPHEUS GPU AOP: CX not confirmed, gdscr=%08x; stopping without forced GDSCR writes\n",state);
}
