// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015 RealSense, Inc. All Rights Reserved.

#if (_MSC_FULL_VER < 180031101)
#error At least Visual Studio 2013 Update 4 is required to compile this backend
#endif

#include <ntverp.h>
#if VER_PRODUCTBUILD <= 9600    // (WinSDK 8.1)
#ifdef ENFORCE_METADATA
#error( "Librealsense Error!: Featuring UVC Metadata requires WinSDK 10.0.10586.0. \
 Install the required toolset to proceed. Alternatively, uncheck ENFORCE_METADATA option in CMake GUI tool")
#else
#pragma message ( "\nLibrealsense notification: Featuring UVC Metadata requires WinSDK 10.0.10586.0 toolset. \
The library will be compiled without the metadata support!\n")
#endif // ENFORCE_METADATA
#else
#define METADATA_SUPPORT
#endif      // (WinSDK 8.1)

#ifndef NOMINMAX
#define NOMINMAX
#endif

#define DEVICE_ID_MAX_SIZE 256

#include "mf-uvc.h"
#include "../types.h"
#include "uvc/uvc-types.h"
#include <src/backend.h>  // monotonic_to_realtime

#include <rsutils/string/from.h>
#include <rsutils/type/fourcc.h>
using rsutils::type::fourcc;

#include "Shlwapi.h"
#include <Windows.h>
#include <limits>
#include "mfapi.h"
#include <vidcap.h>
#include <ksmedia.h>    // Metadata Extension
#include <Mferror.h>

#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

#define type_guid  MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID
#define did_guid  MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK

#define DEVICE_NOT_READY_ERROR _HRESULT_TYPEDEF_(0x80070015L)
#define MF_E_SHUTDOWN_ERROR _HRESULT_TYPEDEF_(0xC00D3E85)
#define SEMAPHORE_TIMEOUT_ERROR _HRESULT_TYPEDEF_(0x80070079L)

#define MAX_PINS 5

namespace librealsense
{
    namespace platform
    {
#ifdef METADATA_SUPPORT

#pragma pack(push, 1)
            struct ms_proprietary_md_blob
            {
                // These fields are identical in layout and content with the standard UVC header
                uint32_t        timestamp;
                uint8_t         source_clock[6];
                // MS internal
                uint8_t         reserved[6];
            };

            struct ms_metadata_header
            {
                KSCAMERA_METADATA_ITEMHEADER    ms_header;
                ms_proprietary_md_blob          ms_blobs[2]; // The blobs content is identical
            };
#pragma pack(pop)

            constexpr uint8_t ms_header_size = sizeof(ms_metadata_header);

            bool try_read_metadata(IMFSample *pSample, uint8_t& metadata_size, uint8_t ** bytes)
            {
                CComPtr<IUnknown>       spUnknown;
                CComPtr<IMFAttributes>  spSample;
                HRESULT hr = S_OK;

                CHECK_HR(hr = pSample->QueryInterface(IID_PPV_ARGS(&spSample)));
                LOG_HR(hr = spSample->GetUnknown(MFSampleExtension_CaptureMetadata, IID_PPV_ARGS(&spUnknown)));

                if (SUCCEEDED(hr))
                {
                    CComPtr<IMFAttributes>          spMetadata;
                    CComPtr<IMFMediaBuffer>         spBuffer;
                    PKSCAMERA_METADATA_ITEMHEADER   pMetadata = nullptr;
                    DWORD                           dwMaxLength = 0;
                    DWORD                           dwCurrentLength = 0;

                    CHECK_HR(hr = spUnknown->QueryInterface(IID_PPV_ARGS(&spMetadata)));
                    CHECK_HR(hr = spMetadata->GetUnknown(MF_CAPTURE_METADATA_FRAME_RAWSTREAM, IID_PPV_ARGS(&spBuffer)));
                    CHECK_HR(hr = spBuffer->Lock((BYTE**)&pMetadata, &dwMaxLength, &dwCurrentLength));

                    if (nullptr == pMetadata) // Bail, no data.
                        return false;

                    if (pMetadata->MetadataId != MetadataId_UsbVideoHeader) // Wrong metadata type, bail.
                        return false;

                    // Microsoft converts the standard UVC (12-byte) header into MS proprietary 40-bytes struct
                    // Therefore we revert it to the original structure for uniform handling
                    static const uint8_t md_lenth_max = 0xff;
                    auto md_raw = reinterpret_cast<uint8_t *>(pMetadata);
                    ms_metadata_header *ms_hdr = reinterpret_cast<ms_metadata_header*>(md_raw);
                    uvc_header *uvc_hdr = reinterpret_cast<uvc_header*>(md_raw + ms_header_size - uvc_header_size);
                    try
                    {        // restore the original timestamp and source clock fields
                        memcpy(&(uvc_hdr->timestamp), &ms_hdr->ms_blobs[0], 10);
                    }
                    catch (...)
                    {
                        return false;
                    }

                    // Metadata for Bulk endpoints is limited to 255 bytes by design
                    auto payload_length = ms_hdr->ms_header.Size - ms_header_size;
                    if ((int)payload_length > (md_lenth_max - uvc_header_size))
                    {
                        LOG_WARNING("Invalid metadata payload, length"
                            << payload_length << ", expected [0-" << int(md_lenth_max - uvc_header_size) << "]");
                        return false;
                    }
                    uvc_hdr->length = static_cast<uint8_t>(payload_length);
                    uvc_hdr->info = 0x0; // TODO - currently not available
                    metadata_size = static_cast<uint8_t>(uvc_hdr->length + uvc_header_size);

                    *bytes = (uint8_t *)uvc_hdr;

                    return true;
                }
                else
                    return false;
            }
#endif // METADATA_SUPPORT

