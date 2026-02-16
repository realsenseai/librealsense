// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2022 RealSense, Inc. All Rights Reserved.

#include "ds-private.h"
#include "ds-timestamp.h"
#include <src/frame.h>
#include <src/core/time-service.h>

#include <mutex>
#include <chrono>
#include <vector>
#include <iterator>

#include "device.h"
#include "image.h"
#include "metadata.h"
#include "d500/d585s-md.h"

namespace librealsense
{
    ds_timestamp_reader_from_metadata::ds_timestamp_reader_from_metadata(std::unique_ptr<frame_timestamp_reader> backup_timestamp_reader)
        :_backup_timestamp_reader(std::move(backup_timestamp_reader)), _has_metadata(pins), one_time_note(false)
    {
        reset();
    }

    bool ds_timestamp_reader_from_metadata::has_metadata(const std::shared_ptr<frame_interface>& frame)
    {
        std::lock_guard<std::recursive_mutex> lock(_mtx);

        auto f = std::dynamic_pointer_cast<librealsense::frame>(frame);
        if (!f)
        {
            LOG_ERROR("Frame is not valid. Failed to downcast to librealsense::frame.");
            return false;
        }
        auto mds = f->additional_data.metadata_size;
        if (mds)
            return true;

        return false;
    }

    rs2_time_t ds_timestamp_reader_from_metadata::get_frame_timestamp(const std::shared_ptr<frame_interface>& frame)
    {
        std::lock_guard<std::recursive_mutex> lock(_mtx);

        auto f = std::dynamic_pointer_cast<librealsense::frame>(frame);
        if (!f)
        {
            LOG_ERROR("Frame is not valid. Failed to downcast to librealsense::frame.");
            return 0;
        }
        size_t pin_index = 0;

        if (frame->get_stream()->get_format() == RS2_FORMAT_Z16)
            pin_index = 1;

        _has_metadata[pin_index] = has_metadata(frame);

        auto md = (librealsense::metadata_intel_basic*)(f->additional_data.metadata_blob.data());
        if(_has_metadata[pin_index] && md)
        {
            return (double)(md->header.timestamp)*MICROSEC_TO_MILLISEC;
        }
        else
        {
            if (!one_time_note)
            {
                LOG_INFO("UVC metadata payloads not available. Please refer to the installation chapter for details.");
                one_time_note = true;
            }
            return _backup_timestamp_reader->get_frame_timestamp(frame);
        }
    }

    unsigned long long ds_timestamp_reader_from_metadata::get_frame_counter(const std::shared_ptr<frame_interface>& frame) const
    {
        std::lock_guard<std::recursive_mutex> lock(_mtx);

        auto f = std::dynamic_pointer_cast<librealsense::frame>(frame);
        if (!f)
        {
            LOG_ERROR("Frame is not valid. Failed to downcast to librealsense::frame.");
            return 0;
        }
        size_t pin_index = 0;

        if (frame->get_stream()->get_format() == RS2_FORMAT_Z16)
            pin_index = 1;

        if(_has_metadata[pin_index] && f->additional_data.metadata_size > platform::uvc_header_size)
        {
            auto md = (librealsense::metadata_intel_basic*)(f->additional_data.metadata_blob.data());
            if (md->capture_valid())
                return md->payload.frame_counter;
        }

        return _backup_timestamp_reader->get_frame_counter(frame);
    }

    void ds_timestamp_reader_from_metadata::reset()
    {
        std::lock_guard<std::recursive_mutex> lock(_mtx);
        one_time_note = false;
        for (auto i = 0; i < pins; ++i)
        {
            _has_metadata[i] = false;
        }
        _backup_timestamp_reader->reset();
    }

    rs2_timestamp_domain ds_timestamp_reader_from_metadata::get_frame_timestamp_domain(const std::shared_ptr<frame_interface>& frame) const
    {
        std::lock_guard<std::recursive_mutex> lock(_mtx);
        auto pin_index = 0;
        if (frame->get_stream()->get_format() == RS2_FORMAT_Z16)
            pin_index = 1;

        return _has_metadata[pin_index] ? RS2_TIMESTAMP_DOMAIN_HARDWARE_CLOCK :
                                          _backup_timestamp_reader->get_frame_timestamp_domain(frame);
    }

    ds_timestamp_reader_from_metadata_depth_mapping::ds_timestamp_reader_from_metadata_depth_mapping(std::unique_ptr<frame_timestamp_reader> backup_timestamp_reader)
        : ds_timestamp_reader_from_metadata(std::move(backup_timestamp_reader)) {} 

