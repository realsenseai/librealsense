#!/usr/bin/env python3
"""Host protocol tests; --fw-root points to common/intent for cross-repo validation."""
import argparse
import os
import json
import hashlib
from pathlib import Path
import subprocess
import sys
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--fw-root', type=Path)
parser.add_argument('--json-include', type=Path, required=True,
                    help='directory containing nlohmann/json.hpp (SDK build dependency)')
parser.add_argument('--sdk-lib', type=Path, help='directory containing built librealsense2.so; also compile/run public API test')
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
contract = root / 'src/intent/contract'
manifest = json.loads((contract / 'export.json').read_text())
for name, expected in manifest['files'].items():
    assert hashlib.sha256((contract / name).read_bytes()).hexdigest() == expected, name + ' manifest drift'
with tempfile.TemporaryDirectory(prefix='intent-json-') as temp:
    out = Path(temp)
    if args.fw_root:
        # Cross-repo: the vendored bundle must be exactly what FW exports, and the
        # FW side of the test is compiled from the FW tree with its generated header.
        fw = args.fw_root.resolve()
        subprocess.run([sys.executable, str(fw / 'tools/export_sdk.py'), '--check', str(contract)], check=True)
        fw_include = out / 'fw-generated'
        subprocess.run([sys.executable, str(fw / 'tools/generate.py'), '--output',
                        str(fw_include / 'IntentDictionary.h')], check=True)
        fw_sources = [fw / 'IntentProtocol.cpp', fw / 'IntentRules.cpp']
        fw_includes = ['-I' + str(fw_include), '-I' + str(fw), '-DINTENT_FW_RULES']
    else:
        print('note: --fw-root not given; FW side uses the vendored codec and a rule stub')
        fw_sources = [contract / 'IntentProtocol.cpp', root / 'unit-tests/intent/rules-stub.cpp']
        fw_includes = ['-I' + str(contract)]
    cxx = os.environ.get('CXX', 'g++')
    flags = [cxx, '-std=c++11', '-Wall', '-Wextra', '-Werror', '-g',
             '-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer',
             '-I' + str(root), '-I' + str(root / 'third-party/rsutils/include'),
             '-I' + str(args.json_include)]
    objects = []
    for source in fw_sources + [root / 'unit-tests/intent/fw-bridge.cpp']:
        objects.append(out / ('fw-' + source.stem + '.o'))
        subprocess.run(flags + fw_includes + ['-Drsintent=fwintent', '-c', str(source), '-o', str(objects[-1])],
                       check=True)
    subprocess.run(flags + ['-I' + str(contract), str(root / 'src/intent/intent-json.cpp'),
                            str(contract / 'IntentProtocol.cpp'),
                            str(root / 'unit-tests/intent/json-host.cpp')] + [str(o) for o in objects] +
                   ['-o', str(out / 'test')], check=True)
    subprocess.run([str(out / 'test'), str(contract / 'vectors.json')], check=True)

    if args.sdk_lib:
        lib = args.sdk_lib.resolve()
        executable = out / 'public-api'
        subprocess.run([cxx, '-std=c++11', '-Wall', '-Wextra', '-Werror',
                        '-isystem', str(root / 'include'),
                        str(root / 'unit-tests/intent/public-api.cpp'),
                        '-L' + str(lib), '-Wl,-rpath,' + str(lib), '-lrealsense2',
                        '-o', str(executable)], check=True)
        subprocess.run([str(executable)], check=True)
        print('PASS: public C++ wrapper, exported C entry points and error ownership')
