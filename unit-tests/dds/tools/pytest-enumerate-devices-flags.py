# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import subprocess
import logging
import sys
import pytest
from rspy import repo

log = logging.getLogger(__name__)

pytestmark = [pytest.mark.dds]


# An unused DDS domain so the dummy device is the only thing the tool can see. Finding anything
# proves --domain-id switched the context's domain.
DUMMY_DOMAIN = 2
DUMMY_SERIAL = "1020304050"
DUMMY_NAME = "Dummy DDS Device"


def _build_dummy_server(dds, participant):
    """Build a minimal single-stream DDS device_server for broadcast on the dummy domain."""
    profile = dds.video_stream_profile( 30, dds.video_encoding.z16, 640, 480 )
    stream = dds.depth_stream_server( "Depth", "Stereo Module" )
    stream.init_profiles( [profile], 0 )
    server = dds.device_server( participant, f"realdds/dummy/{DUMMY_SERIAL}" )
    server.init( [stream], [], {} )
    return server


def test_domain_id_flag():
    """Publish a dummy DDS device on an unused domain and verify --domain-id finds it there."""
    # pyrealdds lives next to pyrealsense2 in the build dir, but the infra removes that
    # dir from sys.path once pyrealsense2 is imported (devices.init_hub). Re-add it so we
    # can import pyrealdds, which isn't pre-imported like pyrealsense2.
    pyrs_dir = repo.find_pyrs_dir()
    if pyrs_dir and pyrs_dir not in sys.path:
        sys.path.insert( 1, pyrs_dir )
    try:
        import pyrealdds as dds
    except ImportError:
        pytest.skip("pyrealdds not available (built without DDS)")

    rs_enumerate_devices = repo.find_built_exe('tools/enumerate-devices', 'rs-enumerate-devices')
    if not rs_enumerate_devices:
        pytest.skip("rs-enumerate-devices not found (built without tools)")

    device_info = dds.message.device_info.from_json( {
        "name": DUMMY_NAME,
        "serial": DUMMY_SERIAL,
        "product-line": "D400",
        "topic-root": f"realdds/dummy/{DUMMY_SERIAL}"
    } )

    participant = dds.participant()
    participant.init( DUMMY_DOMAIN, "enumerate-devices-test-server" )
    server = _build_dummy_server( dds, participant )
    server.broadcast( device_info )
    try:
        # --domain-id moves the context to our dummy domain. -s prints a one-line-per-device table including the serial.
        p = subprocess.run(
            [rs_enumerate_devices, "-s", "--domain-id", str( DUMMY_DOMAIN )],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            universal_newlines=True,
            timeout=10,
            check=False,
        )
        log.debug("rs-enumerate-devices --domain-id %s output:\n%s", DUMMY_DOMAIN, p.stdout)
        assert p.returncode == 0, f"rs-enumerate-devices failed (rc={p.returncode})"
        assert DUMMY_SERIAL in p.stdout, \
            f"dummy device {DUMMY_SERIAL} not found on domain {DUMMY_DOMAIN}"
    finally:
        server.broadcast_disconnect()
        del server
        del participant
