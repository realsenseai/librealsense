// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 Intel Corporation. All Rights Reserved.

namespace Intel.RealSense
{
    /// <summary>
    /// Category of a library-created thread, for use with the thread-start callback
    /// </summary>
    public enum ThreadCategory
    {
        /// <summary> USB I/O threads </summary>
        UsbIo = 0,

        /// <summary> Video capture threads </summary>
        VideoCapture = 1,

        /// <summary> Sensor I/O threads </summary>
        SensorIo = 2,

        /// <summary> Frame processing threads </summary>
        FrameProcessing = 3,

        /// <summary> Device monitoring threads </summary>
        DeviceMonitoring = 4,

        /// <summary> Dispatch threads </summary>
        Dispatch = 5,

        /// <summary> Network threads </summary>
        Network = 6,

        /// <summary> Utility threads </summary>
        Utility = 7,
    }
}
