# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2025 RealSense, Inc. All Rights Reserved.

from rspy import log, test
import os
import numpy as np
import cv2
import time
import pyrealsense2 as rs

# targets are available on the Wiki page: https://rsconf.realsenseai.com/display/RealSense/Image+Quality+Tests
# standard size to display / process the target
WIDTH = 1280
HEIGHT = 720

# transformation matrix from frame to aligned region of interest
M = None

def compute_homography(pts):
    """
    Given 4 points (the detected ArUco marker centers), find the 3×3 matrix that stretches/rotates
    the four ArUco points so they become the corners of an A4 page (used to "flatten" the page in an image)
    """
    pts_sorted = sorted(pts, key=lambda p: (p[1], p[0]))
    top_left, top_right = sorted(pts_sorted[:2], key=lambda p: p[0])
    bottom_left, bottom_right = sorted(pts_sorted[2:], key=lambda p: p[0])

    src = np.array([top_left, top_right, bottom_right, bottom_left], dtype=np.float32)
    dst = np.array([[0,0],[WIDTH-1,0],[WIDTH-1,HEIGHT-1],[0,HEIGHT-1]], dtype=np.float32)
    M = cv2.getPerspectiveTransform(src, dst)
    return M  # we later use M to get our roi


def detect_a4_page(img, required_ids):
    """
    Detect ArUco markers and return centers. Returns 4 points if all are found,
    3 points if exactly one is missing (caller uses affine), or None otherwise.
    """
    # init aruco detector
    aruco = cv2.aruco
    dict_type = cv2.aruco.DICT_4X4_1000
    dictionary = aruco.getPredefinedDictionary(dict_type)
    try:
        # new API (OpenCV >= 4.7)
        parameters = aruco.DetectorParameters()
        detector = aruco.ArucoDetector(dictionary, parameters)
        corners, ids, _ = detector.detectMarkers(img)
    except AttributeError:
        # legacy API (OpenCV <= 4.6) - used on some of our machines
        parameters = aruco.DetectorParameters_create()
        corners, ids, _ = aruco.detectMarkers(img, dictionary, parameters=parameters)

    if ids is None or len([rid for rid in required_ids if rid in ids]) <= 2:
        return None

    id_to_corner = dict(zip(ids.flatten(), corners))  # map id to corners
    values = [id_to_corner[rid][0].mean(axis=0) for rid in required_ids if rid in id_to_corner] # for each required id, get center of marker coords

    if len(values) == 3:
        # Reconstruct missing 4th corner. The page is rectangular, so opposite
        # corners share a midpoint; the diagonal is the longest pairwise distance.
        # Missing 4th = (diag1 + diag2) - third.
        a, b, c = values
        d_ab = np.linalg.norm(a - b)
        d_bc = np.linalg.norm(b - c)
        d_ac = np.linalg.norm(a - c)
        if d_ab >= d_bc and d_ab >= d_ac:
            diag1, diag2, third = a, b, c
        elif d_bc >= d_ac:
            diag1, diag2, third = b, c, a
        else:
            diag1, diag2, third = a, c, b
        values.append(diag1 + diag2 - third)
        missing = next(rid for rid in required_ids if rid not in id_to_corner)
        log.i(f"detect_a4_page: 3/4 markers found, reconstructed id {missing}")

    return np.array(values, dtype=np.float32)


def find_roi_location(pipeline, required_ids, DEBUG_MODE=False, timeout=5):
    """
    Returns a matrix that transforms from frame to region of interest
    This matrix will later be used with cv2.warpPerspective()
    """
    global M
    # stream until page found
    page_pts = None
    start_time = time.time()
    while page_pts is None and time.time() - start_time < timeout:
        frames = pipeline.wait_for_frames()
        aruco_detectable_streams = (rs.stream.color, rs.stream.infrared) # we need one of those streams to detect ArUco markers
        frame = next(f for f in frames if f.get_profile().stream_type() in aruco_detectable_streams)
        img_bgr = np.asanyarray(frame.get_data())

        if DEBUG_MODE:
            cv2.imshow("PageDetect - waiting for page", img_bgr)
            cv2.waitKey(1)

        page_pts = detect_a4_page(img_bgr, required_ids)

    if page_pts is None:
        log.e("Failed to detect page within timeout")
        raise Exception("Page not found")

    # page found - use it to calculate transformation matrix from frame to region of interest
    M = compute_homography(page_pts)
    cv2.destroyAllWindows()
    return M, page_pts

