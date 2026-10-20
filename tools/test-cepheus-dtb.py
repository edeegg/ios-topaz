#!/usr/bin/env python3
"""Bounds and privacy checks for the device-tree inventory reader."""
import importlib.util
from pathlib import Path
import struct
import unittest

spec = importlib.util.spec_from_file_location('cepheus_dtb', Path(__file__).with_name('cepheus-dtb.py'))
dtb = importlib.util.module_from_spec(spec)
spec.loader.exec_module(dtb)


def fixture():
    names = b'compatible\0serial-number\0'
    def prop(offset, value):
        return struct.pack('>III', 3, len(value), offset) + value + bytes(-len(value) % 4)
    body = struct.pack('>I', 1) + bytes(4)
    body += prop(0, b'qcom,sm8150\0') + prop(11, b'private-device-id\0')
    body += struct.pack('>II', 2, 9)
    off = 56  # header plus empty reservation map
    header = struct.pack('>10I', 0xd00dfeed, off + len(body) + len(names), off,
                         off + len(body), 40, 17, 16, 0, len(names), len(body))
    return header + bytes(16) + body + names


class ReaderTests(unittest.TestCase):
    def test_embedded_tree_and_private_property(self):
        trees = list(dtb.scan(b'prefix' + fixture()))
        self.assertEqual(len(trees), 1)
        self.assertEqual(trees[0]['offset'], 6)
        self.assertEqual(trees[0]['nodes']['/']['compatible'], ['qcom,sm8150'])
        self.assertNotIn('private-device-id', str(trees))
        self.assertNotIn('serial-number', str(trees))

    def test_all_truncated_prefixes_rejected(self):
        blob = fixture()
        for length in range(len(blob)):
            with self.subTest(length=length):
                self.assertEqual(list(dtb.scan(blob[:length])), [])

    def test_outside_string_table(self):
        blob = bytearray(fixture())
        struct.pack_into('>I', blob, 72, 0xffffffff)  # first property nameoff
        self.assertEqual(list(dtb.scan(blob)), [])

    def test_outside_structure(self):
        blob = bytearray(fixture())
        struct.pack_into('>I', blob, 68, 0xffffffff)  # first property length
        self.assertEqual(list(dtb.scan(blob)), [])


if __name__ == '__main__':
    unittest.main()
