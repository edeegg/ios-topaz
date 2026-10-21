/* SPDX-License-Identifier: GPL-2.0-only
 * Register data derived from the Linux Foundation Mi9 kernel:
 * drivers/gpu/msm/adreno_a6xx.c and a6xx_reg.h.
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 * Pinned revision: aa587668f86bd02ff1d7f825f5a89a886a84c8d8
 * Offsets are DWORD indices, not byte offsets. GMU uses a separate domain.
 */
#ifndef CEPHEUS_A640_REGISTERS_H
#define CEPHEUS_A640_REGISTERS_H
struct a640_pair { unsigned reg, value; };
#define A6XX_RBBM_CLOCK_CNTL 0xaeu
#define A6XX_GPU_GMU_AO_GMU_CGC_MODE_CNTL 0x23b09u
#define A6XX_GPU_GMU_AO_GMU_CGC_DELAY_CNTL 0x23b0au
#define A6XX_GPU_GMU_AO_GMU_CGC_HYST_CNTL 0x23b0bu
#define A6XX_GPU_GMU_GX_SPTPRAC_CLOCK_CONTROL 0x1a880u
static const struct a640_pair a640_hwcg[] = {
    {0xb0u, 0x2222222u}, /* A6XX_RBBM_CLOCK_CNTL_SP0 */
    {0xb4u, 0x2222220u}, /* A6XX_RBBM_CLOCK_CNTL2_SP0 */
    {0xb8u, 0x80u}, /* A6XX_RBBM_CLOCK_DELAY_SP0 */
    {0xbcu, 0xf3cfu}, /* A6XX_RBBM_CLOCK_HYST_SP0 */
    {0xc0u, 0x2222222u}, /* A6XX_RBBM_CLOCK_CNTL_TP0 */
    {0xc4u, 0x22222222u}, /* A6XX_RBBM_CLOCK_CNTL2_TP0 */
    {0xc8u, 0x22222222u}, /* A6XX_RBBM_CLOCK_CNTL3_TP0 */
    {0xccu, 0x22222u}, /* A6XX_RBBM_CLOCK_CNTL4_TP0 */
    {0xd0u, 0x11111111u}, /* A6XX_RBBM_CLOCK_DELAY_TP0 */
    {0xd4u, 0x11111111u}, /* A6XX_RBBM_CLOCK_DELAY2_TP0 */
    {0xd8u, 0x11111111u}, /* A6XX_RBBM_CLOCK_DELAY3_TP0 */
    {0xdcu, 0x11111u}, /* A6XX_RBBM_CLOCK_DELAY4_TP0 */
    {0xe0u, 0x77777777u}, /* A6XX_RBBM_CLOCK_HYST_TP0 */
    {0xe4u, 0x77777777u}, /* A6XX_RBBM_CLOCK_HYST2_TP0 */
    {0xe8u, 0x77777777u}, /* A6XX_RBBM_CLOCK_HYST3_TP0 */
    {0xecu, 0x77777u}, /* A6XX_RBBM_CLOCK_HYST4_TP0 */
    {0xf0u, 0x22222222u}, /* A6XX_RBBM_CLOCK_CNTL_RB0 */
    {0xf4u, 0x1002222u}, /* A6XX_RBBM_CLOCK_CNTL2_RB0 */
    {0xf8u, 0x2220u}, /* A6XX_RBBM_CLOCK_CNTL_CCU0 */
    {0x100u, 0x40f00u}, /* A6XX_RBBM_CLOCK_HYST_RB_CCU0 */
    {0x104u, 0x5222022u}, /* A6XX_RBBM_CLOCK_CNTL_RAC */
    {0x105u, 0x5555u}, /* A6XX_RBBM_CLOCK_CNTL2_RAC */
    {0x106u, 0x11u}, /* A6XX_RBBM_CLOCK_DELAY_RAC */
    {0x107u, 0x445044u}, /* A6XX_RBBM_CLOCK_HYST_RAC */
    {0x108u, 0x4222222u}, /* A6XX_RBBM_CLOCK_CNTL_TSE_RAS_RBBM */
    {0x111u, 0x2222u}, /* A6XX_RBBM_CLOCK_MODE_VFD */
    {0x114u, 0x222222u}, /* A6XX_RBBM_CLOCK_MODE_GPC */
    {0x117u, 0x2u}, /* A6XX_RBBM_CLOCK_DELAY_HLSQ_2 */
    {0x11bu, 0x2222u}, /* A6XX_RBBM_CLOCK_MODE_HLSQ */
    {0x109u, 0x4000u}, /* A6XX_RBBM_CLOCK_DELAY_TSE_RAS_RBBM */
    {0x112u, 0x2222u}, /* A6XX_RBBM_CLOCK_DELAY_VFD */
    {0x115u, 0x200u}, /* A6XX_RBBM_CLOCK_DELAY_GPC */
    {0x11cu, 0x0u}, /* A6XX_RBBM_CLOCK_DELAY_HLSQ */
    {0x10au, 0x0u}, /* A6XX_RBBM_CLOCK_HYST_TSE_RAS_RBBM */
    {0x113u, 0x0u}, /* A6XX_RBBM_CLOCK_HYST_VFD */
    {0x116u, 0x4104004u}, /* A6XX_RBBM_CLOCK_HYST_GPC */
    {0x11du, 0x0u}, /* A6XX_RBBM_CLOCK_HYST_HLSQ */
    {0x120u, 0x222u}, /* A6XX_RBBM_CLOCK_CNTL_TEX_FCHE */
    {0x121u, 0x111u}, /* A6XX_RBBM_CLOCK_DELAY_TEX_FCHE */
    {0x122u, 0x777u}, /* A6XX_RBBM_CLOCK_HYST_TEX_FCHE */
    {0x10bu, 0x22222222u}, /* A6XX_RBBM_CLOCK_CNTL_UCHE */
    {0x110u, 0x4u}, /* A6XX_RBBM_CLOCK_HYST_UCHE */
    {0x10fu, 0x2u}, /* A6XX_RBBM_CLOCK_DELAY_UCHE */
    {0x533u, 0x182u}, /* A6XX_RBBM_ISDB_CNT */
    {0x44u, 0x0u}, /* A6XX_RBBM_RAC_THRESHOLD_CNT */
    {0x42u, 0x0u}, /* A6XX_RBBM_SP_HYST_CNT */
    {0x118u, 0x222u}, /* A6XX_RBBM_CLOCK_CNTL_GMU_GX */
    {0x119u, 0x111u}, /* A6XX_RBBM_CLOCK_DELAY_GMU_GX */
    {0x11au, 0x555u}, /* A6XX_RBBM_CLOCK_HYST_GMU_GX */
};
static const struct a640_pair a640_gbif[] = {
    {0x3c03u, 0x71620u}, /* A6XX_GBIF_QSB_SIDE0 */
    {0x3c04u, 0x71620u}, /* A6XX_GBIF_QSB_SIDE1 */
    {0x3c05u, 0x71620u}, /* A6XX_GBIF_QSB_SIDE2 */
    {0x3c06u, 0x71620u}, /* A6XX_GBIF_QSB_SIDE3 */
    {0x11u, 0x3u}, /* A6XX_RBBM_GBIF_CLIENT_QOS_CNTL */
};
#endif
