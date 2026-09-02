# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# Verifies recording-with-compression for the rosbag2 (.db3) writer.
# Compressed depth frames are stored as sensor_msgs/CompressedImage carrying a
# compressedDepth-convention PNG, so they are decoded here with the same call
# ROS2's compressed_depth_image_transport uses (cv2.imdecode) — content
# verification independent of librealsense's playback reader.

import logging
import sqlite3
import struct
import time

import cv2
import numpy as np
import pytest
import pyrealsense2 as rs

log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device("D400*"),
    pytest.mark.device_each("D500*"),
]

W, H = 640, 480
NUM_FRAMES = 5
# Depth is not always sensor_0: on DDS devices (e.g. D555) it is a later index.
DEPTH_DATA_TOPIC_SUFFIX = "/Depth_0/image/data/compressedDepth"
LIVE_RECORD_SECONDS = 3
PNG_MAGIC = b"\x89PNG\r\n\x1a\n"
ZSTD_MAGIC = b"\x28\xb5\x2f\xfd"
CONFIG_HEADER_SIZE = 12  # compressed_depth_image_transport ConfigHeader prefix


def _make_pixel_array(frame_number, bpp):
    """Create a deterministic pixel buffer for a given frame number."""
    rng = np.random.default_rng(seed=frame_number)
    return rng.integers(0, 256, size=W * H * bpp, dtype=np.uint8)


def _record_synthetic_bag(filename, stream_type, fmt, bpp):
    """Record NUM_FRAMES frames of the given stream/format with compression into *filename*."""
    intrinsics = rs.intrinsics()
    intrinsics.width = W
    intrinsics.height = H
    intrinsics.ppx = W / 2
    intrinsics.ppy = H / 2
    intrinsics.fx = W
    intrinsics.fy = H
    intrinsics.model = rs.distortion.brown_conrady
    intrinsics.coeffs = [0, 0, 0, 0, 0]

    vs = rs.video_stream()
    vs.type = stream_type
    vs.index = 0
    vs.uid = 0
    vs.width = W
    vs.height = H
    vs.fps = 60
    vs.bpp = bpp
    vs.fmt = fmt
    vs.intrinsics = intrinsics

    sd = rs.software_device()
    sensor = sd.add_sensor("Synthetic")
    profile = sensor.add_video_stream(vs).as_video_stream_profile()

    recorder = rs.recorder(filename, sd)
    sensor.open([profile])
    sensor.start(rs.syncer())

    # Pre-allocate pixel buffers (kept alive past on_video_frame) and use a
    # fresh software_video_frame per call — reuse trips heap corruption.
    arrays = [_make_pixel_array(i, bpp) for i in range(NUM_FRAMES)]
    for i, pixels in enumerate(arrays):
        frame = rs.software_video_frame()
        frame.bpp = bpp
        frame.stride = W * bpp
        frame.domain = rs.timestamp_domain.hardware_clock
        frame.profile = profile
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


def _extract_compressed_image_from_cdr(cdr_bytes):
    # CDR layout for sensor_msgs/msg/CompressedImage: encapsulation(4) + stamp(8)
    # + frame_id(str) + format(str) + data(seq<uint8>). Each str/seq is 4-byte aligned.
    off = 4 + 8
    off = _skip_cdr_string(cdr_bytes, off)
    fmt_off = (off + 3) & ~3
    fmt_len = struct.unpack_from("<I", cdr_bytes, fmt_off)[0]
    fmt = cdr_bytes[fmt_off + 4:fmt_off + 4 + fmt_len - 1].decode()  # strip NUL
    off = fmt_off + 4 + fmt_len
    off = (off + 3) & ~3
    data_len = struct.unpack_from("<I", cdr_bytes, off)[0]
    off += 4
    return fmt, cdr_bytes[off:off + data_len]


def _read_frame_blobs(filename, topic_suffix=DEPTH_DATA_TOPIC_SUFFIX):
    with sqlite3.connect(filename) as conn:
        rows = conn.execute(
            "SELECT m.data FROM messages m JOIN topics t ON m.topic_id = t.id "
            "WHERE t.name LIKE ? ORDER BY m.timestamp",
            ("%" + topic_suffix,),
        ).fetchall()
    return [row[0] for row in rows]


def _topic_info(filename, name_pattern):
    """Return (type, offered_qos_profiles, message_count) for a topic, or None."""
    with sqlite3.connect(filename) as conn:
        row = conn.execute(
            "SELECT t.type, t.offered_qos_profiles, COUNT(m.id) FROM topics t "
            "LEFT JOIN messages m ON m.topic_id = t.id WHERE t.name LIKE ? GROUP BY t.id",
            (name_pattern,),
        ).fetchone()
    return row


def _check_native_ros2_topics(bag, expected_frames, stream_name="Depth_0"):
    """New recordings must carry a standard CameraInfo message per frame."""
    ci = _topic_info(bag, f"%/{stream_name}/image/camera_info")
    assert ci is not None, "no CameraInfo topic recorded next to the depth image topic"
    assert ci[0] == "sensor_msgs/msg/CameraInfo", f"wrong camera_info type: {ci[0]}"
    assert ci[2] == expected_frames, f"expected {expected_frames} CameraInfo messages, got {ci[2]}"