def get_roi_from_frame(frame, interpolation=cv2.INTER_LINEAR):
    """
    Apply the previously computed transformation matrix to the given frame
    to get the region of interest (A4 page).
    Pass interpolation=cv2.INTER_NEAREST when warping depth data — linear
    interpolation blends values across depth discontinuities.
    """
    global M
    if M is None:
        raise Exception("Transformation matrix not computed yet")

    np_frame = np.asanyarray(frame.get_data())
    warped = cv2.warpPerspective(np_frame, M, (WIDTH, HEIGHT), flags=interpolation)
    return warped


SAMPLE_REGION_SIZE = 60  # Default size of the square region for depth sampling


def get_median_depth_from_region(image, x, y, size=SAMPLE_REGION_SIZE, min_value=600):
    """Sample a square region of given size around (x, y) and return the median depth value, filtering out values below min_value."""
    half = size // 2
    h, w = image.shape
    x_min = max(x - half, 0)
    x_max = min(x + half + 1, w)
    y_min = max(y - half, 0)
    y_max = min(y + half + 1, h)
    region = image[y_min:y_max, x_min:x_max]
    filtered = region[region > min_value]
    if filtered.size == 0:
        log.w(f"No valid depth samples in region at ({x},{y})")
        return 0.0
    return float(np.median(filtered))


