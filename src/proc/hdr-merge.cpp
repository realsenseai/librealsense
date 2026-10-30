// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2020 RealSense, Inc. All Rights Reserved.

#include "hdr-merge.h"
#include <src/core/depth-frame.h>

namespace librealsense
{
    hdr_merge::hdr_merge()
        : generic_processing_block("HDR Merge"),
        _previous_depth_frame_counter(0),
        _frames_without_requested_metadata_counter(0)
    {}

    // processing only framesets
    bool hdr_merge::should_process(const rs2::frame& frame)
    {
        if (!frame)
            return false;

        auto set = frame.as<rs2::frameset>();
        if (!set)
            return false;

        auto depth_frame = set.get_depth_frame();
        if (!depth_frame)
            return false;

        reset_warning_counter_on_pipe_restart(depth_frame);

        if (!depth_frame.supports_frame_metadata(RS2_FRAME_METADATA_SEQUENCE_SIZE) ||
            !depth_frame.supports_frame_metadata(RS2_FRAME_METADATA_SEQUENCE_ID))
        {
            // warning message will be sent to user if more than NUMBER_OF_FRAMES_WITHOUT_METADATA_FOR_WARNING
            // frames are received without the needed metadata params
            if (_frames_without_requested_metadata_counter < NUMBER_OF_FRAMES_WITHOUT_METADATA_FOR_WARNING)
            {
                if (++_frames_without_requested_metadata_counter == NUMBER_OF_FRAMES_WITHOUT_METADATA_FOR_WARNING)
                    LOG_WARNING("HDR Merge filter cannot process frames because relevant metadata params are missing");
            }

            return false;
        }

        auto depth_seq_size = depth_frame.get_frame_metadata(RS2_FRAME_METADATA_SEQUENCE_SIZE);
        if (depth_seq_size != 2)
            return false;

        return true;
    }

    void hdr_merge::reset_warning_counter_on_pipe_restart(const rs2::depth_frame& depth_frame)
    {
        auto depth_frame_counter = depth_frame.get_frame_number();

        if (depth_frame_counter < _previous_depth_frame_counter)
            _frames_without_requested_metadata_counter = 0;

        _previous_depth_frame_counter = depth_frame_counter;
    }


    rs2::frame hdr_merge::process_frame(const rs2::frame_source& source, const rs2::frame& f)
    {
        // steps:
        // 1. get depth frame from incoming frameset
        // 2. add the frameset to vector of framesets
        // 3. check if size of this vector is at least 2 (if not - return latest merge frame)
        // 4. pop out both framesets from the vector
        // 5. apply merge algo
        // 6. save merge frame as latest merge frame
        // 7. return the merge frame

        // 1. get depth frame from incoming frameset
        auto fs = f.as<rs2::frameset>();
        auto depth_frame = fs.get_depth_frame();

        // 2. add the frameset to vector of framesets
        auto depth_seq_id = depth_frame.get_frame_metadata(RS2_FRAME_METADATA_SEQUENCE_ID);

        // condition added to ensure that frames are saved in the right order
        // to prevent for example the saving of frame with sequence id 1 before
        // saving frame of sequence id 0
        // so that the merging with be deterministic - always done with frame n and n+1
        // with frame n as basis
        if (_framesets.size() == depth_seq_id)
        {
            _framesets[(int)depth_seq_id] = fs;
        }

        // discard merged frame if not relevant
        discard_depth_merged_frame_if_needed(depth_frame);

        // 3. check if size of this vector is at least 2 (if not - return latest merge frame)
        if (_framesets.size() >= 2)
        {
            // 4. pop out both framesets from the vector
            rs2::frameset fs_0 = _framesets[0];
            rs2::frameset fs_1 = _framesets[1];
            _framesets.clear();

            bool use_ir = false;
            if (check_frames_mergeability(fs_0, fs_1, use_ir))
            {
                // 5. apply merge algo
                rs2::frame new_frame = merging_algorithm(source, fs_0, fs_1, use_ir);
                if (new_frame)
                {
                    // 6. save merge frame as latest merge frame
                    _depth_merged_frame = new_frame;
                }
            }
        }

        // 7. return the merge frame
        if (_depth_merged_frame)
            return _depth_merged_frame;

        return f;
    }

