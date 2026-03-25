// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 Intel Corporation. All Rights Reserved.

#pragma warning disable SA1600 // Elements must be documented
#pragma warning disable SA1124 // Do not use regions

namespace Intel.RealSense
{
    using System;
    using System.Reflection.Emit;
    using System.Runtime.InteropServices;
    using System.Security;
    using System.Security.Permissions;

    [SuppressUnmanagedCodeSecurity]
    [SecurityPermission(SecurityAction.LinkDemand, Flags = SecurityPermissionFlag.UnmanagedCode)]
    internal static class NativeMethods
    {

#if DEBUG
        private const string dllName = "realsense2d";
        private const string dllNetName = "realsense2-netd";
#else
        private const string dllName = "realsense2";
        private const string dllNetName = "realsense2-net";
#endif


        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate IntPtr MemCpyDelegate(IntPtr dest, IntPtr src, int count);

        internal static readonly MemCpyDelegate Memcpy = MemCpy.GetMethod();

        [System.Security.SuppressUnmanagedCodeSecurity]
        internal static class MemCpy
        {
            internal static MemCpyDelegate GetMethod()
            {
                switch (System.Environment.OSVersion.Platform)
                {
                    case PlatformID.Win32NT:
                        return win_memcpy;
                    case PlatformID.Unix:
                    case PlatformID.MacOSX:
                        return unix_memcpy;
                    default:
                        throw new PlatformNotSupportedException(System.Environment.OSVersion.ToString());
                }
            }
        }

        [DllImport("libc", EntryPoint = "memcpy", CallingConvention = CallingConvention.Cdecl, SetLastError = false)]
        internal static extern IntPtr unix_memcpy(IntPtr dest, IntPtr src, int count);

        [DllImport("msvcrt.dll", EntryPoint = "memcpy", CallingConvention = CallingConvention.Cdecl, SetLastError = false)]
        internal static extern IntPtr win_memcpy(IntPtr dest, IntPtr src, int count);

