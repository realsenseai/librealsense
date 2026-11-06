// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 RealSense, Inc. All Rights Reserved.

#pragma once

#include "notifications.h"
#include <rsutils/concurrency/concurrency.h>
#include "../src/algo.h"

#include <random>
#include <string>

namespace rs2
{
    class viewer_model;
    class subdevice_model;
    struct subdevice_ui_selection;

    // On-chip Calibration manager owns the background thread
    // leading the calibration process
    // It is controlled by autocalib_notification_model UI object
    // that invokes the process when needed
    class on_chip_calib_manager : public process_manager
    {
    public:
        on_chip_calib_manager(viewer_model& viewer, std::shared_ptr<subdevice_model> sub, device_model& model, device dev, std::shared_ptr<subdevice_model> sub_color = nullptr, bool uvmapping_calib_full = false);
        ~on_chip_calib_manager();

        bool allow_calib_keep() const { return true; }

        // Get health number from the calibration summary
        float get_health() const { return _health; }
        float get_health_1() const { return _health_1; }
        float get_health_2() const { return _health_2; }
        float get_health_nums(int idx) const { return _health_nums[idx]; }

        // Write new calibration to the device
        void keep();

        // Restore Viewer UI to how it was before auto-calib
        void restore_workspace(invoker invoke);

        // Ask the firmware to use one of the before/after calibration tables
        void apply_calib(bool use_new);

        void update_last_used();

        float ground_truth = 1200.0f;
        int average_step_count = 20;
        int step_count = 20;
        int accuracy = 2;
        int speed = 2;  //"Very Fast", "Fast", "Medium", "Slow", "White Wall"
        int speed_fl = 1;
        bool intrinsic_scan = true;
        bool apply_preset = true;

        enum auto_calib_action
        {
            RS2_CALIB_ACTION_ON_CHIP_OB_CALIB,      // On-Chip calibration extended
            RS2_CALIB_ACTION_ON_CHIP_CALIB,         // On-Chip calibration
            RS2_CALIB_ACTION_ON_CHIP_FL_CALIB,      // On-Chip focal length calibration
            RS2_CALIB_ACTION_TARE_CALIB,            // Tare calibration
            RS2_CALIB_ACTION_TARE_GROUND_TRUTH,     // Tare ground truth
            RS2_CALIB_ACTION_FL_CALIB,              // Focal length calibration
            RS2_CALIB_ACTION_UVMAPPING_CALIB,       // UVMapping calibration
            RS2_CALIB_ACTION_FL_PLUS_CALIB,     // Focal length plus calibration
        };

        auto_calib_action action = RS2_CALIB_ACTION_ON_CHIP_CALIB;
        int host_assistance = 0;
        int step_count_v3 = 256;

        int fl_step_count = 51;
        int fy_scan_range = 40;
        int keep_new_value_after_sucessful_scan = 1;
        int fl_data_sampling = 1;
        int adjust_both_sides = 0;

        int fl_scan_location = 0;
        int fy_scan_direction = 0;
        int white_wall_mode = 0;
 
        int retry_times = 0;
        bool toggle = false;

        float corrected_ratio = 0.0f;
        float tilt_angle = 0.0f;

        const float correction_factor = 0.50f;

        bool tare_health = false;

        std::shared_ptr<subdevice_model> _sub;
        std::shared_ptr<subdevice_model> _sub_color;

        bool py_px_only = false;

        const std::string Y8_FORMAT = "Y8";
        const std::string Z16_FORMAT = "Z16";
        const std::string RGB8_FORMAT = "RGB8";
        std::string device_name_string;

        void calibrate();
        void calibrate_fl();
        void calibrate_uv_mapping();
        //void calibrate_fl_plus();
        void get_ground_truth();

        void turn_roi_on();
        void turn_roi_off();

        void save_options_controlled_by_calib();
        void restore_options_controlled_by_calib();

        void start_gt_viewer();
        void start_fl_viewer();
        void start_uvmapping_viewer(bool b3D = false);
        void start_fl_plus_viewer();
        void stop_viewer();
        void reset_device() { _dev.hardware_reset(); }

    private:
        void save_laser_emitter_state();
        void save_thermal_loop_state();
        void restore_laser_emitter_state();
        void restore_thermal_loop_state();
        void set_laser_emitter_state( float value );
        void set_thermal_loop_state( float value );

