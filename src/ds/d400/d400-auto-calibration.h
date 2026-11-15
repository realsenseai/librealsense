// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015 RealSense, Inc. All Rights Reserved.

#pragma once

#include "auto-calibrated-device.h"
#include "../../core/advanced_mode.h"
#include <src/ds/ds-calib-common.h>

#include <rsutils/deferred.h>


namespace librealsense
{
    class auto_calibrated : public auto_calibrated_interface
    {
        enum class auto_calib_action
        {
            RS2_OCC_ACTION_ON_CHIP_CALIB,         // On-Chip calibration
            RS2_OCC_ACTION_TARE_CALIB            // Tare calibration
        };

        enum class interactive_calibration_state
        {
            RS2_OCC_STATE_NOT_ACTIVE = 0,
            RS2_OCC_STATE_WAIT_TO_CAMERA_START,
            RS2_OCC_STATE_INITIAL_FW_CALL,
            RS2_OCC_STATE_WAIT_TO_CALIB_START,
            RS2_OCC_STATE_DATA_COLLECT,
            RS2_OCC_STATE_WAIT_FOR_FINAL_FW_CALL,
            RS2_OCC_STATE_FINAL_FW_CALL
        };

    public:
        auto_calibrated();
        void write_calibration() const override;
        std::vector<uint8_t> run_on_chip_calibration(int timeout_ms, std::string json, float* const health, rs2_update_progress_callback_sptr progress_callback) override;
        std::vector<uint8_t> run_tare_calibration(int timeout_ms, float ground_truth_mm, std::string json, float* const health, rs2_update_progress_callback_sptr progress_callback) override;
        std::vector<uint8_t> process_calibration_frame(int timeout_ms, const rs2_frame* f, float* const health, rs2_update_progress_callback_sptr progress_callback) override;
        std::vector<uint8_t> get_calibration_table() const override;
        void set_calibration_table(const std::vector<uint8_t>& calibration) override;
        void reset_to_factory_calibration() const override;
        std::vector<uint8_t> run_focal_length_calibration(rs2_frame_queue* left, rs2_frame_queue* right, float target_w, float target_h,
            int adjust_both_sides, float* ratio, float* angle, rs2_update_progress_callback_sptr progress_callback) override;
        std::vector<uint8_t> run_uv_map_calibration(rs2_frame_queue* left, rs2_frame_queue* color, rs2_frame_queue* depth, int py_px_only,
            float* const health, int health_size, rs2_update_progress_callback_sptr progress_callback) override;
        float calculate_target_z(rs2_frame_queue* queue1, rs2_frame_queue* queue2, rs2_frame_queue* queue3,
            float target_width, float target_height, rs2_update_progress_callback_sptr progress_callback) override;
        std::string get_calibration_config() const override;
        void set_calibration_config(const std::string& calibration_config_json_str) const override;
        void set_hw_monitor_for_auto_calib(std::shared_ptr<hw_monitor> hwm);
    private:
        std::vector<uint8_t> get_calibration_results(float* const health = nullptr) const;
        std::vector<uint8_t> get_PyRxFL_calibration_results(float* const health = nullptr, float* health_fl = nullptr) const;
        void handle_calibration_error(int status) const;
        rsutils::deferred change_preset();
        void check_one_button_params(int speed, int keep_new_value_after_sucessful_scan, int data_sampling, int adjust_both_sides, int fl_scan_location, int fy_scan_direction, int white_wall_mode) const;
        void undistort(uint8_t* img, const rs2_intrinsics& intrin, int roi_ws, int roi_hs, int roi_we, int roi_he);
        void change_preset_and_stay();
        void restore_preset();
        void find_z_at_corners(float left_x[4], float left_y[4], rs2_frame_queue* frames, float left_z[4]);
        void get_target_dots_info(rs2_frame_queue* frames, float dots_x[4], float dots_y[4], rs2::stream_profile & profile, rs2_intrinsics & fy, int progress, rs2_update_progress_callback_sptr progress_callback);
        uint16_t calc_fill_rate(const rs2_frame* f);
        void fill_missing_data(uint16_t data[256], int size);
        void remove_outliers(uint16_t data[256], int size);
        void collect_depth_frame_sum(const rs2_frame* f);
        ds_calib_common::dsc_check_status_result get_calibration_status( int timeout_ms, std::function< void( const int count ) > progress_func, bool wait_for_final_results = true );

        std::vector<uint8_t> _curr_calibration;
        std::shared_ptr<hw_monitor> _hw_monitor;

        bool _preset_change = false;
        preset _old_preset_values;
        rs2_rs400_visual_preset _old_preset;

        std::string _json;
        float _ground_truth_mm;
        int _total_frames;
        int _average_step_count;
        int _collected_counter;
        int _collected_frame_num;
        double _collected_sum;
        int _resize_factor;
        auto_calib_action _action;
        interactive_calibration_state _interactive_state;
        bool _interactive_scan;
        rs2_metadata_type _prev_frame_counter;
        uint16_t _fill_factor[256];
        uint16_t _min_valid_depth, _max_valid_depth;
        int _skipped_frames;

    };
}