    void hdr_merge::discard_depth_merged_frame_if_needed(const rs2::frame& f)
    {
        if (_depth_merged_frame)
        {
            // criteria for discarding saved merged_depth_frame:
            // 1 - frame counter for merged depth is greater than the input frame
            // 2 - resolution change
            // 3 - delta between input frame counter and merged depth counter >= SEQUENTIAL_FRAMES_THRESHOLD
            auto depth_merged_frame_counter = _depth_merged_frame.get_frame_metadata(RS2_FRAME_METADATA_FRAME_COUNTER);
            auto input_frame_counter = f.get_frame_metadata(RS2_FRAME_METADATA_FRAME_COUNTER);

            auto merged_d_profile = _depth_merged_frame.get_profile().as<rs2::video_stream_profile>();
            auto new_d_profile = f.get_profile().as<rs2::video_stream_profile>();

            bool counter_diff_over_threshold_detected = ((input_frame_counter - depth_merged_frame_counter) >= SEQUENTIAL_FRAMES_THRESHOLD);
            bool restart_pipe_detected = (depth_merged_frame_counter > input_frame_counter);
            bool resolution_change_detected = (merged_d_profile.width() != new_d_profile.width()) ||
                (merged_d_profile.height() != new_d_profile.height());

            if (restart_pipe_detected || resolution_change_detected || counter_diff_over_threshold_detected)
            {
                _depth_merged_frame = nullptr;
            }
        }
    }

    bool hdr_merge::check_frames_mergeability(const rs2::frameset first_fs, const rs2::frameset second_fs,
        bool& use_ir) const
    {
        auto first_depth = first_fs.get_depth_frame();
        auto second_depth = second_fs.get_depth_frame();
        auto first_ir = first_fs.get_infrared_frame();
        auto second_ir = second_fs.get_infrared_frame();

        auto first_fs_frame_counter = first_depth.get_frame_metadata(RS2_FRAME_METADATA_FRAME_COUNTER);
        auto second_fs_frame_counter = second_depth.get_frame_metadata(RS2_FRAME_METADATA_FRAME_COUNTER);

        // The aim of this checking is that the output merged frame will have frame counter n and
        // frame counter n and will be created by frames n and n+1
        if (first_fs_frame_counter + 1 != second_fs_frame_counter)
            return false;
        // Depth dimensions must align
        if ((first_depth.get_height() != second_depth.get_height()) ||
            (first_depth.get_width() != second_depth.get_width()))
            return false;

        use_ir = should_ir_be_used_for_merging(first_depth, first_ir, second_depth, second_ir);

        return true;
    }

    rs2::frame hdr_merge::merging_algorithm(const rs2::frame_source& source, const rs2::frameset first_fs, const rs2::frameset second_fs, const bool use_ir) const
    {
        auto first = first_fs;
        auto second = second_fs;

        auto first_depth = first.get_depth_frame();
        auto second_depth = second.get_depth_frame();
        auto first_ir = first.get_infrared_frame();
        auto second_ir = second.get_infrared_frame();

        // new frame allocation
        auto vf = first_depth.as<rs2::depth_frame>();
        auto width = vf.get_width();
        auto height = vf.get_height();
        auto new_f = source.allocate_video_frame(first_depth.get_profile(), first_depth,
            vf.get_bytes_per_pixel(), width, height, vf.get_stride_in_bytes(), RS2_EXTENSION_DEPTH_FRAME);

        if (new_f)
        {
            auto ptr = dynamic_cast<librealsense::depth_frame*>((librealsense::frame_interface*)new_f.get());
            if (!ptr)
                throw std::runtime_error("Frame interface is not depth frame");

            auto orig = dynamic_cast<librealsense::depth_frame*>((librealsense::frame_interface*)first_depth.get());
            if (!orig)
                throw std::runtime_error("Frame interface is not depth frame");

            auto d0 = (uint16_t*)first_depth.get_data();
            auto d1 = (uint16_t*)second_depth.get_data();

            auto new_data = (uint16_t*)ptr->get_frame_data();

            ptr->set_sensor(orig->get_sensor());

            memset(new_data, 0, width * height * sizeof(uint16_t));

            int width_height_product = width * height;

            if (use_ir)
            {
                if (first_ir.get_profile().format() == RS2_FORMAT_Y8)
                {
                    merge_frames_using_ir<uint8_t>(new_data, d0, d1, first_ir, second_ir, width_height_product);
                }
                else if (first_ir.get_profile().format() == RS2_FORMAT_Y16)
                {
                    merge_frames_using_ir<uint16_t>(new_data, d0, d1, first_ir, second_ir, width_height_product);
                }
                else
                {
                    merge_frames_using_only_depth(new_data, d0, d1, width_height_product);
                }
            }
            else
            {
                merge_frames_using_only_depth(new_data, d0, d1, width_height_product);
            }

            return new_f;
        }
        return first_fs;
    }

