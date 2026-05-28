# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""Aggregate --retries attempts per logical test.

Background
----------
``--retries N`` is implemented on top of pytest-repeat (see conftest.py): the
whole module re-runs N times when any test in it fails, with the device
recycled between passes. Each test in the module appears N+1 times in the
report (one per pass), with pytest-repeat's ``-<step>-<count>`` suffix
appended to the parametrize id (e.g. ``[D455-114222251278-1-2]`` /
``[D455-114222251278-2-2]``).

Per-line output during the run is intentionally left untouched: every attempt
prints its true PASSED/FAILED status, so flakes remain visible in the console.
This module collapses those attempts in the *aggregate* surfaces (terminal
summary line, JUnit XML, exit code) so a single logical test contributes one
outcome:

- PASSED if any attempt passed.
- FAILED if every attempt failed.
- SKIPPED if every attempt was skipped (e.g. skip-if-clean optimisation).

Hooks live in conftest.py and call into this module.
"""

import logging
import os
import re
import xml.etree.ElementTree as ET

import pytest


log = logging.getLogger('librealsense')

# NOT thread-safe: assumes single-process (non-xdist) execution. If pytest-xdist
# is adopted, these dicts must be replaced with a per-worker mechanism.
#
# Per-logical-test buffer for 'call'-phase reports. Populated as tests run.
# Key:  canonical nodeid (pytest-repeat suffix stripped).
# Value: list of (step, TestReport) tuples.
_call_reports = {}

# Per-logical-test buffer for setup/teardown failures (which surface as 'error'
# in TerminalReporter.stats). Same key/shape as _call_reports.
_error_reports = {}

# Module-scoped retry tracking. Set to True per (module_path, step) when ANY
# phase report for the module's tests at that step is failed. Used by the
# skip-if-clean optimisation - if step N had no failures, step N+1 doesn't
# need to run for that module.
_module_pass_had_failure = {}

# Activated by conftest.py when --retries is in use. While inactive, record_report
# is a no-op so the buffers don't accumulate TestReport objects across a long
# session that never asked for retries.
_enabled = False

_STEP_SUFFIX_RE = re.compile(r'\[([^\]]*)\]$')

# Conftest's skip-if-clean optimisation calls pytest.skip() with this exact prefix.
# We detect those reports here so they can be suppressed from per-line output and stats.
_RETRY_SKIP_PREFIX = "Module retry skipped"


def enable():
    """Activate report buffering. Called by conftest at --retries setup."""
    global _enabled
    _enabled = True


def reset():
    """Clear state (for tests that re-enter pytest_configure within one process)."""
    _call_reports.clear()
    _error_reports.clear()
    _module_pass_had_failure.clear()


def record_module_failure(mod_path, step):
    """Mark that the module's ``step`` had at least one failed report.

    Drives the skip-if-clean optimisation: a clean module pass means the next
    pass doesn't need to run.
    """
    _module_pass_had_failure[(mod_path, step)] = True


def should_skip_if_clean(item):
    """True if this item is a retry pass (step > 0) and the previous pass was clean.

    Called from both the protocol-bypass hook and conftest's pytest_runtest_protocol
    hookwrapper so the latter doesn't open a per-test log file for items that won't run.
    """
    if not getattr(item.config, '_module_retry_mode', False):
        return False
    callspec = getattr(item, 'callspec', None)
    if not callspec:
        return False
    step = callspec.params.get('__pytest_repeat_step_number', 0)
    if step <= 0:
        return False
    mod = item.module.__file__
    return not _module_pass_had_failure.get((mod, step - 1), False)


@pytest.hookimpl(tryfirst=True)
def pytest_runtest_protocol(item, nextitem):
    """Bypass the default protocol entirely for retry-skip-if-clean items.

    Returning True signals pytest we handled this item, which prevents the
    default protocol from running pytest_runtest_logstart (which would print
    a location line to the terminal), the setup/call/teardown phases, and
    pytest_runtest_logfinish. The item disappears completely from the per-line
    output and from the test count - exactly what we want for the
    skip-if-clean optimisation.
    """
    if _enabled and should_skip_if_clean(item):
        return True
    return None


def canonical_id(nodeid_or_name):
    """Strip pytest-repeat's '-<step>-<count>' tail.

    Examples (count=2)::

        test_x[D455-114222251278-1-2]  -> test_x[D455-114222251278]
        test_x[1-2]                     -> test_x
        test_x[D455]                    -> test_x[D455]   # no repeat suffix
        test_x[SN-7-8]                  -> test_x[SN-7-8] # not a valid repeat tail

    The last two components must look like pytest-repeat's ``step-count`` pair:
    both decimal, ``count >= 2``, and ``1 <= step <= count``. Any other tail is
    treated as part of the parametrize id and left alone, so test ids whose
    serial numbers happen to end in digits aren't mis-merged.
    """
    m = _STEP_SUFFIX_RE.search(nodeid_or_name)
    if not m:
        return nodeid_or_name
    parts = m.group(1).split('-')
    if len(parts) < 2 or not parts[-1].isdigit() or not parts[-2].isdigit():
        return nodeid_or_name
    step_val, count_val = int(parts[-2]), int(parts[-1])
    if count_val < 2 or step_val < 1 or step_val > count_val:
        return nodeid_or_name
    base = nodeid_or_name[:m.start()]
    remaining = parts[:-2]
    return f"{base}[{'-'.join(remaining)}]" if remaining else base


def _step_from_nodeid(nodeid):
    """Return the pytest-repeat step (1-indexed) or None if the tail isn't a valid pair."""
    m = _STEP_SUFFIX_RE.search(nodeid)
    if not m:
        return None
    parts = m.group(1).split('-')
    if len(parts) < 2 or not parts[-1].isdigit() or not parts[-2].isdigit():
        return None
    step_val, count_val = int(parts[-2]), int(parts[-1])
    if count_val < 2 or step_val < 1 or step_val > count_val:
        return None
    return step_val