        STDMETHODIMP source_reader_callback::QueryInterface(REFIID iid, void** ppv)
        {
#pragma warning( push )
#pragma warning(disable : 4838)
            static const QITAB qit[] =
            {
                QITABENT(source_reader_callback, IMFSourceReaderCallback),
                { nullptr },
            };
            return QISearch(this, qit, iid, ppv);
#pragma warning( pop )
        };

        STDMETHODIMP_(ULONG) source_reader_callback::AddRef() { return InterlockedIncrement(&_refCount); }

        STDMETHODIMP_(ULONG) source_reader_callback::Release()  {
            ULONG count = InterlockedDecrement(&_refCount);
            if (count <= 0)
            {
                delete this;
            }
            return count;
        }


        STDMETHODIMP source_reader_callback::OnReadSample(HRESULT hrStatus,
            DWORD dwStreamIndex,
            DWORD dwStreamFlags,
            LONGLONG llTimestamp,
            IMFSample *sample)
        {
            auto owner = _owner.lock();
            if (owner && owner->_reader)
            {
                if (FAILED(hrStatus))
                {
                    owner->_readsample_result = hrStatus;
                    if (dwStreamFlags == MF_SOURCE_READERF_ERROR)
                    {
                        owner->close_all();
                        return S_OK;
                    }
                }
                owner->_has_started.set();

                LOG_HR(owner->_reader->ReadSample(dwStreamIndex, 0, nullptr, nullptr, nullptr, nullptr));

                if (!owner->_is_started)
                    return S_OK;

                if (sample)
                {
                    CComPtr<IMFMediaBuffer> buffer = nullptr;
                    if (SUCCEEDED(sample->GetBufferByIndex(0, &buffer)))
                    {
                        uint8_t * byte_buffer=nullptr;
                        DWORD max_length{}, current_length{};
                        if (SUCCEEDED(buffer->Lock(&byte_buffer, &max_length, &current_length)))
                        {
                            uint8_t * metadata = nullptr;
                            uint8_t metadata_size = 0;
#ifdef METADATA_SUPPORT
                            try_read_metadata(sample, metadata_size, &metadata);
#endif
                            try
                            {
                                auto& stream = owner->_streams[dwStreamIndex];
                                std::lock_guard<std::mutex> lock(owner->_streams_mutex);
                                auto profile = stream.profile;
                                frame_object f{ current_length, metadata_size, byte_buffer, metadata, monotonic_to_realtime(llTimestamp * 0.0001) };

                                auto continuation = [buffer, this]()
                                {
                                    buffer->Unlock();
                                };

                                stream.callback(profile, f, continuation);
                            }
                            catch (...)
                            {
                                // TODO: log
                            }
                        }
                    }
                }
            }

            return S_OK;
        };
        STDMETHODIMP source_reader_callback::OnEvent(DWORD /*sidx*/, IMFMediaEvent* /*event*/) { return S_OK; }
        STDMETHODIMP source_reader_callback::OnFlush(DWORD)
        {
            auto owner = _owner.lock();
            if (owner)
            {
                owner->_is_flushed.set();
            }
            return S_OK;
        }

        bool wmf_uvc_device::is_connected(const uvc_device_info& info)
        {
            auto result = false;
            foreach_uvc_device([&result, &info](const uvc_device_info& i, IMFActivate*)
            {
                if (i == info) result = true;
            });
            return result;
        }

        IKsControl* wmf_uvc_device::get_ks_control(const extension_unit & xu) const
        {
            auto it = _ks_controls.find(xu.node);
            if (it != std::end(_ks_controls)) return it->second;
            throw std::runtime_error("Extension control must be initialized before use!");
        }

        void wmf_uvc_device::init_xu(const extension_unit& xu)
        {
            if (!_source)
                throw std::runtime_error("Could not initialize extensions controls!");

            // Attempt to retrieve IKsControl
            CComPtr<IKsTopologyInfo> ks_topology_info = nullptr;
            CHECK_HR(_source->QueryInterface(__uuidof(IKsTopologyInfo),
                reinterpret_cast<void **>(&ks_topology_info)));

            DWORD nNodes=0;
            LOG_HR_STR("get_NumNodes", ks_topology_info->get_NumNodes(&nNodes));

            CComPtr<IUnknown> unknown = nullptr;
            CHECK_HR(ks_topology_info->CreateNodeInstance(xu.node, IID_IUnknown,
                reinterpret_cast<LPVOID *>(&unknown)));

            CComPtr<IKsControl> ks_control = nullptr;
            CHECK_HR(unknown->QueryInterface(__uuidof(IKsControl),
                reinterpret_cast<void **>(&ks_control)));
            _ks_controls[xu.node] = ks_control;
        }

        bool wmf_uvc_device::set_xu(const extension_unit& xu, uint8_t ctrl, const uint8_t* data, int len)
        {
            auto ks_control = get_ks_control(xu);

            KSP_NODE node;
            memset(&node, 0, sizeof(KSP_NODE));
            node.Property.Set = reinterpret_cast<const GUID &>(xu.id);
            node.Property.Id = ctrl;
            node.Property.Flags = KSPROPERTY_TYPE_SET | KSPROPERTY_TYPE_TOPOLOGY;
            node.NodeId = xu.node;

            ULONG bytes_received = 0;
            auto hr = ks_control->KsProperty(reinterpret_cast<PKSPROPERTY>(&node),
                sizeof(KSP_NODE), (void*)data, len, &bytes_received);

            if (hr == DEVICE_NOT_READY_ERROR)
                return false;

            CHECK_HR(hr);
            return true;
        }

