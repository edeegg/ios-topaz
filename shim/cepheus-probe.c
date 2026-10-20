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
	con_clear(BLUE);
	con_hex("EXC", slot);
	con_hex("ESR", rsr("esr_el1"));
	con_hex("ELR", rsr("elr_el1"));
	con_hex("FAR", rsr("far_el1"));
	park();
}

void shim_main(uint64_t dtb)
{
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
}
