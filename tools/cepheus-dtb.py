#!/usr/bin/env python3
"""Inspect DTBs embedded in a Mi 9 boot backup without printing device IDs.

Only hardware properties are included. This does not modify the input image
or contact a phone. Output belongs in research/ (device dumps are not tracked).
"""
import argparse
import json
import struct
from pathlib import Path


def parse_fdt(blob):
    if len(blob) < 40:
        raise ValueError('truncated FDT header')
    magic, total, off, strings, _, version, _, _, nstrings, nstruct = struct.unpack_from('>10I', blob)
    if magic != 0xd00dfeed or version != 17 or not 40 <= total <= len(blob):
        raise ValueError('invalid FDT header')
    if off + nstruct > total or strings + nstrings > total:
        raise ValueError('FDT section outside blob')
    end = off + nstruct
    names = blob[strings:strings + nstrings]
    nodes, stack = {}, []
    while off + 4 <= end:
        token, = struct.unpack_from('>I', blob, off)
        off += 4
        if token == 1:
            stop = blob.find(b'\0', off, end)
            if stop < 0:
                raise ValueError('unterminated node')
            stack.append(blob[off:stop].decode('ascii'))
            nodes['/' + '/'.join(stack[1:])] = {}
            off = (stop + 4) & ~3
        elif token == 2:
            if not stack:
                raise ValueError('unbalanced nodes')
            stack.pop()
        elif token == 3:
            if not stack or off + 8 > end:
                raise ValueError('property outside node')
            length, nameoff = struct.unpack_from('>II', blob, off)
            off += 8
            stop = names.find(b'\0', nameoff)
            if stop < 0 or off + length > end:
                raise ValueError('invalid property bounds')
            name = names[nameoff:stop].decode('ascii')
            nodes['/' + '/'.join(stack[1:])][name] = blob[off:off + length]
            off = (off + length + 3) & ~3
        elif token == 4:
            continue
        elif token == 9:
            if stack:
                raise ValueError('unclosed nodes')
            return nodes
        else:
            raise ValueError(f'unknown FDT token {token}')
    raise ValueError('missing FDT_END')


def strings(value):
    return value.rstrip(b'\0').decode('ascii', errors='replace').split('\0')


def cells(value):
    if len(value) % 4:
        return value.hex()
    return [f'0x{x:x}' for x in struct.unpack(f'>{len(value) // 4}I', value)]


def inventory(nodes):
    # Explicit allowlist: chosen bootargs, serials, MACs and calibration omitted.
    result = {}
    for path, props in nodes.items():
        compat = strings(props.get('compatible', b''))
        relevant = (path == '/' or path.startswith(('/cpus/', '/memory', '/reserved-memory/'))
                    or any(any(key in c for key in ('gic', 'ufshc', 'geni-i2c', 'touch', 'timer', 'framebuffer'))
                           for c in compat)
                    or any(key in path.lower() for key in ('splash', 'touch', 'fts', 'synaptics')))
        if not relevant:
            continue
        entry = {}
        for key in ('model', 'compatible', 'device_type', 'status', 'enable-method'):
            if key in props:
                entry[key] = strings(props[key])
        for key in ('reg', '#address-cells', '#size-cells', 'interrupts', 'clock-frequency',
                    'redistributor-stride', 'width', 'height', 'stride', 'qcom,msm-id', 'qcom,board-id'):
            if key in props:
                entry[key] = cells(props[key])
        if entry:
            result[path] = entry
    return result


def scan(data):
    offset = 0
    while True:
        offset = data.find(b'\xd0\x0d\xfe\xed', offset)
        if offset < 0:
            return
        if offset + 40 <= len(data):
            size, = struct.unpack_from('>I', data, offset + 4)
            if 40 <= size <= len(data) - offset:
                try:
                    nodes = parse_fdt(data[offset:offset + size])
                except (ValueError, UnicodeError, struct.error):
                    pass
                else:
                    yield {'offset': offset, 'size': size, 'nodes': inventory(nodes)}
        offset += 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    parser.add_argument('--out', required=True, type=Path)
    args = parser.parse_args()
    trees = list(scan(args.image.read_bytes()))
    if not trees:
        parser.error('no valid v17 FDT found; input may be compressed')
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps({'trees': trees}, indent=2) + '\n', encoding='utf-8')
    print(f'{len(trees)} DTBs inspected; hardware inventory: {args.out}')


if __name__ == '__main__':
    main()
