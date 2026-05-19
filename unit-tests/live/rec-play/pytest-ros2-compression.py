# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# Verifies recording-with-compression for the rosbag2 (.db3) writer.
# Reads the compressed SQLite blobs via zstd + a hand-rolled CDR parser to
# provide content verification independent of librealsense's playback reader.

import logging
import sqlite3
import struct
import time

import numpy as np
import pytest
import pyrealsense2 as rs
import zstandard as zstd

log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device("D400*"),
    pytest.mark.device("D500*"),
]

W, H, BPP = 640, 480, 2
NUM_FRAMES = 5
FRAME_DATA_TOPIC = "/device_0/sensor_0/Depth_0/image/data"
LIVE_RECORD_SECONDS = 3


def _make_pixel_array(frame_number):
    """Create a deterministic pixel buffer for a given frame number."""
    rng = np.random.default_rng(seed=frame_number)
    return rng.integers(0, 256, size=W * H * BPP, dtype=np.uint8)


def _record_synthetic_bag(filename):
    """Record NUM_FRAMES depth frames with compression into *filename*."""
    depth_intrinsics = rs.intrinsics()
    depth_intrinsics.width = W
    depth_intrinsics.height = H
    depth_intrinsics.ppx = W / 2
    depth_intrinsics.ppy = H / 2
    depth_intrinsics.fx = W
    depth_intrinsics.fy = H
    depth_intrinsics.model = rs.distortion.brown_conrady
    depth_intrinsics.coeffs = [0, 0, 0, 0, 0]

    vs = rs.video_stream()
    vs.type = rs.stream.depth
    vs.index = 0
    vs.uid = 0
    vs.width = W
    vs.height = H
    vs.fps = 60
    vs.bpp = BPP
    vs.fmt = rs.format.z16
    vs.intrinsics = depth_intrinsics

    sd = rs.software_device()
    sensor = sd.add_sensor("Synthetic")
    depth_profile = sensor.add_video_stream(vs).as_video_stream_profile()

    recorder = rs.recorder(filename, sd)
    sensor.open([depth_profile])
    sensor.start(rs.syncer())

    # Pre-allocate pixel buffers (kept alive past on_video_frame) and use a
    # fresh software_video_frame per call — reuse trips heap corruption.
    arrays = [_make_pixel_array(i) for i in range(NUM_FRAMES)]
    for i, pixels in enumerate(arrays):
        frame = rs.software_video_frame()
        frame.bpp = BPP
        frame.stride = W * BPP
        frame.domain = rs.timestamp_domain.hardware_clock
        frame.profile = depth_profile
        frame.pixels = pixels
        frame.timestamp = 10000 + i * 16667  # ~60 fps spacing in µs
        frame.frame_number = i
        sensor.on_video_frame(frame)

    sensor.stop()
    sensor.close()
    recorder.pause()
    recorder = None


def _skip_cdr_string(buf, off):
    off = (off + 3) & ~3  # align to 4
    slen = struct.unpack_from("<I", buf, off)[0]
    return off + 4 + slen


def _extract_image_data_from_cdr(cdr_bytes):
    # CDR layout for sensor_msgs/msg/Image: encapsulation(4) + stamp(8) +
    # frame_id(str) + height(4) + width(4) + encoding(str) + is_bigendian(1)
    # + step(4) + data(seq<uint8>). Each str/seq is 4-byte aligned.
    off = 4 + 8
    off = _skip_cdr_string(cdr_bytes, off)
    off = (off + 3) & ~3
    off += 8
    off = _skip_cdr_string(cdr_bytes, off)
    off += 1
    off = (off + 3) & ~3
    off += 4
    off = (off + 3) & ~3
    data_len = struct.unpack_from("<I", cdr_bytes, off)[0]
    off += 4
    return cdr_bytes[off:off + data_len]


