#!/usr/bin/env python3
"""Build an offline cepheus CPU probe using LLVM and the original boot DTBs.

No flashing or phone access. Output stays under ignored shim/out/. This is
not an iOS boot image. Bootloader acceptance and framebuffer need validation.
"""
import argparse
import hashlib
import importlib.util
from pathlib import Path
import shutil
import struct
import adt
import subprocess


def run(*args):
    # CreateProcess does not append .exe to names containing a dot (ld.lld).
    executable = shutil.which(str(args[0]))
    if executable is None:
        raise FileNotFoundError(f'{args[0]} not found on PATH')
    subprocess.run([executable, *(str(a) for a in args[1:])], check=True)


def align(value, page):
    return (value + page - 1) & -page


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--stock-boot', type=Path, required=True)
    parser.add_argument('--entry-test', action='store_true',
                        help='avoid framebuffer; reboot to fastboot after 5 seconds')
    parser.add_argument('--payload', type=Path,
                        help='validated mkpayload.py XNU console payload (boot MPIDR 0)')
    args = parser.parse_args()
    if args.payload and args.entry_test:
        parser.error('--payload and --entry-test are mutually exclusive')
    xnu_payload = None
    if args.payload:
        xnu_payload = args.payload.read_bytes()
        if len(xnu_payload) < 0x2a8 or xnu_payload[:8] != b'SWEETXNU':
            parser.error('invalid XNU payload header')
        version, nsegs = struct.unpack_from('<II', xnu_payload, 8)
        virt, entry, adt_off, adt_size, end, phys, mem = struct.unpack_from('<7Q', xnu_payload, 16)
        if (version != 1 or not 1 <= nsegs <= 32 or
                phys != 0x140000000 or mem != 0xc0000000 or
                not virt <= entry < end or end - virt + adt_size + 0x8000 > mem or
                adt_off < 0x2a8 + nsegs * 32 or adt_off % 8 or
                adt_size < 4 or adt_off + adt_size > len(xnu_payload)):
            parser.error('payload does not fit the verified cepheus high RAM bank')
        for i in range(nsegs):
            va, size, off, length = struct.unpack_from('<4Q', xnu_payload, 0x2a8 + i * 32)
            if (va < virt or va + size > end or length > size or
                    off % 8 or off < 0x2a8 + nsegs * 32 or
                    off + align(length, 8) > len(xnu_payload) or size % 8):
                parser.error(f'payload segment {i} is out of bounds or unaligned')
        tree, consumed = adt.parse(xnu_payload[adt_off:adt_off + adt_size])
        if consumed != adt_size:
            parser.error('payload ADT length mismatch')
        chosen = next(c for c in tree.children if c.name == 'chosen')
        cpus = next(c for c in tree.children if c.name == 'cpus')
        if (chosen.prop('dram-base') != struct.pack('<Q', phys) or
                chosen.prop('dram-size') != struct.pack('<Q', mem) or
                len(cpus.children) != 1 or cpus.children[0].prop('reg') != bytes(4) or
                b'xiaomi,cepheus\0' not in tree.prop('compatible') or
                any(c.name == 'sweet-ufs' for c in tree.children)):
            parser.error('payload requires the cepheus single-CPU no-storage ADT')
    for tool in ('clang', 'ld.lld', 'llvm-objcopy'):
        if not shutil.which(tool):
            parser.error(f'{tool} not found on PATH')
    root = Path(__file__).resolve().parents[1]
    source = args.stock_boot.read_bytes()
    if source[:8] != b'ANDROID!' or len(source) < 1632:
        parser.error('expected an Android boot image')
    kernel_size, kernel_addr, _, ramdisk_addr, _, second_addr, tags, page, version, osver = struct.unpack_from('<10I', source, 8)
    if version != 0 or page < 2048 or page > 65536 or page & (page - 1):
        parser.error('only the verified legacy v0 cepheus boot layout is supported')
    spec = importlib.util.spec_from_file_location('cepheus_dtb', root / 'tools/cepheus-dtb.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    kernel = source[page:page + kernel_size]
    trees = list(module.scan(kernel))
    if not trees or any(not any(c in ('qcom,sm8150', 'qcom,sm8150p')
                                   for c in t['nodes'].get('/', {}).get('compatible', [])) for t in trees):
        parser.error('expected SM8150 DTBs appended to the stock kernel')
    # Reject gaps/trailers: do not accidentally append compressed kernel data.
    cursor = trees[0]['offset']
    for tree in trees:
        if tree['offset'] != cursor:
            parser.error('non-contiguous DTBs; inspect the stock layout first')
        cursor += tree['size']
    if cursor != len(kernel):
        parser.error('unexpected data after appended DTBs')
    variant = ('cepheus-xnu' if xnu_payload else
               'cepheus-entry-probe' if args.entry_test else 'cepheus-probe')
    out = root / 'shim/out' / variant
    out.mkdir(parents=True, exist_ok=True)
    flags = ['--target=aarch64-none-elf', '-mcpu=cortex-a55', '-O2', '-ffreestanding',
             '-nostdlib', '-fno-pic', '-mgeneral-regs-only', '-mstrict-align',
             '-fno-stack-protector', '-Wall', '-Wextra', '-Werror',
             '-DFB_BASE=0x9c000000UL', '-DFB_H=2340', '-I', str(root / 'shim')]
    if args.entry_test:
        flags.append('-DCEPHEUS_ENTRY_TEST=1')
    objects = []
    sources = (('start.S', 'console.c', 'cepheus-xnu.c', 'loader.c', 'fdt.c') if xnu_payload else
               ('start.S', 'cepheus-probe.c') if args.entry_test else
               ('start.S', 'console.c', 'cepheus-probe.c'))
    for name in sources:
        obj = out / (Path(name).stem + '.o')
        run('clang', *flags, '-c', root / 'shim' / name, '-o', obj)
        objects.append(obj)
    elf = out / 'probe.elf'
    run('ld.lld', '-T', root / 'shim/shim.ld', '--build-id=none', *objects, '-o', elf)
    image = out / 'Image'
    run('llvm-objcopy', '-O', 'binary', elf, image)
    payload = bytearray(image.read_bytes())
    image_size, = struct.unpack_from('<Q', payload, 16)
    if image_size < len(payload) or image_size > 16 * 1024 * 1024:
        parser.error('invalid linked ARM64 image_size')
    payload.extend(bytes(image_size - len(payload)))
    if xnu_payload:
        payload.extend(bytes(align(len(payload), 4096) - len(payload)))
        payload.extend(xnu_payload)
        struct.pack_into('<Q', payload, 16, len(payload))
    image.write_bytes(payload)
    # Mi 9 ABL's stock layout wraps the raw ARM64 Image with a Qualcomm
    # UNCOMPRESSED_IMG marker and its file length, then appends the DTBs.
    # Match this verified layout instead of assuming topaz accepts raw Image.
    wrapped = kernel.startswith(b'UNCOMPRESSED_IMG')
    if wrapped:
        if len(kernel) < 20 or struct.unpack_from('<I', kernel, 16)[0] + 20 != trees[0]['offset']:
            parser.error('unexpected Qualcomm uncompressed kernel wrapper length')
        payload = bytearray(b'UNCOMPRESSED_IMG' + struct.pack('<I', len(payload))) + payload
    payload += kernel[trees[0]['offset']:]
    # Fresh header: retain placement addresses/OS version, omit stock cmdline,
    # ramdisk, second stage and any footer/signature from the backup.
    header = bytearray(page)
    header[:8] = b'ANDROID!'
    struct.pack_into('<10I', header, 8, len(payload), kernel_addr, 0, ramdisk_addr,
                     0, second_addr, tags, page, 0, osver)
    header[48:55] = b'cepheus'
    digest = hashlib.sha1()
    digest.update(payload)
    digest.update(struct.pack('<I', len(payload)))
    digest.update(struct.pack('<I', 0))  # empty ramdisk
    digest.update(struct.pack('<I', 0))  # empty second stage
    header[576:596] = digest.digest()
    boot = out / f'{variant}-boot.img'
    boot.write_bytes(header + payload + bytes(align(len(payload), page) - len(payload)))
    sha = hashlib.sha256(boot.read_bytes()).hexdigest()
    (out / 'SHA256SUMS').write_text(f'{sha}  {boot.name}\n', encoding='ascii')
    print(f'{boot}\n{boot.stat().st_size} bytes; {len(trees)} original DTBs; Qualcomm wrapper={wrapped}; SHA256 {sha}')
    print('Built only. Hardware boot NOT tested.' +
          (' Contains experimental single-CPU XNU.' if xnu_payload else ' This image does not run XNU.'))


if __name__ == '__main__':
    main()
