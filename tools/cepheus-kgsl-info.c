/* Android/recovery-only KGSL metadata probe, not an XNU GPU driver.
 * ABI: include/uapi/linux/msm_kgsl.h (Mi9 Android kernel).
 * Opens the existing driver and requests DEVICE_INFO; submits no GPU commands.
 * stdout: signed64 open result, signed64 ioctl result, 40-byte kgsl_devinfo.
 */
typedef unsigned long long u64;
struct info {unsigned device_id,chip_id,mmu_enabled; unsigned long gmem_base; unsigned gpu_id; unsigned long gmem_size;};
struct property {unsigned type; void *value; unsigned long size;};
static struct {long opened,queried; struct info gpu;} output;
static struct property property={1,&output.gpu,sizeof(struct info)};
_Static_assert(sizeof(struct info)==40,"KGSL arm64 device ABI");
_Static_assert(sizeof(struct property)==24,"KGSL arm64 property ABI");
static long call(long n,long a,long b,long c,long d) {
 register long x0 __asm__("x0")=a; register long x1 __asm__("x1")=b;
 register long x2 __asm__("x2")=c; register long x3 __asm__("x3")=d;
 register long x8 __asm__("x8")=n;
 __asm__ volatile("svc #0":"+r"(x0):"r"(x1),"r"(x2),"r"(x3),"r"(x8):"memory","cc");return x0;
}
void _start(void) {
 output.opened=call(56,-100,(long)"/dev/kgsl-3d0",2,0);
 output.queried=output.opened<0 ? output.opened : call(29,output.opened,0xc0180902UL,(long)&property,0);
 if(output.opened>=0)call(57,output.opened,0,0,0);
 call(64,1,(long)&output,sizeof(output),0);
 call(93,output.queried<0 ? 1:0,0,0,0);
 for(;;){}
}
