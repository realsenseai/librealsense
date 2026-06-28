# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""Collection-phase filtering and sorting for RealSense tests."""

import pytest


# Module-scoped fixtures that are intentionally device-independent (not re-created per camera).
_PER_CAMERA_FIXTURE_ALLOWLIST = {
    "_test_device_serial",          # the per-camera anchor itself (parametrized per device)
    # Local override of pytest-repeat's fixture (lives in conftest, so the site-packages
    # bypass below doesn't filter it out); per-repeat pass, device-independent.
    "__pytest_repeat_step_number",
}


def _fixture_reaches(fm, fixturedef, target, node, visited=None):
    """True if fixturedef transitively depends on the fixture named `target`.

    `visited` tracks already-explored fixturedefs so diamond dependencies aren't re-walked
    and fixture-override cycles can't recurse forever.
    """
    if visited is None:
        visited = set()
    if id(fixturedef) in visited:
        return False
    visited.add(id(fixturedef))
    return any(arg == target
               or any(_fixture_reaches(fm, dep, target, node, visited)
                      for dep in (fm.getfixturedefs(arg, node) or []))
               for arg in fixturedef.argnames)


def assert_module_fixtures_are_per_camera(session, items):
    """Guard: in a module that runs across more than one camera, every module-scoped fixture
    must be re-created per camera, i.e. (transitively) depend on _test_device_serial. One that
    doesn't is created once and SHARED across all of the module's cameras, so its teardown runs
    at module end instead of per camera — fail collection so that mistake can't slip in.

    Only modules with >1 distinct camera are checked: with a single camera, module scope is
    already per-camera, so a module-scoped fixture there is harmless (e.g. a device_each that
    matches one device, or a no-device CI run that resolves to a single skip sentinel).
    """
    fm = session._fixturemanager
    # Group device-parametrized items by module and collect each module's distinct cameras.
    module_items = {}    # module path -> [items]
    module_cameras = {}  # module path -> set of distinct _test_device_serial values
    for item in items:
        callspec = getattr(item, "callspec", None)
        if not callspec or "_test_device_serial" not in callspec.params:
            continue  # not bound to a camera (key absent != present-but-None)
        mod = item.nodeid.split("::", 1)[0]
        module_items.setdefault(mod, []).append(item)
        module_cameras.setdefault(mod, set()).add(str(callspec.params.get("_test_device_serial")))

    offenders = set()
    for mod, mod_items in module_items.items():
        if len(module_cameras[mod]) < 2:
            continue  # single camera → module scope is already per-camera
        for item in mod_items:
            for name in item.fixturenames:
                if name in _PER_CAMERA_FIXTURE_ALLOWLIST:
                    continue
                defs = fm.getfixturedefs(name, item)
                if not defs:
                    continue
                fixturedef = defs[-1]
                if fixturedef.scope != "module":
                    continue
                if "site-packages" in fixturedef.func.__code__.co_filename:
                    continue  # don't police plugin fixtures (pytest-retry/repeat/etc.)
                if not _fixture_reaches(fm, fixturedef, "_test_device_serial", item):
                    offenders.add(name)
    if offenders:
        raise pytest.UsageError(
            f"Module-scoped fixture(s) {sorted(offenders)} are used by a module that runs on more "
            "than one camera but do not depend on _test_device_serial, so they are created once and "
            "SHARED across the module's cameras — their teardown runs at module end, not per camera. "
            "Depend on test_device (or _test_device_serial) for per-camera lifecycle, or add the "
            "fixture to _PER_CAMERA_FIXTURE_ALLOWLIST if it is genuinely device-independent."
        )


def filter_and_sort_items(config, items):
    """Auto-skip nightly/dds tests unless opted in, filter --live/--not-live, and sort by priority.

    Called from the pytest_collection_modifyitems hook.
    """
    markexpr = config.getoption("-m", default="")
    context = config.getoption("--context", default="").split()

    # Generic context gating: tests marked with @pytest.mark.context("X") are skipped
    # unless "X" appears in --context or -m. No infra changes needed for new contexts.
    for item in items:
        for marker in item.iter_markers("context"):
            if not marker.args:
                continue
            required_context = marker.args[0]
            if required_context in context:
                continue
            if markexpr and required_context in markexpr:
                continue
            item.add_marker(pytest.mark.skip(
                reason=f"Requires --context {required_context} (or -m {required_context})"))

    # Skip non-device tests when --live is specified
    if config.getoption("--live", default=False):
        skip_no_device = pytest.mark.skip(reason="--live: test has no device requirement")
        for item in items:
            has_device = any(item.iter_markers("device")) or any(item.iter_markers("device_each"))
            if not has_device:
                item.add_marker(skip_no_device)

    # Skip device tests when --not-live is specified (no hardware, e.g. GHA runners)
    if config.getoption("--not-live", default=False):
        skip_device = pytest.mark.skip(reason="--not-live: test requires a live device")
        for item in items:
            has_device = any(item.iter_markers("device")) or any(item.iter_markers("device_each"))
            if has_device:
                item.add_marker(skip_device)

    def get_priority(item):
        marker = item.get_closest_marker("priority")
        if marker and marker.args:
            return marker.args[0]
        return 500

    items.sort(key=get_priority)

    # Group parametrized tests by device within each module, so all tests run on one
    # device before switching to the next (matching run-unit-tests.py behavior).
    # Within a (module, device) bucket, also sort by pytest-repeat step so pass 0
    # completes before pass 1 — preserves --repeat N module-scoped ordering so
    # module-scoped fixtures see one pass at a time.
    def get_device_group_key(item):
        module = item.module.__name__
        params = item.callspec.params if hasattr(item, 'callspec') else {}
        device_serial = params.get('_test_device_serial', '')
        step = params.get('__pytest_repeat_step_number', 0)
        return (module, device_serial, step)

    items.sort(key=get_device_group_key)
