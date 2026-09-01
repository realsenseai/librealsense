# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# Frame-number matchers group a fixed set of streams: depth with one or two IRs, and optionally
# confidence. A software device lets us drive them with known frame numbers and no hardware.

import logging
import pyrealsense2 as rs

log = logging.getLogger(__name__)

W = 640
H = 480
BPP = 2
FPS = 60
DOMAIN = rs.timestamp_domain.hardware_clock

PIXELS = bytearray(b'\x00' * (W * H * BPP))


def video_stream(uid, stream, index, fmt=rs.format.z16):
    s = rs.video_stream()
    s.type = stream
    s.index = index
    s.uid = uid
    s.width = W
    s.height = H
    s.fps = FPS
    s.bpp = BPP
    s.fmt = fmt
    return s


class sw_device:
    """A software device whose streams are driven frame by frame through a syncer."""

    def __init__(self, streams, matcher):
        self.device = rs.software_device()
        self.device.create_matcher(matcher)
        self.sensor = self.device.add_sensor("sensor")
        for uid, (stream, index, fmt) in enumerate(streams):
            self.sensor.add_video_stream(video_stream(uid, stream, index, fmt))
        self.profiles = self.sensor.get_stream_profiles()
        self.syncer = rs.syncer(100)
        self.sensor.open(self.profiles)
        self.sensor.start(self.syncer)

    def close(self):
        self.sensor.stop()
        self.sensor.close()

    def push(self, frame_number, timestamp):
        """One frame on every stream, all sharing a frame number, as a device produces them."""
        for profile in self.profiles:
            frame = rs.software_video_frame()
            frame.pixels = PIXELS
            frame.stride = W * BPP
            frame.bpp = BPP
            frame.timestamp = timestamp
            frame.domain = DOMAIN
            frame.frame_number = frame_number
            frame.profile = profile.as_video_stream_profile()
            self.sensor.on_video_frame(frame)

    def collect(self, pushes=6):
        """Push a few sets of frames and return the largest frameset the syncer produced."""
        best = []
        for n in range(pushes):
            self.push(frame_number=n, timestamp=n * (1000 / FPS))
            while True:
                frames = self.syncer.poll_for_frames()
                if not frames:
                    break
                got = sorted((str(f.profile.stream_type()), f.profile.stream_index()) for f in frames)
                log.debug("frameset: %s", got)
                if len(got) > len(best):
                    best = got
        return best


DEPTH0 = (rs.stream.depth, 0, rs.format.z16)
DEPTH1 = (rs.stream.depth, 1, rs.format.z16)
IR1 = (rs.stream.infrared, 1, rs.format.y8)
IR2 = (rs.stream.infrared, 2, rs.format.y8)
COLOR = (rs.stream.color, 0, rs.format.rgb8)
CONFIDENCE = (rs.stream.confidence, 0, rs.format.raw8)
DETECTION = (rs.stream.object_detection, 0, rs.format.y8)


def expected(*streams):
    return sorted((str(s[0]), s[1]) for s in streams)


#############################################################################################
def test_dlr_matches_depth_and_both_irs():
    """The established DLR grouping still matches all three streams together."""
    dev = sw_device([DEPTH0, IR1, IR2], rs.matchers.dlr)
    try:
        assert dev.collect() == expected(DEPTH0, IR1, IR2)
    finally:
        dev.close()


#############################################################################################
def test_dlr_matches_two_depth_streams():
    """A device may expose raw and device-aligned depth; both share the source frame number and so
    belong in the same group. Selecting only depth index 0 would leave the second one unmatched."""
    dev = sw_device([DEPTH0, DEPTH1, IR1, IR2], rs.matchers.dlr)
    try:
        assert dev.collect() == expected(DEPTH0, DEPTH1, IR1, IR2)
    finally:
        dev.close()


#############################################################################################
def test_dlr_c_matches_two_depth_streams_with_color():
    """With color present the group is frame-number matched and then timestamp matched to color -
    the path a DDS device takes."""
    dev = sw_device([DEPTH0, DEPTH1, IR1, IR2, COLOR], rs.matchers.dlr_c)
    try:
        assert dev.collect() == expected(DEPTH0, DEPTH1, IR1, IR2, COLOR)
    finally:
        dev.close()


#############################################################################################
def test_di_matches_two_depth_streams():
    """DI groups depth with a single IR, and must not drop a second depth stream either."""
    dev = sw_device([DEPTH0, DEPTH1, IR1], rs.matchers.di)
    try:
        assert dev.collect() == expected(DEPTH0, DEPTH1, IR1)
    finally:
        dev.close()


#############################################################################################
def test_incomplete_group_falls_back_to_timestamp():
    """DLR needs both IRs. Without IR2 the group cannot be frame-number matched, so every stream is
    matched by timestamp instead - which still yields a full frameset."""
    dev = sw_device([DEPTH0, IR1], rs.matchers.dlr)
    try:
        assert dev.collect() == expected(DEPTH0, IR1)
    finally:
        dev.close()