    rs2_time_t ds_timestamp_reader_from_metadata_depth_mapping::get_frame_timestamp(const std::shared_ptr<frame_interface>& frame)
    {
        std::lock_guard<std::recursive_mutex> lock(_mtx);

        auto f = std::dynamic_pointer_cast<librealsense::frame>(frame);
        if (!f)
        {
            LOG_ERROR("Frame is not valid. Failed to downcast to librealsense::frame.");
            return 0;
        }

        _has_metadata[0] = has_metadata(frame);

        auto md = (librealsense::md_occupancy*)(f->additional_data.metadata_blob.data() + platform::uvc_header_size);
        if (_has_metadata[0] && md)
        {
            return (double)(md->frame_timestamp) * MICROSEC_TO_MILLISEC;
        }
        else
        {
            if (!one_time_note)
            {
                LOG_INFO("UVC metadata payloads not available. Please refer to the installation chapter for details.");
                one_time_note = true;
            }
            return _backup_timestamp_reader->get_frame_timestamp(frame);
        }
    }
    unsigned long long ds_timestamp_reader_from_metadata_depth_mapping::get_frame_counter(const std::shared_ptr<frame_interface>& frame) const
    {
        std::lock_guard<std::recursive_mutex> lock(_mtx);

        auto f = std::dynamic_pointer_cast<librealsense::frame>(frame);
        if (!f)
        {
            LOG_ERROR("Frame is not valid. Failed to downcast to librealsense::frame.");
            return 0;
        }

        if (_has_metadata[0] && f->additional_data.metadata_size > platform::uvc_header_size)
        {
            auto md = (librealsense::md_occupancy*)(f->additional_data.metadata_blob.data() + platform::uvc_header_size);
            return md->frame_counter;
        }

        return _backup_timestamp_reader->get_frame_counter(frame);
    }

    ds_timestamp_reader_from_metadata_safety::ds_timestamp_reader_from_metadata_safety(std::unique_ptr<frame_timestamp_reader> backup_timestamp_reader)
        : ds_timestamp_reader_from_metadata(std::move(backup_timestamp_reader)) {}

    rs2_time_t ds_timestamp_reader_from_metadata_safety::get_frame_timestamp(const std::shared_ptr<frame_interface>& frame)
    {
        std::lock_guard<std::recursive_mutex> lock(_mtx);

        auto f = std::dynamic_pointer_cast<librealsense::frame>(frame);
        if (!f)
        {
            LOG_ERROR("Frame is not valid. Failed to downcast to librealsense::frame.");
            return 0;
        }

        _has_metadata[0] = has_metadata(frame);

        auto md = (librealsense::md_safety_info*)(f->additional_data.metadata_blob.data() + platform::uvc_header_size);
        if (_has_metadata[0] && md)
        {
            return (double)(md->frame_timestamp) * MICROSEC_TO_MILLISEC;
        }
        else
        {
            if (!one_time_note)
            {
                LOG_INFO("UVC metadata payloads not available. Please refer to the installation chapter for details.");
                one_time_note = true;
            }
            return _backup_timestamp_reader->get_frame_timestamp(frame);
        }
    }
    unsigned long long ds_timestamp_reader_from_metadata_safety::get_frame_counter(const std::shared_ptr<frame_interface>& frame) const
    {
        std::lock_guard<std::recursive_mutex> lock(_mtx);

        auto f = std::dynamic_pointer_cast<librealsense::frame>(frame);
        if (!f)
        {
            LOG_ERROR("Frame is not valid. Failed to downcast to librealsense::frame.");
            return 0;
        }

        if (_has_metadata[0] && f->additional_data.metadata_size > platform::uvc_header_size)
        {
            auto md = (librealsense::md_safety_info*)(f->additional_data.metadata_blob.data() + platform::uvc_header_size);
            return md->frame_counter;
        }

        return _backup_timestamp_reader->get_frame_counter(frame);
    }

    ds_timestamp_reader_from_metadata_mipi::ds_timestamp_reader_from_metadata_mipi(std::unique_ptr<frame_timestamp_reader> backup_timestamp_reader)
    : ds_timestamp_reader_from_metadata(std::move(backup_timestamp_reader)) {}