        bool wmf_uvc_device::get_xu(const extension_unit& xu, uint8_t ctrl, uint8_t* data, int len) const
        {
            auto ks_control = get_ks_control(xu);

            KSP_NODE node;
            memset(&node, 0, sizeof(KSP_NODE));
            node.Property.Set = reinterpret_cast<const GUID &>(xu.id);
            node.Property.Id = ctrl;
            node.Property.Flags = KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_TOPOLOGY;
            node.NodeId = xu.node;

            ULONG bytes_received = 0;
            auto hr = ks_control->KsProperty(reinterpret_cast<PKSPROPERTY>(&node),
                sizeof(node), data, len, &bytes_received);

            if (hr == DEVICE_NOT_READY_ERROR)
                return false;
            CHECK_HR( hr );

            if (bytes_received != len)
                throw std::runtime_error( rsutils::string::from() << "Get XU n:" << (int)ctrl << " received "
                                                                  << bytes_received << "/" << len << " bytes" );

            return true;
        }

        void ReadFromBuffer(control_range& cfg, BYTE* buffer, int length)
        {
            BYTE* next_struct = buffer;

            PKSPROPERTY_DESCRIPTION pDesc = reinterpret_cast<PKSPROPERTY_DESCRIPTION>(next_struct);
            next_struct += sizeof(KSPROPERTY_DESCRIPTION);

            if (pDesc->MembersListCount < 1)
                throw std::exception("no data ksprop");

            PKSPROPERTY_MEMBERSHEADER pHeader = reinterpret_cast<PKSPROPERTY_MEMBERSHEADER>(next_struct);
            next_struct += sizeof(KSPROPERTY_MEMBERSHEADER);

            if (pHeader->MembersCount < 1)
                throw std::exception("no data ksprop");

            // The data fields are up to four bytes
            auto field_width = std::min(sizeof(uint32_t), (size_t)length);
            auto option_range_size = std::max(sizeof(uint32_t), (size_t)length);
            switch (pHeader->MembersFlags)
            {
                /* member flag is not set correctly in current IvCam Implementation */
            case KSPROPERTY_MEMBER_RANGES:
            case KSPROPERTY_MEMBER_STEPPEDRANGES:
            {
                if (pDesc->DescriptionSize < sizeof(KSPROPERTY_DESCRIPTION) + sizeof(KSPROPERTY_MEMBERSHEADER) + 3 * sizeof(UCHAR))
                {
                    throw std::exception("no data ksprop");
                }

                auto pStruct = next_struct;
                cfg.step.resize(option_range_size);
                std::memcpy( cfg.step.data(), pStruct, field_width );
                pStruct += length;
                cfg.min.resize(option_range_size);
                std::memcpy( cfg.min.data(), pStruct, field_width );
                pStruct += length;
                cfg.max.resize(option_range_size);
                std::memcpy( cfg.max.data(), pStruct, field_width );
                return;
            }
            case KSPROPERTY_MEMBER_VALUES:
            {
                /*
                *   we don't yet support reading a list of values, only min-max.
                *   so we only support reading default value from a list
                */

                if (pHeader->Flags == KSPROPERTY_MEMBER_FLAG_DEFAULT && pHeader->MembersCount == 1)
                {
                    if (pDesc->DescriptionSize < sizeof(KSPROPERTY_DESCRIPTION) + sizeof(KSPROPERTY_MEMBERSHEADER) + sizeof(UCHAR))
                    {
                        throw std::exception("no data ksprop");
                    }

                    cfg.def.resize(option_range_size);
                    std::memcpy( cfg.def.data(), next_struct, field_width );
                }
                return;
            }
            default:
                throw  std::exception("unsupported");
            }
        }

        control_range wmf_uvc_device::get_xu_range(const extension_unit& xu, uint8_t ctrl, int len) const
        {
            auto ks_control = get_ks_control(xu);

            /* get step, min and max values*/
            KSP_NODE node;
            memset(&node, 0, sizeof(KSP_NODE));
            node.Property.Set = reinterpret_cast<const GUID &>(xu.id);
            node.Property.Id = ctrl;
            node.Property.Flags = KSPROPERTY_TYPE_BASICSUPPORT | KSPROPERTY_TYPE_TOPOLOGY;
            node.NodeId = xu.node;

            KSPROPERTY_DESCRIPTION description;
            unsigned long bytes_received = 0;
            CHECK_HR(ks_control->KsProperty(
                reinterpret_cast<PKSPROPERTY>(&node),
                sizeof(node),
                &description,
                sizeof(KSPROPERTY_DESCRIPTION),
                &bytes_received));

            auto size = description.DescriptionSize;
            std::vector<BYTE> buffer(static_cast<long>(size));

            CHECK_HR(ks_control->KsProperty(
                reinterpret_cast<PKSPROPERTY>(&node),
                sizeof(node),
                buffer.data(),
                size,
                &bytes_received));

            if (bytes_received != size) { throw  std::runtime_error("wrong data"); }

            control_range result{};
            ReadFromBuffer(result, buffer.data(), len);

            /* get def value*/
            memset(&node, 0, sizeof(KSP_NODE));
            node.Property.Set = reinterpret_cast<const GUID &>(xu.id);
            node.Property.Id = ctrl;
            node.Property.Flags = KSPROPERTY_TYPE_DEFAULTVALUES | KSPROPERTY_TYPE_TOPOLOGY;
            node.NodeId = xu.node;

            bytes_received = 0;
            CHECK_HR(ks_control->KsProperty(
                reinterpret_cast<PKSPROPERTY>(&node),
                sizeof(node),
                &description,
                sizeof(KSPROPERTY_DESCRIPTION),
                &bytes_received));

            size = description.DescriptionSize;
            buffer.clear();
            buffer.resize(size);

            CHECK_HR(ks_control->KsProperty(
                reinterpret_cast<PKSPROPERTY>(&node),
                sizeof(node),
                buffer.data(),
                size,
                &bytes_received));

            if (bytes_received != size) { throw  std::runtime_error("wrong data"); }

            ReadFromBuffer(result, buffer.data(), len);

            return result;
        }