def is_retry_skip_if_clean(report):
    """True if this report is the skip-if-clean optimisation's SKIPPED, not a real test skip.

    The conftest emits ``pytest.skip("Module retry skipped — no failures …")`` from
    ``pytest_runtest_setup`` when step > 0 and the previous module pass was clean.
    Those entries are pure UX noise — the test didn't run and isn't a real skip.
    """
    if not (report.skipped and report.when == 'setup'):
        return False
    longrepr = getattr(report, 'longrepr', None)
    if longrepr is None:
        return False
    if isinstance(longrepr, tuple) and len(longrepr) >= 3:
        return _RETRY_SKIP_PREFIX in str(longrepr[2])
    return _RETRY_SKIP_PREFIX in str(longrepr)


def record_report(report):
    """Buffer report keyed by canonical id.

    No-op unless ``enable()`` was called (i.e. unless --retries is active), so a
    plain pytest run doesn't accumulate TestReport objects in module-level dicts.

    'call' phase reports are tracked for pass/fail aggregation. Failed
    'setup'/'teardown' reports (which surface as 'error' in the terminal stats)
    are tracked separately so a later successful call attempt can rescue them.
    Skip-if-clean reports are ignored (they're optimisation noise, not test results).
    """
    if not _enabled:
        return
    if is_retry_skip_if_clean(report):
        return
    cid = canonical_id(report.nodeid)
    step = _step_from_nodeid(report.nodeid) or 0
    if report.when == 'call':
        _call_reports.setdefault(cid, []).append((step, report))
    elif report.when in ('setup', 'teardown') and report.failed:
        _error_reports.setdefault(cid, []).append((step, report))


def _classify_groups():
    """Group buffered reports per logical test and decide what to drop from stats.

    Returns ``(rescued, hard_failed, dup_passes, failed_to_drop, errors_to_drop)``:
        rescued         -- canonical ids where some attempt didn't pass but a later one did
        hard_failed     -- canonical ids where every attempt failed (call FAILED or setup ERROR)
        dup_passes      -- TestReport objects to remove from stats['passed']
        failed_to_drop  -- TestReport objects to remove from stats['failed']
        errors_to_drop  -- TestReport objects to remove from stats['error']

    Rescued tests have all their failed call reports + all error reports dropped.
    Hard-failed tests keep only the most recent failure/error overall.
    All-passed tests that ran more than once keep the first pass.
    """
    rescued, hard_failed = [], []
    dup_passes, failed_to_drop, errors_to_drop = [], [], []

    all_cids = set(_call_reports) | set(_error_reports)
    for cid in all_cids:
        call_attempts = sorted(_call_reports.get(cid, []), key=lambda x: x[0])
        error_attempts = sorted(_error_reports.get(cid, []), key=lambda x: x[0])
        passed = [r for _, r in call_attempts if r.passed]
        failed_calls = [r for _, r in call_attempts if r.failed]
        errors = [r for _, r in error_attempts]
        had_failure = bool(failed_calls or errors)

        if not had_failure:
            if len(passed) > 1:
                dup_passes.extend(passed[1:])
            continue

        if passed:
            rescued.append(cid)
            failed_to_drop.extend(failed_calls)
            errors_to_drop.extend(errors)
            if len(passed) > 1:
                dup_passes.extend(passed[1:])
            continue

        hard_failed.append(cid)
        all_failures = sorted(
            [(s, r, 'call') for s, r in call_attempts if r.failed]
            + [(s, r, 'error') for s, r in error_attempts],
            key=lambda x: x[0],
        )
        for _, r, kind in all_failures[:-1]:
            if kind == 'call':
                failed_to_drop.append(r)
            else:
                errors_to_drop.append(r)

    return rescued, hard_failed, dup_passes, failed_to_drop, errors_to_drop


