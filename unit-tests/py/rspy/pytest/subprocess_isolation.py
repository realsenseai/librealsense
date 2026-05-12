# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""Run each (file, device) group of pytest items in an isolated child pytest.

A native crash (SIGSEGV / SIGABRT) in the child terminates only that group; the
parent's session keeps going and other groups still run. Per-test reports come
back via pytest-reportlog so the terminal reporter, JUnit XML, and Jenkins
parsers see test-level outcomes. The child receives an internal `--rs-child`
flag so conftest skips parent-only setup (hub, per-test log) and short-circuits
module_device_setup (parent already arranged the hub state).

Group key is (fspath, device-id-from-brackets) - the same key the per-test log
file uses. Parametrized device-each tests get one subprocess per device;
non-parametrized tests in the same file share one.
"""

import json
import logging
import os
import shutil
import subprocess
import sys
import tempfile

import pytest
from _pytest.reports import TestReport

from rspy import devices, repo
from rspy.pytest import logging_setup
from rspy.pytest.device_helpers import resolve_item_serials

log = logging.getLogger(__name__)

PLUGIN_NAME = "rs_subprocess_isolation"

# Returncode -> human name for fatal signals. Python returns -signo on POSIX;
# 128+signo also seen via shells. Windows access violation = 0xC0000005.
_CRASH_SIGNALS = {
    -11: "SIGSEGV", 139: "SIGSEGV",
    -6:  "SIGABRT", 134: "SIGABRT",
    -1073741819: "access violation", 3221225477: "access violation",
}

_TIMED_OUT = "__rs_timed_out__"      # subprocess.TimeoutExpired sentinel
_DEFAULT_TEST_TIMEOUT = 200          # matches conftest default

# nodeid -> list[TestReport] for items already covered by an earlier group's
# subprocess. The protocol hook drains this for every item it's called for.
_pending_reports = {}


# ---------------------------------------------------------------------------
# Hook
# ---------------------------------------------------------------------------

def pytest_sessionstart(session):
    """Clear cross-session state in case the harness reuses the process."""
    _pending_reports.clear()

def _sync_setup_state(item, nextitem):
    """Keep pytest's SetupState stack in sync with nextitem.

    Returning True from pytest_runtest_protocol short-circuits the default
    protocol, so pytest_runtest_teardown (which normally calls
    session._setupstate.teardown_exact(nextitem)) never fires. Without this
    call the parent's stack accumulates stale collector entries from earlier
    in-process items and the next in-process item's setup trips the
    "previous item was not torn down properly" assertion.
    """
    item.session._setupstate.teardown_exact(nextitem)


@pytest.hookimpl(tryfirst=True)
def pytest_runtest_protocol(item, nextitem):
    """Run the (fspath, device-id) group containing *item* in a child pytest."""
    # Items already covered by a previous group's subprocess.
    if item.nodeid in _pending_reports:
        for report in _pending_reports.pop(item.nodeid):
            item.ihook.pytest_runtest_logreport(report=report)
        _sync_setup_state(item, nextitem)
        return True

    # Skip-marked items: emit a skipped report directly. Avoids spawning a
    # subprocess just to mark it as skipped (--live / --not-live filters add
    # skip markers to many items; one subprocess each adds up).
    skip_marker = item.get_closest_marker("skip")
    if skip_marker:
        reason = skip_marker.kwargs.get("reason", "")
        log.info(f"SKIPPED: {reason}" if reason else "SKIPPED")
        item.ihook.pytest_runtest_logreport(report=_make_skipped_report(item, reason))
        item.ihook.pytest_runtest_logreport(report=_make_passed_teardown(item))
        _sync_setup_state(item, nextitem)
        return True

    group = _find_group(item)
    _announce_group(group)
    by_nodeid = _run_group_in_subprocess(item.config, group)
    _pending_reports.update(by_nodeid)
    for report in _pending_reports.pop(item.nodeid, []):
        item.ihook.pytest_runtest_logreport(report=report)
    _sync_setup_state(item, nextitem)
    return True


def _announce(line):
    """Emit a parent-side line to both the per-test .log (via log.info) and
    stdout (so libci's build console picks it up even without `-s`). Skip the
    print when log_cli is already streaming to stdout (`-s` mode), to avoid
    duplicate output. Leading "\\n" ensures we land on our own line even when
    pytest's terminal reporter just wrote "PASSED [ X%]" without a newline.
    """
    log.info(line)
    if not getattr(logging_setup, "live_logging", False):
        print(f"\n-I- {line}", flush=True)


def _announce_group(items):
    # Skip single-test groups: the per-test line already shows the file + test
    # name, so a header would just duplicate that.
    if len(items) <= 1:
        return
    rel = getattr(items[0], "location", (None,))[0] or items[0].fspath
    func_names = [it.name for it in items]
    _announce(f"group {rel}: {', '.join(func_names)}")


# ---------------------------------------------------------------------------
# Grouping
# ---------------------------------------------------------------------------

def _group_key(item):
    """(fspath, device-id-from-brackets) - same key the per-test log uses.

    Strips pytest-repeat's "{step+1}-{count}" suffix so all repeat passes for
    the same (file, device) share one subprocess. That preserves the in-process
    skip-if-clean-pass optimisation in conftest's _module_retry_mode logic
    (the failure dict is process-local; splitting passes across subprocesses
    would empty it for retry passes and either always-skip or never-skip).
    """
    name = item.name
    lb, rb = name.find("["), name.rfind("]")
    if not (0 <= lb < rb):
        return (str(item.fspath), None)
    device_id = name[lb + 1:rb]
    callspec = getattr(item, "callspec", None)
    if callspec is not None:
        step = callspec.params.get("__pytest_repeat_step_number")
        if step is not None:
            try:
                count = int(item.config.getoption("count", default=1) or 1)
            except Exception:
                count = 1
            suffix = f"{step + 1}-{count}"
            if device_id == suffix:
                device_id = ""
            elif device_id.endswith("-" + suffix):
                device_id = device_id[: -(len(suffix) + 1)]
    return (str(item.fspath), device_id)


def _find_group(item):
    """All non-skip items in session.items sharing item's group key.

    Most groups are contiguous (filter_and_sort_items sorts by module +
    device_serial), but pytest-repeat with --repeat-scope=module interleaves
    by step: step-0 of every parametrize, then step-1 of every parametrize.
    For groups whose key strips the pytest-repeat suffix (so step-0 and
    step-1 of the same parametrize share a key), the matching items are NOT
    adjacent in the item list. We therefore scan the whole list rather than
    walking forward until the key changes. The first item we walk through
    "owns" the group; subsequent items in the same group hit _pending_reports
    via the early-return branch in pytest_runtest_protocol. Skip-marked items
    are passed over - they're handled by the skip-marker branch when their
    own protocol fires.
    """
    items = item.session.items
    key = _group_key(item)
    return [it for it in items
            if _group_key(it) == key and not it.get_closest_marker("skip")]


# ---------------------------------------------------------------------------
# Hub state per group (parent-side)
# ---------------------------------------------------------------------------

def _enable_target_devices_for_group(items, config):
    """Enable only the group's target device(s) on the hub before launching the child.

    Always recycle - matches module_device_setup's per-module cadence. With no
    hub (Jetson / dev workstations) enable_only falls through to hw_reset(target).
    """
    if config.getoption("--no-reset", default=False):
        return
    target = set()
    for item in items:
        target.update(resolve_item_serials(item, config))
    if not target:
        return
    sys.stdout.write("\n")
    sys.stdout.flush()
    try:
        devices.enable_only(list(target), recycle=True)
    except Exception as e:
        log.warning("Could not enable target devices %s: %s", sorted(target), e)


# ---------------------------------------------------------------------------
# Child invocation
# ---------------------------------------------------------------------------

def _forwarded_args(config):
    """CLI flags that need to flow through to the child pytest.

    Forwarded:    --context (runtime), --rslog, --repeat, --retries, --debug, --timeout
    Not forwarded: --device / --exclude-device (parent's enable_only already
                  filters the hub), --no-reset / --hub-reset (parent owns the
                  hub), --live / --not-live (parent emits skipped reports
                  directly).
    """
    args = []
    context = config.getoption("--context", default="")
    if context:
        args.extend(["--context", context])
    if config.getoption("--rslog", default=False):
        args.append("--rslog")
    repeat_val = config.getoption("repeat_count", default=0)
    if repeat_val:
        args.extend(["--repeat", str(repeat_val)])
    # Forward --retries so the child fires conftest's --retries -> --count + repeat-scope
    # expansion (which sets _module_retry_mode). Without this, the skip-if-clean hook
    # in the child won't activate and pytest-repeat's count won't be set, so the child's
    # collected items won't match the parametrized nodeids the parent dispatched.
    # Read the original retries value -- conftest zeroes config.option.retries to disable
    # pytest-retry's function-level retry, so config.getoption("retries") returns 0 here.
    retries_val = getattr(config, "_rs_original_retries", 0)
    if retries_val:
        args.extend(["--retries", str(retries_val)])
    # --debug and --timeout are consumed before pytest parses sys.argv, so
    # config doesn't track them; re-read from the original invocation argv.
    invocation_args = list(getattr(getattr(config, "invocation_params", None), "args", ()) or ())
    if "--debug" in invocation_args:
        args.append("--debug")
    for i, arg in enumerate(invocation_args):
        if arg == "--timeout" and i + 1 < len(invocation_args):
            args.extend(["--timeout", str(invocation_args[i + 1])])
            break
        elif arg.startswith("--timeout="):
            args.extend(["--timeout", arg[len("--timeout="):]])
            break
    return args


def _child_env():
    env = os.environ.copy()
    pyrs_dir = repo.find_pyrs_dir()
    if pyrs_dir:
        existing = env.get("PYTHONPATH", "")
        env["PYTHONPATH"] = pyrs_dir + os.pathsep + existing if existing else pyrs_dir
    return env


def _summary_for(returncode):
    if returncode == _TIMED_OUT:
        return "child pytest timed out and was killed"
    if returncode in _CRASH_SIGNALS:
        return f"child pytest crashed ({_CRASH_SIGNALS[returncode]})"
    return f"child pytest exited with code {returncode}"


# ---------------------------------------------------------------------------
# Reports (parsing + fabrication)
# ---------------------------------------------------------------------------

def _location(item):
    return getattr(item, "location", None) or (str(getattr(item, "fspath", "")), 0, item.nodeid)


def _make_report(item, when, outcome, longrepr=None):
    return TestReport(
        nodeid=item.nodeid, location=_location(item), keywords={},
        outcome=outcome, longrepr=longrepr, when=when, sections=[],
        duration=0.0, user_properties=[],
    )


def _make_skipped_report(item, reason):
    """Mirror pytest's skip-during-setup shape (longrepr is a (file, line, msg) tuple)."""
    fspath, lineno, _ = _location(item)
    return _make_report(item, "setup", "skipped", (fspath, lineno or 0, f"Skipped: {reason}"))


def _make_passed_teardown(item):
    """Pairs with a skip/setup report so the terminal reporter sees a complete pair."""
    return _make_report(item, "teardown", "passed")


def _fabricate_failed(item, longrepr):
    return _make_report(item, "call", "failed", longrepr)


def _parse_reportlog(path):
    """All TestReport objects from a pytest-reportlog file."""
    if not os.path.isfile(path):
        return []
    reports = []
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            try:
                data = json.loads(line)
            except json.JSONDecodeError:
                continue
            if data.get("$report_type") != "TestReport":
                continue
            try:
                report = TestReport._from_json(data)
            except Exception as e:
                log.warning("Failed to deserialize TestReport: %s", e)
                continue
            # Skip-report longrepr is a (filename, lineno, reason) tuple in
            # pytest; JSON round-trip turns it into a list and the terminal
            # reporter then asserts isinstance(longrepr, tuple).
            if report.outcome == "skipped" and isinstance(report.longrepr, list):
                report.longrepr = tuple(report.longrepr)
            reports.append(report)
    return reports


def _collect_reports(report_log_path, items, returncode, log_path):
    """Parse the reportlog and synthesize fabricated failures where missing.

    The first uncovered nodeid gets the actual exit code / signal info so the
    failure list points at one obvious culprit; subsequent ones get a cheaper
    "did not run" message.
    """
    by_nodeid = {it.nodeid: [] for it in items}
    item_by_nodeid = {it.nodeid: it for it in items}
    for r in _parse_reportlog(report_log_path):
        if r.nodeid in by_nodeid:
            by_nodeid[r.nodeid].append(r)

    crash_attributed = False
    for nid, reports in by_nodeid.items():
        phases = {r.when for r in reports}
        if not reports:
            if returncode == 0:
                summary = "did not run (no report from child)"
            elif crash_attributed:
                summary = "did not run; earlier test in group crashed"
            else:
                summary = _summary_for(returncode)
                crash_attributed = True
        elif "call" not in phases and returncode != 0:
            summary = _summary_for(returncode)
            crash_attributed = True
        else:
            continue
        longrepr = f"{summary}; see {log_path}" if log_path else summary
        reports.append(_fabricate_failed(item_by_nodeid[nid], longrepr))
    return by_nodeid


# ---------------------------------------------------------------------------
# Output forwarding
# ---------------------------------------------------------------------------

def _forward_child_output(child_output, log_path):
    """Stream the child's captured output into the parent's per-test log file.

    Uses shutil.copyfileobj so a large crash backtrace doesn't sit in memory.
    Falls back to sys.stdout when no FileHandler is active or live logging is on.
    """
    child_output.seek(0)
    handler = logging_setup._current_file_handler
    forward_to_terminal = handler is None or getattr(logging_setup, "live_logging", False)

    if log_path and handler is not None:
        try:
            handler.stream.flush()
            with open(log_path, "ab") as f:
                shutil.copyfileobj(child_output, f)
            handler.stream.seek(0, os.SEEK_END)
        except Exception as e:
            log.debug("Could not append child stdout to per-test log: %s", e)
            forward_to_terminal = True

    if forward_to_terminal:
        try:
            child_output.seek(0)
            sys.stdout.buffer.write(child_output.read())
            sys.stdout.flush()
        except Exception as e:
            log.debug("Could not forward child stdout to terminal: %s", e)


# ---------------------------------------------------------------------------
# Per-group subprocess
# ---------------------------------------------------------------------------

def _build_child_cmd(nodeids, report_log_path, config):
    cmd = [
        sys.executable, "-u", "-m", "pytest", *nodeids,
        "--rs-child",
        f"--report-log={report_log_path}",
        "-p", "no:cacheprovider",
        "--no-header",
        "--tb=short",
        # -s: child print() / logging reaches its fd 1 -> our temp file
        # -> per-test .log. Matches legacy run-unit-tests.py (no capture).
        "-s",
    ]
    cmd.extend(_forwarded_args(config))
    return cmd


def _run_child(cmd, child_output, child_timeout):
    """Run the child pytest. Returns its returncode or _TIMED_OUT on timeout."""
    try:
        p = subprocess.run(
            cmd, stdout=child_output, stderr=subprocess.STDOUT,
            env=_child_env(), timeout=child_timeout, check=False,
        )
        return p.returncode
    except subprocess.TimeoutExpired:
        try:
            child_output.write(
                f"\nchild pytest exceeded timeout {child_timeout}s and was killed\n".encode()
            )
        except Exception:
            pass
        return _TIMED_OUT


def _marker_timeout(item):
    m = item.get_closest_marker("timeout")
    if m is None:
        return None
    return m.args[0] if m.args else m.kwargs.get("timeout")


def _run_group_in_subprocess(config, items):
    """Run *items* in a fresh pytest child and return {nodeid: [TestReports]}."""
    _enable_target_devices_for_group(items, config)

    nodeids = [it.nodeid for it in items]
    default_timeout = getattr(config, "_rs_default_per_test_timeout", _DEFAULT_TEST_TIMEOUT)
    # +60s slack for startup + final teardown.
    child_timeout = sum((_marker_timeout(it) or default_timeout) for it in items) + 60

    fd, report_log_path = tempfile.mkstemp(prefix="rs-subproc-", suffix=".jsonl")
    os.close(fd)
    child_output = tempfile.TemporaryFile(mode="w+b")
    handler = logging_setup._current_file_handler
    log_path = getattr(handler, "baseFilename", None) if handler else None

    try:
        cmd = _build_child_cmd(nodeids, report_log_path, config)
        log.debug("subprocess isolation: %d nodeid(s) in group; cmd=%s",
                  len(nodeids), " ".join(cmd))
        returncode = _run_child(cmd, child_output, child_timeout)
        _forward_child_output(child_output, log_path)
        return _collect_reports(report_log_path, items, returncode, log_path)
    finally:
        try: child_output.close()
        except Exception: pass
        try: os.unlink(report_log_path)
        except OSError: pass