        struct pu_control { rs2_option option; long property; bool enable_auto; };
        static const pu_control pu_controls[] = {
            { RS2_OPTION_BRIGHTNESS,                    KSPROPERTY_VIDEOPROCAMP_BRIGHTNESS },
            { RS2_OPTION_CONTRAST,                      KSPROPERTY_VIDEOPROCAMP_CONTRAST },
            { RS2_OPTION_HUE,                           KSPROPERTY_VIDEOPROCAMP_HUE },
            { RS2_OPTION_SATURATION,                    KSPROPERTY_VIDEOPROCAMP_SATURATION },
            { RS2_OPTION_SHARPNESS,                     KSPROPERTY_VIDEOPROCAMP_SHARPNESS },
            { RS2_OPTION_GAMMA,                         KSPROPERTY_VIDEOPROCAMP_GAMMA },
            { RS2_OPTION_WHITE_BALANCE,                 KSPROPERTY_VIDEOPROCAMP_WHITEBALANCE },
            { RS2_OPTION_ENABLE_AUTO_WHITE_BALANCE,     KSPROPERTY_VIDEOPROCAMP_WHITEBALANCE, true },
            { RS2_OPTION_BACKLIGHT_COMPENSATION,        KSPROPERTY_VIDEOPROCAMP_BACKLIGHT_COMPENSATION },
            { RS2_OPTION_GAIN,                          KSPROPERTY_VIDEOPROCAMP_GAIN },
            { RS2_OPTION_POWER_LINE_FREQUENCY,          KSPROPERTY_VIDEOPROCAMP_POWERLINE_FREQUENCY }
        };

        // Camera Terminal controls will be handled with  PU option transport and handling mechanism
        static const pu_control ct_controls[] = {
            { RS2_OPTION_AUTO_EXPOSURE_PRIORITY,        KSPROPERTY_CAMERACONTROL_AUTO_EXPOSURE_PRIORITY },
        };

        long to_100micros(long v)
        {
            double res = pow(2.0, v);
            return static_cast<long>(res * 10000);
        }

        long from_100micros(long val)
        {
            double d = val * 0.0001;
            double l = (d != 0) ? std::log2(d) : 1;
            long v = static_cast<long>(std::roundl(l));
            // Exposure values use logarithmic scale and can reach -13 with D400
            assert(v <= 0 && v >= -15);
            return v;
        }

        bool wmf_uvc_device::get_pu(rs2_option opt, int32_t& value) const
        {
            long val = 0, flags = 0;
            if ((opt == RS2_OPTION_EXPOSURE) || (opt == RS2_OPTION_ENABLE_AUTO_EXPOSURE))
            {
                auto hr = get_camera_control()->Get(CameraControl_Exposure, &val, &flags);
                if (hr == DEVICE_NOT_READY_ERROR)
                    return false;

                value = (opt == RS2_OPTION_EXPOSURE) ? to_100micros(val) : (flags == CameraControl_Flags_Auto);
                CHECK_HR(hr);
                return true;
            }

            for (auto & pu : pu_controls)
            {
                if (opt == pu.option)
                {
                    auto hr = get_video_proc()->Get(pu.property, &val, &flags);
                    if (hr == DEVICE_NOT_READY_ERROR)
                        return false;

                    value = (pu.enable_auto) ? (flags == VideoProcAmp_Flags_Auto) : val;

                    CHECK_HR(hr);
                    return true;
                }
            }

            for (auto & ct : ct_controls)
            {
                if (opt == ct.option)
                {
                    auto hr = get_camera_control()->Get(ct.property, &val, &flags);
                    if (hr == DEVICE_NOT_READY_ERROR)
                        return false;

                    value = val;

                    CHECK_HR(hr);
                    return true;
                }
            }

            throw std::runtime_error( rsutils::string::from() << "Unsupported control - " << opt );
        }

