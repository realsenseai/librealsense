# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
Coverage for the CUDA zero-copy GPU frame path (RSDSO-21841 / RSDEV-12094).

Exercises the public GPU-frame API on a live frame:
  - frame.get_gpu_data_or_upload() always yields a usable device pointer on a CUDA build,
    with copied=False for a true zero-copy frame and copied=True for the upload fallback.
  - the gpu_frame extension (rs.gpu_frame(frame)) and the strict frame.get_gpu_data() through
    it resolve ONLY when the frame is GPU-resident, and are null/invalid otherwise.
  - the CPU-side frame data stays fully readable and the expected size, i.e. the mapped
    allocator behaves like the default one (allocator correctness).

Degrades gracefully (Definition of Done):
  - Non-CUDA / non-RS2_USE_CUDA_ZEROCOPY build: get_gpu_data_or_upload() returns None -> the
    test skips, so every existing (non-zero-copy) LibCI leg skips cleanly.
  - Discrete / non-integrated GPU (or a buffer that is not GPU-mapped): the path intentionally
    falls back to an upload (copied=True); the zero-copy-only assertions are gated off there,
    while the always-usable _or_upload contract and the null-safety of the strict API are
    still checked.
"""

import platform
import pytest
import numpy as np
import pyrealsense2 as rs
import logging
log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device("D400*"),
    # zero-copy needs CUDA, and only Jetson (aarch64) builds CUDA on CI. Skip at collection
    # on every other platform so we don't stream then skip, and don't need a device there.
    pytest.mark.skipif(platform.machine() != "aarch64",
                       reason="zero-copy needs CUDA; only Jetson (aarch64) builds CUDA on CI"),
]

WIDTH, HEIGHT, FPS = 640, 480, 30
WARMUP = 30  # exclude first frames (AWB / one-time CUDA init) before probing GPU data


def test_zero_copy_gpu_frame_path(test_device):
    dev, ctx = test_device
    if not hasattr(rs.frame, "get_gpu_data_or_upload") or not hasattr(rs, "gpu_frame"):
        pytest.skip("pyrealsense2 built without the GPU-frame API")

    pipe = rs.pipeline(ctx)
    cfg = rs.config()
    cfg.enable_device(dev.get_info(rs.camera_info.serial_number))
    cfg.enable_stream(rs.stream.depth, WIDTH, HEIGHT, rs.format.z16, FPS)
    pipe.start(cfg)
    try:
        # Warm up (AWB / one-time CUDA init), then grab one valid depth frame; retry on a
        # dropped/null frame and fail fast if none arrives (instead of asserting only the last).
        for _ in range(WARMUP):
            pipe.wait_for_frames()
        frame = None
        for _ in range(30):
            depth = pipe.wait_for_frames().get_depth_frame()
            if depth:
                frame = depth
                break
        assert frame, "no valid depth frame received"

        # --- allocator correctness: size + full CPU-side readability ---
        # frame::data uses frame_data_allocator, which under zero-copy is CUDA host-mapped
        # memory. It must still report the same logical size and be readable end to end on the
        # CPU (touching the last element exercises the buffer right up to its tail).
        expected_size = frame.get_stride_in_bytes() * frame.get_height()
        assert frame.get_data_size() == expected_size, \
            "get_data_size() {} != stride*height {} (allocator/size regression)".format(frame.get_data_size(), expected_size)
        data = np.asanyarray(frame.get_data())
        assert data.size > 0
        assert int(data.flat[0]) >= 0 and int(data.flat[-1]) >= 0  # read head + tail, no fault

        # --- get_gpu_data_or_upload(): always usable on a CUDA build ---
        gpu_data = frame.get_gpu_data_or_upload()
        if gpu_data is None:
            pytest.skip("no CUDA / RS2_USE_CUDA_ZEROCOPY build: GPU device pointer unavailable")
        dev_addr, copied = gpu_data
        assert isinstance(dev_addr, int) and dev_addr != 0, "expected a non-null CUDA device address"
        assert isinstance(copied, bool)

        # --- gpu_frame extension + strict get_gpu_data(): only when GPU-resident ---
        gpu_ext = rs.gpu_frame(frame)
        strict_addr = gpu_ext.get_gpu_data() if gpu_ext else None
        # The extension, the strict pointer, and the copied flag must all agree.
        assert bool(gpu_ext) == (strict_addr is not None), "gpu_frame validity must match get_gpu_data() null-ness"
        assert bool(gpu_ext) == (not copied), "gpu_frame is reported iff the frame is GPU-resident (not uploaded)"

        if not copied:
            # true zero-copy (integrated GPU, GPU-mapped frame)
            assert strict_addr == dev_addr, "zero-copy: strict get_gpu_data() must equal the _or_upload address"
            log.info("zero-copy ACTIVE: device ptr 0x%x (no host->device copy)", dev_addr)
        else:
            # CUDA build but the frame is not GPU-mapped (discrete GPU, or a non-mapped backend
            # buffer): the strict API is null and the _or_upload path uploaded a copy.
            assert strict_addr is None, "strict get_gpu_data() must be null when the frame is not GPU-resident"
            log.info("zero-copy fell back to upload (frame not GPU-resident); _or_upload address 0x%x", dev_addr)
    finally:
        pipe.stop()


def test_capture_zero_copy_covers_depth(test_device):
    """A zero-copy captured frame must stay GPU-resident regardless of which backend captured it.

    Depth is passed through verbatim, so its frame borrows the backend capture buffer; color goes
    through a format conversion and lands in an SDK-allocated pool frame. Both must resolve to a
    device pointer with copied=False.

    Color doubles as the capability probe: if color itself is uploaded then this build/platform has
    no zero-copy at all (non-RS2_USE_CUDA_ZEROCOPY build, or a discrete GPU) and there is nothing to
    assert, so the test skips. When color IS zero-copy, depth must be too - it is not if the backend
    capture buffers are not GPU-visible.
    """
    dev, ctx = test_device
    if not hasattr(rs.frame, "get_gpu_data_or_upload"):
        pytest.skip("pyrealsense2 built without the GPU-frame API")

    pipe = rs.pipeline(ctx)
    cfg = rs.config()
    cfg.enable_device(dev.get_info(rs.camera_info.serial_number))
    cfg.enable_stream(rs.stream.depth, WIDTH, HEIGHT, rs.format.z16, FPS)
    cfg.enable_stream(rs.stream.color, WIDTH, HEIGHT, rs.format.rgb8, FPS)
    try:
        pipe.start(cfg)
    except RuntimeError as e:
        pytest.skip("device cannot stream depth+color at {}x{}: {}".format(WIDTH, HEIGHT, e))
    try:
        for _ in range(WARMUP):
            pipe.wait_for_frames()

        copied_by_stream = {}
        for _ in range(30):
            frames = pipe.wait_for_frames()
            for name, frame in (("depth", frames.get_depth_frame()),
                                ("color", frames.get_color_frame())):
                if not frame or name in copied_by_stream:
                    continue
                gpu_data = frame.get_gpu_data_or_upload()
                if gpu_data is None:
                    pytest.skip("no CUDA / RS2_USE_CUDA_ZEROCOPY build: GPU device pointer unavailable")
                dev_addr, copied = gpu_data
                assert isinstance(dev_addr, int) and dev_addr != 0, \
                    "{}: expected a non-null CUDA device address".format(name)
                copied_by_stream[name] = copied
            if len(copied_by_stream) == 2:
                break
        assert len(copied_by_stream) == 2, \
            "did not receive both a depth and a color frame: got {}".format(sorted(copied_by_stream))

        if copied_by_stream["color"]:
            pytest.skip("zero-copy is not active on this build/platform (color was uploaded too)")

        assert not copied_by_stream["depth"], \
            "depth was uploaded host->device while color was zero-copy, i.e. the frame borrowed a " \
            "capture buffer the GPU cannot see. The backend capture buffers must be CUDA-visible " \
            "(frame_data_allocator on the RSUSB path, rs_v4l2_zc_register on V4L2)."
        log.info("zero-copy capture ACTIVE for both depth and color")
    finally:
        pipe.stop()
