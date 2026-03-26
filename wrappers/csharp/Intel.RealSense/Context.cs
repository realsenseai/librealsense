// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 Intel Corporation. All Rights Reserved.

namespace Intel.RealSense
{
    using System;
    using System.Runtime.InteropServices;
    using Microsoft.Win32.SafeHandles;
    using System.Runtime.ConstrainedExecution;
    using System.Security.Permissions;


    [SecurityPermission(SecurityAction.LinkDemand, UnmanagedCode = true)]
    public sealed class ContextHandle : SafeHandleZeroOrMinusOneIsInvalid
    {
        public ContextHandle()
            : base(true)
        {
        }

        [ReliabilityContract(Consistency.WillNotCorruptState, Cer.MayFail)]
        override protected bool ReleaseHandle()
        {
            if (!IsClosed)
            {
                NativeMethods.rs2_delete_context(this);
            }
            return true;
        }
    }

    /// <summary>
    /// default librealsense context class
    /// </summary>
    public class Context : IDisposable
    {
        private ContextHandle handle;

        static Context()
        {
            object error;
            ApiVersion = NativeMethods.rs2_get_api_version(out error);
        }

        /// <summary>
        /// Initializes a new instance of the <see cref="Context"/> class.
        /// </summary>
        public Context()
        {
            object error;
            handle = NativeMethods.rs2_create_context(ApiVersion, out error);
            onDevicesChangedCallback = new rs2_devices_changed_callback(OnDevicesChangedInternal);
        }

        /// <summary>
        /// Initializes a new instance of the <see cref="Context"/> class with json_settings.
        /// 
        /// </summary>
        public Context(string json_settings)
        {
            object error;

            handle = NativeMethods.rs2_create_context_ex(ApiVersion, json_settings, out error);
            onDevicesChangedCallback = new rs2_devices_changed_callback(OnDevicesChangedInternal);
        }

        /// <summary>
        /// Gets the safe handle
        /// </summary>
        /// <exception cref="ObjectDisposedException">Thrown when <see cref="SafeHandle.IsInvalid"/></exception>
        public ContextHandle Handle
        {
            get
            {
                if (handle.IsInvalid)
                {
                    throw new ObjectDisposedException(GetType().Name);
                }

                return handle;
            }
        }

        /// <summary>
        /// the version API encoded into integer value "1.9.3" -> 10903
        /// </summary>
        public static readonly int ApiVersion;

        /// <summary>
        /// Gets the API version
        /// </summary>
        public string Version
        {
            get
            {
                if (ApiVersion / 10000 == 0)
                {
                    return ApiVersion.ToString();
                }

                return $"{ApiVersion / 10000}.{(ApiVersion % 10000) / 100}.{ApiVersion % 100}";
            }
        }

        // Keeps the delegate alive, if we were to assign onDevicesChanged directly, there'll be
        // no managed reference it, it will be collected and cause a native exception.
        private readonly rs2_devices_changed_callback onDevicesChangedCallback;

        /// <summary>
        /// Delegate to register as per-notifications callback
        /// </summary>
        /// <param name="removed">list of removed devices</param>
        /// <param name="added">list of added devices</param>
        public delegate void OnDevicesChangedDelegate(DeviceList removed, DeviceList added);

        private event OnDevicesChangedDelegate OnDevicesChangedEvent;

        private readonly object deviceChangedEventLock = new object();

        /// <summary>
        /// these events will be raised by the context whenever new RealSense device is connected or existing device gets disconnected
        /// </summary>
        public event OnDevicesChangedDelegate OnDevicesChanged
        {
            add
            {
                lock (deviceChangedEventLock)
                {
                    if (OnDevicesChangedEvent == null)
                    {
                        object error;
                        NativeMethods.rs2_set_devices_changed_callback(handle, onDevicesChangedCallback, IntPtr.Zero, out error);
                    }

                    OnDevicesChangedEvent += value;
                }
            }

            remove
            {
                lock (deviceChangedEventLock)
                {
                    OnDevicesChangedEvent -= value;
                }
            }
        }

        /// <summary>
        /// Create a static snapshot of all connected devices at the time of the call
        /// </summary>
        /// <param name="include_platform_camera">Controls what kind of devices will be returned</param>
        /// <returns>the list of devices</returns>
        /// <remarks>devices in the collection should be disposed</remarks>
        public DeviceList QueryDevices(bool include_platform_camera = false)
        {
            object error;
            var ptr = NativeMethods.rs2_query_devices_ex(handle, include_platform_camera ? 0xff : 0xfe, out error);
            return new DeviceList(ptr);
        }

        /// <summary>
        /// Gets a static snapshot of all connected devices at the time of the call
        /// </summary>
        /// <value>the list of devices</value>
        public DeviceList Devices
        {
            get
            {
                return QueryDevices();
            }
        }

        /// <summary>Create a new device and add it to the context</summary>
        /// <param name="file">The file from which the device should be created</param>
        /// <returns>a device that plays data from the file</returns>
        public PlaybackDevice AddDevice(string file)
        {
            object error;
            var ptr = NativeMethods.rs2_context_add_device(handle, file, out error);
            return Device.Create<PlaybackDevice>(ptr);
        }

        /// <summary>Removes a playback device from the context, if exists</summary>
        /// <param name="file">The file name that was used to add the device</param>
        public void RemoveDevice(string file)
        {
            object error;
            NativeMethods.rs2_context_remove_device(handle, file, out error);
        }

        /// <summary>Converts a legacy ROS1 .bag recording to a ROS2 .db3 file</summary>
        /// <param name="inputBagPath">Path to the input .bag file</param>
        /// <param name="outputDb3Path">Path for the output .db3 file</param>
        public void ConvertBagToDb3(string inputBagPath, string outputDb3Path)
        {
            object error;
            NativeMethods.rs2_convert_bag_to_db3(inputBagPath, outputDb3Path, handle, IntPtr.Zero, IntPtr.Zero, out error);
        }

        private void OnDevicesChangedInternal(IntPtr removedList, IntPtr addedList, IntPtr userData)
        {
            var e = OnDevicesChangedEvent;
            if (e != null)
            {
                using (var removed = new DeviceList(removedList))
                using (var added = new DeviceList(addedList))
                {
                    e(removed, added);
                }
            }
        }

        public void Dispose()
        {
            handle.Dispose();
        }
    }
}
