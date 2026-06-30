// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2022 RealSense, Inc. All Rights Reserved.

#pragma once

#include <map>

#include "ds-private.h"

#include "algo.h"
#include "error-handling.h"

namespace librealsense
{
    class ds_timestamp_reader_from_metadata : public frame_timestamp_reader
    {
    protected:
       std::unique_ptr<frame_timestamp_reader> _backup_timestamp_reader;
       static const int pins = 2;
       std::vector<std::atomic<bool>> _has_metadata;
       bool one_time_note;
       mutable std::recursive_mutex _mtx;

    public:
        ds_timestamp_reader_from_metadata(std::unique_ptr<frame_timestamp_reader> backup_timestamp_reader);

        bool has_metadata(const std::shared_ptr<frame_interface>& frame);

        rs2_time_t get_frame_timestamp(const std::shared_ptr<frame_interface>& frame) override;

        unsigned long long get_frame_counter(const std::shared_ptr<frame_interface>& frame) const override;

        void reset() override;

        rs2_timestamp_domain get_frame_timestamp_domain(const std::shared_ptr<frame_interface>& frame) const override;
    };

    class ds_timestamp_reader_from_metadata_depth_mapping : public ds_timestamp_reader_from_metadata
    {
    public:
        ds_timestamp_reader_from_metadata_depth_mapping(std::unique_ptr<frame_timestamp_reader> backup_timestamp_reader);

        rs2_time_t get_frame_timestamp(const std::shared_ptr<frame_interface>& frame) override;

        unsigned long long get_frame_counter(const std::shared_ptr<frame_interface>& frame) const override;
    };

    class ds_timestamp_reader_from_metadata_safety : public ds_timestamp_reader_from_metadata
    {
    public:
        ds_timestamp_reader_from_metadata_safety(std::unique_ptr<frame_timestamp_reader> backup_timestamp_reader);

        rs2_time_t get_frame_timestamp(const std::shared_ptr<frame_interface>& frame) override;

        unsigned long long get_frame_counter(const std::shared_ptr<frame_interface>& frame) const override;
    };

    class ds_timestamp_reader_from_metadata_mipi : public ds_timestamp_reader_from_metadata
    {
    public:
        ds_timestamp_reader_from_metadata_mipi(std::unique_ptr<frame_timestamp_reader> backup_timestamp_reader);

        rs2_time_t get_frame_timestamp(const std::shared_ptr<frame_interface>& frame) override;

        unsigned long long get_frame_counter(const std::shared_ptr<frame_interface>& frame) const override;
    };

    class ds_timestamp_reader_from_metadata_mipi_color : public ds_timestamp_reader_from_metadata
    {
    public:
        ds_timestamp_reader_from_metadata_mipi_color(std::unique_ptr<frame_timestamp_reader> backup_timestamp_reader);

        rs2_time_t get_frame_timestamp(const std::shared_ptr<frame_interface>& frame) override;

        unsigned long long get_frame_counter(const std::shared_ptr<frame_interface>& frame) const override;
    };

    class ds_timestamp_reader_from_metadata_mipi_motion : public ds_timestamp_reader_from_metadata
    {
    public:
        ds_timestamp_reader_from_metadata_mipi_motion(std::unique_ptr<frame_timestamp_reader> backup_timestamp_reader);

        rs2_time_t get_frame_timestamp(const std::shared_ptr<frame_interface>& frame) override;

        unsigned long long get_frame_counter(const std::shared_ptr<frame_interface>& frame) const override;

        void reset() override;
    };

    class ds_timestamp_reader : public frame_timestamp_reader
    {
        mutable std::map<int, int64_t> counter; // Per-stream software frame counter
        mutable std::recursive_mutex _mtx;
    public:
        ds_timestamp_reader();

        void reset() override;

        rs2_time_t get_frame_timestamp(const std::shared_ptr<frame_interface>& frame) override;

        unsigned long long get_frame_counter(const std::shared_ptr<frame_interface>& frame) const override;

        rs2_timestamp_domain get_frame_timestamp_domain(const std::shared_ptr<frame_interface>& frame) const override;
    };

    class ds_custom_hid_timestamp_reader : public frame_timestamp_reader
    {
        static const int sensors = 4; // TODO: implement frame-counter for each GPIO or
                                      //       reading counter field report
        mutable std::vector<int64_t> counter;
        mutable std::recursive_mutex _mtx;
    public:
        ds_custom_hid_timestamp_reader();

        void reset() override;

        rs2_time_t get_frame_timestamp(const std::shared_ptr<frame_interface>& frame) override;

        bool has_metadata(const std::shared_ptr<frame_interface>& frame) const;

        unsigned long long get_frame_counter(const std::shared_ptr<frame_interface>& frame) const override;

        rs2_timestamp_domain get_frame_timestamp_domain(const std::shared_ptr<frame_interface>& frame) const override;
    };
}