def reconcile_terminal_stats(terminalreporter):
    """Rewrite ``terminalreporter.stats`` so each logical test contributes once.

    Returns ``(rescued, hard_failed)`` lists for the caller to print as a summary.
    """
    if not _call_reports and not _error_reports:
        return [], []

    rescued, hard_failed, dup_passes, failed_drop, errors_drop = _classify_groups()

    if not (dup_passes or failed_drop or errors_drop):
        return rescued, hard_failed

    stats = terminalreporter.stats
    drop_failed_ids = {id(r) for r in failed_drop}
    drop_error_ids = {id(r) for r in errors_drop}
    drop_passed_ids = {id(r) for r in dup_passes}

    if 'failed' in stats:
        stats['failed'] = [r for r in stats['failed'] if id(r) not in drop_failed_ids]
    if 'error' in stats:
        stats['error'] = [r for r in stats['error'] if id(r) not in drop_error_ids]
    if 'passed' in stats:
        stats['passed'] = [r for r in stats['passed'] if id(r) not in drop_passed_ids]

    return rescued, hard_failed


def adjust_session_exitstatus(session, terminalreporter):
    """Recompute ``session.testsfailed`` from reconciled stats and flip exit status.

    Counts both 'failed' (call assertions) and 'error' (setup/teardown failures);
    pytest's wrap_session reads ``session.exitstatus`` after pytest_sessionfinish,
    so updating it here propagates to the process return code.
    """
    stats = terminalreporter.stats
    n_unrescued = len(stats.get('failed', [])) + len(stats.get('error', []))
    session.testsfailed = n_unrescued
    if n_unrescued == 0 and session.exitstatus == pytest.ExitCode.TESTS_FAILED:
        session.exitstatus = pytest.ExitCode.OK


def print_retry_summary(terminalreporter, rescued, hard_failed):
    """Print a clearly-marked retry summary so CI logs surface what was retried."""
    if not rescued and not hard_failed:
        return
    tr = terminalreporter
    tr.write_sep('=', 'retry summary', bold=True)
    if rescued:
        tr.write_line(
            f"{len(rescued)} test(s) recovered after retry - overall result PASS:",
            yellow=True,
        )
        for cid in rescued:
            tr.write_line(f"  RETRY-RESCUED {cid}", yellow=True)
    if hard_failed:
        tr.write_line(
            f"{len(hard_failed)} test(s) still failing after retry:",
            red=True,
        )
        for cid in hard_failed:
            tr.write_line(f"  RETRY-FAIL    {cid}", red=True)
    tr.write_line("")


def rewrite_junit_xml(xmlpath):
    """Collapse parametrized retry attempts into one ``<testcase>`` per logical test.

    For each group of ``<testcase>`` sharing the same (classname, canonical name):
        - If any attempt has no failure/error/skipped child → keep one PASSED
          testcase (strip any failure/error children from the kept entry).
        - Else → keep the last testcase (most recent failure).

    The kept ``<testcase>``'s ``time`` attribute is replaced with the SUM of all
    attempts so trend graphs reflect the real cost of a retried test.

    Updates the parent ``<testsuite>`` aggregate counts.
    """
    if not xmlpath or not os.path.exists(xmlpath):
        return
    try:
        tree = ET.parse(xmlpath)
    except ET.ParseError:
        log.warning(f"JUnit XML at {xmlpath} could not be parsed; skipping retry aggregation rewrite")
        return
    root = tree.getroot()

    for testsuite in root.iter('testsuite'):
        testcases = list(testsuite.findall('testcase'))
        groups = {}
        order = []
        for tc in testcases:
            key = (tc.get('classname', ''), canonical_id(tc.get('name', '')))
            if key not in groups:
                groups[key] = []
                order.append(key)
            groups[key].append(tc)

        for key in order:
            tcs = groups[key]
            if len(tcs) == 1:
                tcs[0].set('name', key[1])
                continue
            has_pass = any(
                not tc.findall('failure')
                and not tc.findall('error')
                and not tc.findall('skipped')
                for tc in tcs
            )
            if has_pass:
                keep = next(
                    (tc for tc in tcs
                     if not tc.findall('failure')
                     and not tc.findall('error')
                     and not tc.findall('skipped')),
                    tcs[-1],
                )
                for child in list(keep):
                    if child.tag in ('failure', 'error'):
                        keep.remove(child)
            else:
                keep = tcs[-1]
            total_time = 0.0
            for tc in tcs:
                try:
                    total_time += float(tc.get('time', '0') or 0)
                except ValueError:
                    pass
            keep.set('time', f'{total_time:.3f}')
            keep.set('name', key[1])
            for tc in tcs:
                if tc is not keep:
                    testsuite.remove(tc)

        remaining = list(testsuite.findall('testcase'))
        testsuite.set('tests', str(len(remaining)))
        testsuite.set('failures', str(sum(1 for tc in remaining if tc.findall('failure'))))
        testsuite.set('errors', str(sum(1 for tc in remaining if tc.findall('error'))))
        testsuite.set('skipped', str(sum(1 for tc in remaining if tc.findall('skipped'))))

    tree.write(xmlpath, encoding='utf-8', xml_declaration=True)
