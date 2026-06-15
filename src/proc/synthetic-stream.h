// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 RealSense, Inc. All Rights Reserved.

#pragma once

#include <src/core/synthetic-source-interface.h>
#include "../core/processing-block-interface.h"
#include "../source.h"

#include <src/core/info.h>
#include <src/core/options-container.h>

#include <librealsense2/hpp/rs_frame.hpp>
#include <librealsense2/hpp/rs_processing.hpp>

#ifdef ENABLED_STATS
#include <atomic>
#endif

namespace librealsense
{


    // A synthetic source is simply a wrapper around a new frame_source and its exposure thru the rs2_source APIs
    //
    class synthetic_source : public synthetic_source_interface
    {
    public:
        synthetic_source( frame_source & actual );

        frame_interface* allocate_video_frame(std::shared_ptr<stream_profile_interface> stream,
            frame_interface* original,
            int new_bpp = 0,
            int new_width = 0,
            int new_height = 0,
            int new_stride = 0,
            rs2_extension frame_type = RS2_EXTENSION_VIDEO_FRAME) override;

        frame_interface* allocate_motion_frame(std::shared_ptr<stream_profile_interface> stream,
            frame_interface* original,
            rs2_extension frame_type = RS2_EXTENSION_MOTION_FRAME) override;

        frame_interface* allocate_composite_frame(std::vector<frame_holder> frames) override;

        frame_interface* allocate_points(std::shared_ptr<stream_profile_interface> stream, 
            frame_interface* original, rs2_extension frame_type = RS2_EXTENSION_POINTS) override;

        void frame_ready(frame_holder result) override;

        rs2_source* get_rs2_source() const { return _c_wrapper.get(); }

    private:
        frame_source & _actual_source;
        std::shared_ptr<rs2_source> _c_wrapper;
    };

    class LRS_EXTENSION_API processing_block : public processing_block_interface, public options_container, public info_container
    {
    public:
        processing_block(const char* name);

        void set_processing_callback( rs2_frame_processor_callback_sptr callback) override;
        void set_output_callback( rs2_frame_callback_sptr callback) override;
        void invoke(frame_holder frames) override;
        synthetic_source_interface& get_source() override { return _source_wrapper; }

        virtual ~processing_block() { _source.flush(); }
    protected:
        frame_source _source;
        std::mutex _mutex;
        rs2_frame_processor_callback_sptr _callback;
        synthetic_source _source_wrapper;
#ifdef ENABLED_STATS
        std::atomic< bool > _rum_applied{ false };  // RUM: report first frame through this block once
#endif
    };

    class LRS_EXTENSION_API generic_processing_block : public processing_block
    {
    public:
        generic_processing_block(const char* name);
        virtual ~generic_processing_block() { _source.flush(); }

    protected:
        virtual rs2::frame prepare_output(const rs2::frame_source& source, rs2::frame input, std::vector<rs2::frame> results);

        virtual bool should_process(const rs2::frame& frame) = 0;
        virtual rs2::frame process_frame(const rs2::frame_source& source, const rs2::frame& f) = 0;
    };

    struct stream_filter
    {
        rs2_stream stream;
        rs2_format format;
        int index;

        stream_filter() : stream(RS2_STREAM_ANY), format(RS2_FORMAT_ANY), index(-1) {}
        stream_filter(rs2_stream s, rs2_format f, int i) : stream(s), format(f), index(i) {}

        bool match(const rs2::frame& frame)
        {
            stream_filter filter(frame.get_profile().stream_type(), frame.get_profile().format(), frame.get_profile().stream_index());
            return match(filter);
        }

        bool match(const stream_filter& other)
        {
            if (stream != RS2_STREAM_ANY && stream != other.stream)
                return false;
            if (format != RS2_FORMAT_ANY && format != other.format)
                return false;
            if (index != -1 && index != other.index)
                return false;
            return true;
        }