        std::vector<uint8_t> safe_send_command(const std::vector<uint8_t>& cmd, const std::string& name);
        rs2::depth_frame fetch_depth_frame(invoker invoke, int timeout_ms = 5000); // Wait for next depth frame and return it
        void process_flow(std::function<void()> cleanup, invoker invoke) override;

        float _health = -1.0f;
        float _health_1 = -1.0f;    //percentage of error, relative to ground truth, before the current iteration.
        float _health_2 = -1.0f;    //percentage of error, relative to ground truth, after the current iteration.

        float _health_nums[4] = { -0.1f, -0.1f, -0.1f, -0.1f };
        std::vector<uint8_t> color_intrin_raw_data;

        device _dev;

        bool _options_saved = false;
        float _laser_status_prev = 0.0f;
        float _thermal_loop_prev = 0.0f;

        bool _was_streaming = false;
        bool _synchronized = false;
        bool _post_processing = false;
        std::shared_ptr<subdevice_ui_selection> _ui { nullptr };
        bool _in_3d_view = false;
        int _uid = 0;
        int _uid2 = 0;
        int _uid_color = 0;
        std::shared_ptr<subdevice_ui_selection> _ui_color{ nullptr };

        viewer_model& _viewer;

        std::vector<uint8_t> _old_calib, _new_calib;
        device_model& _model;

        bool _restored = true;

        float _ppx = 0.0f;
        float _ppy = 0.0f;
        float _fx = 0.0f;
        float _fy = 0.0f;

        void stop_viewer(invoker invoke);
        bool start_viewer(int w, int h, int fps, invoker invoke);
        void try_start_viewer(int w, int h, int fps, invoker invoke);
        void restore_ui_selection();
    };

    // Auto-calib notification model is managing the UI state-machine
    // controling auto-calibration
    struct autocalib_notification_model : public process_notification_model
    {
        enum auto_calib_ui_state
        {
            RS2_CALIB_STATE_INITIAL_PROMPT,  // First screen, would you like to run Health-Check?
            RS2_CALIB_STATE_FAILED,          // Failed, show _error_message
            RS2_CALIB_STATE_COMPLETE,        // After write, quick blue notification
            RS2_CALIB_STATE_CALIB_IN_PROCESS,// Calibration in process... Shows progressbar
            RS2_CALIB_STATE_CALIB_COMPLETE,  // Calibration complete, show before/after toggle and metrics
            RS2_CALIB_STATE_TARE_INPUT,      // Collect input parameters for Tare calib
            RS2_CALIB_STATE_TARE_INPUT_ADVANCED,      // Collect input parameters for Tare calib
            RS2_CALIB_STATE_SELF_INPUT,      // Collect input parameters for Self calib
            RS2_CALIB_STATE_GET_TARE_GROUND_TRUTH,            // Calculating ground truth
            RS2_CALIB_STATE_GET_TARE_GROUND_TRUTH_IN_PROCESS, // Calculating ground truth in process... Shows progressbar
            RS2_CALIB_STATE_GET_TARE_GROUND_TRUTH_COMPLETE,   // Calculating ground truth complete, show succeeded or failed
            RS2_CALIB_STATE_GET_TARE_GROUND_TRUTH_FAILED,     // Failed to calculating the ground truth
            RS2_CALIB_STATE_FL_INPUT,        // Collect input parameters for focal length calib
            RS2_CALIB_STATE_UVMAPPING_INPUT, // Collect input parameters for UVMapping calibration with specific target
            RS2_CALIB_STATE_FL_PLUS_INPUT,        // Collect input parameters for focal length plus calib
        };

        autocalib_notification_model(std::string name, std::shared_ptr<process_manager> manager, bool expaned);

        on_chip_calib_manager& get_manager() { return *std::dynamic_pointer_cast<on_chip_calib_manager>(update_manager); }

        void set_color_scheme(float t) const override;
        void draw_content(ux_window& win, int x, int y, float t, std::string& error_message) override;
        void draw_dismiss(ux_window& win, int x, int y) override;
        void draw_expanded(ux_window& win, std::string& error_message) override;
        void draw_intrinsic_extrinsic(int x, int y);
        int calc_height() override;
        void dismiss(bool snooze) override;

        bool use_new_calib = true;
        std::string _error_message = "";
    };

}
