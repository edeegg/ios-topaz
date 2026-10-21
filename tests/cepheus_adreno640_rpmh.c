/* SPDX-License-Identifier: GPL-2.0-only */
#include "../drivers/cepheus/adreno640_rpmh.h"
static uint32_t regs[3072],config;
static unsigned writes,triggered,waits,block,fail_at,bad_status;
static struct a640_rpmh owner;
static int read_reg(void *cookie,unsigned domain,uint32_t offset,uint32_t *value)
{
    (void)cookie;
    if(domain==0) {if(offset!=12)return -1;*value=config;return 0;}
    if(domain!=1 || offset>=sizeof(regs) || (offset&3))return -1;
    *value=regs[offset/4];return 0;
}
static int write_reg(void *cookie,unsigned domain,uint32_t offset,uint32_t value)
{
    (void)cookie;
    if(domain!=1 || offset>=sizeof(regs) || (offset&3))return -1;
    ++writes;if(fail_at && writes==fail_at)return -1;
    if(offset==8)regs[1]&=~value;
    else regs[offset/4]=value;
    if(offset==0x2b4 && (value&(1u<<24))) {triggered=1;regs[0x2b8/4]=0;}
    return 0;
}
static int barrier(void *cookie){(void)cookie;return 0;}
static int wait_us(void *cookie,unsigned us)
{
    (void)cookie;(void)us;++waits;
    if(triggered && !block) {
        regs[1]|=2;regs[0x2dc/4]=bad_status?0x100:0x10100;regs[0x2b8/4]=1;
    }
    return 0;
}
static void reset(void)
{
    for(unsigned i=0;i<3072;++i)regs[i]=0;
    config=(16u<<27)|(8u<<12);regs[0x2b8/4]=1;regs[0x18/4]=1;
    writes=triggered=waits=block=fail_at=bad_status=0;
    owner.io.cookie=0;owner.io.read=read_reg;owner.io.write=write_reg;
    owner.io.barrier=barrier;owner.io.wait_us=wait_us;owner.exclusive=1;owner.poisoned=0;
    owner.voltage_policy_validated=1;
}
int test(void)
{
    reset();owner.exclusive=0;
    if(a640_rpmh_vote(&owner,0x30000,3)!=A640_RPMH_ARGUMENT || writes)return 1;
    reset();owner.voltage_policy_validated=0;
    if(a640_rpmh_vote(&owner,0x30000,3)!=A640_RPMH_ARGUMENT || writes)return 10;
    reset();
    if(a640_rpmh_vote(&owner,0x30000,0)!=A640_RPMH_ARGUMENT || writes)return 2;
    config=0;
    if(a640_rpmh_vote(&owner,0x30000,3)!=A640_RPMH_CAPABILITY || writes)return 3;
    reset();regs[0x18/4]=regs[0x2b8/4]=0;
    if(a640_rpmh_vote(&owner,0x30000,3)!=A640_RPMH_BUSY || writes)return 4;
    reset();regs[0]=0x82;regs[1]=0x40;
    if(a640_rpmh_vote(&owner,0x30000,3) || owner.poisoned || !triggered ||
       regs[0]!=0x82 || regs[1]!=0x40 || regs[0x2d0/4]!=0x10108 ||
       regs[0x2d4/4]!=0x30000 || regs[0x2d8/4]!=3 || regs[0x2bc/4])return 5;
    unsigned total=writes;
    for(unsigned i=1;i<=total;++i) {
        reset();fail_at=i;
        if(a640_rpmh_vote(&owner,0x30000,3)!=A640_RPMH_IO || !owner.poisoned || writes!=i)return 6;
    }
    reset();block=1;
    if(a640_rpmh_vote(&owner,0x30010,2)!=A640_RPMH_TIMEOUT || !owner.poisoned || waits!=50000)return 7;
    unsigned before=writes;
    if(a640_rpmh_vote(&owner,0x30010,2)!=A640_RPMH_ARGUMENT || writes!=before)return 8;
    reset();bad_status=1;
    if(a640_rpmh_vote(&owner,0x30090,3)!=A640_RPMH_IO || !owner.poisoned)return 9;
    return 0;
}