        bool operator==(const stream_filter& other)
        {
            if (stream != other.stream)
                return false;
            if (format != other.format)
                return false;
            if (index != other.index)
                return false;
            return true;
        }

        bool operator!=(const stream_filter& other)
        {
            return !(*this == other);
        }

        void operator=(const stream_filter& other)
        {
            stream = other.stream;
            format = other.format;
            index = other.index;
        }
    };

    class LRS_EXTENSION_API stream_filter_processing_block : public generic_processing_block
    {
    public:
        stream_filter_processing_block(const char* name);
        virtual ~stream_filter_processing_block() { _source.flush(); }

    protected:
        stream_filter _stream_filter;

        bool should_process(const rs2::frame& frame) override;
    };

    // process frames with a given function
    class LRS_EXTENSION_API functional_processing_block : public stream_filter_processing_block
    {
    public:
        functional_processing_block(const char* name, rs2_format target_format, rs2_stream target_stream = RS2_STREAM_ANY, rs2_extension extension_type = RS2_EXTENSION_VIDEO_FRAME);

    protected:
        virtual void init_profiles_info(const rs2::frame* f);
        rs2::frame process_frame(const rs2::frame_source & source, const rs2::frame & f) override;
        virtual rs2::frame prepare_frame(const rs2::frame_source& source, const rs2::frame& f);
        virtual void process_function(uint8_t * const dest[], const uint8_t * source, int width, int height, int actual_size, int input_size) = 0;

        rs2::stream_profile _target_stream_profile;
        rs2::stream_profile _source_stream_profile;
        rs2_format _target_format;
        rs2_stream _target_stream;
        rs2_extension _extension_type;
        int _target_bpp = 0;
    };

    // process interleaved frames with a given function
    class interleaved_functional_processing_block : public processing_block
    {
    public:
        interleaved_functional_processing_block(const char* name,
            rs2_format source_format,
            rs2_format left_target_format,
            rs2_stream left_target_stream,
            rs2_extension left_extension_type,
            int left_idx,
            rs2_format right_target_format,
            rs2_stream right_target_stream,
            rs2_extension right_extension_type,
            int right_idx);

    protected:
        virtual void process_function(uint8_t * const dest[], const uint8_t * source, int width, int height, int actual_size, int input_size) = 0;
        void configure_processing_callback();

        std::shared_ptr<stream_profile_interface> _source_stream_profile;
        std::shared_ptr<stream_profile_interface> _left_target_stream_profile;
        std::shared_ptr<stream_profile_interface> _right_target_stream_profile;
        rs2_format _source_format;
        rs2_format _left_target_format;
        rs2_stream _left_target_stream;
        rs2_extension _left_extension_type;
        rs2_format _right_target_format;
        rs2_stream _right_target_stream;
        rs2_extension _right_extension_type;
        int _left_target_bpp = 0;
        int _right_target_bpp = 0;
        int _left_target_profile_idx = 1;
        int _right_target_profile_idx = 2;
    };

    class depth_processing_block : public stream_filter_processing_block
    {
    public:
        depth_processing_block(const char* name) : stream_filter_processing_block(name) {}

        virtual ~depth_processing_block() { _source.flush(); }

    protected:
        bool should_process(const rs2::frame& frame) override;
    };

}

// API structures
struct rs2_options
{
    rs2_options(librealsense::options_interface* options) : options(options) { }

    librealsense::options_interface* options;

    virtual ~rs2_options() = default;

    rsutils::subscription subscription;
};

struct rs2_processing_block : public rs2_options
{
    rs2_processing_block(std::shared_ptr<librealsense::processing_block_interface> block)
        : rs2_options((librealsense::options_interface*)block.get()),
        block(block) { }

    std::shared_ptr<librealsense::processing_block_interface> block;

    rs2_processing_block& operator=(const rs2_processing_block&) = delete;
    rs2_processing_block(const rs2_processing_block&) = delete;
};
