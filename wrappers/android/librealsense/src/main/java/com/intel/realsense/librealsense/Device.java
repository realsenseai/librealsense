package com.intel.realsense.librealsense;

import java.util.ArrayList;
import java.util.List;

public class Device extends LrsClass {
    private List<Sensor> _sensors = new ArrayList<>();

    Device(long handle){
        mHandle = handle;
        long[] sensorsHandles = nQuerySensors(mHandle);
        for(long h : sensorsHandles){
            _sensors.add(new Sensor(h));
        }
    }

    public List<Sensor> querySensors(){
        return _sensors;
    }

    public boolean supportsInfo(CameraInfo info){
        return nSupportsInfo(mHandle, info.value());
    }

    public String getInfo(CameraInfo info){
        return nGetInfo(mHandle, info.value());
    }

    public void toggleAdvancedMode(boolean enable){
        nToggleAdvancedMode(mHandle, enable);
    }

    public boolean isInAdvancedMode(){
        return nIsInAdvancedMode(mHandle);
    }

    public void loadPresetFromJson(byte[] data){
        nLoadPresetFromJson(mHandle, data);
    }

    public byte[] serializePresetToJson(){
        return nSerializePresetToJson(mHandle);
    }

    public boolean is(Extension extension) {
        return nIsDeviceExtendableTo(mHandle, extension.value());
    }

    public <T extends Device> T as(Extension extension) {
        switch (extension){
            case UPDATABLE: return (T) new Updatable(mHandle);
            case UPDATE_DEVICE: return (T) new UpdateDevice(mHandle);
            case DEBUG: return (T) new DebugProtocol(mHandle);
            case FW_LOGGER: return (T) new FwLogger(mHandle);
            case AUTO_CALIBRATED_DEVICE: return (T) new AutoCalibratedDevice(mHandle);
        }
        throw new RuntimeException("this device is not extendable to " + extension.name());
    }

    public void hardwareReset(){
        nHardwareReset(mHandle);
    }

    @Override
    public void close() {
        for (Sensor s : _sensors)
            s.close();
        if(mOwner)
            nRelease(mHandle);
    }

    private static native boolean nSupportsInfo(long handle, int info);
    private static native String nGetInfo(long handle, int info);
    private static native void nToggleAdvancedMode(long handle, boolean enable);
    private static native boolean nIsInAdvancedMode(long handle);
    private static native void nLoadPresetFromJson(long handle, byte[] data);
    private static native byte[] nSerializePresetToJson(long handle);
    private static native long[] nQuerySensors(long handle);
    private static native void nHardwareReset(long handle);
    private static native boolean nIsDeviceExtendableTo(long handle, int extension);
    private static native void nRelease(long handle);
}
