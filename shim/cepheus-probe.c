/* First-stage cepheus probe, deliberately separate from the topaz loader.
 * Only the splash framebuffer is written. No UFS, clocks, CPU_ON or reset.
 * Framebuffer base: cont_splash_region in the local boot backup; geometry:
 * initial cepheus Linux DTS. Geometry still requires hardware verification.
 */
#include <stdint.h>
#include "console.h"

#define rsr(r) ({ uint64_t v; __asm__ volatile("mrs %0, " r : "=r"(v)); v; })

static void __attribute__((noreturn)) park(void)
{
	for (;;)
		__asm__ volatile("wfe");
}

void secondary_main(uint64_t unused)
{
	(void)unused;
	park();
}

void exc_handler(uint64_t slot, uint64_t *regs)
{
	(void)regs;
#ifdef CEPHEUS_ENTRY_TEST
	/* An exception must not look like the successful timed reboot. */
	(void)slot;
	park();
#else
	con_clear(BLUE);
	con_hex("EXC", slot);
	con_hex("ESR", rsr("esr_el1"));
	con_hex("ELR", rsr("elr_el1"));
	con_hex("FAR", rsr("far_el1"));
	park();
#endif
}

void shim_main(uint64_t dtb)
{
#ifdef CEPHEUS_ENTRY_TEST
	/* No display access: returning to fastboot after ~5 seconds proves this
	 * code ran. Address is msm-imem base + restart_reason from the live FDT.
	 * The bootloader reason and PSCI reset match the original shim protocol.
	 */
	(void)dtb;
	uint64_t start = rsr("cntpct_el0");
	uint64_t delay = rsr("cntfrq_el0") * 5;
	while (rsr("cntpct_el0") - start < delay)
		;
	*(volatile uint32_t *)0x146bf65cUL = 0x77665500U;
	__asm__ volatile("dsb sy" ::: "memory");
	register uint64_t x0 __asm__("x0") = 0x84000009UL;
	__asm__ volatile("smc #0" : "+r"(x0) : : "x1", "x2", "x3", "x4",
	    "x5", "x6", "x7", "x8", "x9", "x10", "x11", "x12", "x13",
	    "x14", "x15", "x16", "x17", "memory");
	park();
#else
	con_clear(RED);
	con_str("CEPHEUS", "stage 0 - CPU probe");
	con_str("BUILD", "no XNU / no storage access");
	con_hex("EL", rsr("CurrentEL"));
	con_hex("MIDR", rsr("midr_el1"));
	con_hex("MPIDR", rsr("mpidr_el1"));
	con_hex("MMFR0", rsr("id_aa64mmfr0_el1"));
	con_hex("TGRAN16", (rsr("id_aa64mmfr0_el1") >> 20) & 15);
	con_hex("ISAR0", rsr("id_aa64isar0_el1"));
	con_hex("CNTFRQ", rsr("cntfrq_el0"));
	con_hex("SCTLR", rsr("sctlr_el1"));
	con_hex("DTB", dtb);
	con_skip();
	con_str("STATE", "parked; Power + Vol- for fastboot");
	park();
#endif
}
