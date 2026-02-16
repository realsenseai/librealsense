# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2025 RealSense, Inc. All Rights Reserved.

from rspy import log, test
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
    Detect ArUco markers and return center of each one
    Returns None if not all required markers are found
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

    if ids is None or not all(rid in ids for rid in required_ids):
        return None

    id_to_corner = dict(zip(ids.flatten(), corners))  # map id to corners
    values = [id_to_corner[rid][0].mean(axis=0) for rid in required_ids] # for each required id, get center of marker coords

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

def get_roi_from_frame(frame):
    """
    Apply the previously computed transformation matrix to the given frame
    to get the region of interest (A4 page)
    """
    global M
    if M is None:
        raise Exception("Transformation matrix not computed yet")

    np_frame = np.asanyarray(frame.get_data())
    warped = cv2.warpPerspective(np_frame, M, (WIDTH, HEIGHT)) # using A4 size for its ratio
    return warped


def is_color_close(actual, expected, tolerance):
    return all(abs(int(a) - int(e)) <= tolerance for a, e in zip(actual, expected))
