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
    args = parser.parse_args()
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
    out = root / 'shim/out/cepheus-probe'
    out.mkdir(parents=True, exist_ok=True)
    flags = ['--target=aarch64-none-elf', '-mcpu=cortex-a55', '-O2', '-ffreestanding',
             '-nostdlib', '-fno-pic', '-mgeneral-regs-only', '-mstrict-align',
             '-fno-stack-protector', '-Wall', '-Wextra', '-Werror',
             '-DFB_BASE=0x9c000000UL', '-DFB_H=2340', '-I', str(root / 'shim')]
    objects = []
    for name in ('start.S', 'console.c', 'cepheus-probe.c'):
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
    image.write_bytes(payload)
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
    boot = out / 'cepheus-probe-boot.img'
    boot.write_bytes(header + payload + bytes(align(len(payload), page) - len(payload)))
    sha = hashlib.sha256(boot.read_bytes()).hexdigest()
    (out / 'SHA256SUMS').write_text(f'{sha}  {boot.name}\n', encoding='ascii')
    print(f'{boot}\n{boot.stat().st_size} bytes; {len(trees)} original DTBs; SHA256 {sha}')
    print('Built only. Hardware boot NOT tested. This image does not run XNU.')


if __name__ == '__main__':
    main()
