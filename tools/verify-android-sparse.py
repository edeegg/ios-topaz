#!/usr/bin/env python3
"""Verify an Android sparse image against the SHA256 of its expanded raw data."""
import argparse
import hashlib
from pathlib import Path
import struct


def verify(path, expected, size):
    digest = hashlib.sha256()
    expanded = 0
    with path.open('rb') as stream:
        header = stream.read(28)
        if len(header) != 28:
            raise ValueError('truncated sparse header')
        magic, major, minor, file_hdr, chunk_hdr, block, blocks, chunks, checksum = struct.unpack('<I4H4I', header)
        if magic != 0xed26ff3a or major != 1 or minor != 0 or file_hdr < 28 or chunk_hdr < 12:
            raise ValueError('unsupported sparse header')
        if block != 4096 or blocks * block != size:
            raise ValueError('unexpected expanded image geometry')
        if len(stream.read(file_hdr - 28)) != file_hdr - 28:
            raise ValueError('truncated extended header')
        for index in range(chunks):
            header = stream.read(chunk_hdr)
            if len(header) != chunk_hdr:
                raise ValueError(f'truncated chunk {index}')
            kind, reserved, count, total = struct.unpack_from('<2H2I', header)
            length = count * block
            payload = total - chunk_hdr
            if length > size - expanded:
                raise ValueError('expanded chunks exceed declared image')
            if kind == 0xcac1:
                if payload != length:
                    raise ValueError('RAW chunk size mismatch')
                remaining = length
                while remaining:
                    data = stream.read(min(1024 * 1024, remaining))
                    if not data:
                        raise ValueError('truncated RAW data')
                    digest.update(data)
                    remaining -= len(data)
            elif kind in (0xcac2, 0xcac3):
                if payload != (4 if kind == 0xcac2 else 0):
                    raise ValueError('FILL/DONT_CARE payload mismatch')
                pattern = stream.read(4) if kind == 0xcac2 else bytes(4)
                if len(pattern) != 4:
                    raise ValueError('truncated FILL data')
                data = pattern * (1024 * 1024 // 4)
                remaining = length
                while remaining:
                    take = min(len(data), remaining)
                    digest.update(data[:take])
                    remaining -= take
            elif kind == 0xcac4:
                if length or payload != 4 or len(stream.read(4)) != 4:
                    raise ValueError('invalid CRC chunk')
            else:
                raise ValueError(f'unknown chunk kind {kind:#x}')
            expanded += length
        if expanded != size or stream.read(1):
            raise ValueError('expanded size mismatch or trailing data')
    actual = digest.hexdigest()
    if actual != expected.lower():
        raise ValueError(f'expanded hash mismatch: {actual}')
    print(f'PASS: {chunks} sparse chunks, {expanded} raw bytes, SHA256 {actual}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    parser.add_argument('--sha256', required=True)
    parser.add_argument('--size', required=True, type=int)
    args = parser.parse_args()
    verify(args.image, args.sha256, args.size)
