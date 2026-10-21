/* SPDX-License-Identifier: GPL-2.0-only
 * APPS active-TCS protocol adapted from Linux Foundation (2016-2019)
 * drivers/mailbox/qcom-rpmh-mailbox.c at aa587668f86bd02ff1d7f825f5a89a886a84c8d8.
 * Policy bound to live Mi9 DRV2/active0,1 and verified CMD DB level indices.
 */
#include "adreno640_rpmh.h"
#define STRIDE 0x2a0u
static int read_reg(struct a640_rpmh *r,uint32_t offset,uint32_t *value)
{ return r->io.read(r->io.cookie,1,offset,value); }
static int write_reg(struct a640_rpmh *r,uint32_t offset,uint32_t value)
{ return r->io.write(r->io.cookie,1,offset,value); }
static int write_sync(struct a640_rpmh *r,uint32_t offset,uint32_t value)
{
    for(unsigned i=0;i<100;++i) {
        uint32_t actual;
        if(write_reg(r,offset,value) || r->io.barrier(r->io.cookie) || read_reg(r,offset,&actual)) return -1;
        if(actual==value) return 0;
        if(r->io.wait_us(r->io.cookie,1)) return -1;
    }
    return -1;
}
enum a640_rpmh_result a640_rpmh_vote(struct a640_rpmh *r,uint32_t resource,uint32_t index)
{
    if(!r || r->exclusive!=1 || r->voltage_policy_validated!=1 || r->poisoned || !r->io.read || !r->io.write ||
       !r->io.barrier || !r->io.wait_us ||
       !((resource==0x30010 && index==2) || (resource==0x30000 && index==3) ||
         (resource==0x30090 && index==3))) return A640_RPMH_ARGUMENT;
    uint32_t config;
    if(r->io.read(r->io.cookie,0,0x0c,&config)) goto io_error;
    if(((config>>12)&63)<8 || ((config>>27)&31)<1 || ((config>>27)&31)>16)
        return A640_RPMH_CAPABILITY;
    unsigned selected=2;
    for(unsigned candidate=0;candidate<2;++candidate) {
        unsigned tcs=1-candidate;
        uint32_t status,enabled;
        if(read_reg(r,tcs*STRIDE+0x18,&status) || read_reg(r,tcs*STRIDE+0x1c,&enabled)) goto io_error;
        if(status==1 && !enabled) {selected=tcs;break;}
    }
    if(selected==2) return A640_RPMH_BUSY;
    const uint32_t base=selected*STRIDE,bit=1u<<selected;
    uint32_t interrupt_enable,control,actual;
    if(read_reg(r,0,&interrupt_enable) || read_reg(r,base+0x14,&control)) goto io_error;
    /* Mask only the owned TCS interrupt; poll its latched status. */
    if(write_sync(r,0,interrupt_enable&~bit) || write_reg(r,8,bit) ||
       r->io.barrier(r->io.cookie) || read_reg(r,4,&actual) || (actual&bit)) goto io_error;
    if(write_reg(r,base+0x30,0x10108) || write_reg(r,base+0x34,resource) ||
       write_reg(r,base+0x38,index) || write_reg(r,base+0x10,1) ||
       write_reg(r,base+0x1c,1)) goto io_error;
    control&=~(1u<<24);
    if(write_sync(r,base+0x14,control)) goto io_error;
    control&=~(1u<<16);
    if(write_sync(r,base+0x14,control) || write_sync(r,base+0x14,1u<<16) ||
       r->io.barrier(r->io.cookie) || write_reg(r,base+0x14,(1u<<16)|(1u<<24)) ||
       r->io.barrier(r->io.cookie)) goto io_error;
    for(unsigned elapsed=0;elapsed<=50000;++elapsed) {
        if(read_reg(r,4,&actual)) goto io_error;
        if(actual&bit) {
            uint32_t command,status;
            if(read_reg(r,base+0x3c,&command) || read_reg(r,base+0x18,&status)) goto io_error;
            if((command&0x10100)!=0x10100 || !status) goto io_error;
            if(write_reg(r,base+0x1c,0) || write_reg(r,base+0x10,0) ||
               write_reg(r,8,bit) || r->io.barrier(r->io.cookie) ||
               read_reg(r,4,&actual) || (actual&bit) ||
               write_sync(r,0,interrupt_enable)) goto io_error;
            return A640_RPMH_OK;
        }
        if(elapsed==50000) break;
        if(r->io.wait_us(r->io.cookie,1)) goto io_error;
    }
    r->poisoned=1;
    return A640_RPMH_TIMEOUT;
io_error:
    r->poisoned=1;
    return A640_RPMH_IO;
}
