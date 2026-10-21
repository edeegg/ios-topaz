#!/usr/bin/env python3
"""Windows LLVM check of the portable A640 component; no device access."""
import ctypes
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def run(*args):
    subprocess.run(args, cwd=ROOT, check=True)


def check_component(component):
    output = ROOT / ('research/cepheus/a640-' + component + '-build')
    output.mkdir(parents=True, exist_ok=True)
    flags = ('clang', '--target=x86_64-pc-windows-msvc', '-ffreestanding',
             '-fno-stack-protector', '-fno-builtin', '-Wall', '-Wextra', '-Werror')
    driver = 'drivers/cepheus/adreno640_' + component + '.c'
    for source, name in [(driver, 'clock'),
                         ('tests/cepheus_adreno640_' + component + '.c', 'test')]:
        run(*flags, '-c', source, '-o', str(output / (name + '.obj')))
    run('lld-link', '/dll', '/noentry', '/nodefaultlib', '/export:test',
        '/out:' + str(output / 'clock-test.dll'),
        str(output / 'clock.obj'), str(output / 'test.obj'))
    result = ctypes.CDLL(str(output / 'clock-test.dll')).test()
    if result:
        raise SystemExit('A640 ' + component + ' test failed: ' + str(result))
    run('clang', '--target=arm64-apple-ios16.0', '-ffreestanding',
        '-fno-stack-protector', '-Wall', '-Wextra', '-Werror', '-c',
        driver, '-o', str(output / ('adreno640-' + component + '-arm64.o')))
    print('PASS A640 ' + component + ' sequencing/guards/errors; ARM64 Mach-O object compiled.')


def main():
    if os.name != 'nt':
        raise SystemExit('This runner uses Windows LLVM/lld-link; the C component is portable.')
    check_component('clock')
    check_component('gmu')
    print('Hardware execution and GPU/Metal support are NOT validated by this check.')


if __name__ == '__main__':
    main()
