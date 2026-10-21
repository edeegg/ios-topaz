#!/usr/bin/env python3
"""Extract A640 clock/GBIF data from the pinned Mi9 Android sources.

No hardware access. Run with adreno_a6xx.c, a6xx_reg.h and output header paths.
"""
import argparse
from pathlib import Path
import re

REVISION = 'aa587668f86bd02ff1d7f825f5a89a886a84c8d8'


def derive(source, definitions):
    def offset(name):
        match = re.search(r'^#define\s+' + re.escape(name) +
                          r'\s+\(?\s*(0x[0-9a-fA-F]+|[0-9]+)\s*\)?\s*$',
                          definitions, re.M)
        if not match:
            raise ValueError('unresolved register: ' + name)
        return int(match[1], 0)

    def table(name, expected):
        match = re.search(r'\b' + name + r'\[\]\s*=\s*\{(.*?)\n\};', source, re.S)
        if not match:
            raise ValueError('missing table: ' + name)
        entries = re.findall(r'\{(A6XX_\w+),\s*(0x[0-9a-fA-F]+)\}', match[1])
        if len(entries) != expected:
            raise ValueError(f'{name}: expected {expected} entries, got {len(entries)}')
        result = []
        for symbol, value in entries:
            register = offset(symbol)
            if register * 4 >= 0x40000:
                raise ValueError('outside Mi9 GPU aperture: ' + symbol)
            result.append((symbol, register, int(value, 16)))
        return result

    text = '''/* SPDX-License-Identifier: GPL-2.0-only
 * Register data derived from the Linux Foundation Mi9 kernel:
 * drivers/gpu/msm/adreno_a6xx.c and a6xx_reg.h.
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 * Pinned revision: ''' + REVISION + '''
 * Offsets are DWORD indices, not byte offsets. GMU uses a separate domain.
 */
#ifndef CEPHEUS_A640_REGISTERS_H
#define CEPHEUS_A640_REGISTERS_H
struct a640_pair { unsigned reg, value; };
'''
    for name in ('A6XX_RBBM_CLOCK_CNTL', 'A6XX_GPU_GMU_AO_GMU_CGC_MODE_CNTL',
                 'A6XX_GPU_GMU_AO_GMU_CGC_DELAY_CNTL',
                 'A6XX_GPU_GMU_AO_GMU_CGC_HYST_CNTL',
                 'A6XX_GPU_GMU_GX_SPTPRAC_CLOCK_CONTROL'):
        text += f'#define {name} {offset(name):#x}u\n'
    for name, upstream, count in [('a640_hwcg', 'a640_hwcg_regs', 49),
                                  ('a640_gbif', 'a640_gbif', 5)]:
        text += f'static const struct a640_pair {name}[] = {{\n'
        for symbol, register, value in table(upstream, count):
            text += f'    {{{register:#x}u, {value:#x}u}}, /* {symbol} */\n'
        text += '};\n'
    return text + '#endif\n'


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('registers', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    args.output.write_text(derive(args.source.read_text(), args.registers.read_text()),
                           encoding='utf-8', newline='\n')
