/* SPDX-License-Identifier: GPL-2.0-only */
#include "../drivers/cepheus/adreno640_qmp.h"
#define UP 0xffffu
#define DOWN 0xffff0000u
static uint32_t ram[1024];
static unsigned writes, notifications, waits, block_tx, change_range, disconnect_tx, fail_write, fail_wait;
static struct a640_qmp q;
static int rd(void *ctx,uint32_t offset,uint32_t *v)
{ (void)ctx; if(offset>=4096 || (offset&3)) return -1; *v=ram[offset/4]; return 0; }
static int wr(void *ctx,uint32_t offset,uint32_t v)
{
    (void)ctx;
    if(offset>=4096 || (offset&3)) return -1;
    ++writes;
    if(fail_write && writes==fail_write) return -1;
    ram[offset/4]=v; return 0;
}
static int barrier(void *ctx) { (void)ctx;return 0; }
static int notify(void *ctx)
{
    (void)ctx;++notifications;
    if(ram[9]==UP) ram[10]=UP;
    if(ram[11]==UP) {ram[12]=UP;ram[5]=UP;}
    return 0;
}
static int wait_us(void *ctx,unsigned us)
{
    (void)ctx;(void)us;++waits;
    if(fail_wait) return -1;
    if(ram[64]) {
        if(disconnect_tx) ram[5]=DOWN;
        if(change_range) ram[14]=768;
        if(!block_tx) ram[64]=0;
    }
    return 0;
}
static void reset(void)
{
    for(unsigned i=0;i<1024;++i) ram[i]=0;
    writes=notifications=waits=block_tx=change_range=disconnect_tx=fail_write=fail_wait=0;
    ram[0]=0x4d41494c;ram[1]=1;
    ram[3]=UP;ram[4]=DOWN;ram[5]=DOWN;ram[6]=DOWN;ram[7]=128;ram[8]=128;
    ram[9]=DOWN;ram[10]=DOWN;ram[11]=DOWN;ram[12]=DOWN;ram[13]=128;ram[14]=256;
    q.io.cookie=0;q.io.read=rd;q.io.write=wr;q.io.barrier=barrier;q.io.notify=notify;q.io.wait_us=wait_us;
    q.exclusive=1;q.connected=0;q.tx_offset=q.tx_bytes=q.rx_offset=q.rx_bytes=0;
}
int test(void)
{
    static const uint8_t packet[8]={'t','e','s','t',0,0,0,0};
    reset();q.exclusive=0;
    if(a640_qmp_connect(&q,5000)!=A640_QMP_ARGUMENT || writes) return 1;
    reset();ram[0]=0;
    if(a640_qmp_connect(&q,5000)!=A640_QMP_DESCRIPTOR || writes) return 2;
    reset();ram[14]=ram[8];
    if(a640_qmp_connect(&q,5000)!=A640_QMP_DESCRIPTOR || writes) return 3;
    reset();ram[14]=4092;
    if(a640_qmp_connect(&q,5000)!=A640_QMP_DESCRIPTOR || writes) return 4;
    reset();ram[64]=4;
    if(a640_qmp_connect(&q,5000)!=A640_QMP_BUSY || writes) return 5;
    reset();
    if(a640_qmp_connect(&q,5000) || q.connected!=1 || notifications!=3) return 6;
    if(a640_qmp_send(&q,packet,8,5000) || ram[65]!=0x74736574 || ram[66] || ram[64]) return 7;
    unsigned before=writes;
    ram[32]=4;
    if(a640_qmp_send(&q,packet,8,5000)!=A640_QMP_BUSY || writes!=before) return 8;
    reset();
    if(a640_qmp_connect(&q,5000)) return 9;
    block_tx=1;
    if(a640_qmp_send(&q,packet,8,2000)!=A640_QMP_TIMEOUT || q.connected || !ram[64]) return 10;
    before=writes;
    if(a640_qmp_send(&q,packet,8,2000)!=A640_QMP_DISCONNECTED || writes!=before) return 11;
    reset();if(a640_qmp_connect(&q,5000)) return 12;
    change_range=1;
    if(a640_qmp_send(&q,packet,8,5000)!=A640_QMP_DESCRIPTOR || q.connected) return 13;
    reset();if(a640_qmp_connect(&q,5000)) return 14;
    disconnect_tx=1;
    if(a640_qmp_send(&q,packet,8,5000)!=A640_QMP_DISCONNECTED || q.connected) return 15;
    for(unsigned i=1;i<=4;++i) {
        reset();fail_write=i;
        if(a640_qmp_connect(&q,5000)!=A640_QMP_IO || writes!=i || q.connected) return 16;
    }
    reset();if(a640_qmp_connect(&q,5000)) return 17;
    fail_write=writes+2;
    if(a640_qmp_send(&q,packet,8,5000)!=A640_QMP_IO || q.connected || ram[64]) return 18;
    return 0;
}