    rs2_time_t ds_timestamp_reader_from_metadata_mipi::get_frame_timestamp(const std::shared_ptr<frame_interface>& frame)
    {
        std::lock_guard<std::recursive_mutex> lock(_mtx);

        auto f = std::dynamic_pointer_cast<librealsense::frame>(frame);
        if (!f)
        {
            LOG_ERROR("Frame is not valid. Failed to downcast to librealsense::frame.");
            return 0;
        }
        size_t pin_index = 0;

        if (frame->get_stream()->get_format() == RS2_FORMAT_Z16)
            pin_index = 1;

        _has_metadata[pin_index] = has_metadata(frame);

        auto md = (librealsense::metadata_mipi_depth_raw*)(f->additional_data.metadata_blob.data());
        if(_has_metadata[pin_index] && md)
        {
            return (double)(md->header.header.timestamp) * MICROSEC_TO_MILLISEC;
        }
        else
        {
            if (!one_time_note)
            {
                LOG_INFO("UVC metadata payloads not available. Please refer to the installation chapter for details.");
                one_time_note = true;
            }
            return _backup_timestamp_reader->get_frame_timestamp(frame);
        }
    }

    unsigned long long ds_timestamp_reader_from_metadata_mipi::get_frame_counter(const std::shared_ptr<frame_interface>& frame) const
    {
        std::lock_guard<std::recursive_mutex> lock(_mtx);

        auto f = std::dynamic_pointer_cast<librealsense::frame>(frame);
        if (!f)
        {
            LOG_ERROR("Frame is not valid. Failed to downcast to librealsense::frame.");
            return 0;
        }
        size_t pin_index = 0;

        if (frame->get_stream()->get_format() == RS2_FORMAT_Z16)
            pin_index = 1;

        if(_has_metadata[pin_index] && f->additional_data.metadata_size > platform::uvc_header_mipi_size)
        {
            auto md = (librealsense::metadata_mipi_depth_raw*)(f->additional_data.metadata_blob.data());
            if (md->capture_valid())
                return md->header.frame_counter;
        }

        return _backup_timestamp_reader->get_frame_counter(frame);
    }

    ds_timestamp_reader_from_metadata_mipi_color::ds_timestamp_reader_from_metadata_mipi_color(std::unique_ptr<frame_timestamp_reader> backup_timestamp_reader)
    : ds_timestamp_reader_from_metadata(std::move(backup_timestamp_reader)) {}

    rs2_time_t ds_timestamp_reader_from_metadata_mipi_color::get_frame_timestamp(const std::shared_ptr<frame_interface>& frame)
    {
        std::lock_guard<std::recursive_mutex> lock(_mtx);

        auto f = std::dynamic_pointer_cast<librealsense::frame>(frame);
        if (!f)
        {
            LOG_ERROR("Frame is not valid. Failed to downcast to librealsense::frame.");
            return 0;
        }
        size_t pin_index = 0;

        if (frame->get_stream()->get_format() == RS2_FORMAT_Z16)
            pin_index = 1;

        _has_metadata[pin_index] = has_metadata(frame);

        auto md = (librealsense::metadata_mipi_rgb_raw*)(f->additional_data.metadata_blob.data());
        if(_has_metadata[pin_index] && md)
        {
            return (double)(md->header.header.timestamp) * MICROSEC_TO_MILLISEC;
        }
        else
        {
            if (!one_time_note)
            {
                LOG_INFO("UVC metadata payloads not available. Please refer to the installation chapter for details.");
                one_time_note = true;
            }
            return _backup_timestamp_reader->get_frame_timestamp(frame);
        }
    }

    unsigned long long ds_timestamp_reader_from_metadata_mipi_color::get_frame_counter(const std::shared_ptr<frame_interface>& frame) const
    {
        std::lock_guard<std::recursive_mutex> lock(_mtx);

        auto f = std::dynamic_pointer_cast<librealsense::frame>(frame);
        if (!f)
        {
            LOG_ERROR("Frame is not valid. Failed to downcast to librealsense::frame.");
            return 0;
        }
        size_t pin_index = 0;

        if (frame->get_stream()->get_format() == RS2_FORMAT_Z16)
            pin_index = 1;

        if(_has_metadata[pin_index] && f->additional_data.metadata_size > platform::uvc_header_mipi_size)
        {
            auto md = (librealsense::metadata_mipi_rgb_raw*)(f->additional_data.metadata_blob.data());
            if (md->capture_valid())
                return md->header.frame_counter;
        }

        return _backup_timestamp_reader->get_frame_counter(frame);
    }

    ds_timestamp_reader_from_metadata_mipi_motion::ds_timestamp_reader_from_metadata_mipi_motion(std::unique_ptr<frame_timestamp_reader> backup_timestamp_reader)
    : ds_timestamp_reader_from_metadata(std::move(backup_timestamp_reader)) {}

