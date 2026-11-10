#!/usr/bin/env python3
"""Host protocol tests; --fw-root points to common/intent for cross-repo validation."""
import argparse
import os
import json
import hashlib
import shutil
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
        shared = fw / 'contract'
        subprocess.run([sys.executable, str(shared / 'tools/export_sdk.py'), '--check', str(contract)], check=True)
        fw_include = out / 'fw-generated'
        subprocess.run([sys.executable, str(shared / 'tools/generate.py'), '--output',
                        str(fw_include / 'IntentDictionary.h')], check=True)
        fw_sources = [shared / 'IntentProtocol.cpp', fw / 'IntentRules.cpp', fw / 'IntentController.cpp']
        fw_includes = ['-I' + str(fw_include), '-I' + str(fw), '-I' + str(shared), '-DINTENT_FW_RULES']
    else:
        print('note: --fw-root not given; FW side uses the vendored codec and a rule stub')
        fw_sources = [contract / 'IntentProtocol.cpp', root / 'unit-tests/intent/rules-stub.cpp']
        fw_includes = ['-I' + str(contract)]
    cxx = os.environ.get('CXX', 'g++')
    flags = [cxx, '-std=c++11', '-Wall', '-Wextra', '-Werror', '-g',
             '-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer',
             '-I' + str(root), '-I' + str(root / 'third-party/rsutils/include'),
             '-I' + str(args.json_include)]
    host_flags = list(flags)
    links = []
    if args.sdk_lib:
        lib = args.sdk_lib.resolve()
        host_flags += ['-DBUILD_EASYLOGGINGPP', '-DELPP_NO_DEFAULT_LOG_FILE',
                       '-DELPP_THREAD_SAFE', '-DEASYLOGGINGPP_ASYNC']
        links = ['-L' + str(lib), '-Wl,-rpath,' + str(lib), '-lrealsense2', '-pthread']
    objects = []
    for source in fw_sources + [root / 'unit-tests/intent/fw-bridge.cpp']:
        objects.append(out / ('fw-' + source.stem + '.o'))
        subprocess.run(flags + fw_includes + ['-Drsintent=fwintent', '-c', str(source), '-o', str(objects[-1])],
                       check=True)
    subprocess.run(host_flags + ['-I' + str(contract), str(root / 'src/intent/intent-json.cpp'),
                            str(contract / 'IntentProtocol.cpp'),
                            str(root / 'unit-tests/intent/json-host.cpp'),
                            str(root / 'unit-tests/intent/compatibility.cpp')] + [str(o) for o in objects] +
                   links + ['-o', str(out / 'test')], check=True)
    subprocess.run([str(out / 'test'), str(contract / 'vectors.json')], check=True)

    if args.fw_root:
        # Test-only dictionary: existing COLOR/RGB and RGB_R arrive at minor 1.
        future = out / 'future'
        (future / 'contract').mkdir(parents=True)
        for name in ('intent-json.cpp', 'intent-json.h', 'intent-tlv.h'):
            shutil.copyfile(root / 'src/intent' / name, future / name)
        for name in ('IntentProtocol.cpp', 'IntentProtocol.h', 'IntentStatus.h'):
            shutil.copyfile(contract / name, future / 'contract' / name)
        schema = json.loads((shared / 'schema/dictionary.json').read_text())
        schema['minor'] = 1
        for sensor in schema['sensors']:
            if sensor['name'] == 'COLOR': sensor['since'] = 1
        for branch in schema['branches']:
            if branch['name'] in ('RGB', 'RGB_R'): branch['since'] = 1
        schema_path = future / 'dictionary.json'
        schema_path.write_text(json.dumps(schema))
        subprocess.run([sys.executable, str(shared / 'tools/generate.py'), '--schema', str(schema_path),
                        '--output', str(future / 'contract/IntentDictionary.h')], check=True)
        executable = out / 'test-versions'
        subprocess.run(host_flags + ['-I' + str(future), '-I' + str(future / 'contract'),
                        '-I' + str(root / 'unit-tests/intent'), str(future / 'intent-json.cpp'),
                        str(future / 'contract/IntentProtocol.cpp'),
                        str(root / 'unit-tests/intent/version-host.cpp')] + links +
                        ['-o', str(executable)], check=True)
        subprocess.run([str(executable)], check=True)

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
