## License: Apache 2.0. See LICENSE file in root directory.
## Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# Object Detection example
# ========================
# Streams Color and Object Detection from a RealSense device that
# supports inference, and draws labelled bounding boxes on the live color feed.
#
# Prerequisites:
#   pip install pyrealsense2 numpy opencv-python
#
# Controls:
#   q  — quit

import pyrealsense2 as rs
import numpy as np
import cv2

# ---------------------------------------------------------------------------
# Class label lookup — extend to match the model on your device
# ---------------------------------------------------------------------------
CLASS_LABELS = {0: "Person", 1: "Face"}

def class_label(class_id):
    return CLASS_LABELS.get(class_id, f"Unknown({class_id})")

# Color used for bounding boxes and labels
BOX_COLOR   = (0, 220, 0)   # green
TEXT_COLOR  = (255, 255, 255)
FONT        = cv2.FONT_HERSHEY_SIMPLEX
FONT_SCALE  = 0.55
FONT_THICK  = 1
BOX_THICK   = 2

# ---------------------------------------------------------------------------
# Pipeline setup
# ---------------------------------------------------------------------------
pipeline = rs.pipeline()
config   = rs.config()

# Color must be active before the device starts inferencing. Use bgr8 as it is the default OpenCV format.
config.enable_stream(rs.stream.color, 1280, 720, rs.format.bgr8, 30)
config.enable_stream(rs.stream.object_detection)

pipeline.start(config)

print("Streaming — press 'q' to quit.")

try:
    while True:
        frames = pipeline.wait_for_frames()

        color_frame = frames.get_color_frame()
        if color_frame:
            image = np.asanyarray(color_frame.get_data())

            # ---------------------------------------------------------------
            # Draw detections when a new Object Detection frame is available.
            # OD frames may arrive less frequently than video frames; when no
            # OD frame is in the current frameset the color feed is shown as-is.
            # ---------------------------------------------------------------
            odf = frames.get_object_detection_frame()
            if odf:
                count = odf.get_detection_count()
                for i in range(count):
                    det = odf.get_detection(i)

                    x1, y1 = det.top_left_x,     det.top_left_y
                    x2, y2 = det.bottom_right_x, det.bottom_right_y

                    # Bounding box
                    cv2.rectangle(image, (x1, y1), (x2, y2), BOX_COLOR, BOX_THICK)

                    # Label: "Person  82%  1.35 m"
                    label = f"{class_label(det.class_id)}  {det.score}%"
                    if det.depth > 0.0:
                        label += f"  {det.depth:.2f} m"

                    # Background chip behind the text for readability
                    (tw, th), baseline = cv2.getTextSize(label, FONT, FONT_SCALE, FONT_THICK)
                    chip_y1 = max(y1 - th - baseline - 4, 0)
                    chip_y2 = max(y1, th + baseline + 4)
                    cv2.rectangle(image, (x1, chip_y1), (x1 + tw + 6, chip_y2), BOX_COLOR, cv2.FILLED)
                    cv2.putText(image, label, (x1 + 3, chip_y2 - baseline - 2),
                                FONT, FONT_SCALE, TEXT_COLOR, FONT_THICK, cv2.LINE_AA)

            cv2.imshow("Object Detection", image)

        if cv2.waitKey(1) & 0xFF == ord('q'):
            break

finally:
    pipeline.stop()
    cv2.destroyAllWindows()