def _deserialize_blob(blob, lane="depth_png"):
    fmt, payload = _extract_compressed_image_from_cdr(blob)
    if lane == "depth_png":
        assert "compressedDepth png" in fmt, f"unexpected format string: '{fmt}'"
        png = payload[CONFIG_HEADER_SIZE:]
        assert png[:8] == PNG_MAGIC, \
            f"expected PNG magic after ConfigHeader, got {png[:8].hex()}"
        img = cv2.imdecode(np.frombuffer(png, np.uint8), cv2.IMREAD_UNCHANGED)
        assert img is not None, "cv2.imdecode failed on recorded PNG"
        assert img.dtype == np.uint16, f"expected 16-bit depth PNG, got {img.dtype}"
        return img.tobytes()
    if lane in ("color_png_bgr", "color_png_rgb"):
        assert "png compressed bgr8" in fmt, f"unexpected format string: '{fmt}'"
        assert payload[:8] == PNG_MAGIC, f"expected PNG magic, got {payload[:8].hex()}"
        img = cv2.imdecode(np.frombuffer(payload, np.uint8), cv2.IMREAD_UNCHANGED)
        assert img is not None, "cv2.imdecode failed on recorded PNG"
        # cv2.imdecode returns BGR-ordered channels — byte-identical to BGR8 input,
        # channel-swapped relative to RGB8 input
        if lane == "color_png_rgb":
            img = img[:, :, ::-1]
        return img.tobytes()
    # zstd fallback lane: the "; zstd; " marker couples writer and reader by exact
    # string; pixel content is verified through SDK playback (which decompresses)
    assert "; zstd; " in fmt and f"{W}x{H} step=" in fmt, f"unexpected format string: '{fmt}'"
    assert payload[:4] == ZSTD_MAGIC, f"expected zstd magic, got {payload[:4].hex()}"
    return None


def _playback_frames(filename, stream_type):
    playback = rs.context().load_device(filename)
    playback.set_real_time(False)
    sensor = next((s for s in playback.query_sensors()
                   if any(p.stream_type() == stream_type for p in s.get_stream_profiles())), None)
    if sensor is None:
        pytest.fail(f"no {stream_type} sensor found in playback of '{filename}'")

    sync = rs.syncer()
    sensor.open(sensor.get_stream_profiles())
    sensor.start(sync)

    frames = []
    success, fset = sync.try_wait_for_frames()
    while success:
        frame = fset.first_or_default(stream_type)
        if frame:
            frames.append(bytes(frame.as_video_frame().get_data()))
        success, fset = sync.try_wait_for_frames()

    sensor.stop()
    sensor.close()
    return frames


@pytest.mark.parametrize("stream_type,fmt,bpp,lane,stream_name,topic_suffix", [
    (rs.stream.depth, rs.format.z16, 2, "depth_png", "Depth_0", DEPTH_DATA_TOPIC_SUFFIX),
    (rs.stream.color, rs.format.bgr8, 3, "color_png_bgr", "Color_0", "/Color_0/image/data/compressed"),
    (rs.stream.color, rs.format.rgb8, 3, "color_png_rgb", "Color_0", "/Color_0/image/data/compressed"),
    (rs.stream.color, rs.format.yuyv, 2, "zstd", "Color_0", "/Color_0/image/data/compressed"),
])
def test_compressed_frames_match_playback(tmp_path, stream_type, fmt, bpp, lane, stream_name, topic_suffix):
    bag = str(tmp_path / "recording.db3")
    _record_synthetic_bag(bag, stream_type, fmt, bpp)

    blobs = _read_frame_blobs(bag, topic_suffix)
    assert len(blobs) == NUM_FRAMES, \
        f"expected {NUM_FRAMES} frame blobs, got {len(blobs)}"
    sqlite_pixels = [_deserialize_blob(b, lane) for b in blobs]

    playback_pixels = _playback_frames(bag, stream_type)
    assert len(playback_pixels) == NUM_FRAMES, \
        f"expected {NUM_FRAMES} playback frames, got {len(playback_pixels)}"

    _check_native_ros2_topics(bag, NUM_FRAMES, stream_name)

    expected = [_make_pixel_array(i, bpp).tobytes() for i in range(NUM_FRAMES)]
    for i in range(NUM_FRAMES):
        if sqlite_pixels[i] is not None:
            assert sqlite_pixels[i] == expected[i], \
                f"frame {i}: SQLite3-deserialized pixels differ from original"
        assert playback_pixels[i] == expected[i], \
            f"frame {i}: playback pixels differ from original"
        log.info("frame %d: %d bytes match", i, len(expected[i]))


def _record_live_bag(filename, dev):
    """Record depth from a live device for LIVE_RECORD_SECONDS with
    compression enabled."""
    depth_sensor = dev.first_depth_sensor()
    depth_profile = next(
        p for p in depth_sensor.profiles
        if p.is_default() and p.stream_type() == rs.stream.depth
    )

    recorder = rs.recorder(filename, dev, True)  # force compression

    frame_queue = rs.frame_queue(100)
    depth_sensor.open(depth_profile)
    depth_sensor.start(frame_queue)

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
    playback_pixels = _playback_frames(bag, rs.stream.depth)

    assert sqlite_pixels, "no frames recorded"
    assert len(sqlite_pixels) == len(playback_pixels), \
        f"frame count mismatch: {len(sqlite_pixels)} blobs vs {len(playback_pixels)} playback frames"

    _check_native_ros2_topics(bag, len(sqlite_pixels))
    # Live devices have real extrinsics — they must appear as a standard latched /tf_static topic
    tf = _topic_info(bag, "/tf_static")
    assert tf is not None, "no /tf_static topic recorded"
    assert tf[0] == "tf2_msgs/msg/TFMessage", f"wrong /tf_static type: {tf[0]}"
    assert tf[2] >= 1, "expected at least one /tf_static transform"
    assert "durability: 1" in (tf[1] or ""), \
        f"/tf_static must offer transient_local durability, got QoS: '{tf[1]}'"

    for i in range(len(sqlite_pixels)):
        assert sqlite_pixels[i] == playback_pixels[i], \
            f"frame {i}: SQLite3-deserialized pixels differ from playback"
        log.info("live frame %d: %d bytes match", i, len(sqlite_pixels[i]))
