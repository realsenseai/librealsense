package com.intel.realsense.librealsense;

public enum StreamFormat {
    ANY(0),
    Z16 (1),
    DISPARITY16(2),
    XYZ32F(3),
    YUYV(4),
    RGB8(5),
    BGR8(6),
    RGBA8(7),
    BGRA8(8),
    Y8(9),
    Y16(10),
    RAW10(11),
    RAW16(12),
    RAW8(13),
    UYVY(14),
    MOTION_RAW(15),
    MOTION_XYZ32F(16),
    GPIO_RAW(17),
    POSE(18),
    DISPARITY32(19),
    Y10BPACK(20),
    DISTANCE(21),
    MJPEG(22),
    Y8I(23),
    Y12I(24),
    INZI(25),
    INVI(26),
    W10(27),
    Z16H(28),
    FG(29),
    Y411(30),
    Y16I(31),
    M420(32),
    COMBINED_MOTION(33),
    NV12(34),
    H264(35);
    private final int mValue;

    private StreamFormat(int value) { mValue = value; }
    public int value() { return mValue; }
}