def _read_frame_blobs(filename):
    with sqlite3.connect(filename) as conn:
        rows = conn.execute(
            "SELECT m.data FROM messages m JOIN topics t ON m.topic_id = t.id "
            "WHERE t.name = ? ORDER BY m.timestamp",
            (FRAME_DATA_TOPIC,),
        ).fetchall()
    return [row[0] for row in rows]


def _deserialize_blob(blob):
    ZSTD_MAGIC = b"\x28\xb5\x2f\xfd"
    assert blob[:4] == ZSTD_MAGIC, \
        f"expected zstd magic {ZSTD_MAGIC.hex()}, got {blob[:4].hex()}"
    cdr = zstd.ZstdDecompressor().decompress(blob)
    return _extract_image_data_from_cdr(cdr)


def _playback_depth_frames(filename):
    playback = rs.context().load_device(filename)
    playback.set_real_time(False)
    sensor = playback.query_sensors()[0]

    sync = rs.syncer()
    sensor.open(sensor.get_stream_profiles())
    sensor.start(sync)

    frames = []
    success, fset = sync.try_wait_for_frames()
    while success:
        depth = fset.first_or_default(rs.stream.depth)
        if depth:
            frames.append(bytes(depth.as_video_frame().get_data()))
        success, fset = sync.try_wait_for_frames()

    sensor.stop()
    sensor.close()
    return frames


def test_compressed_frames_match_playback(tmp_path):
    bag = str(tmp_path / "recording.db3")
    _record_synthetic_bag(bag)

    blobs = _read_frame_blobs(bag)
    assert len(blobs) == NUM_FRAMES, \
        f"expected {NUM_FRAMES} frame blobs, got {len(blobs)}"
    sqlite_pixels = [_deserialize_blob(b) for b in blobs]

    playback_pixels = _playback_depth_frames(bag)
    assert len(playback_pixels) == NUM_FRAMES, \
        f"expected {NUM_FRAMES} playback frames, got {len(playback_pixels)}"

    expected = [_make_pixel_array(i).tobytes() for i in range(NUM_FRAMES)]
    for i in range(NUM_FRAMES):
        assert sqlite_pixels[i] == expected[i], \
            f"frame {i}: SQLite3-deserialized pixels differ from original"
        assert playback_pixels[i] == expected[i], \
            f"frame {i}: playback pixels differ from original"
        assert sqlite_pixels[i] == playback_pixels[i], \
            f"frame {i}: SQLite3 path and playback path disagree"
        log.info("frame %d: %d bytes match across all three sources", i, len(expected[i]))


def _record_live_bag(filename, dev):
    """Record depth from a live device for LIVE_RECORD_SECONDS with
    compression enabled."""
    depth_sensor = dev.first_depth_sensor()
    depth_profile = next(
        p for p in depth_sensor.profiles
        if p.is_default() and p.stream_type() == rs.stream.depth
    )

    frame_queue = rs.frame_queue(100)
    depth_sensor.open(depth_profile)
    depth_sensor.start(frame_queue)

    recorder = rs.recorder(filename, dev, True)  # force compression
    time.sleep(LIVE_RECORD_SECONDS)
    recorder.pause()
    recorder = None

    depth_sensor.stop()
    depth_sensor.close()


def test_live_compressed_frames_match_playback(tmp_path, test_device):
    dev, ctx = test_device
    bag = str(tmp_path / "live_recording.db3")
    _record_live_bag(bag, dev)

    blobs = _read_frame_blobs(bag)
    sqlite_pixels = [_deserialize_blob(b) for b in blobs]
    playback_pixels = _playback_depth_frames(bag)

    assert sqlite_pixels, "no frames recorded"
    assert len(sqlite_pixels) == len(playback_pixels), \
        f"frame count mismatch: {len(sqlite_pixels)} blobs vs {len(playback_pixels)} playback frames"

    for i in range(len(sqlite_pixels)):
        assert sqlite_pixels[i] == playback_pixels[i], \
            f"frame {i}: SQLite3-deserialized pixels differ from playback"
        log.info("live frame %d: %d bytes match", i, len(sqlite_pixels[i]))