        bool wmf_uvc_device::set_pu(rs2_option opt, int value)
        {
            if (opt == RS2_OPTION_EXPOSURE)
            {
                auto hr = get_camera_control()->Set(CameraControl_Exposure, from_100micros(value), CameraControl_Flags_Manual);
                if (hr == DEVICE_NOT_READY_ERROR)
                    return false;

                CHECK_HR(hr);
                return true;
            }
            if (opt == RS2_OPTION_ENABLE_AUTO_EXPOSURE)
            {
                if (value)
                {
                    auto hr = get_camera_control()->Set(CameraControl_Exposure, 0, CameraControl_Flags_Auto);
                    if (hr == DEVICE_NOT_READY_ERROR)
                        return false;

                    CHECK_HR(hr);
                }
                else
                {
                    long min, max, step, def, caps;
                    auto hr = get_camera_control()->GetRange(CameraControl_Exposure, &min, &max, &step, &def, &caps);
                    if (hr == DEVICE_NOT_READY_ERROR)
                        return false;

                    CHECK_HR(hr);

                    hr = get_camera_control()->Set(CameraControl_Exposure, def, CameraControl_Flags_Manual);
                    if (hr == DEVICE_NOT_READY_ERROR)
                        return false;

                    CHECK_HR(hr);
                }
                return true;
            }


            for (auto & pu : pu_controls)
            {
                if (opt == pu.option)
                {
                    if (pu.enable_auto)
                    {
                        if (value)
                        {
                            auto hr = get_video_proc()->Set(pu.property, 0, VideoProcAmp_Flags_Auto);
                            if (hr == DEVICE_NOT_READY_ERROR)
                                return false;

                            CHECK_HR(hr);
                        }
                        else
                        {
                            long min, max, step, def, caps;
                            auto hr = get_video_proc()->GetRange(pu.property, &min, &max, &step, &def, &caps);
                            if (hr == DEVICE_NOT_READY_ERROR)
                                return false;

                            CHECK_HR(hr);

                            hr = get_video_proc()->Set(pu.property, def, VideoProcAmp_Flags_Manual);
                            if (hr == DEVICE_NOT_READY_ERROR)
                                return false;

                            CHECK_HR(hr);
                        }
                    }
                    else
                    {
                        auto hr = get_video_proc()->Set(pu.property, value, VideoProcAmp_Flags_Manual);

                        // We found 2 cases when we want to return false and let the backend retry mechanism call another set command.
                        // DEVICE_NOT_READY_ERROR: Can be return if the device is busy, not a real error.
                        // SEMAPHORE_TIMEOUT_ERROR: We get this error at a very low statistics when setting multiple PU commands (i.e. gain command)
                        // It is not expected but we decided to raise a log_debug and allow a retry on that case [DSO-17181].
                        if( hr == DEVICE_NOT_READY_ERROR || hr == SEMAPHORE_TIMEOUT_ERROR )
                        {
                            if( hr == SEMAPHORE_TIMEOUT_ERROR )
                                LOG_DEBUG( "set_pu returned error code: "
                                           << rsutils::hresult::hr_to_string( hr ) );
                            return false;
                        }

                        CHECK_HR(hr);
                    }
                    return true;
                }
            }
            for (auto & ct : ct_controls)
            {
                if (opt == ct.option)
                {
                    if (ct.enable_auto)
                    {
                        if (value)
                        {
                            auto hr = get_camera_control()->Set(ct.property, 0, CameraControl_Flags_Auto);
                            if (hr == DEVICE_NOT_READY_ERROR)
                                return false;

                            CHECK_HR(hr);
                        }
                        else
                        {
                            long min, max, step, def, caps;
                            auto hr = get_camera_control()->GetRange(ct.property, &min, &max, &step, &def, &caps);
                            if (hr == DEVICE_NOT_READY_ERROR)
                                return false;

                            CHECK_HR(hr);

                            hr = get_camera_control()->Set(ct.property, def, CameraControl_Flags_Manual);
                            if (hr == DEVICE_NOT_READY_ERROR)
                                return false;

                            CHECK_HR(hr);
                        }
                    }
                    else
                    {
                        auto hr = get_camera_control()->Set(ct.property, value, CameraControl_Flags_Manual);
                        if (hr == DEVICE_NOT_READY_ERROR)
                            return false;

                        CHECK_HR(hr);
                    }
                    return true;
                }
            }
            throw std::runtime_error( rsutils::string::from() << "Unsupported control - " << opt );
        }

        control_range wmf_uvc_device::get_pu_range(rs2_option opt) const
        {
            if (opt == RS2_OPTION_ENABLE_AUTO_EXPOSURE ||
                opt == RS2_OPTION_ENABLE_AUTO_WHITE_BALANCE)
            {
                static const int32_t min = 0, max = 1, step = 1, def = 1;
                control_range result(min, max, step, def);
                return result;
            }

            long minVal = 0, maxVal = 0, steppingDelta = 0, defVal = 0, capsFlag = 0;
            if (opt == RS2_OPTION_EXPOSURE)
            {
                CHECK_HR(get_camera_control()->GetRange(CameraControl_Exposure, &minVal, &maxVal, &steppingDelta, &defVal, &capsFlag));
                long min = to_100micros(minVal), max = to_100micros(maxVal), def = to_100micros(defVal);
                control_range result(min, max, min, def);
                return result;
            }
            for (auto & pu : pu_controls)
            {
                if (opt == pu.option)
                {
                    CHECK_HR(get_video_proc()->GetRange(pu.property, &minVal, &maxVal, &steppingDelta, &defVal, &capsFlag));
                    control_range result(minVal, maxVal, steppingDelta, defVal);
                    return result;
                }
            }
            for (auto & ct : ct_controls)
            {
                if (opt == ct.option)
                {
                    CHECK_HR(get_camera_control()->GetRange(ct.property, &minVal, &maxVal, &steppingDelta, &defVal, &capsFlag));
                    control_range result(minVal, maxVal, steppingDelta, defVal);
                    return result;
                }
            }
            throw std::runtime_error("unsupported control");
        }

