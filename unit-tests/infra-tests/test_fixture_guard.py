# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
Tests for rspy/pytest/collection.py fixture guard (assert_module_fixtures_are_per_camera).

Verifies the collection-phase guard that fails collection when a module running on more
than one camera uses a module-scoped fixture that is NOT re-created per camera (i.e. does
not transitively depend on _test_device_serial), since its teardown would then run at
module end instead of per camera.
"""

import types
import pytest
from rspy.pytest.collection import (
    assert_module_fixtures_are_per_camera,
    _fixture_reaches,
    _PER_CAMERA_FIXTURE_ALLOWLIST,
)


# ---------------------------------------------------------------------------
# Builders for the minimal slice of pytest objects the guard touches.
# ---------------------------------------------------------------------------

def make_fixturedef(argnames=(), scope="module", co_filename="/repo/unit-tests/conftest.py"):
    """A fake FixtureDef with the attributes the guard reads."""
    fd = types.SimpleNamespace()
    fd.argnames = tuple(argnames)
    fd.scope = scope
    fd.func = types.SimpleNamespace(__code__=types.SimpleNamespace(co_filename=co_filename))
    return fd


class FakeFixtureManager:
    """Maps fixture name -> [fixturedef]; node arg is accepted and ignored."""
    def __init__(self, defs_by_name):
        self._defs = defs_by_name

    def getfixturedefs(self, name, node):
        return self._defs.get(name)


def make_item(nodeid, serial, fixturenames, has_callspec=True, serial_in_params=True):
    item = types.SimpleNamespace()
    item.nodeid = nodeid
    item.fixturenames = list(fixturenames)
    if has_callspec:
        params = {}
        if serial_in_params:
            params["_test_device_serial"] = serial
        item.callspec = types.SimpleNamespace(params=params)
    # else: no callspec attribute at all
    return item


def make_session(defs_by_name):
    session = types.SimpleNamespace()
    session._fixturemanager = FakeFixtureManager(defs_by_name)
    return session


def two_camera_items(fixture_name):
    """Two items in the same module, on two distinct cameras, both using fixture_name."""
    fixturenames = ["_test_device_serial", fixture_name]
    return [
        make_item("mod.py::test_a[D455-111]", "111", fixturenames),
        make_item("mod.py::test_a[D435-222]", "222", fixturenames),
    ]


# ---------------------------------------------------------------------------
# assert_module_fixtures_are_per_camera
# ---------------------------------------------------------------------------

class TestPerCameraGuard:
    def test_positive_fixture_reaches_serial(self):
        """Module-scoped fixture that transitively reaches _test_device_serial → passes."""
        defs = {
            "_test_device_serial": [make_fixturedef(scope="function")],
            "needs_device": [make_fixturedef(argnames=("_test_device_serial",))],
        }
        items = two_camera_items("needs_device")
        # No raise.
        assert_module_fixtures_are_per_camera(make_session(defs), items)

    def test_negative_fixture_does_not_reach_serial(self):
        """Module-scoped fixture not reaching _test_device_serial in a 2-camera module → raises."""
        defs = {
            "_test_device_serial": [make_fixturedef(scope="function")],
            "shared_state": [make_fixturedef(argnames=())],
        }
        items = two_camera_items("shared_state")
        with pytest.raises(pytest.UsageError, match="shared_state"):
            assert_module_fixtures_are_per_camera(make_session(defs), items)

    def test_allowlist_escape_hatch(self):
        """An allowlisted fixture is exempt even when module-scoped and device-independent."""
        name = next(iter(_PER_CAMERA_FIXTURE_ALLOWLIST - {"_test_device_serial"}))
        defs = {
            "_test_device_serial": [make_fixturedef(scope="function")],
            name: [make_fixturedef(argnames=())],
        }
        items = two_camera_items(name)
        assert_module_fixtures_are_per_camera(make_session(defs), items)

    def test_site_packages_fixture_ignored(self):
        """A third-party plugin fixture (co_filename under site-packages) is not policed."""
        defs = {
            "_test_device_serial": [make_fixturedef(scope="function")],
            "plugin_fx": [make_fixturedef(
                argnames=(), co_filename="/x/site-packages/pytest_thing/plugin.py")],
        }
        items = two_camera_items("plugin_fx")
        assert_module_fixtures_are_per_camera(make_session(defs), items)

    def test_single_camera_module_not_checked(self):
        """With a single distinct camera, a device-independent module fixture is harmless."""
        defs = {
            "_test_device_serial": [make_fixturedef(scope="function")],
            "shared_state": [make_fixturedef(argnames=())],
        }
        fixturenames = ["_test_device_serial", "shared_state"]
        items = [
            make_item("mod.py::test_a[D455-111]", "111", fixturenames),
            make_item("mod.py::test_b[D455-111]", "111", fixturenames),
        ]
        assert_module_fixtures_are_per_camera(make_session(defs), items)

    def test_function_scoped_fixture_not_checked(self):
        """A non-module-scoped fixture is fine even if device-independent."""
        defs = {
            "_test_device_serial": [make_fixturedef(scope="function")],
            "fn_fx": [make_fixturedef(argnames=(), scope="function")],
        }
        items = two_camera_items("fn_fx")
        assert_module_fixtures_are_per_camera(make_session(defs), items)

    def test_item_without_callspec_skipped(self):
        """Non-parametrized items (no callspec) are not bound to a camera → ignored."""
        defs = {"shared_state": [make_fixturedef(argnames=())]}
        items = [make_item("mod.py::test_plain", None,
                            ["shared_state"], has_callspec=False)]
        assert_module_fixtures_are_per_camera(make_session(defs), items)

    def test_serial_key_absent_skipped_but_present_none_checked(self):
        """Key-absent means 'not device-bound' (skip); present-but-None means device-bound.

        A 2-camera module where one item carries _test_device_serial=None alongside a real
        camera still counts that item as bound, so its offending fixture is caught.
        """
        defs = {
            "_test_device_serial": [make_fixturedef(scope="function")],
            "shared_state": [make_fixturedef(argnames=())],
        }
        fixturenames = ["_test_device_serial", "shared_state"]
        items = [
            make_item("mod.py::test_a[D455-111]", "111", fixturenames),
            make_item("mod.py::test_a[none]", None, fixturenames),  # present-but-None
        ]
        with pytest.raises(pytest.UsageError, match="shared_state"):
            assert_module_fixtures_are_per_camera(make_session(defs), items)


# ---------------------------------------------------------------------------
# _fixture_reaches
# ---------------------------------------------------------------------------

class TestFixtureReaches:
    def test_direct_dependency(self):
        defs = {"_test_device_serial": [make_fixturedef(scope="function")]}
        fm = FakeFixtureManager(defs)
        fd = make_fixturedef(argnames=("_test_device_serial",))
        assert _fixture_reaches(fm, fd, "_test_device_serial", node=None) is True

    def test_transitive_dependency(self):
        defs = {
            "_test_device_serial": [make_fixturedef(scope="function")],
            "mid": [make_fixturedef(argnames=("_test_device_serial",))],
        }
        fm = FakeFixtureManager(defs)
        fd = make_fixturedef(argnames=("mid",))
        assert _fixture_reaches(fm, fd, "_test_device_serial", node=None) is True

    def test_no_path_returns_false(self):
        defs = {"other": [make_fixturedef(argnames=())]}
        fm = FakeFixtureManager(defs)
        fd = make_fixturedef(argnames=("other",))
        assert _fixture_reaches(fm, fd, "_test_device_serial", node=None) is False

    def test_cycle_does_not_recurse_forever(self):
        """A→B→A override cycle must terminate (visited-set guard), not RecursionError."""
        a = make_fixturedef(argnames=("b",))
        b = make_fixturedef(argnames=("a",))
        fm = FakeFixtureManager({"a": [a], "b": [b]})
        assert _fixture_reaches(fm, a, "_test_device_serial", node=None) is False
