/* SPDX-License-Identifier: GPL-2.0-only
 * QMP protocol adapted from Linux Foundation msm_qmp.c (2017-2020), pinned
 * android_kernel_xiaomi_cepheus aa587668f86bd02ff1d7f825f5a89a886a84c8d8.
 * This transport does not replace RPMh regulator votes or enable GPU power.
 */
#include "adreno640_qmp.h"
#define UP 0x0000ffffu
#define DOWN 0xffff0000u
static int valid_state(uint32_t v) { return v == UP || v == DOWN; }
static int valid_range(uint32_t offset, uint32_t bytes)
{ return offset >= 64 && !(offset & 3) && bytes >= 8 && !(bytes & 3) && offset < 4096 && bytes <= 4096-offset; }
static enum a640_qmp_result snapshot(struct a640_qmp *q, uint32_t d[16])
{
    for (unsigned i=0;i<16;++i)
        if(q->io.read(q->io.cookie,i*4,&d[i])) return A640_QMP_IO;
    if(d[0]!=0x4d41494c || d[1]!=1 || d[2]!=0 ||
       !valid_range(d[8],d[7]) || !valid_range(d[14],d[13]) ||
       (d[8]<d[14]+d[13] && d[14]<d[8]+d[7])) return A640_QMP_DESCRIPTOR;
    for(unsigned i=3;i<7;++i) if(!valid_state(d[i])) return A640_QMP_DESCRIPTOR;
    for(unsigned i=9;i<13;++i) if(!valid_state(d[i])) return A640_QMP_DESCRIPTOR;
    return A640_QMP_OK;
}
static int fully_connected(const uint32_t d[16])
{ return d[3]==UP && d[4]==UP && d[5]==UP && d[6]==UP && d[9]==UP && d[10]==UP && d[11]==UP && d[12]==UP; }
static int valid_io(const struct a640_qmp *q, unsigned timeout)
{ return q && q->exclusive==1 && timeout && timeout<=5000000 && q->io.read && q->io.write && q->io.barrier && q->io.notify && q->io.wait_us; }
static int signal_remote(struct a640_qmp *q)
{ return q->io.barrier(q->io.cookie) || q->io.notify(q->io.cookie); }
static int wait_step(struct a640_qmp *q, unsigned *remaining)
{
    unsigned step=*remaining>1000?1000:*remaining;
    *remaining-=step;
    return q->io.wait_us(q->io.cookie,step);
}
enum a640_qmp_result a640_qmp_connect(struct a640_qmp *q,unsigned timeout)
{
    if(!valid_io(q,timeout)) return A640_QMP_ARGUMENT;
    q->connected=0;
    uint32_t d[16],pending;
    enum a640_qmp_result status=snapshot(q,d);
    if(status) return status;
    if(q->io.read(q->io.cookie,d[14],&pending)) return A640_QMP_IO;
    if(pending) return A640_QMP_BUSY;
    if(q->io.read(q->io.cookie,d[8],&pending)) return A640_QMP_IO;
    if(pending) return A640_QMP_BUSY;
    const uint32_t tx=d[14],tx_bytes=d[13],rx=d[8],rx_bytes=d[7];
    for(;;) {
        int changed=0;
        if(d[14]!=tx || d[13]!=tx_bytes || d[8]!=rx || d[7]!=rx_bytes)
            return A640_QMP_DESCRIPTOR;
        if(d[9]==DOWN) {
            /* Same local initialization as init_mcore_state, preserving ranges. */
            if(q->io.write(q->io.cookie,40,DOWN) || q->io.write(q->io.cookie,44,DOWN) ||
               q->io.write(q->io.cookie,48,DOWN) || q->io.write(q->io.cookie,36,UP)) return A640_QMP_IO;
            changed=1;
        }
        if(d[4]!=d[3]) {
            if(q->io.write(q->io.cookie,16,d[3])) return A640_QMP_IO;
            changed=1;
        }
        if(d[3]==UP && d[4]==UP && d[9]==UP && d[10]==UP) {
            if(d[11]==DOWN) {
                if(q->io.write(q->io.cookie,44,UP)) return A640_QMP_IO;
                changed=1;
            }
            if(d[6]!=d[5]) {
                if(q->io.write(q->io.cookie,24,d[5])) return A640_QMP_IO;
                changed=1;
            }
        }
        if(changed && signal_remote(q)) return A640_QMP_IO;
        if(fully_connected(d)) {
            if(q->io.read(q->io.cookie,tx,&pending)) return A640_QMP_IO;
            if(pending) return A640_QMP_BUSY;
            if(q->io.read(q->io.cookie,rx,&pending)) return A640_QMP_IO;
            if(pending) return A640_QMP_BUSY;
            q->tx_offset=tx; q->tx_bytes=tx_bytes; q->rx_offset=rx; q->rx_bytes=rx_bytes;
            q->connected=1;
            return A640_QMP_OK;
        }
        if(!timeout) return A640_QMP_TIMEOUT;
        if(wait_step(q,&timeout)) return A640_QMP_IO;
        status=snapshot(q,d);
        if(status) return status;
    }
}
enum a640_qmp_result a640_qmp_send(struct a640_qmp *q,const uint8_t *data,size_t bytes,unsigned timeout)
{
    if(!valid_io(q,timeout) || !data || !bytes || (bytes&3) || bytes>4092)
        return A640_QMP_ARGUMENT;
    if(q->connected!=1) return A640_QMP_DISCONNECTED;
    uint32_t d[16],pending;
    enum a640_qmp_result status=snapshot(q,d);
    if(status) goto lost;
    if(!fully_connected(d)) {status=A640_QMP_DISCONNECTED;goto lost;}
    if(d[14]!=q->tx_offset || d[13]!=q->tx_bytes || d[8]!=q->rx_offset || d[7]!=q->rx_bytes) {
        status=A640_QMP_DESCRIPTOR;goto lost;
    }
    if(bytes>q->tx_bytes-4) return A640_QMP_ARGUMENT;
    if(q->io.read(q->io.cookie,q->tx_offset,&pending)) {status=A640_QMP_IO;goto lost;}
    if(pending) return A640_QMP_BUSY;
    if(q->io.read(q->io.cookie,q->rx_offset,&pending)) {status=A640_QMP_IO;goto lost;}
    if(pending) return A640_QMP_BUSY;
    for(size_t i=0;i<bytes;i+=4) {
        uint32_t value=(uint32_t)data[i] | (uint32_t)data[i+1]<<8 | (uint32_t)data[i+2]<<16 | (uint32_t)data[i+3]<<24;
        if(q->io.write(q->io.cookie,q->tx_offset+4+(uint32_t)i,value)) {status=A640_QMP_IO;goto lost;}
    }
    if(q->io.barrier(q->io.cookie) || q->io.write(q->io.cookie,q->tx_offset,(uint32_t)bytes) ||
       q->io.read(q->io.cookie,q->tx_offset,&pending) || pending!=bytes || signal_remote(q)) {
        status=A640_QMP_IO;goto lost;
    }
    for(;;) {
        status=snapshot(q,d);
        if(status) goto lost;
        if(!fully_connected(d)) {status=A640_QMP_DISCONNECTED;goto lost;}
        if(d[14]!=q->tx_offset || d[13]!=q->tx_bytes || d[8]!=q->rx_offset || d[7]!=q->rx_bytes) {
            status=A640_QMP_DESCRIPTOR;goto lost;
        }
        if(q->io.read(q->io.cookie,q->tx_offset,&pending)) {status=A640_QMP_IO;goto lost;}
        if(!pending) return A640_QMP_OK;
        if(!timeout) {status=A640_QMP_TIMEOUT;goto lost;}
        if(wait_step(q,&timeout)) {status=A640_QMP_IO;goto lost;}
    }
lost:
    q->connected=0;
    return status;
}
