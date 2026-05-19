"""
E2E conftest: mock ONLY the hardware layer, then exec() the REAL conftest.py.
If someone changes conftest.py, these tests exercise the real change.

This file is copied to a temp dir for subprocess isolation. The real
unit-tests/ path is passed via INFRA_UNIT_TESTS_DIR env var.
"""
import sys, os, types

_unit_tests_dir = os.environ['INFRA_UNIT_TESTS_DIR']
_py_dir = os.path.join(_unit_tests_dir, 'py')
if _py_dir not in sys.path:
    sys.path.insert(0, _py_dir)

# Fake pyrealsense2 — track log_to_console calls so tests can verify --rslog
import json as _json
_tracking_log = os.path.join(os.path.dirname(os.path.abspath(__file__)), '_tracking.json')
_tracking = {"rslog_calls": [], "query_kwargs": [], "enable_only_calls": []}
def _save_tracking():
    with open(_tracking_log, 'w') as _f:
        _json.dump(_tracking, _f)

_rs = types.ModuleType("pyrealsense2")
_rs.__file__ = "fake_pyrealsense2"
def _mock_log_to_console(level):
    _tracking["rslog_calls"].append({"level": level})
    _save_tracking()
_rs.log_to_console = _mock_log_to_console
class _CameraInfo:
    name = "name"
    product_line = "product_line"
    physical_port = "physical_port"
    connection_type = "connection_type"
_rs.camera_info = _CameraInfo
class _LogSeverity:
    debug = 0
_rs.log_severity = _LogSeverity
class _FakeContext:
    @property
    def devices(self):
        return []
_rs.context = _FakeContext
sys.modules["pyrealsense2"] = _rs

# Mock rspy.devices
class FakeDevice:
    def __init__(self, sn, name):
        self.sn = sn
        self.name = name
        self.connection_type = "USB"

_inventory = {
    "D455": ("111", "D400"),
    "D435": ("222", "D400"),
    "D515": ("555", "D500"),
    "D401": ("777", "D400"),
}
_sn_map = {"111": "D455", "222": "D435", "555": "D515", "777": "D401"}

import rspy.devices as _dev
def _mock_by_spec(pattern, ignored):
    if pattern.endswith("*"):
        pl = pattern[:-1]
        for name, (sn, p) in _inventory.items():
            if p == pl:
                yield sn
    elif pattern in _inventory:
        yield _inventory[pattern][0]
    elif pattern in _sn_map:
        yield pattern

def _mock_get(sn):
    name = _sn_map.get(sn)
    return FakeDevice(sn, name) if name else None

_dev.by_spec = _mock_by_spec
_dev.get = _mock_get
_dev._device_by_sn = {sn: FakeDevice(sn, n) for sn, n in _sn_map.items()}
_dev.hub = None
_dev._context = None
def _mock_query(**kw):
    _tracking["query_kwargs"].append(kw)
    _save_tracking()
_dev.query = _mock_query
_dev.map_unknown_ports = lambda: None
_dev.wait_until_all_ports_disabled = lambda: None

# Track enable_only calls so tests can verify hub port behavior
def _mock_enable_only(serials, recycle=True):
    _tracking["enable_only_calls"].append({"serials": list(serials), "recycle": recycle})
    _save_tracking()
_dev.enable_only = _mock_enable_only

# exec() the REAL conftest.py
_conftest_path = os.path.join(_unit_tests_dir, 'conftest.py')
with open(_conftest_path) as _f:
    _src = _f.read()
current_dir = _unit_tests_dir
py_dir = os.path.join(_unit_tests_dir, "py")
if py_dir not in sys.path:
    sys.path.insert(0, py_dir)
exec(compile(_src, _conftest_path, "exec"), globals())