        void wmf_uvc_device::foreach_uvc_device(enumeration_callback action)
        {
            for (auto attributes_params_set : attributes_params)
            {
                CComPtr<IMFAttributes> pAttributes = nullptr;
                CHECK_HR(MFCreateAttributes(&pAttributes, 1));
                for (auto attribute_params : attributes_params_set)
                {
                    CHECK_HR(pAttributes->SetGUID(attribute_params.first, attribute_params.second));
                }

                IMFActivate ** ppDevices;
                UINT32 numDevices;
                CHECK_HR(MFEnumDeviceSources(pAttributes, &ppDevices, &numDevices));

                for (UINT32 i = 0; i < numDevices; ++i)
                {
                    CComPtr<IMFActivate> pDevice;
                    *&pDevice = ppDevices[i];

                    WCHAR * wchar_name = nullptr; UINT32 length;
                    CHECK_HR(pDevice->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK, &wchar_name, &length));
                    auto name = rsutils::string::windows::win_to_utf(wchar_name);
                    CoTaskMemFree(wchar_name);

                    uint16_t vid, pid, mi; std::string unique_id, guid;
                    if (!parse_usb_path_multiple_interface(vid, pid, mi, unique_id, name, guid)) continue;

                    uvc_device_info info;
                    info.vid = vid;
                    info.pid = pid;
                    info.unique_id = unique_id;
                    info.mi = mi;
                    info.device_path = name;
                    try
                    {
                        action(info, ppDevices[i]);
                    }
                    catch (...)
                    {
                        // TODO
                    }
                }
                safe_release(pAttributes);
                CoTaskMemFree(ppDevices);
            }
        }

        void wmf_uvc_device::set_power_state(power_state state)
        {
            if (state == _power_state)
                return;

            switch (state)
            {
            case D0: set_d0(); break;
            case D3: set_d3(); break;
            default:
                throw std::runtime_error("illegal power state request");
            }
        }

        wmf_uvc_device::wmf_uvc_device(const uvc_device_info& info,
            std::shared_ptr<const wmf_backend> backend)
            : _streamIndex(MAX_PINS), _info(info), _is_flushed(), _has_started(), _backend(std::move(backend)),
            _systemwide_lock(info.unique_id.c_str(), WAIT_FOR_MUTEX_TIME_OUT),
            _location(""), _device_usb_spec(usb3_type)
        {
            if (!is_connected(info))
            {
                throw std::runtime_error("Camera not connected!");
            }
            try
            {
                if (!get_usb_descriptors(info.vid, info.pid, info.unique_id, _location, _device_usb_spec, _device_serial))
                {
                    LOG_WARNING("Could not retrieve USB descriptor for device " << std::hex << info.vid << ":"
                        << info.pid << " , id:" << info.unique_id << std::dec);
                }
            }
            catch (...)
            {
                LOG_WARNING("Accessing USB info failed for " << std::hex << info.vid << ":"
                    << info.pid << " , id:" << info.unique_id << std::dec);
            }
            foreach_uvc_device([this](const uvc_device_info& i, IMFActivate* device)
            {
                if (i == _info && device)
                {
                    _device_id.resize(DEVICE_ID_MAX_SIZE);
                    CHECK_HR(device->GetString(did_guid, const_cast<LPWSTR>(_device_id.c_str()), UINT32(_device_id.size()), nullptr));
                }
            });
        }

        wmf_uvc_device::~wmf_uvc_device()
        {
            try {
                if (_streaming)
                {
                    flush(MF_SOURCE_READER_ALL_STREAMS);
                }

                set_power_state(D3);

                safe_release(_device_attrs);
                safe_release(_reader_attrs);
                for (auto&& c : _ks_controls)
                    safe_release(c.second);
                _ks_controls.clear();
            }
            catch (...)
            {
                LOG_WARNING("Exception thrown while flushing MF source");
            }
        }

