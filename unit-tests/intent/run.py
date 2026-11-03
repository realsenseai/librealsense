#!/usr/bin/env python3
"""Host protocol tests; --fw-root points to common/intent for cross-repo validation."""
import argparse
import os
import json
import hashlib
from pathlib import Path
import struct
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--fw-root', type=Path)
parser.add_argument('--json-include', type=Path, required=True,
                    help='directory containing nlohmann/json.hpp (SDK build dependency)')
parser.add_argument('--sdk-lib', type=Path, help='directory containing built librealsense2.so; also compile/run public API test')
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
contract = root / 'src/intent/contract'
fw = (args.fw_root or contract).resolve()
manifest = json.loads((contract / 'export.json').read_text())
for name, expected in manifest['files'].items():
    assert hashlib.sha256((contract / name).read_bytes()).hexdigest() == expected, name + ' manifest drift'

for name in ('IntentDictionary.h', 'IntentProtocol.h', 'IntentProtocol.cpp'):
    assert (fw / name).read_bytes() == (contract / name).read_bytes(), name + ' export drift'

def tlv(kind, flags, value):
    return struct.pack('<BBH', kind, flags, len(value)) + value + b'\0' * (-len(value) % 4)

# Independent specification fixture, not produced by either C++ codec.
stream = tlv(0x21, 1, b'\1') + tlv(0x22, 1, b'Z16 ')
stream += b''.join(tlv(kind, 1, struct.pack('<H', value))
                   for kind, value in ((0x23, 1280), (0x24, 720), (0x25, 30)))
body = tlv(0x10, 3, tlv(0x11, 1, b'\1\0\0\0') + tlv(0x20, 3, stream))
golden = struct.pack('<BBHHBB', 1, 0, len(body), 0, 0, 0) + body
golden += b'\0' * (1024 - len(golden))
with tempfile.TemporaryDirectory(prefix='intent-json-') as temp:
    out = Path(temp)
    (out / 'golden.bin').write_bytes(golden)
    cxx = os.environ.get('CXX', 'g++')
    flags = [cxx, '-std=c++11', '-Wall', '-Wextra', '-Werror', '-g',
             '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
             '-I' + str(root), '-I' + str(root / 'third-party/rsutils/include'),
             '-I' + str(args.json_include), '-I' + str(fw)]
    for source, target in ((fw / 'IntentProtocol.cpp', 'fw.o'),
                           (root / 'unit-tests/intent/fw-bridge.cpp', 'bridge.o')):
        subprocess.run(flags + ['-Drsintent=fwintent', '-c', str(source), '-o', str(out / target)], check=True)
    subprocess.run(flags + [str(root / 'src/intent/intent-json.cpp'), str(contract / 'IntentProtocol.cpp'),
                            str(root / 'unit-tests/intent/test-json.cpp'), str(out / 'fw.o'), str(out / 'bridge.o'),
                            '-o', str(out / 'test')], check=True)
    subprocess.run([str(out / 'test'), str(out / 'golden.bin')], check=True)

    if args.sdk_lib:
        lib = args.sdk_lib.resolve()
        executable = out / 'public-api'
        subprocess.run([cxx, '-std=c++11', '-Wall', '-Wextra', '-Werror',
                        '-isystem', str(root / 'include'),
                        str(root / 'unit-tests/intent/test-public-api.cpp'),
                        '-L' + str(lib), '-Wl,-rpath,' + str(lib), '-lrealsense2',
                        '-o', str(executable)], check=True)
        subprocess.run([str(executable)], check=True)
        print('PASS: public C++ wrapper, exported C entry points and error ownership')
