package com.intel.realsense.librealsense;

import android.util.Pair;

import java.nio.ByteBuffer;

public class AutoCalibratedDevice extends Device
{
    AutoCalibratedDevice(long handle) {
        super(handle);
        mOwner = false;
    }

    public CalibrationResult RunOnChipCalibration(String json, int timeout, boolean oneButtonCalibration)
    {
        CalibrationResult calibrationResult = new CalibrationResult();

        nRunOnChipCalibration(mHandle, json, calibrationResult, timeout, oneButtonCalibration);

        return calibrationResult;
    }

    public CalibrationResult RunOnChipCalibration(String json, boolean oneButtonCalibration) {
        return RunOnChipCalibration(json,5000, oneButtonCalibration);
    }

    public ByteBuffer GetTable() {
        ByteBuffer results = ByteBuffer.allocateDirect(512);
        nGetTable(mHandle, results);
        return results;
    }

    public void SetTable(ByteBuffer buffer, boolean write) {
        nSetTable(mHandle, buffer, write);
    }

    private static native void nRunOnChipCalibration(long handle, String json, CalibrationResult calibrationResult, int timeout, boolean oneButtonCalibration);
    private static native void nSetTable(long handle, ByteBuffer buffer, boolean write);
    private static native void nGetTable(long handle, ByteBuffer buffer);
}