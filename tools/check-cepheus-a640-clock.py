#!/usr/bin/env python3
"""Windows LLVM check of the portable A640 component; no device access."""
import ctypes
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def run(*args):
    subprocess.run(args, cwd=ROOT, check=True)


def main():
    if os.name != 'nt':
        raise SystemExit('This runner uses Windows LLVM/lld-link; the C component is portable.')
    output = ROOT / 'research/cepheus/a640-clock-build'
    output.mkdir(parents=True, exist_ok=True)
    flags = ('clang', '--target=x86_64-pc-windows-msvc', '-ffreestanding',
             '-fno-stack-protector', '-fno-builtin', '-Wall', '-Wextra', '-Werror')
    for source, name in [('drivers/cepheus/adreno640_clock.c', 'clock'),
                         ('tests/cepheus_adreno640_clock.c', 'test')]:
        run(*flags, '-c', source, '-o', str(output / (name + '.obj')))
    run('lld-link', '/dll', '/noentry', '/nodefaultlib', '/export:test',
        '/out:' + str(output / 'clock-test.dll'),
        str(output / 'clock.obj'), str(output / 'test.obj'))
    result = ctypes.CDLL(str(output / 'clock-test.dll')).test()
    if result:
        raise SystemExit('A640 C sequence test failed: ' + str(result))
    run('clang', '--target=arm64-apple-ios16.0', '-ffreestanding',
        '-fno-stack-protector', '-Wall', '-Wextra', '-Werror', '-c',
        'drivers/cepheus/adreno640_clock.c', '-o', str(output / 'adreno640-clock-arm64.o'))
    print('PASS A640 sequencing, guards, RMW, errors; real ARM64 Mach-O object compiled.')
    print('Hardware execution and GPU/Metal support are NOT validated by this check.')


if __name__ == '__main__':
    main()
