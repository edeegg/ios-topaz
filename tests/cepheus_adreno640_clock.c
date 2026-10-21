/* SPDX-License-Identifier: GPL-2.0-only
 * Runs the real clock component against an instrumented I/O boundary.
 * This validates sequencing/error handling, never hardware readiness.
 */
#include "../drivers/cepheus/adreno640_clock.h"
struct operation { unsigned domain, reg, value; };
static struct operation log_ops[128];
static unsigned writes, reads, fail_at, clock_value, sp_value;
static int read_reg(void *cookie, enum a640_domain d, uint32_t r, uint32_t *v)
{
    (void)cookie; ++reads;
    if (d == A640_GPU && r == 0xae) *v = clock_value;
    else if (d == A640_GMU && r == 0x1a880) *v = sp_value;
    else return -1;
    return 0;
}
static int write_reg(void *cookie, enum a640_domain d, uint32_t r, uint32_t v)
{
    (void)cookie;
    if (writes >= 128) return -1;
    log_ops[writes].domain=d; log_ops[writes].reg=r; log_ops[writes].value=v;
    ++writes;
    if (fail_at && writes == fail_at) return -1;
    if (d == A640_GMU && r == 0x1a880) sp_value=v;
    if (d == A640_GPU && r == 0xae) clock_value=v;
    return 0;
}
static void reset(void)
{ writes=reads=fail_at=clock_value=0; sp_value=0xa5; }

int test(void)
{
    const struct a640_io io={0,read_reg,write_reg};
    reset();
    if (a640_set_hwcg(&io,0x07040000,15,1)!=A640_UNSUPPORTED || writes || reads) return 1;
    for (unsigned mask=0;mask<15;++mask)
        if (a640_set_hwcg(&io,0x06040001,mask,1)!=A640_NOT_READY || writes || reads) return 2;
    if (a640_set_hwcg(&io,0x06040001,15,2)!=A640_INVALID || writes || reads) return 3;
    if (a640_set_hwcg(&io,0x06040001,15,1)!=A640_OK || writes!=55 || reads!=3) return 4;
    if (log_ops[0].domain!=A640_GMU || log_ops[0].reg!=0x23b09 || log_ops[0].value!=0x20202) return 5;
    if (log_ops[3].reg!=0x1a880 || log_ops[3].value!=0xa4) return 6;
    if (log_ops[4].reg!=0xb0 || log_ops[4].value!=0x02222222) return 7;
    if (log_ops[53].reg!=0x1a880 || log_ops[53].value!=0xa5) return 8;
    if (clock_value!=0x8aa8aa82) return 9;
    writes=reads=0;
    if (a640_set_hwcg(&io,0x06040001,15,1)!=A640_OK || writes!=3 || reads!=1) return 10;
    reset(); clock_value=0x8aa8aa82;
    if (a640_set_hwcg(&io,0x06040001,15,0)!=A640_OK || clock_value) return 11;
    for (unsigned i=4;i<53;++i) if(log_ops[i].value) return 12;
    reset(); fail_at=7;
    if (a640_set_hwcg(&io,0x06040001,15,1)!=A640_IO_ERROR || writes!=7) return 13;
    reset();
    if (a640_set_gbif(&io,0x06040001,15)!=A640_OK || writes!=5 || reads) return 14;
    for (unsigned i=0;i<4;++i)
        if(log_ops[i].reg!=0x3c03+i || log_ops[i].value!=0x71620) return 15;
    if(log_ops[4].reg!=0x11 || log_ops[4].value!=3) return 16;
    return 0;
}
