#!/usr/bin/env python3
"""Compile and exercise inactive GPU tables; never accesses hardware."""
import ctypes
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'research/cepheus/gpu-tables-build'

def run(*args):
    subprocess.run(args, cwd=ROOT, check=True)

def main():
    OUT.mkdir(parents=True, exist_ok=True)
    sources = ['drivers/cepheus/third_party/ArmSmmu4K/ArmSmmu4K.c',
               'drivers/cepheus/gpu_tables.c', 'tests/cepheus_gpu_tables.c']
    objects = []
    for i, source in enumerate(sources):
        obj = str(OUT / (str(i) + '.obj'))
        run('clang', '--target=x86_64-pc-windows-msvc', '-ffreestanding',
            '-fno-stack-protector', '-fno-builtin', '-Wall', '-Wextra', '-Werror',
            '-c', source, '-o', obj)
        objects.append(obj)
    run('lld-link', '/dll', '/noentry', '/nodefaultlib', '/export:test',
        '/out:' + str(OUT / 'tables.dll'), *objects)
    memory = ctypes.create_string_buffer(32768)
    aligned = (ctypes.addressof(memory) + 16383) & ~16383
    test = ctypes.CDLL(str(OUT / 'tables.dll')).test
    test.argtypes = [ctypes.c_void_p]
    result = test(aligned)
    if result:
        raise SystemExit('Table test failed: ' + str(result))
    for i, source in enumerate(sources[:2]):
        run('clang', '--target=arm64-apple-ios16.0', '-ffreestanding',
            '-fno-stack-protector', '-Wall', '-Wextra', '-Werror', '-c', source,
            '-o', str(OUT / (str(i) + '-arm64.o')))
    print('PASS: 16K owned buffers -> four 4K leaves, table walk, guards, ARM64 compile.')
    print('Inactive tables only: GPU execution, coherence and Metal remain unvalidated.')

if __name__ == '__main__':
    main()
