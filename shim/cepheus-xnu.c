/* Mi 9 console-only XNU handoff. No storage, clocks or secondary CPU writes. */
#include <stdint.h>
#include "console.h"
#include "loader.h"

#define rsr(r) ({ uint64_t v; __asm__ volatile("mrs %0, " r : "=r"(v)); v; })

static void __attribute__((noreturn)) park(void)
{
	for (;;) __asm__ volatile("wfe");
}

void secondary_main(uint64_t unused)
{
	(void)unused;
	park();
}

void exc_handler(uint64_t slot, uint64_t *regs)
{
	(void)regs;
	con_hex("EXC", slot);
	con_hex("ESR", rsr("esr_el1"));
	con_hex("ELR", rsr("elr_el1"));
	con_hex("FAR", rsr("far_el1"));
	park();
}

void shim_main(uint64_t dtb)
{
	con_clear(RED);
	con_str("CEPHEUS", "experimental XNU console / no storage");
	uint64_t mpidr = rsr("mpidr_el1");
	uint64_t granule = (rsr("id_aa64mmfr0_el1") >> 20) & 15;
	con_hex("MPIDR", mpidr);
	con_hex("TGRAN16", granule);
	con_hex("CNTFRQ", rsr("cntfrq_el0"));
	/* Match the candidate single-CPU ADT. Never hand off on another core. */
	if ((mpidr & 0xff00ffffffULL) != 0 || (granule != 1 && granule != 2) ||
	    rsr("CurrentEL") != 4 || (rsr("sctlr_el1") & 5) != 0 ||
	    rsr("cntfrq_el0") != 19200000) {
		con_str("STOP", "CPU/16K/timer/entry state differs from candidate");
		park();
	}
	con_str("STOP", loader_boot(dtb));
	park();
}