    rs2_time_t ds_timestamp_reader_from_metadata_mipi_motion::get_frame_timestamp(const std::shared_ptr<frame_interface>& frame)
    {
        std::lock_guard<std::recursive_mutex> lock(_mtx);

        auto f = std::dynamic_pointer_cast<librealsense::frame>(frame);
        if (!f)
        {
            LOG_ERROR("Frame is not valid. Failed to downcast to librealsense::frame.");
            return 0;
        }

        size_t pin_index = 0;
        _has_metadata[pin_index] = true;

        auto md = (librealsense::metadata_hid_raw*)(f->additional_data.metadata_blob.data());
        if(md)
        {
            return (double)(md->header.timestamp) * MICROSEC_TO_MILLISEC;
        }
        else
        {
            if (!one_time_note)
            {
                LOG_INFO("UVC motion metadata payloads not available. Please refer to the installation chapter for details.");
                one_time_note = true;
            }
            return _backup_timestamp_reader->get_frame_timestamp(frame);
        }
    }

    unsigned long long ds_timestamp_reader_from_metadata_mipi_motion::get_frame_counter(const std::shared_ptr<frame_interface>& frame) const
    {
        // no counter is provided in the synthetic metadata structure - librealsense::metadata_hid_raw
        std::lock_guard<std::recursive_mutex> lock(_mtx);

        return _backup_timestamp_reader->get_frame_counter(frame);
    }

    void ds_timestamp_reader_from_metadata_mipi_motion::reset()
    {
        // no counter is provided in the synthetic metadata structure - librealsense::metadata_hid_raw
        _backup_timestamp_reader->reset();
    }

    ds_timestamp_reader::ds_timestamp_reader()
        : counter(pins)
    {
        reset();
    }

    void ds_timestamp_reader::reset()
    {
        std::lock_guard<std::recursive_mutex> lock(_mtx);
        for (auto i = 0; i < pins; ++i)
        {
            counter[i] = 0;
        }
    }

    rs2_time_t ds_timestamp_reader::get_frame_timestamp(const std::shared_ptr<frame_interface>& frame)
    {
        return time_service::get_time();
    }

    unsigned long long ds_timestamp_reader::get_frame_counter(const std::shared_ptr<frame_interface>& frame) const
    {
        std::lock_guard<std::recursive_mutex> lock(_mtx);
        auto pin_index = 0;
        if (frame->get_stream()->get_format() == RS2_FORMAT_Z16)
            pin_index = 1;

        return ++counter[pin_index];
    }

    rs2_timestamp_domain ds_timestamp_reader::get_frame_timestamp_domain(const std::shared_ptr<frame_interface>& frame) const
    {
        return RS2_TIMESTAMP_DOMAIN_SYSTEM_TIME;
    }

    ds_custom_hid_timestamp_reader::ds_custom_hid_timestamp_reader()
    {
        counter.resize(sensors);
        reset();
    }

    void ds_custom_hid_timestamp_reader::reset()
    {
        std::lock_guard<std::recursive_mutex> lock(_mtx);
        for (auto i = 0; i < sensors; ++i)
        {
            counter[i] = 0;
        }
    }

    rs2_time_t ds_custom_hid_timestamp_reader::get_frame_timestamp(const std::shared_ptr<frame_interface>& frame)
    {
        std::lock_guard<std::recursive_mutex> lock(_mtx);
        static const uint8_t timestamp_offset = 17;
        auto f = std::dynamic_pointer_cast<librealsense::frame>(frame);
        if (!f)
        {
            LOG_ERROR("Frame is not valid. Failed to downcast to librealsense::frame.");
            return 0;
        }

        // The timewstamp shall be trimmed back to 32 bit to allow HID/UVC intra-stream sync
        // See d400_iio_hid_timestamp_reader description
        auto timestamp = *((uint32_t*)((const uint8_t*)f->get_frame_data() + timestamp_offset));
        // TODO - verify units with custom report
        return static_cast<rs2_time_t>(timestamp) * MICROSEC_TO_MILLISEC;
    }

    bool ds_custom_hid_timestamp_reader::has_metadata(const std::shared_ptr<frame_interface>& frame) const
    {
        return true;
    }

    unsigned long long ds_custom_hid_timestamp_reader::get_frame_counter(const std::shared_ptr<frame_interface>& frame) const
    {
        std::lock_guard<std::recursive_mutex> lock(_mtx);
        return ++counter.front();
    }

    rs2_timestamp_domain ds_custom_hid_timestamp_reader::get_frame_timestamp_domain(const std::shared_ptr<frame_interface>& frame) const
    {
        return RS2_TIMESTAMP_DOMAIN_HARDWARE_CLOCK;
    }
}
