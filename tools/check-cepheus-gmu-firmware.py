#!/usr/bin/env python3
"""Exercise the real A640 uploader against RAM callbacks, never device MMIO."""
import argparse
import ctypes as C
from pathlib import Path
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'research/cepheus/a640-firmware-build'

class Block(C.Structure):
    _fields_ = [('address', C.c_uint32), ('bytes', C.c_uint32),
                ('type', C.c_uint32), ('value', C.c_uint32),
                ('payload_offset', C.c_size_t), ('region_identifier', C.c_uint32)]

TCM = C.CFUNCTYPE(C.c_int, C.c_void_p, C.c_uint32, C.c_uint32)
CACHE = C.CFUNCTYPE(C.c_int, C.c_void_p, C.c_int, C.c_uint32, C.c_void_p, C.c_size_t)
BARRIER = C.CFUNCTYPE(C.c_int, C.c_void_p)

class IO(C.Structure):
    _fields_ = [('cookie', C.c_void_p), ('tcm', TCM), ('cache', CACHE), ('barrier', BARRIER)]

def run(*args):
    subprocess.run(args, cwd=ROOT, check=True)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--stock-firmware', type=Path)
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    sources = ['drivers/cepheus/adreno640_firmware.c',
               'drivers/cepheus/third_party/AdrenoFirmware/AdrenoFirmware.c']
    objects = []
    for i, source in enumerate(sources):
        obj = str(OUT / (str(i) + '.obj'))
        run('clang', '--target=x86_64-pc-windows-msvc', '-ffreestanding',
            '-fno-stack-protector', '-fno-builtin', '-Wall', '-Wextra', '-Werror',
            '-c', source, '-o', obj)
        objects.append(obj)
        run('clang', '--target=arm64-apple-ios16.0', '-ffreestanding',
            '-fno-stack-protector', '-Wall', '-Wextra', '-Werror', '-c', source,
            '-o', str(OUT / (str(i) + '-arm64.o')))
    run('lld-link', '/dll', '/noentry', '/nodefaultlib', '/export:a640_gmu_upload',
        '/out:' + str(OUT / 'firmware.dll'), *objects)
    upload = C.CDLL(str(OUT / 'firmware.dll')).a640_gmu_upload
    upload.argtypes = [C.POINTER(IO), C.c_uint32, C.c_uint, C.c_void_p,
                       C.c_size_t, C.POINTER(Block), C.c_size_t]
    operations = []
    fail_at = 0
    def record(operation):
        operations.append(operation)
        return -1 if fail_at and len(operations) == fail_at else 0
    tcm = TCM(lambda _, reg, value: record(('tcm', reg, value)))
    cache = CACHE(lambda _, region, addr, data, n:
                  record(('cache', region, addr, C.string_at(data, n))))
    barrier = BARRIER(lambda _: record(('barrier',)))
    io = IO(None, tcm, cache, barrier)
    scratch = (Block * 64)()
    def call(data, chip=0x06040001, ready=15, capacity=64):
        operations.clear()
        memory = C.create_string_buffer(data)
        return upload(C.byref(io), chip, ready, memory, len(data), scratch, capacity)
    def block(addr, data):
        return struct.pack('<4I', addr, len(data), 0xefbeadde, 0xefbeadde) + data
    fixture = block(0x200, struct.pack('<I', 0x12345678))
    fixture += block(0x4000, b'abcdefgh') + block(0x40000, b'ABCD')
    fixture += block(0x57000, b'ijkl')
    assert call(fixture) == 0
    assert operations == [('tcm', 0x1b480, 0x12345678),
                          ('cache', 1, 0x4000, b'abcdefgh'),
                          ('tcm', 0x1c400, 0x44434241),
                          ('cache', 3, 0x57000, b'ijkl'), ('barrier',)]
    for invalid in [fixture[:-1], fixture + b'x', block(0x80000, b'abcd'),
                    block(0x3ffc, b'abcdefgh'), block(0x202, b'abcd')]:
        assert call(invalid) == -3 and not operations
    assert call(fixture, chip=0x07040000) == -2 and not operations
    for ready in range(15):
        assert call(fixture, ready=ready) == -2 and not operations
    assert call(fixture, capacity=3) == -3 and not operations
    for fail_at in range(1, 6):
        assert call(fixture) == -4 and len(operations) == fail_at
    fail_at = 0
    print('PASS: TCM/cache routing, malformed input atomicity, guards, I/O failures, ARM64 compile.')
    if args.stock_firmware:
        data = args.stock_firmware.read_bytes()
        expected = []
        offset = 0
        while offset < len(data):
            addr, n, _, _ = struct.unpack_from('<4I', data, offset)
            payload = data[offset+16:offset+16+n]
            if n and (addr < 0x4000 or 0x40000 <= addr < 0x44000):
                first = 0x1b400 + addr//4 if addr < 0x4000 else 0x1c400 + (addr-0x40000)//4
                expected.extend(('tcm', first+i//4, struct.unpack_from('<I', payload, i)[0])
                                for i in range(0, n, 4))
            elif n:
                expected.append(('cache', 1 if addr < 0x40000 else 3, addr, payload))
            offset += 16+n
        expected.append(('barrier',))
        assert offset == len(data) and call(data) == 0 and operations == expected
        print(f'PASS: stock A640 image {len(data)} bytes validated and routed to RAM callbacks.')
    print('No firmware was uploaded to hardware; reset/HFI/GPU/Metal remain unvalidated.')

if __name__ == '__main__':
    main()