        CComPtr<IMFAttributes> wmf_uvc_device::create_device_attrs()
        {
            CComPtr<IMFAttributes> device_attrs = nullptr;

            CHECK_HR(MFCreateAttributes(&device_attrs, 2));
            CHECK_HR(device_attrs->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, type_guid));
            CHECK_HR(device_attrs->SetString(did_guid, _device_id.c_str()));
            return device_attrs;
        }

        CComPtr<IMFAttributes> wmf_uvc_device::create_reader_attrs()
        {
            CComPtr<IMFAttributes> reader_attrs = nullptr;

            CHECK_HR(MFCreateAttributes(&reader_attrs, 3));
            CHECK_HR(reader_attrs->SetUINT32(MF_SOURCE_READER_DISCONNECT_MEDIASOURCE_ON_SHUTDOWN, FALSE));
            CHECK_HR(reader_attrs->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE));
            CHECK_HR(reader_attrs->SetUnknown(MF_SOURCE_READER_ASYNC_CALLBACK,
                static_cast<IUnknown*>(new source_reader_callback(shared_from_this()))));
            return reader_attrs;
        }

        void wmf_uvc_device::set_d0()
        {
            if (!_device_attrs)
                _device_attrs = create_device_attrs();

            if (!_reader_attrs)
                _reader_attrs = create_reader_attrs();
            _streams.resize(_streamIndex);

            // Release any stale COM pointers from a previously failed set_d0() or set_d3()
            safe_release(_camera_control);
            safe_release(_video_proc);
            safe_release(_reader);
            if (_source)
            {
                _source->Shutdown();
                safe_release(_source);
            }

            //enable source
            CHECK_HR(MFCreateDeviceSource(_device_attrs, &_source));
            LOG_HR(_source->QueryInterface(__uuidof(IAMCameraControl), reinterpret_cast<void **>(&_camera_control)));
            // The IAMVideoProcAmp interface adjusts the qualities of an incoming video signal, such as brightness,
            // contrast, hue, saturation, gamma, and sharpness.
            auto hr = _source->QueryInterface( __uuidof( IAMVideoProcAmp ), reinterpret_cast< void ** >( &_video_proc ) );
            // E_NOINTERFACE is expected... especially when no video camera
            if( hr != E_NOINTERFACE )
                LOG_HR_STR( "QueryInterface(IAMVideoProcAmp)", hr );

            //enable reader
            CHECK_HR(MFCreateSourceReaderFromMediaSource(_source, _reader_attrs, &_reader));
            CHECK_HR(_reader->SetStreamSelection(static_cast<DWORD>(MF_SOURCE_READER_ALL_STREAMS), TRUE));
            _power_state = D0;
        }

        void wmf_uvc_device::set_d3()
        {
            safe_release(_camera_control);
            safe_release(_video_proc);
            safe_release(_reader);
            if (_source)
            {
                _source->Shutdown();
                safe_release(_source);
            }
            for (auto& elem : _streams)
                elem.callback = nullptr;
            _power_state = D3;
        }

        void wmf_uvc_device::foreach_profile(std::function<void(const mf_profile& profile, CComPtr<IMFMediaType> media_type, bool& quit)> action) const
        {
            bool quit = false;
            CComPtr<IMFMediaType> pMediaType = nullptr;
            for (unsigned int sIndex = 0; sIndex < _streams.size(); ++sIndex)
            {
                for (auto k = 0;; k++)
                {
                    auto hr = _reader->GetNativeMediaType(sIndex, k, &pMediaType.p);
                    if (FAILED(hr) || pMediaType == nullptr)
                    {
                        safe_release(pMediaType);
                        if (hr != MF_E_NO_MORE_TYPES) // An object ran out of media types to suggest therefore the requested chain of streaming objects cannot be completed
                            LOG_HR_STR("_reader->GetNativeMediaType(sIndex, k, &pMediaType.p)",hr);

                        break;
                    }

                    GUID subtype;
                    CHECK_HR(pMediaType->GetGUID(MF_MT_SUBTYPE, &subtype));

                    unsigned width = 0;
                    unsigned height = 0;

                    CHECK_HR(MFGetAttributeSize(pMediaType, MF_MT_FRAME_SIZE, &width, &height));

                    frame_rate frameRateMin;
                    frame_rate frameRateMax;

                    CHECK_HR(MFGetAttributeRatio(pMediaType, MF_MT_FRAME_RATE_RANGE_MIN, &frameRateMin.numerator, &frameRateMin.denominator));
                    CHECK_HR(MFGetAttributeRatio(pMediaType, MF_MT_FRAME_RATE_RANGE_MAX, &frameRateMax.numerator, &frameRateMax.denominator));

                    if (static_cast<float>(frameRateMax.numerator) / frameRateMax.denominator <
                        static_cast<float>(frameRateMin.numerator) / frameRateMin.denominator)
                    {
                        std::swap(frameRateMax, frameRateMin);
                    }
                    int currFps = frameRateMax.numerator / frameRateMax.denominator;

                    uint32_t device_fourcc = reinterpret_cast<const big_endian<uint32_t> &>(subtype.Data1);

                    // On D585S, we need to distinguish the occupancy and the label point cloud streams.
                    // The condition currently support 2 resolutions for LPC
                    // This needs to be refactored!
                    if (this->_info.pid == 0x0b6b && width == 2880 && (height == 1040 || height == 260)) 
                    {
                        device_fourcc = 0x50414C38; // PAL8 used instead of FGREY in order to distinguish  between occupancy and point cloud streams
                    }

                    if (fourcc_map.count(device_fourcc))
                        device_fourcc = fourcc_map.at(device_fourcc);
                    
                    stream_profile sp;
                    sp.width = width;
                    sp.height = height;
                    sp.fps = currFps;
                    sp.format = device_fourcc;

                    mf_profile mfp;
                    mfp.index = sIndex;
                    mfp.min_rate = frameRateMin;
                    mfp.max_rate = frameRateMax;
                    mfp.profile = sp;

                    action(mfp, pMediaType, quit);

                    safe_release(pMediaType);

                    if (quit)
                        return;
                }
            }
        }

        std::vector<stream_profile> wmf_uvc_device::get_profiles() const
        {
            check_connection();

            if (get_power_state() != D0)
                throw std::runtime_error("Device must be powered to query supported profiles!");

            std::vector<stream_profile> results;
            foreach_profile(
                [&results]( const mf_profile & mfp, CComPtr< IMFMediaType > media_type, bool & quit )
                {
                    //LOG_DEBUG( mfp.profile.width << 'x' << mfp.profile.height << ' ' << fourcc( mfp.profile.format )
                    //                             << " @ " << mfp.profile.fps << " Hz" );
                    results.push_back( mfp.profile );
                } );

            return results;
        }

        void wmf_uvc_device::play_profile(stream_profile profile, frame_callback callback)
        {
            bool profile_found = false;
            foreach_profile([this, profile, callback, &profile_found](const mf_profile& mfp, CComPtr<IMFMediaType> media_type, bool& quit)
            {
                if (mfp.profile.format != profile.format &&
                    (fourcc_map.count(mfp.profile.format) == 0 ||
                        profile.format != fourcc_map.at(mfp.profile.format)))
                    return;

                if ((mfp.profile.width == profile.width) && (mfp.profile.height == profile.height))
                {
                    if (mfp.max_rate.denominator && mfp.min_rate.denominator)
                    {
                        if (mfp.profile.fps == int(profile.fps))
                        {
                            auto hr = _reader->SetCurrentMediaType(mfp.index, nullptr, media_type);
                            if (SUCCEEDED(hr) && media_type)
                            {
                                for (unsigned int i = 0; i < _streams.size(); ++i)
                                {
                                    if (mfp.index == i || (_streams[i].callback))
                                        continue;

                                    _reader->SetStreamSelection(i, FALSE);
                                }

                                CHECK_HR(_reader->SetStreamSelection(mfp.index, TRUE));

                                {
                                    std::lock_guard<std::mutex> lock(_streams_mutex);
                                    if (_streams[mfp.index].callback)
                                        throw std::runtime_error("Camera already streaming via this stream index!");

                                    _streams[mfp.index].profile = profile;
                                    _streams[mfp.index].callback = callback;
                                }

                                _readsample_result = S_OK;
                                CHECK_HR(_reader->ReadSample(mfp.index, 0, nullptr, nullptr, nullptr, nullptr));

                                const auto timeout_ms = RS2_DEFAULT_TIMEOUT;
                                if (_has_started.wait(timeout_ms))
                                {
                                    LOG_HR_STR("_reader->ReadSample(...)", _readsample_result);
                                }
                                else
                                {
                                    LOG_WARNING("First frame took more then " << timeout_ms << "ms to arrive!");
                                }
                                profile_found = true;
                                quit = true;
                                return;
                            }
                            else
                            {
                                throw std::runtime_error("Could not set Media Type. Device may be locked");
                            }
                        }
                    }
                }
            });
            if (!profile_found)
                throw std::runtime_error("Stream profile not found!");
        }

        void wmf_uvc_device::probe_and_commit(stream_profile profile, frame_callback callback, int /*buffers*/)
        {
            if (_streaming)
                throw std::runtime_error("Device is already streaming!");

            _profiles.push_back(profile);
            _frame_callbacks.push_back(callback);
        }

        IAMVideoProcAmp* wmf_uvc_device::get_video_proc() const
        {
            if (get_power_state() != D0)
                throw std::runtime_error("Device must be powered to query video_proc!");
            if (!_video_proc.p)
                throw std::runtime_error("The device does not support adjusting the qualities of an incoming video signal, such as brightness, contrast, hue, saturation, gamma, and sharpness.");
            return _video_proc.p;
        }

        IAMCameraControl* wmf_uvc_device::get_camera_control() const
        {
            if (get_power_state() != D0)
                throw std::runtime_error("Device must be powered to query camera_control!");
            if (!_camera_control.p)
                throw std::runtime_error("The device does not support camera settings such as zoom, pan, aperture adjustment, or shutter speed.");
            return _camera_control.p;
        }

        void wmf_uvc_device::stream_on(std::function<void(const notification& n)> error_handler)
        {
            if (_profiles.empty())
                throw std::runtime_error("Stream not configured");

            if (_streaming)
                throw std::runtime_error("Device is already streaming!");

            check_connection();

            try
            {
                for (uint32_t i = 0; i < _profiles.size(); ++i)
                {
                    play_profile(_profiles[i], _frame_callbacks[i]);
                }

                _streaming = true;
            }
            catch (...)
            {
                close_all();

                throw;
            }
        }

        void wmf_uvc_device::start_callbacks()
        {
            _is_started = true;
        }

        void wmf_uvc_device::stop_callbacks()
        {
            _is_started = false;
        }

        void wmf_uvc_device::stop_stream_cleanup(const stream_profile& profile, std::vector<profile_and_callback>::iterator& elem)
        {
            if (elem != _streams.end())
            {
                elem->callback = nullptr;
                elem->profile.format = 0;
                elem->profile.fps = 0;
                elem->profile.width = 0;
                elem->profile.height = 0;
            }

            auto pos = std::find(_profiles.begin(), _profiles.end(), profile) - _profiles.begin();
            if (pos != _profiles.size())
            {
                _profiles.erase(_profiles.begin() + pos);
                _frame_callbacks.erase(_frame_callbacks.begin() + pos);
            }

            if (_profiles.empty())
                _streaming = false;

            _has_started.reset();
        }

        void wmf_uvc_device::close(stream_profile profile)
        {
            _is_started = false;

            check_connection();

            auto elem = std::find_if( _streams.begin(),
                                      _streams.end(),
                                      [&]( const profile_and_callback & pac )
                                      { return ( pac.profile == profile && ( pac.callback ) ); } );

            if (elem == _streams.end() && _frame_callbacks.empty())
                throw std::runtime_error("Camera is not streaming!");

            if (elem != _streams.end())
            {
                try {
                    flush(int(elem - _streams.begin()));
                }
                catch (...)
                {
                    stop_stream_cleanup(profile, elem); // TODO: move to RAII
                    throw;
                }
            }
            stop_stream_cleanup(profile, elem);
        }

        // ReSharper disable once CppMemberFunctionMayBeConst
        void wmf_uvc_device::flush(int sIndex)
        {
            if (is_connected(_info))
            {
                if (_reader != nullptr)
                {
                    auto sts = _reader->Flush(sIndex);
                    if (sts != S_OK)
                    {
                        if (sts == MF_E_HW_MFT_FAILED_START_STREAMING)
                            throw std::runtime_error("Camera already streaming");

                        throw std::runtime_error( rsutils::string::from() << "Flush failed" << sts );
                    }

                    _is_flushed.wait(INFINITE);
                }
            }
        }

        void wmf_uvc_device::check_connection() const
        {
            if (!is_connected(_info))
                throw std::runtime_error("Camera is no longer connected!");
        }

        void wmf_uvc_device::close_all()
        {
            for (auto& elem : _streams)
                if (elem.callback)
                {
                    try
                    {
                        close(elem.profile);
                    }
                    catch (...) {}
                }
                       
            _profiles.clear();
            _frame_callbacks.clear();
        }
    }
}
