# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# Validates the DDS-related command-line flags exposed by rs2::cli (common/cli.h), using
# rs-enumerate-devices as the host tool. On every build the flags are registered and a single
# valid use of each is accepted; only the *validation* (range / mutual-exclusivity) and the
# flags' effect are gated on BUILD_WITH_DDS.

import subprocess
import logging
import pytest
from rspy import repo

log = logging.getLogger(__name__)

pytestmark = [pytest.mark.dds]


# Output markers (see common/cli.h and the tool's TCLAP parser):
#   - TCLAP prints this only when an argument is *not registered* with the parser.
UNREGISTERED_MARKER = "Couldn't find match for argument"
#   - cli.h validation messages, active only on a BUILD_WITH_DDS build.
RANGE_MARKER = "expecting [0, 232]"
EXCLUSIVE_MARKER = "mutually exclusive"

# Every DDS-related flag exposed by rs2::cli. Each must be accepted (and registered) on any
# build -- add() used to live under #ifdef BUILD_WITH_DDS, so passing one on a non-DDS build was
# an "unrecognized argument" parse error. Their effect/validation is what is gated on DDS.
ALL_DDS_FLAGS = [ ["--eth"], ["--eth-only"], ["--no-eth"], ["--domain-id", "2"] ]


_TOOL = repo.find_built_exe( 'tools/enumerate-devices', 'rs-enumerate-devices' )


def _run( *args, timeout=15 ):
    """Run rs-enumerate-devices in short view with args; stdout+stderr merged. Skips if the tool
    wasn't built (BUILD_TOOLS=OFF)."""
    if not _TOOL:
        pytest.skip( "rs-enumerate-devices not found (built without tools)" )
    p = subprocess.run(
        [_TOOL, "-s", *args],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        universal_newlines=True,
        timeout=timeout,
        check=False,
    )
    log.debug( "rs-enumerate-devices -s %s -> rc=%s\n%s", " ".join( args ), p.returncode, p.stdout )
    return p


def _tool_built_with_dds():
    """Detect the tool's BUILD_WITH_DDS from its own behavior (authoritative, and immune to a
    stale pyrealdds.pyd left over from a previous DDS build): --domain-id is range-validated only
    when built with DDS; otherwise the flag is accepted and ignored. Returns True/False, or None
    when the tool isn't available so the build-specific tests simply skip."""
    if not _TOOL:
        return None
    p = subprocess.run(
        [_TOOL, "-s", "--domain-id", "999"],
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        universal_newlines=True, timeout=15, check=False )
    return RANGE_MARKER in p.stdout


TOOL_HAS_DDS = _tool_built_with_dds()
requires_dds = pytest.mark.skipif( TOOL_HAS_DDS is not True, reason="requires a tool built WITH DDS" )
requires_no_dds = pytest.mark.skipif( TOOL_HAS_DDS is not False, reason="requires a tool built WITHOUT DDS" )


# --- Accepted on every build --------------------------------------------------------------
@pytest.mark.parametrize( "flag", ALL_DDS_FLAGS, ids=lambda f: " ".join( f ) )
def test_dds_flag_accepted( flag ):
    """Each DDS flag, used once, is a recognized and accepted argument regardless of
    BUILD_WITH_DDS -- no unknown-argument, range, or mutual-exclusivity error."""
    out = _run( *flag ).stdout
    assert UNREGISTERED_MARKER not in out, f"{' '.join( flag )} not recognized:\n{out}"
    assert RANGE_MARKER not in out, out
    assert EXCLUSIVE_MARKER not in out, out


# --- Validation: BUILD_WITH_DDS = ON only -------------------------------------------------
@requires_dds
@pytest.mark.parametrize( "domain", ["-1", "233", "999"] )
def test_domain_id_out_of_range_rejected( domain ):
    """--domain-id is validated to [0, 232] when DDS is enabled."""
    p = _run( "--domain-id", domain )
    assert p.returncode != 0
    assert RANGE_MARKER in p.stdout, p.stdout


@requires_dds
@pytest.mark.parametrize( "args", [ ["--eth", "--no-eth"],
                                    ["--eth", "--eth-only"],
                                    ["--eth-only", "--no-eth"],
                                    ["--domain-id", "2", "--no-eth"] ],
                          ids=lambda a: " ".join( a ) )
def test_mutually_exclusive_flags_rejected( args ):
    """--eth/--eth-only/--no-eth are mutually exclusive, and --domain-id conflicts with --no-eth."""
    p = _run( *args )
    assert p.returncode != 0
    assert EXCLUSIVE_MARKER in p.stdout, p.stdout


# --- No validation: BUILD_WITH_DDS = OFF only ---------------------------------------------
# The validation above is compiled out without DDS, so the very same otherwise-invalid usage is
# accepted as a no-op rather than rejected.
@requires_no_dds
@pytest.mark.parametrize( "args", [ ["--domain-id", "999"],   # range check compiled out
                                    ["--eth", "--no-eth"] ],  # exclusivity check compiled out
                          ids=lambda a: " ".join( a ) )
def test_invalid_flag_usage_ignored_without_dds( args ):
    out = _run( *args ).stdout
    assert UNREGISTERED_MARKER not in out, out
    assert RANGE_MARKER not in out, out
    assert EXCLUSIVE_MARKER not in out, out
