// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2024 RealSense, Inc. All Rights Reserved.

#pragma once

#include <src/auto-calibrated-device.h>
#include <src/calibration-engine-interface.h>  // should remain for members _mode, _state, _result
#include <src/ds/ds-calib-common.h>


namespace librealsense
{
    class debug_interface;
    class d500_debug_protocol_calibration_engine;
    class ds_advanced_mode_base;
    class sensor_base;

    class d500_auto_calibrated : public auto_calibrated_interface
    {
    public:
        d500_auto_calibrated( std::shared_ptr< d500_debug_protocol_calibration_engine > calib_engine,
                              debug_interface * debug_dev,
                              sensor_base * ds = nullptr );
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
        void set_depth_sensor( sensor_base * ds ) { _depth_sensor = ds; }

    private:
        void check_preconditions_and_set_state();
        void get_mode_from_json(const std::string& json);
        std::vector<uint8_t> update_calibration_status(int timeout_ms, rs2_update_progress_callback_sptr progress_callback);
        std::vector<uint8_t> update_abort_status();
        std::vector< uint8_t > run_triggered_calibration( int timeout_ms, std::string json,
                                                          rs2_update_progress_callback_sptr progress_callback );
        std::vector< uint8_t > run_interactive_triggered_calibration( int timeout_ms, std::string json,
                                                                      float * const health,
                                                                      rs2_update_progress_callback_sptr progress_callback );
        std::vector< uint8_t > update_interactive_calibration_status( int timeout_ms,
                                                                      rs2_update_progress_callback_sptr progress_callback );
        std::vector< uint8_t > run_occ( int timeout_ms, std::string json, float * const health,
                                        rs2_update_progress_callback_sptr progress_callback );
        bool device_uses_interactive_triggered_calibration() const;
        void cancel_and_wait_for_idle();   // best-effort SET_CALIB_MODE(ABORT) + poll until IDLE (or ~2s)
        try_calibration_selection _try_selection;
        ds_calib_common::dsc_check_status_result get_calibration_status( int timeout_ms,
                                                            std::function< void( const int count ) > progress_func,
                                                            bool wait_for_final_results = true ) const;
        std::vector< uint8_t > get_calibration_results( float * const health = nullptr ) const;
        std::shared_ptr< option > change_preset();

        mutable std::vector< uint8_t > _curr_calibration;
        std::shared_ptr<d500_debug_protocol_calibration_engine> _calib_engine;
        calibration_mode _mode;
        calibration_state _state;
        calibration_result _result;
        sensor_base * _depth_sensor;
        debug_interface * _debug_dev;
    };
}
