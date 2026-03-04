package com.intel.realsense.librealsense;

public enum ThreadCategory {
    USB_IO(0),
    VIDEO_CAPTURE(1),
    SENSOR_IO(2),
    FRAME_PROCESSING(3),
    DEVICE_MONITORING(4),
    DISPATCH(5),
    NETWORK(6),
    UTILITY(7);

    private final int mValue;

    private ThreadCategory(int value) { mValue = value; }
    public int value() { return mValue; }
}
