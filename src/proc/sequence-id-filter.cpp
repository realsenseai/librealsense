// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2020 RealSense, Inc. All Rights Reserved.

#include "sequence-id-filter.h"

namespace librealsense
{
    sequence_id_filter::sequence_id_filter()
        : generic_processing_block("Filter By Sequence id"),
        _selected_stream_id(1.f)
    {
        auto selected_stream_id = std::make_shared<ptr_option<float>>(0.f, 2.f, 1.f, 1.f,
            &_selected_stream_id, "Which HDR exposure passes through. One exposure = lower frame rate",
            std::map<float, std::string>{ {0.f, "all"}, { 1.f, "1" }, { 2.f, "2" }});
        register_option(RS2_OPTION_SEQUENCE_ID, selected_stream_id);
    }

    // processing only simple frames (not framesets)
    // only depth frames
    // only index 0
    bool sequence_id_filter::should_process(const rs2::frame& frame)
    {
        if (!frame)
            return false;

        if (frame.is<rs2::frameset>())
            return false;

        if (!frame.supports_frame_metadata(RS2_FRAME_METADATA_SEQUENCE_SIZE))
            return false;
        if (!frame.supports_frame_metadata(RS2_FRAME_METADATA_SEQUENCE_ID))
            return false;
        int seq_size = (int)frame.get_frame_metadata( RS2_FRAME_METADATA_SEQUENCE_SIZE );
        if (seq_size == 0)
            return false;
        return true;
    }

    rs2::frame sequence_id_filter::process_frame(const rs2::frame_source& source, const rs2::frame& f)
    {
        // steps:
        // check hdr seq id in metadata -
        // if not as the option selected id, return last frame with the selected id
        // else return current frame

        int seq_id = (int)f.get_frame_metadata( RS2_FRAME_METADATA_SEQUENCE_ID );
        auto unique_id = f.get_profile().unique_id();
        auto current_key = std::make_pair(seq_id, unique_id);

        if (is_selected_id(seq_id + 1))
        {
            _last_frames[current_key] = f;
            return f;
        }
        else
        {
            int seq_id_selected = (seq_id == 0) ? 1 : 0;
            auto key_with_selected_id = std::make_pair(seq_id_selected, unique_id);
            if (_last_frames[key_with_selected_id])
                return _last_frames[key_with_selected_id];
            return f;
        }
    }

    bool sequence_id_filter::is_selected_id(int stream_index) const
    {
        if (static_cast<int>(_selected_stream_id) != 0 &&
            stream_index != static_cast<int>(_selected_stream_id))
            return false;
        return true;
    }
}
