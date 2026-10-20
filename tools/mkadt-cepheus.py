#!/usr/bin/env python3
"""Generate a candidate single-CPU cepheus console ADT from a live FDT.

This is configuration preparation, not a bootable port. The topaz kernel's
hardcoded hardware accesses must be ported before pairing it with this tree.
"""
import argparse
import hashlib
import importlib.util
from pathlib import Path
import struct

import adt
import mkadt

spec = importlib.util.spec_from_file_location('cepheus_dtb', Path(__file__).with_name('cepheus-dtb.py'))
fdt = importlib.util.module_from_spec(spec)
spec.loader.exec_module(fdt)


def ranges(value):
    if len(value) % 16:
        raise ValueError('expected 64-bit address/size pairs')
    return list(struct.iter_unpack('>QQ', value))


def overlap(a, b):
    return a[0] < b[0] + b[1] and b[0] < a[0] + a[1]


def setprop(node, name, value):
    node.props = [(k, flags, value if k == name else v) for k, flags, v in node.props]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--live-dtb', required=True, type=Path)
    parser.add_argument('--boot-mpidr', required=True, type=lambda s: int(s, 0),
                        help='must be confirmed with the CPU probe before boot')
    parser.add_argument('--out', required=True, type=Path)
    parser.add_argument('--display', action='store_true',
                        help='publish the CPU-copy framebuffer at the verified ABL geometry')
    parser.add_argument('--storage-read-only', action='store_true',
                        help='include verified Mi 9 UFS map for the read-only kernel target')
    args = parser.parse_args()
    blob = args.live_dtb.read_bytes()
    nodes = fdt.parse_fdt(blob)
    if not any('cepheus' in text.lower() for key in ('model', 'compatible')
               for text in fdt.strings(nodes['/'].get(key, b''))):
        parser.error('expected a live cepheus FDT, not a generic stock base DTB')
    bank_candidates = [r for r in ranges(nodes['/memory']['reg']) if r[0] >= 0x100000000 and r[1]]
    if not bank_candidates:
        parser.error('no high RAM bank: inspect memory layout manually')
    bank = max(bank_candidates, key=lambda r: r[1])
    if any(v % 0x4000 for v in bank):
        parser.error('RAM must be aligned to 16K')
    # Check both the firmware reservation map and DT reserved-memory. Dynamic
    # carve-outs are conservatively treated as occupying their whole alloc range.
    off = struct.unpack_from('>I', blob, 16)[0]
    firmware = []
    while off + 16 <= len(blob):
        entry = struct.unpack_from('>QQ', blob, off)
        off += 16
        if entry == (0, 0):
            break
        firmware.append(entry)
    else:
        parser.error('unterminated firmware reservation map')
    if any(overlap(bank, r) for r in firmware):
        parser.error('high bank overlaps a firmware reservation')
    for path, props in nodes.items():
        if not path.startswith('/reserved-memory/'):
            continue
        region = props.get('reg', props.get('alloc-ranges'))
        if region is None and 'size' in props:
            parser.error(f'{path}: dynamic reservation has no allocation bounds')
        if region is not None and any(overlap(bank, r) for r in ranges(region)):
            parser.error(f'high bank overlaps reserved/allocatable area {path}')
    cpus = [int.from_bytes(p['reg'], 'big') for path, p in nodes.items()
            if path.startswith('/cpus/') and p.get('device_type') == b'cpu\0']
    if args.boot_mpidr not in cpus:
        parser.error('boot MPIDR is not in the live CPU topology')
    # These addresses were independently checked in the actual recovery FDT.
    gic = nodes['/soc/interrupt-controller@17a00000']
    if struct.unpack('>4I', gic['reg']) != (0x17a00000, 0x10000, 0x17a60000, 0x100000):
        parser.error('unexpected GIC layout')
    mkadt.HIGH_PHYS_BASE, mkadt.HIGH_MEM_SIZE = bank
    mkadt.GICD, mkadt.GICR = 0x17a00000, 0x17a60000
    if args.storage_read_only:
        ufs = nodes['/soc/ufshc@1d84000']
        if (ufs.get('reg-names') != b'ufs_mem\0ufs_ice\0' or
                struct.unpack('>4I', ufs['reg']) != (0x1d84000, 0x2500, 0x1d90000, 0x8000)):
            parser.error('unexpected Mi 9 UFS register layout')
        mkadt.UFS, mkadt.UFS_SIZE = 0x1d84000, 0x2500
    mkadt.FB, mkadt.FB_SIZE = ranges(nodes['/reserved-memory/cont_splash_region']['reg'])[0]
    mkadt.FB_W, mkadt.FB_H, mkadt.FB_STRIDE = 1080, 2340, 4320
    mkadt.TIMEBASE = int.from_bytes(nodes['/soc/timer']['clock-frequency'], 'big')
    mkadt.SWEET_ECID = 0x4345504845555301  # synthetic, never read the device ID
    mkadt.BOOT_MANIFEST_HASH = hashlib.sha384(b'cepheus boot manifest').digest()
    root = mkadt.build(highmem=True, native=True)
    setprop(root, 'compatible', b'D22AP\0iPhone10,3\0xiaomi,cepheus\0xiaomi,sweet\0qcom,sm8150\0AppleARM\0')
    for child in root.children:
        if child.name == 'chosen':
            for name in ('firmware-version', 'system-firmware-version'):
                setprop(child, name, b'cepheus-shim-candidate\0')
        elif child.name == 'cpus':
            child.children = child.children[:1]
            setprop(child.children[0], 'reg', mkadt.u32(args.boot_mpidr))
            # Retain kryo470 as the existing IOKit CPU match alias.
            setprop(child.children[0], 'compatible', b'qcom,kryo485\0qcom,kryo470\0ARM,v8\0')
        elif child.name == 'arm-io':
            child.children = [c for c in child.children
                              if c.name != 'wlan' and (args.display or c.name != 'disp0')]
            for device in child.children:
                if device.name == 'disp0':
                    setprop(device, 'canvas-width', mkadt.u32(mkadt.FB_W))
                    setprop(device, 'canvas-height', mkadt.u32(mkadt.FB_H))
    if not args.storage_read_only:
        root.children = [c for c in root.children if c.name != 'sweet-ufs']
    args.out.parent.mkdir(parents=True, exist_ok=True)
    result = adt.serialize(root)
    args.out.write_bytes(result)
    print(f'{args.out}: {len(result)} bytes; RAM {bank[0]:#x}+{bank[1]:#x}, MPIDR {args.boot_mpidr:#x}')
    print('Candidate ADT: ' + ('read-only UFS target; ' if args.storage_read_only else 'no UFS; ') +
          'no WLAN; ' + ('1080x2340 CPU-copy display. ' if args.display else 'no display service. ') +
          'Requires the matching Mi 9 kernel.')


if __name__ == '__main__':
    main()
