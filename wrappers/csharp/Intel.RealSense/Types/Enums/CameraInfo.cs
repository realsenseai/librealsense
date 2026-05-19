// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 Intel Corporation. All Rights Reserved.

namespace Intel.RealSense
{
    /// <summary>
    /// Read-only strings that can be queried from the device.
    /// </summary>
    /// <remarks>
    /// Not all information attributes are available on all camera types.
    /// This information is mainly available for camera debug and troubleshooting and should not be used in applications. */
    /// </remarks>
    public enum CameraInfo
    {
        /// <summary> Friendly name</summary>
        Name = 0,

        /// <summary> Device serial number</summary>
        SerialNumber = 1,

        /// <summary> Primary firmware version</summary>
        FirmwareVersion = 2,

        /// <summary> Recommended firmware version</summary>
        RecommendedFirmwareVersion = 3,

        /// <summary> Unique identifier of the port the device is connected to (platform specific)</summary>
        PhysicalPort = 4,

        /// <summary> If device supports firmware logging, this is the command to send to get logs from firmware</summary>
        DebugOpCode = 5,

        /// <summary> True iff the device is in advanced mode</summary>
        AdvancedMode = 6,

        /// <summary> Product ID as reported in the USB descriptor</summary>
        ProductId = 7,

        /// <summary> True iff EEPROM is locked</summary>
        CameraLocked = 8,

        /// <summary> Designated USB specification: USB2/USB3</summary>
        UsbTypeDescriptor = 9,

        /// <summary> Device product line D400, etc.</summary>
        ProductLine = 10,

        /// <summary> ASIC serial number</summary>
        AsicSerialNumber = 11,

        /// <summary> Firmware update ID</summary>
        FirmwareUpdateId = 12,

        /// <summary> IP address for remote camera</summary>
        IpAddress = 13,

        /// <summary> DFU Device node path</summary>
        DfuDevicePath = 14,

        /// <summary> Connection type, for example USB, GMSL, DDS</summary>
        ConnectionType = 15,

        /// <summary> Safety MCU FW Version</summary>
        SmcuFwVersion = 16,

        /// <summary> IMU Type</summary>
        ImuType = 17,

        /// <summary> MIPI driver version (Jetson platform only)</summary>
        MipiDriverVersion = 18,
    }
}
