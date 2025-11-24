// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015 RealSense, Inc. All Rights Reserved.
#pragma once

#include <src/platform/uvc-device.h>
#include <src/platform/uvc-device-info.h>
#include "win/win-helpers.h"

#include <mfidl.h>
#include <mfreadwrite.h>
#include <atlcomcli.h>
#include <strmif.h>
#include <Ks.h>
#include <ksproxy.h>
#include <unordered_map>
#include <mutex>
#include <atomic>

#ifndef KSCATEGORY_SENSOR_CAMERA
DEFINE_GUIDSTRUCT("24E552D7-6523-47F7-A647-D3465BF1F5CA", KSCATEGORY_SENSOR_CAMERA);
#define KSCATEGORY_SENSOR_CAMERA DEFINE_GUIDNAMED(KSCATEGORY_SENSOR_CAMERA)
#endif // !KSCATEGORY_SENSOR_CAMERA


static const std::vector<std::vector<std::pair<GUID, GUID>>> attributes_params = {
    {
        { MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID },
        { MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_CATEGORY, KSCATEGORY_SENSOR_CAMERA }
    },
    {
        { MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID }
    },
};

namespace librealsense
{
    namespace platform
    {
        class wmf_backend;

        struct profile_and_callback
        {
            stream_profile profile;
            frame_callback callback = nullptr;
        };

        typedef std::function<void(const uvc_device_info&, IMFActivate*)>
            enumeration_callback;

        typedef struct frame_rate {
            unsigned int denominator;
            unsigned int numerator;
        } frame_rate;

        typedef struct mf_profile {
            uint32_t index;
            frame_rate min_rate;
            frame_rate max_rate;
            stream_profile profile;
        } mf_profile;


        class wmf_uvc_device : public std::enable_shared_from_this<wmf_uvc_device>,
            public uvc_device
        {
        public:
            wmf_uvc_device(const uvc_device_info& info, std::shared_ptr<const wmf_backend> backend);
            ~wmf_uvc_device();

            void probe_and_commit(stream_profile profile, frame_callback callback, int buffers) override;
            void stream_on(std::function<void(const notification& n)> error_handler = [](const notification& n){}) override;
            void start_callbacks() override;
            void stop_callbacks() override;
            void close(stream_profile profile) override;
            void set_power_state(power_state state) override;
            power_state get_power_state() const override;
            std::vector<stream_profile> get_profiles() const override;

            static bool is_connected(const uvc_device_info& info);
            static void foreach_uvc_device(enumeration_callback action);

            void register_xu( platform::extension_unit && xu ) override { _xus.push_back( std::move( xu ) ); }
            bool set_xu(const extension_unit& xu, uint8_t ctrl, const uint8_t* data, int len) override;
            bool get_xu(const extension_unit& xu, uint8_t ctrl, uint8_t* data, int len) const override;
            control_range get_xu_range(const extension_unit& xu, uint8_t ctrl, int len) const override;

            bool get_pu(rs2_option opt, int32_t& value) const override;
            bool set_pu(rs2_option opt, int value) override;
            control_range get_pu_range(rs2_option opt) const override;

            void lock() const override { _systemwide_lock.lock(); }
            void unlock() const override { _systemwide_lock.unlock(); }

            bool is_platform_jetson() const override { return false;}

            std::string get_device_location() const override { return _location; }
            usb_spec get_usb_specification() const override { return _device_usb_spec; }
            IAMVideoProcAmp* get_video_proc() const;
            IAMCameraControl* get_camera_control() const;

        private:
            friend class source_reader_callback;

            void init_xu(const extension_unit& xu);
            void play_profile(stream_profile profile, frame_callback callback);
            void stop_stream_cleanup(const stream_profile& profile, std::vector<profile_and_callback>::iterator& elem);
            void flush(int sIndex);
            void check_connection() const;
            void close_all();
            IKsControl* get_ks_control(const extension_unit& xu) const;
            CComPtr<IMFAttributes> create_device_attrs();
            CComPtr<IMFAttributes> create_reader_attrs();
            void foreach_profile(std::function<void(const mf_profile& profile, CComPtr<IMFMediaType> media_type, bool& quit)> action) const;

            void set_d0();
            void set_d3();

            // Don't move the position of wmf_backend member. This object must be destroyed only after COM objects.
            std::shared_ptr<const wmf_backend>      _backend;

            const uvc_device_info                   _info;

            CComPtr<IMFSourceReader>                _reader = nullptr;
            CComPtr<IMFMediaSource>                 _source = nullptr;
            CComPtr<IMFAttributes>                  _device_attrs = nullptr;
            CComPtr<IMFAttributes>                  _reader_attrs = nullptr;

            CComPtr<IAMCameraControl>               _camera_control = nullptr;
            CComPtr<IAMVideoProcAmp>                _video_proc = nullptr;
            std::unordered_map<int, CComPtr<IKsControl>>      _ks_controls;

            auto_reset_event                        _is_flushed;
            manual_reset_event                      _has_started;
            HRESULT                                 _readsample_result = S_OK;

            uint16_t                                _streamIndex;
            std::vector<profile_and_callback>       _streams;
            std::mutex                              _streams_mutex;

            mutable std::recursive_mutex            _source_lock; // Guarding access to _source
            named_mutex                             _systemwide_lock;
            std::string                             _location;
            usb_spec                                _device_usb_spec;
            std::string                             _device_serial;
            std::vector<stream_profile>             _profiles;
            std::vector<frame_callback>             _frame_callbacks;
            bool                                    _streaming = false;
            std::atomic<bool>                       _is_started = false;
            std::wstring                            _device_id;
            std::atomic< int >                      _power_counter;
            std::vector< platform::extension_unit > _xus;
        };

        class source_reader_callback : public IMFSourceReaderCallback
        {
        public:
            explicit source_reader_callback(std::weak_ptr<wmf_uvc_device> owner) : _owner(owner)
            {
            };
            virtual ~source_reader_callback() {};
            STDMETHODIMP QueryInterface(REFIID iid, void** ppv) override;
            STDMETHODIMP_(ULONG) AddRef() override;
            STDMETHODIMP_(ULONG) Release() override;
            STDMETHODIMP OnReadSample(HRESULT /*hrStatus*/,
                DWORD dwStreamIndex,
                DWORD /*dwStreamFlags*/,
                LONGLONG /*llTimestamp*/,
                IMFSample *sample) override;
            STDMETHODIMP OnEvent(DWORD /*sidx*/, IMFMediaEvent* /*event*/) override;
            STDMETHODIMP OnFlush(DWORD) override;
        private:
            std::weak_ptr<wmf_uvc_device> _owner;
            long _refCount = 0;
        };

    }
}
