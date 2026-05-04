# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
E2E: Custom CLI options — verify each flag is accepted and has the expected effect.

Each test checks both that the flag parses without error AND that it produces
the correct behavior (device filtering, context gating, recycle control, etc.).
"""

from helpers import run_e2e, assert_outcomes


class TestCliOptionsRegistered:

    def test_device(self):
        """--device D455 should restrict device_each('D400*') to only D455."""
        rc, out, *_ = run_e2e("pytest-cli.py", "-k", "test_include", "--device", "D455")
        assert_outcomes(out, passed=1)

    def test_exclude_device(self):
        """--exclude-device D455 should remove D455 from device_each('D400*')."""
        rc, out, *_ = run_e2e("pytest-cli.py", "-k", "test_exclude and not multi",
                               "--exclude-device", "D455")
        assert_outcomes(out, passed=2)  # D435, D401 remain

    def test_no_reset(self):
        """--no-reset should call enable_only with recycle=False."""
        rc, out, tracking = run_e2e("pytest-device-setup.py", "-k", "test_d455 and not excluded",
                                     "--no-reset")
        assert_outcomes(out, passed=1)
        calls = tracking["enable_only_calls"]
        assert len(calls) == 1
        assert calls[0]['recycle'] is False

    def test_hub_reset(self):
        """--hub-reset should pass hub_reset=True to devices.query()."""
        rc, out, tracking = run_e2e("pytest-passthrough.py", "--hub-reset")
        assert rc == 0
        assert any(kw.get("hub_reset") is True for kw in tracking["query_kwargs"])

    def test_defaults(self):
        """Without any flags, conftest should set correct defaults:
        hub_reset=False, rslog off, timeout 200s/thread."""
        rc, out, tracking = run_e2e("pytest-passthrough.py")
        assert rc == 0
        assert any(kw.get("hub_reset") is False for kw in tracking["query_kwargs"])
        assert len(tracking["rslog_calls"]) == 0
        assert "timeout: 200s" in out
        assert "timeout method: thread" in out

    def test_rslog(self):
        """--rslog should call rs.log_to_console."""
        rc, out, tracking = run_e2e("pytest-passthrough.py", "--rslog")
        assert rc == 0
        assert len(tracking["rslog_calls"]) > 0

    def test_debug(self):
        """--debug should enable pytest debug output."""
        rc, out, *_ = run_e2e("pytest-passthrough.py", "--debug")
        assert rc == 0
        assert "pytest debug information" in out or "registered" in out

    def test_rs_help(self):
        """--rs-help is accepted (documentation flag, no observable behavior)."""
        rc, *_ = run_e2e("pytest-passthrough.py", "--rs-help")
        assert rc == 0

    def test_retries(self):
        """--retries 1 should retry a failed test and recycle the device on retry."""
        rc, out, tracking = run_e2e("pytest-retry.py", "--retries", "1")
        assert_outcomes(out, passed=1)
        calls = tracking["enable_only_calls"]
        assert len(calls) == 2
        assert all(c['recycle'] is True for c in calls)

    def test_repeat(self):
        """--repeat 3 should repeat the test 3 times, recycling the device each time."""
        rc, out, tracking = run_e2e("pytest-device-setup.py", "-k", "test_d455 and not excluded",
                                     "--repeat", "3")
        assert_outcomes(out, passed=3)
        calls = tracking["enable_only_calls"]
        assert len(calls) == 3
        assert all(c['recycle'] is True for c in calls)

    def test_repeat_no_reset(self):
        """--repeat 3 --no-reset should repeat without recycling."""
        rc, out, tracking = run_e2e("pytest-device-setup.py", "-k", "test_d455 and not excluded",
                                     "--repeat", "3", "--no-reset")
        assert_outcomes(out, passed=3)
        calls = tracking["enable_only_calls"]
        # First run enables without recycle, subsequent runs skip enable_only entirely
        assert len(calls) == 1
        assert calls[0]['recycle'] is False

    def test_device_nonexistent(self):
        """--device D999 with no matching device should produce 0 parametrized instances."""
        rc, out, *_ = run_e2e("pytest-each.py", "-k", "test_d400 and not exclude", "--device", "D999")
        assert_outcomes(out, passed=0)

    def test_multiple_device_flags(self):
        """--device can be used multiple times to include several devices."""
        rc, out, *_ = run_e2e("pytest-cli.py", "-k", "test_multi_include",
                               "--device", "D455", "--device", "D435")
        assert_outcomes(out, passed=2)

    def test_multiple_exclude_device_flags(self):
        """--exclude-device can be used multiple times to exclude several devices."""
        rc, out, *_ = run_e2e("pytest-cli.py", "-k", "test_multi_exclude",
                               "--exclude-device", "D455", "--exclude-device", "D435")
        assert_outcomes(out, passed=1)  # only D401 remains

    def test_exclude_device_space_separated(self):
        """--exclude-device 'D455 D435' (single flag, space-separated) — Jenkins TEST_EXCLUDE_DEVICES form."""
        rc, out, *_ = run_e2e("pytest-cli.py", "-k", "test_multi_exclude",
                               "--exclude-device", "D455 D435")
        assert_outcomes(out, passed=1)  # only D401 remains

    def test_device_space_separated(self):
        """--device 'D455 D435' (single flag, space-separated) must include both devices."""
        rc, out, *_ = run_e2e("pytest-cli.py", "-k", "test_multi_include",
                               "--device", "D455 D435")
        assert_outcomes(out, passed=2)

    def test_device_and_exclude_combined(self):
        """--device and --exclude-device can be combined."""
        rc, out, *_ = run_e2e("pytest-cli.py", "-k", "test_combined",
                               "--device", "D455", "--device", "D435", "--exclude-device", "D435")
        assert_outcomes(out, passed=1)  # only D455 remains

    def test_not_live(self):
        """--not-live is accepted and skips device tests."""
        rc, out, *_ = run_e2e("pytest-live.py", "--not-live")
        assert_outcomes(out, passed=1, skipped=1)

    def test_tag_filters_by_marker(self):
        """--tag <name> should run only tests with pytest.mark.<name> (alias for -m)."""
        rc, out, *_ = run_e2e("pytest-priority.py", "--tag", "priority")
        assert_outcomes(out, passed=3, deselected=1)