# Center of the cube in the warped ROI — the cube is placed at the center
# of the A3 target so the warped image center always lands on it.
CUBE_CENTER = (WIDTH // 2, HEIGHT // 2)

# Standard bg sample positions for an A3 target with a centered cube.
# Two points on the left and right paper strips at the cube's vertical
# midline — symmetric, horizontally off a centered cube, and vertically
# far from the corner ArUco markers. Shared by depth and color checks.
BG_SAMPLE_POINTS = (
    (int(WIDTH * 0.10), HEIGHT // 2),
    (int(WIDTH * 0.90), HEIGHT // 2),
)


def sample_bg_depth(depth_image, points=BG_SAMPLE_POINTS):
    """
    Sample median depth at each bg point and return (median_of_medians, per_region_readings).
    Empty regions are dropped. Returns (0.0, []) if every region is empty.
    """
    readings = [get_median_depth_from_region(depth_image, x, y) for x, y in points]
    readings = [v for v in readings if v]
    if not readings:
        return 0.0, []
    return float(np.median(readings)), readings


def get_median_color_from_region(image, x, y, size=SAMPLE_REGION_SIZE):
    """Sample a square region around (x, y) and return per-channel median color as (R, G, B).
    Input image is assumed to be BGR (as produced by RealSense bgr8 frames)."""
    half = size // 2
    h, w = image.shape[:2]
    x_min = max(x - half, 0)
    x_max = min(x + half + 1, w)
    y_min = max(y - half, 0)
    y_max = min(y + half + 1, h)
    region = image[y_min:y_max, x_min:x_max]
    b = int(np.median(region[:, :, 0]))
    g = int(np.median(region[:, :, 1]))
    r = int(np.median(region[:, :, 2]))
    return (r, g, b)


def sample_bg_color(color_image, points=BG_SAMPLE_POINTS):
    """
    Sample median color at each bg point and return (median-of-medians (R,G,B), per-region readings).
    """
    readings = [get_median_color_from_region(color_image, x, y) for x, y in points]
    r = int(np.median([c[0] for c in readings]))
    g = int(np.median([c[1] for c in readings]))
    b = int(np.median([c[2] for c in readings]))
    return (r, g, b), readings


def make_depth_filter_chain():
    """
    Build the spatial + temporal filter chain mirroring realsense-viewer
    defaults. Returns a callable that applies the filters to a depth frame.
    Hole-filling is deliberately omitted because it can fill cube-face holes
    with surrounding paper depth and bias the reading.
    """
    spatial = rs.spatial_filter()
    temporal = rs.temporal_filter()

    def apply(depth_frame):
        return temporal.process(spatial.process(depth_frame))

    return apply


# HSV tolerances for is_color_close. Per-channel RGB tolerance is sensitive to
# overall scene brightness — when lab lighting changes, every bright color shifts
# together and trips the per-channel check. HSV separates the concerns: hue catches
# real color errors, value absorbs illumination shifts.
#
# Each axis means a different thing, so each gets a different slack:
#   hue: position on the color wheel — the "color name" (yellow vs orange vs red).
#        H wraps at 180 in OpenCV. Tight (~5% of the wheel) because hue *is* the
#        identity of a chromatic color; loose tolerance here would let yellow
#        pass for orange.
#   sat: how vivid vs how grayish the color is. Cameras desaturate dim scenes,
#        so loose — but not so loose that a pure color passes for a muted one.
#   val: brightness. Loose because brightness is what shifts most across rigs and
#        times of day; this is the axis that absorbs illumination changes.
#   rgb: per-channel fallback for achromatic samples only (gray/black/white) where
#        hue is undefined and HSV S is dominated by sensor noise.
TOLERANCE = {'hue': 10, 'sat': 70, 'val': 70, 'rgb': 70}
ACHROMATIC_S = 40       # expected S below this → treat as gray/white/black


def is_color_close(actual, expected):
    """Compare two RGB triples: HSV for chromatic colors, per-channel RGB for achromatic."""
    expected_h, expected_s, expected_v = (int(c) for c in
        cv2.cvtColor(np.uint8([[[expected[2], expected[1], expected[0]]]]), cv2.COLOR_BGR2HSV)[0, 0])

    # Gray/black/white have no real hue, so compare RGB directly instead of HSV.
    if expected_s < ACHROMATIC_S:
        r_diff = abs(actual[0] - expected[0])
        g_diff = abs(actual[1] - expected[1])
        b_diff = abs(actual[2] - expected[2])
        return max(r_diff, g_diff, b_diff) <= TOLERANCE['rgb']

    actual_h, actual_s, actual_v = (int(c) for c in
        cv2.cvtColor(np.uint8([[[actual[2], actual[1], actual[0]]]]), cv2.COLOR_BGR2HSV)[0, 0])
    hue_diff = abs(actual_h - expected_h)
    hue_diff = min(hue_diff, 180 - hue_diff)  # H wraps at 180
    return (hue_diff <= TOLERANCE['hue']
            and abs(actual_s - expected_s) <= TOLERANCE['sat']
            and abs(actual_v - expected_v) <= TOLERANCE['val'])


_snapshot_saved = set()

def save_failure_snapshot( test_file, pipeline, annotated_image=None ):
    """
    Save one failure snapshot per (test file, device) pair. When the same test
    runs against multiple devices on a multi-device rig (e.g. Jetson with D457
    on MIPI and D436 on USB), each device's failure produces its own snapshot.
    Repeated failures for the same (test, device) pair are deduped.
    If *annotated_image* is provided it is saved directly; otherwise a raw
    frame is grabbed from the still-running *pipeline* as a fallback
    (useful for page-detection failures).

    :param test_file:        pass ``__file__`` from the calling test
    :param pipeline:         an active ``rs.pipeline`` (for the raw-frame fallback)
    :param annotated_image:  optional pre-built debug image (numpy array)
    """
    name = os.path.basename( test_file ).replace( '.py', '' )
    # Resolve the device name up front so dedup keys (test, device) together.
    try:
        dev_name = pipeline.get_active_profile().get_device().get_info( rs.camera_info.name ).split()[-1]
    except:
        dev_name = None

    key = (name, dev_name)
    if key in _snapshot_saved:
        return

    image = annotated_image
    if image is None:
        frames = pipeline.wait_for_frames()
        f = frames.get_color_frame() or frames.get_infrared_frame()
        if f:
            image = np.asanyarray( f.get_data() )

    if image is None:
        return

    filename = f"{name}_{dev_name}.png" if dev_name else f"{name}.png"
    logdir = os.path.join( os.path.dirname( rs.__file__ ), 'unit-tests' )
    if os.path.isdir( logdir ):
        filename = os.path.join( logdir, filename )
    cv2.imwrite( filename, image )
    log.i( f"Saved failure snapshot: {filename}" )
    _snapshot_saved.add( key )