    void hdr_merge::merge_frames_using_only_depth(uint16_t* new_data, uint16_t* d0, uint16_t* d1, int width_height_prod) const
    {
        for (int i = 0; i < width_height_prod; i++)
        {
            if (d0[i])
                new_data[i] = d0[i];
            else if (d1[i])
                new_data[i] = d1[i];
            else
                new_data[i] = 0;
        }
    }

    bool hdr_merge::should_ir_be_used_for_merging(const rs2::depth_frame& first_depth, const rs2::video_frame& first_ir,
        const rs2::depth_frame& second_depth, const rs2::video_frame& second_ir) const
    {
        // checking ir frames are not null
        bool use_ir = (first_ir && second_ir);

        if (use_ir)
        {
            // IR and Depth dimensions must be aligned
            if ((first_depth.get_height() != first_ir.get_height()) ||
                (first_depth.get_width() != first_ir.get_width()) ||
                (second_ir.get_height() != first_ir.get_height()) ||
                (second_ir.get_width() != first_ir.get_width()))
                use_ir = false;
        }

        // checking first depth and ir come from the same exposure
        // (frame counters are per stream and may be offset between depth and ir, sensor timestamps are not)
        if (use_ir)
        {
            // on devices that does not support meta data on IR frames, do not use IR for hdr merging
            if (!first_ir.supports_frame_metadata(RS2_FRAME_METADATA_SENSOR_TIMESTAMP)  ||
                !second_ir.supports_frame_metadata(RS2_FRAME_METADATA_SENSOR_TIMESTAMP) ||
                !first_ir.supports_frame_metadata(RS2_FRAME_METADATA_SEQUENCE_ID)       ||
                !second_ir.supports_frame_metadata(RS2_FRAME_METADATA_SEQUENCE_ID))
            return false;

            use_ir = (first_depth.get_frame_metadata(RS2_FRAME_METADATA_SENSOR_TIMESTAMP) ==
                      first_ir.get_frame_metadata(RS2_FRAME_METADATA_SENSOR_TIMESTAMP));

            // checking second depth and ir come from the same exposure
            if (use_ir)
            {
                use_ir = (second_depth.get_frame_metadata(RS2_FRAME_METADATA_SENSOR_TIMESTAMP) ==
                          second_ir.get_frame_metadata(RS2_FRAME_METADATA_SENSOR_TIMESTAMP));

                // checking sequence id of first depth and ir are the same
                if (use_ir)
                {
                    auto depth_seq_id = first_depth.get_frame_metadata(RS2_FRAME_METADATA_SEQUENCE_ID);
                    auto ir_seq_id = first_ir.get_frame_metadata(RS2_FRAME_METADATA_SEQUENCE_ID);
                    use_ir = (depth_seq_id == ir_seq_id);

                    // checking sequence id of second depth and ir are the same
                    if (use_ir)
                    {
                        depth_seq_id = second_depth.get_frame_metadata(RS2_FRAME_METADATA_SEQUENCE_ID);
                        ir_seq_id = second_ir.get_frame_metadata(RS2_FRAME_METADATA_SEQUENCE_ID);
                        use_ir = (depth_seq_id == ir_seq_id);

                        // checking both ir have the same format
                        if (use_ir)
                        {
                            use_ir = (first_ir.get_profile().format() == second_ir.get_profile().format());
                        }
                    }
                }
            }
        }

        return use_ir;
    }
}