        #region rs_record_playback
        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_create_record_device(IntPtr device, [MarshalAs(UnmanagedType.LPStr)] string file, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_record_device_pause(IntPtr device, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_record_device_resume(IntPtr device, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_record_device_filename(IntPtr device, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_create_playback_device([MarshalAs(UnmanagedType.LPStr)] string file, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_playback_device_get_file_path(IntPtr device, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern ulong rs2_playback_get_duration(IntPtr device, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_playback_seek(IntPtr device, long time, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern ulong rs2_playback_get_position(IntPtr device, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_playback_device_resume(IntPtr device, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_playback_device_pause(IntPtr device, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_playback_device_set_real_time(IntPtr device, int real_time, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int rs2_playback_device_is_real_time(IntPtr device, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_playback_device_set_status_changed_callback(IntPtr device, IntPtr callback, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern PlaybackStatus rs2_playback_device_get_current_status(IntPtr device, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_playback_device_set_playback_speed(IntPtr device, float speed, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_playback_device_stop(IntPtr device, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);
        #endregion
        #region rs_processing
        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_create_colorizer([MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_create_sync_processing_block([MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_create_pointcloud([MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_create_processing_block_fptr([MarshalAs(UnmanagedType.FunctionPtr)] frame_processor_callback on_frame, IntPtr user, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_start_processing_fptr(IntPtr block, [MarshalAs(UnmanagedType.FunctionPtr)] frame_callback on_frame, IntPtr user, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int rs2_processing_block_register_simple_option(IntPtr block, Option option_id, float min, float max, float step, float def, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_get_processing_block_info(IntPtr block, CameraInfo info, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int rs2_supports_processing_block_info(IntPtr block, CameraInfo info, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_start_processing_queue(IntPtr block, IntPtr queue, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_process_frame(IntPtr block, IntPtr frame, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_delete_processing_block(IntPtr block);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_create_frame_queue(int capacity, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_delete_frame_queue(IntPtr queue);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_wait_for_frame(IntPtr queue, uint timeout_ms, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int rs2_try_wait_for_frame(IntPtr queue, uint timeout_ms, out IntPtr output_frame, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int rs2_poll_for_frame(IntPtr queue, out IntPtr frame, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_enqueue_frame(IntPtr frame, IntPtr queue);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int rs2_frame_queue_size(IntPtr queue, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_create_align(Stream align_to, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_create_decimation_filter_block([MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_create_temporal_filter_block([MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_create_spatial_filter_block([MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_create_disparity_transform_block(byte transform_to_disparity, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_create_hole_filling_filter_block([MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_create_threshold([MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_create_units_transform([MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_create_zero_order_invalidation_block([MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_create_hdr_merge_processing_block([MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_create_sequence_id_filter([MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);
        #endregion
        #region rs_option
        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int rs2_is_option_read_only(IntPtr options, Option option, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern float rs2_get_option(IntPtr options, Option option, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_set_option(IntPtr options, Option option, float value, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_get_options_list(IntPtr options, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int rs2_get_options_list_size(IntPtr options, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern Option rs2_get_option_from_list(IntPtr options_list, int i, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_delete_options_list(IntPtr options_list);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int rs2_supports_option(IntPtr options, Option option, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_get_option_range(IntPtr sensor, Option option, out float min, out float max, out float step, out float def, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_get_option_description(IntPtr options, Option option, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_get_option_value_description(IntPtr options, Option option, float value, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);
        #endregion
        #region rs_frame
        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern long rs2_get_frame_metadata(IntPtr frame, FrameMetadataValue frame_metadata, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int rs2_supports_frame_metadata(IntPtr frame, FrameMetadataValue frame_metadata, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern TimestampDomain rs2_get_frame_timestamp_domain(IntPtr frameset, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern double rs2_get_frame_timestamp(IntPtr frame, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_get_frame_sensor(IntPtr frame, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern ulong rs2_get_frame_number(IntPtr frame, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int rs2_get_frame_data_size(IntPtr frame, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_get_frame_data(IntPtr frame, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int rs2_get_frame_width(IntPtr frame, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int rs2_get_frame_height(IntPtr frame, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int rs2_get_frame_stride_in_bytes(IntPtr frame, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int rs2_get_frame_bits_per_pixel(IntPtr frame, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_frame_add_ref(IntPtr frame, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_release_frame(IntPtr frame);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_keep_frame(IntPtr frame);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_get_frame_vertices(IntPtr frame, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_get_frame_texture_coordinates(IntPtr frame, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int rs2_get_frame_points_count(IntPtr frame, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_export_to_ply(IntPtr frame, string fname, IntPtr texture, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_get_frame_stream_profile(IntPtr frame, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int rs2_is_frame_extendable_to(IntPtr frame, Extension extension_type, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_allocate_synthetic_video_frame(IntPtr source, IntPtr new_stream, IntPtr original, int new_bpp, int new_width, int new_height, int new_stride, Extension frame_type, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_allocate_synthetic_motion_frame(IntPtr source, IntPtr new_stream, IntPtr original, Extension frame_type, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_allocate_composite_frame(IntPtr source, [In] IntPtr[] frames, int count, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_allocate_composite_frame(IntPtr source, [In] IntPtr frames, int count, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_extract_frame(IntPtr composite, int index, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int rs2_embedded_frames_count(IntPtr composite, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_synthetic_frame_ready(IntPtr source, IntPtr frame, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_pose_frame_get_pose_data(IntPtr frame, [In, Out] Pose pose, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);
        #endregion
        #region rs_sensor
        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_delete_sensor_list(IntPtr info_list);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int rs2_get_sensors_count(IntPtr info_list, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_delete_sensor(IntPtr sensor);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_create_sensor(IntPtr list, int index, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_create_device_from_sensor(IntPtr sensor, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_get_sensor_info(IntPtr sensor, CameraInfo info, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int rs2_supports_sensor_info(IntPtr sensor, CameraInfo info, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int rs2_is_sensor_extendable_to(IntPtr sensor, Extension extension, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern float rs2_get_depth_scale(IntPtr sensor, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_set_region_of_interest(IntPtr sensor, int min_x, int min_y, int max_x, int max_y, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_get_region_of_interest(IntPtr sensor, out int min_x, out int min_y, out int max_x, out int max_y, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_open(IntPtr device, IntPtr profile, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_open_multiple(IntPtr device, [In] IntPtr[] profiles, int count, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_close(IntPtr sensor, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_start(IntPtr sensor, [MarshalAs(UnmanagedType.FunctionPtr)] frame_callback on_frame, IntPtr user, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_start_queue(IntPtr sensor, IntPtr queue, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_stop(IntPtr sensor, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_set_notifications_callback(IntPtr sensor, [MarshalAs(UnmanagedType.FunctionPtr)] frame_callback on_notification, IntPtr user, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_get_notification_description(IntPtr notification, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern double rs2_get_notification_timestamp(IntPtr notification, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern LogSeverity rs2_get_notification_severity(IntPtr notification, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern NotificationCategory rs2_get_notification_category(IntPtr notification, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_get_stream_profiles(IntPtr device, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_get_recommended_processing_blocks(IntPtr device, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_get_processing_block(IntPtr device, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_get_stream_profile(IntPtr list, int index, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_get_processing_block(IntPtr list, int index, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_get_stream_profile_data(IntPtr mode, out Stream stream, out Format format, out int index, out int unique_id, out int framerate, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_set_stream_profile_data(IntPtr mode, Stream stream, int index, Format format, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_clone_stream_profile(IntPtr mode, Stream stream, int index, Format format, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_clone_video_stream_profile(IntPtr mode, Stream stream, int index, Format format, int width, int height, Intrinsics intr, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_delete_stream_profile(IntPtr mode);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int rs2_stream_profile_is(IntPtr mode, Extension type, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int rs2_is_processing_block_extendable_to(IntPtr mode, Extension type, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_get_video_stream_resolution(IntPtr from, out int width, out int height, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int rs2_is_stream_profile_default(IntPtr profile, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int rs2_get_stream_profiles_count(IntPtr list, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_delete_stream_profiles_list(IntPtr list);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int rs2_get_recommended_processing_blocks_count(IntPtr list, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_delete_recommended_processing_blocks(IntPtr list);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_get_extrinsics(IntPtr from, IntPtr to, out Extrinsics extrin, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_register_extrinsics(IntPtr from, IntPtr to, Extrinsics extrin, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_get_video_stream_intrinsics(IntPtr from, out Intrinsics intrinsics, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_export_localization_map(IntPtr sensor, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int rs2_import_localization_map(IntPtr sensor, IntPtr raw_data_to_send, uint size_of_raw_data_to_send, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int rs2_set_static_node(IntPtr sensor, [MarshalAs(UnmanagedType.LPStr)] string guid, Math.Vector pos, Math.Quaternion orient, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int rs2_get_static_node(IntPtr sensor, [MarshalAs(UnmanagedType.LPStr)] string guid, out Math.Vector pos, out Math.Quaternion orient, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int rs2_remove_static_node(IntPtr sensor, [MarshalAs(UnmanagedType.LPStr)] string guid, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int rs2_load_wheel_odometry_config(IntPtr sensor, IntPtr wheel_odometry_cfg_buf, uint size_of_buf, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int rs2_send_wheel_odometry(IntPtr sensor, byte wo_sensor_id, uint frame_num, Math.Vector translational_velocity, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_get_motion_intrinsics(IntPtr profile, out MotionDeviceIntrinsics intrinsics, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern float rs2_get_max_usable_depth_range(IntPtr sensor, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);
        #endregion
        #region rs_device
        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int rs2_get_device_count(IntPtr info_list, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_delete_device_list(IntPtr info_list);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int rs2_device_list_contains(IntPtr info_list, IntPtr device, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_create_device(IntPtr info_list, int index, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_delete_device(IntPtr device);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_get_device_info(IntPtr device, CameraInfo info, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int rs2_supports_device_info(IntPtr device, CameraInfo info, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_hardware_reset(IntPtr device, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_send_and_receive_raw_data(IntPtr device, IntPtr raw_data_to_send, uint size_of_raw_data_to_send, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_build_debug_protocol_command(IntPtr device, uint opdoce, uint param1, uint param2, uint param3, uint param4, IntPtr raw_data_to_send, uint size_of_raw_data_to_send, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int rs2_is_device_extendable_to(IntPtr device, Extension extension, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_query_sensors(IntPtr device, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_enter_update_state(IntPtr device, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int rs2_check_firmware_compatibility(IntPtr device, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_update_firmware(IntPtr device, IntPtr fw_image, int fw_image_size, rs2_update_progress_callback callback, IntPtr user_data, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_create_flash_backup(IntPtr device, rs2_update_progress_callback callback, IntPtr user_data, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);
        #endregion
        #region rs_context
        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern ContextHandle rs2_create_context(int api_version, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern ContextHandle rs2_create_context_ex(int api_version, [MarshalAs(UnmanagedType.LPStr)] string json_settings, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_delete_context(ContextHandle context);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_context_add_device(ContextHandle ctx, [MarshalAs(UnmanagedType.LPStr)] string file, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_context_remove_device(ContextHandle ctx, [MarshalAs(UnmanagedType.LPStr)] string file, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_query_devices(IntPtr context, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_query_devices_ex(ContextHandle context, int mask, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_create_device_hub(IntPtr context, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_delete_device_hub(IntPtr hub);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_device_hub_wait_for_device(IntPtr ctx, IntPtr hub, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int rs2_device_hub_is_device_connected(IntPtr hub, IntPtr device, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_set_devices_changed_callback(ContextHandle ctx, rs2_devices_changed_callback callback, IntPtr user_data, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_context_add_software_device(ContextHandle ctx, IntPtr device, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_convert_bag_to_db3([MarshalAs(UnmanagedType.LPStr)] string input_bag_path, [MarshalAs(UnmanagedType.LPStr)] string output_db3_path, ContextHandle ctx, IntPtr callback, IntPtr client_data, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);
        #endregion
        #region rs
        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int rs2_get_raw_data_size(IntPtr buffer, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_delete_raw_data(IntPtr buffer);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_get_raw_data(IntPtr buffer, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int rs2_get_api_version([MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_log_to_console(LogSeverity min_severity, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_log_to_file(LogSeverity min_severity, [MarshalAs(UnmanagedType.LPStr)] string file_path, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_log(LogSeverity severity, [MarshalAs(UnmanagedType.LPStr)] string message, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern float rs2_depth_frame_get_distance(IntPtr frame_ref, int x, int y, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern float rs2_depth_frame_get_units(IntPtr frame_ref, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern float rs2_depth_stereo_frame_get_baseline(IntPtr frame_ref, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern double rs2_get_time([MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);
        #endregion
        #region rs_advanced_mode
        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_toggle_advanced_mode(IntPtr dev, int enable, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_is_enabled(IntPtr dev, out int enabled, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_set_depth_control(IntPtr dev, IntPtr group, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_get_depth_control(IntPtr dev, IntPtr group, int mode, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_set_rsm(IntPtr dev, IntPtr group, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_get_rsm(IntPtr dev, IntPtr group, int mode, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_set_rau_support_vector_control(IntPtr dev, IntPtr group, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_get_rau_support_vector_control(IntPtr dev, IntPtr group, int mode, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_set_color_control(IntPtr dev, IntPtr group, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_get_color_control(IntPtr dev, IntPtr group, int mode, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_set_rau_thresholds_control(IntPtr dev, IntPtr group, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_get_rau_thresholds_control(IntPtr dev, IntPtr group, int mode, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_set_slo_color_thresholds_control(IntPtr dev, IntPtr group, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_get_slo_color_thresholds_control(IntPtr dev, IntPtr group, int mode, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_set_slo_penalty_control(IntPtr dev, IntPtr group, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_get_slo_penalty_control(IntPtr dev, IntPtr group, int mode, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_set_hdad(IntPtr dev, IntPtr group, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_get_hdad(IntPtr dev, IntPtr group, int mode, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_set_color_correction(IntPtr dev, IntPtr group, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_get_color_correction(IntPtr dev, IntPtr group, int mode, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_set_depth_table(IntPtr dev, IntPtr group, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_get_depth_table(IntPtr dev, IntPtr group, int mode, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_set_ae_control(IntPtr dev, IntPtr group, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_get_ae_control(IntPtr dev, IntPtr group, int mode, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_set_census(IntPtr dev, IntPtr group, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_get_census(IntPtr dev, IntPtr group, int mode, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_set_amp_factor(IntPtr dev, IntPtr group, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_get_amp_factor(IntPtr dev, IntPtr group, int mode, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_load_json(IntPtr dev, [MarshalAs(UnmanagedType.LPStr)] string json_content, uint content_size, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_serialize_json(IntPtr dev, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);
        #endregion
        #region rs_calibrated_device
        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_write_calibration(IntPtr dev, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_reset_to_factory_calibration(IntPtr dev, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);
        #endregion
        #region rs_auto_calibrated_device
        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_run_on_chip_calibration(IntPtr dev, [MarshalAs(UnmanagedType.LPStr)] string json_content, int content_size, out float health, [MarshalAs(UnmanagedType.FunctionPtr)] rs2_update_progress_callback callback, IntPtr client_data, int timeout_ms, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_run_tare_calibration(IntPtr dev, float ground_truth_mm, [MarshalAs(UnmanagedType.LPStr)] string json_content, int content_size, [MarshalAs(UnmanagedType.FunctionPtr)] rs2_update_progress_callback callback, IntPtr client_data, int timeout_ms, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_run_focal_length_calibration(IntPtr dev, IntPtr left_queue, IntPtr right_queue, float target_width_mm, float target_height_mm, int adjust_both_sides, out float ratio, out float angle,
            [MarshalAs(UnmanagedType.FunctionPtr)] rs2_update_progress_callback callback, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_run_uv_map_calibration(IntPtr dev, IntPtr left_queue, IntPtr rgb_queue, IntPtr depth_queue, int px_py_only, out float ratio, out float angle,
            [MarshalAs(UnmanagedType.FunctionPtr)] rs2_update_progress_callback callback, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern float rs2_calculate_target_z(IntPtr dev, IntPtr input_frame_queue1, IntPtr input_frame_queue2, IntPtr input_frame_queue3, float target_width_mm, float target_height_mm,
            [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern float rs2_calculate_target_z(IntPtr dev, IntPtr input_frame_queue1, IntPtr input_frame_queue2, IntPtr input_frame_queue3, float target_width_mm, float target_height_mm,
            [MarshalAs(UnmanagedType.FunctionPtr)] rs2_update_progress_callback callback, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_get_calibration_table(IntPtr dev, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_set_calibration_table(IntPtr dev, IntPtr calibration, int content_size, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);
        #endregion
        #region rs_internal
        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_create_recording_context(int api_version, [MarshalAs(UnmanagedType.LPStr)] string filename, [MarshalAs(UnmanagedType.LPStr)] string section, RecordingMode mode, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_create_mock_context(int api_version, [MarshalAs(UnmanagedType.LPStr)] string filename, [MarshalAs(UnmanagedType.LPStr)] string section, [MarshalAs(UnmanagedType.LPStr)] string min_api_version, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        #region software_device
        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_create_software_device([MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_software_device_add_sensor(IntPtr dev, [MarshalAs(UnmanagedType.LPStr)] string sensor_name, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_software_sensor_on_video_frame(IntPtr sensor, SoftwareVideoFrame frame, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_software_sensor_on_motion_frame(IntPtr sensor, SoftwareMotionFrame frame, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_software_sensor_on_pose_frame(IntPtr sensor, SoftwarePoseFrame frame, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_software_sensor_set_metadata(IntPtr sensor, long value, FrameMetadataValue type, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_software_device_create_matcher(IntPtr dev, Matchers matcher, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_software_sensor_add_video_stream(IntPtr sensor, SoftwareVideoStream video_stream, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_software_sensor_add_motion_stream(IntPtr sensor, SoftwareMotionStream motion_stream, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_software_sensor_add_pose_stream(IntPtr sensor, SoftwarePoseStream pose_stream, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_software_sensor_add_read_only_option(IntPtr sensor, Option option, float val, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_software_sensor_update_read_only_option(IntPtr sensor, Option option, float val, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);
        #endregion
        #region fw_logs
        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_create_fw_log_message(IntPtr device, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_delete_fw_log_message(IntPtr fw_log);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern uint rs2_get_number_of_fw_logs(IntPtr device, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int rs2_get_fw_log(IntPtr device, IntPtr fw_log_, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int rs2_get_flash_log(IntPtr device, IntPtr fw_log, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_fw_log_message_data(IntPtr fw_log, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int rs2_fw_log_message_size(IntPtr fw_log, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern uint rs2_fw_log_message_timestamp(IntPtr fw_log, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int rs2_fw_log_message_severity(IntPtr fw_log, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int rs2_init_fw_log_parser(IntPtr device, [MarshalAs(UnmanagedType.LPStr)] string xml_content, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_create_fw_log_parsed_message(IntPtr device, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_delete_fw_log_parsed_message(IntPtr fw__parsed_log);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int rs2_parse_firmware_log(IntPtr device, IntPtr fw_log, IntPtr fw_parsed_log, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_get_fw_log_parsed_message(IntPtr fw_parsed_log, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_get_fw_log_parsed_file_name(IntPtr fw_parsed_log, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_get_fw_log_parsed_thread_name(IntPtr fw_parsed_log, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int rs2_get_fw_log_parsed_severity(IntPtr fw_parsed_log, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern uint rs2_get_fw_log_parsed_line(IntPtr fw_parsed_log, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern uint rs2_get_fw_log_parsed_timestamp(IntPtr fw_parsed_log, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern uint rs2_get_fw_log_parsed_sequence_id(IntPtr fw_parsed_log, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);
        #endregion
        #region terminal_parser
        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_create_terminal_parser([MarshalAs(UnmanagedType.LPStr)] string xml_content, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_delete_terminal_parser(IntPtr terminal_parser);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_terminal_parse_command(IntPtr terminal_parser, [MarshalAs(UnmanagedType.LPStr)] string command, uint size_of_command,
            [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_terminal_parse_response(IntPtr terminal_parser, [MarshalAs(UnmanagedType.LPStr)] string command, uint size_of_command,
            IntPtr response_bytes, uint size_of_response,
            [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);
        #endregion
        #endregion
        #region rs_pipeline
        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_create_pipeline(ContextHandle ctx, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_pipeline_stop(IntPtr pipe, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_pipeline_wait_for_frames(IntPtr pipe, uint timeout_ms, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int rs2_pipeline_try_wait_for_frames(IntPtr pipe, out IntPtr output_frame, uint timeout_ms, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int rs2_pipeline_poll_for_frames(IntPtr pipe, out IntPtr output_frame, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_delete_pipeline(IntPtr pipe);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_pipeline_start(IntPtr pipe, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_pipeline_start_with_config(IntPtr pipe, IntPtr config, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_pipeline_start_with_callback(IntPtr pipe, [MarshalAs(UnmanagedType.FunctionPtr)] frame_callback on_frame, IntPtr user, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_pipeline_start_with_config_and_callback(IntPtr pipe, IntPtr config, [MarshalAs(UnmanagedType.FunctionPtr)] frame_callback on_frame, IntPtr user, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_pipeline_get_active_profile(IntPtr pipe, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_pipeline_profile_get_device(IntPtr profile, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_pipeline_profile_get_streams(IntPtr profile, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_delete_pipeline_profile(IntPtr profile);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_create_config([MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_delete_config(IntPtr config);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_config_enable_stream(IntPtr config, Stream stream, int index, int width, int height, Format format, int framerate, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_config_enable_all_stream(IntPtr config, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_config_enable_device(IntPtr config, [MarshalAs(UnmanagedType.LPStr)] string serial, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_config_enable_device_from_file(IntPtr config, [MarshalAs(UnmanagedType.LPStr)] string file, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_config_enable_device_from_file_repeat_option(IntPtr config, [MarshalAs(UnmanagedType.LPStr)] string file, int repeat_playback, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_config_enable_record_to_file(IntPtr config, [MarshalAs(UnmanagedType.LPStr)] string file, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_config_disable_stream(IntPtr config, int stream, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_config_disable_indexed_stream(IntPtr config, Stream stream, int index, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void rs2_config_disable_all_streams(IntPtr config, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_config_resolve(IntPtr config, IntPtr pipe, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int rs2_config_can_resolve(IntPtr config, IntPtr pipe, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);
        #endregion
        #region rs_net_device
        [DllImport(dllNetName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_create_net_device(int api_version, string address, [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ErrorMarshaler))] out object error);
        #endregion


        #region Error Handling
        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_get_failed_function(IntPtr error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_get_failed_args(IntPtr error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_get_error_message(IntPtr error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern ExceptionType rs2_get_librealsense_exception_type(IntPtr error);

        [DllImport(dllName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr rs2_free_error(IntPtr error);
        #endregion

    }
}

#pragma warning restore SA1600 // Elements must be documented
#pragma warning restore SA1124 // Do not use regions