#############################################################################################
def test_timestamp_matcher_takes_every_stream():
    """The default matcher groups nothing - it matches whatever is streaming by timestamp, so extra
    depth streams need no special handling there."""
    dev = sw_device([DEPTH0, DEPTH1, IR1, IR2, COLOR], rs.matchers.default)
    try:
        assert dev.collect() == expected(DEPTH0, DEPTH1, IR1, IR2, COLOR)
    finally:
        dev.close()


#############################################################################################
def test_dic_matches_depth_ir_and_confidence():
    """DIC is timestamp matched, not frame-number matched, whatever rs2_matchers says."""
    dev = sw_device([DEPTH0, IR1, CONFIDENCE], rs.matchers.dic)
    try:
        assert dev.collect() == expected(DEPTH0, IR1, CONFIDENCE)
    finally:
        dev.close()


#############################################################################################
def test_di_c_matches_group_against_color():
    """A grouped matcher and color, composed by timestamp."""
    dev = sw_device([DEPTH0, DEPTH1, IR1, COLOR], rs.matchers.di_c)
    try:
        assert dev.collect() == expected(DEPTH0, DEPTH1, IR1, COLOR)
    finally:
        dev.close()


#############################################################################################
def test_group_matcher_carries_streams_outside_its_group():
    """Streams the group does not name - here motion-less extras like a second color - must still
    reach the caller rather than being dropped for having no matcher."""
    color1 = (rs.stream.color, 1, rs.format.rgb8)
    dev = sw_device([DEPTH0, IR1, IR2, COLOR, color1], rs.matchers.dlr_c)
    try:
        assert dev.collect() == expected(DEPTH0, IR1, IR2, COLOR, color1)
    finally:
        dev.close()


#############################################################################################
def test_incomplete_group_with_color_does_not_double_claim_color():
    """When the group cannot be matched it falls back to relating everything it was given. Color is
    matched separately, so it must not be handed to the group as well or it lands in two matchers."""
    dev = sw_device([DEPTH0, IR1, COLOR], rs.matchers.dlr_c)   # dlr needs IR2, so the group is incomplete
    try:
        assert dev.collect() == expected(DEPTH0, IR1, COLOR)
    finally:
        dev.close()


def all_streams_seen(dev, pushes=6):
    """Every (stream, index) the syncer delivered, and the largest frameset it grouped."""
    seen, best = set(), []
    for n in range(pushes):
        dev.push(frame_number=n, timestamp=n * (1000 / FPS))
        while True:
            frames = dev.syncer.poll_for_frames()
            if not frames:
                break
            got = sorted((str(f.profile.stream_type()), f.profile.stream_index()) for f in frames)
            seen.update(got)
            if len(got) > len(best):
                best = got
    return seen, best


#############################################################################################
def test_perception_is_carried_by_the_default_matcher():
    """Perception frames are not produced per depth frame, so the default matcher must carry them
    alongside rather than timestamp match them into the frameset."""
    dev = sw_device([DEPTH0, COLOR, DETECTION], rs.matchers.default)
    try:
        seen, best = all_streams_seen(dev)
        assert seen == set(expected(DEPTH0, COLOR, DETECTION)), "a stream was dropped"
        assert best == expected(DEPTH0, COLOR), "perception was matched into the frameset"
    finally:
        dev.close()


#############################################################################################
def test_perception_is_carried_by_dlr_c():
    """Same for DLR_C, which is where perception streams reach the syncer over DDS."""
    dev = sw_device([DEPTH0, IR1, IR2, COLOR, DETECTION], rs.matchers.dlr_c)
    try:
        seen, best = all_streams_seen(dev)
        assert seen == set(expected(DEPTH0, IR1, IR2, COLOR, DETECTION)), "a stream was dropped"
        assert best == expected(DEPTH0, IR1, IR2, COLOR), "perception was matched into the frameset"
    finally:
        dev.close()


#############################################################################################
def test_dic_matches_every_infrared_stream():
    """DIC names infrared without an index, so both IRs join the group. The old code took only the
    first, leaving the second with no matcher at all."""
    dev = sw_device([DEPTH0, IR1, IR2, CONFIDENCE], rs.matchers.dic)
    try:
        assert dev.collect() == expected(DEPTH0, IR1, IR2, CONFIDENCE)
    finally:
        dev.close()


#############################################################################################
def test_two_color_streams_are_matched_together():
    """Color streams are told apart by index, so both reach the color matcher and neither leaks into
    the group - the case the de-duplicating map used to guard."""
    color1 = (rs.stream.color, 1, rs.format.rgb8)
    dev = sw_device([DEPTH0, IR1, IR2, COLOR, color1], rs.matchers.dlr_c)
    try:
        assert dev.collect() == expected(DEPTH0, IR1, IR2, COLOR, color1)
    finally:
        dev.close()
