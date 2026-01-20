package com.intel.realsense.librealsense;

import java.nio.ByteBuffer;

public class CalibrationResult
{
    public float mHealth = -1;
    public float mHealth1 = -1;
    public float mHealth2 = -1;
    public ByteBuffer mCalibrationData = null;

    public CalibrationResult()
    {
        mCalibrationData = ByteBuffer.allocateDirect(512);
    }
}