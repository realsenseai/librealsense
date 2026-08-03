// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2024 RealSense, Inc. All Rights Reserved.

#pragma once

#include "notifications.h"
#include <rsutils/concurrency/concurrency.h>
#include "../src/algo.h"
#include <map>
#include <set>

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
    class d500_on_chip_calib_manager : public process_manager
    {
    public:
        d500_on_chip_calib_manager(viewer_model& viewer, std::shared_ptr<subdevice_model> sub, device_model& model, 
            device dev);

        enum calib_action
        {
            RS2_CALIB_ACTION_ON_CHIP_CALIB,         // On-Chip calibration
            RS2_CALIB_ACTION_ON_CHIP_CALIB_DRY_RUN, // Dry Run
            RS2_CALIB_ACTION_ON_CHIP_CALIB_ABORT,   // Abort
            RS2_CALIB_ACTION_ON_CHIP_CALIB_COMMIT,  // D5x5 interactive only — approve HEALTH_CHECK candidate
            RS2_CALIB_ACTION_ON_CHIP_CALIB_TRY_NEW, // D5x5 interactive only — preview new candidate live
            RS2_CALIB_ACTION_ON_CHIP_CALIB_TRY_OLD  // D5x5 interactive only — restore currently-committed table live
        };

        calib_action action = RS2_CALIB_ACTION_ON_CHIP_CALIB;

        std::shared_ptr<subdevice_model> _sub;

        void reset_device() { _dev.hardware_reset(); }
        bool abort();
        void prepare_for_calibration();
        std::string get_device_pid() const;

        // D5x5 interactive triggered calibration only. The viewer sees only the scalar `rect_health` populated in the `float* health`
        // out-param of run_on_chip_calibration — the full CalibrationHealthMetrics struct requires an SDK-internal
        // include, deliberately not surfaced through common/ to avoid a public API addition.
        bool uses_interactive_triggered_calibration() const;
        float get_scalar_health() const { return _scalar_health; }
        // Must match librealsense::rect_health_pass_threshold_px in src/calibration-engine-interface.h.
        // The viewer cannot include that SDK-internal header, so the value is mirrored here.
        static constexpr float k_rect_health_pass_threshold_px = 0.4f;
        bool health_passes() const { return _scalar_health >= 0.f && _scalar_health < k_rect_health_pass_threshold_px; }

    private:
        void process_flow(std::function<void()> cleanup, invoker invoke) override;
        std::string convert_action_to_json_string();
        float _scalar_health = -1.f;

        // D5x5 interactive triggered calibration only — mirrors on_chip_calib_manager::start_viewer for the depth-only case.
        // Selects Z16 @ w×h @ fps on the depth subdevice, kicks off streaming, waits for the first frame.
        bool start_viewer(int w, int h, int fps, invoker invoke);
        void try_start_viewer(int w, int h, int fps, invoker invoke);
        void stop_viewer(invoker invoke);
        // Undo the RUN-phase auto-start: stop the calibration stream, restore _sub->ui/stream_enabled, replay
        // the user's prior stream if they were streaming before. No-op if we never auto-started.
        void restore_workspace(invoker invoke);

        template<class T>
        void set_option_if_needed(T& sensor, rs2_option opt, float required_value);
        device _dev;
        device_model& _model;
        viewer_model& _viewer;
        bool _was_streaming = false;
        std::shared_ptr<subdevice_ui_selection> _saved_ui;
        std::map<int, bool> _saved_stream_enabled;
    };

    template<class T>
    void d500_on_chip_calib_manager::set_option_if_needed(T& sensor,
        rs2_option opt, float required_value)
    {
        auto curr_value = sensor.get_option(opt);
        if (curr_value != required_value)
        {
            sensor.set_option(opt, required_value);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            curr_value = sensor.get_option(opt);
            if (curr_value != required_value)
            {
                std::stringstream s;
                s << "Could not set " << rs2_option_to_string(opt) << " to " << required_value;
                throw std::runtime_error(s.str().c_str());
            }
        }
    }

    // Auto-calib notification model is managing the UI state-machine
    // controling auto-calibration
    struct d500_autocalib_notification_model : public process_notification_model
    {
        enum auto_calib_ui_state
        {
            RS2_CALIB_STATE_INIT_CALIB,      // First screen
            RS2_CALIB_STATE_FAILED,          // Failed, show _error_message
            RS2_CALIB_STATE_COMPLETE,        // After write, quick blue notification
            RS2_CALIB_STATE_CALIB_IN_PROCESS,// Calibration in process... Shows progressbar
            RS2_CALIB_STATE_INIT_DRY_RUN,
            RS2_CALIB_STATE_ABORT,
            RS2_CALIB_STATE_ABORT_CALLED,
            RS2_CALIB_STATE_HEALTH_CHECK,    // D5x5 interactive only — candidate ready, awaiting user Commit/Discard/Try
            RS2_CALIB_STATE_COMMIT_IN_PROGRESS  // D5x5 interactive only — flash write in progress
        };

        d500_autocalib_notification_model(std::string name, std::shared_ptr<process_manager> manager, bool expaned);

        d500_on_chip_calib_manager& get_manager() { return *std::dynamic_pointer_cast<d500_on_chip_calib_manager>(update_manager); }
        void draw_content(ux_window& win, int x, int y, float t, std::string& error_message) override;
        int calc_height() override;
        void calibration_button(ux_window& win, int x, int y, int bar_width);
        void draw_abort(ux_window& win, int x, int y);
        void update_ui_after_abort_called(ux_window& win, int x, int y);
        void update_ui_on_calibration_complete(ux_window& win, int x, int y);
        void update_ui_on_failure(ux_window& win, int x, int y);
        void draw_health_check(ux_window& win, int x, int y, int bar_width);   // D5x5 interactive only
        void start_action_phase(d500_on_chip_calib_manager::calib_action a);   // helper for Commit / Try / Discard buttons
        std::string _error_message = "";
        bool reset_called = false;
        bool _has_abort_succeeded = false;
        // Radio-button state on the HEALTH_CHECK screen: 0 = NEW (candidate) is active, 1 = OLD (flashed) is active.
        // Initialised to OLD because per the SDK enum docs (src/calibration-engine-interface.h) the HEALTH_CHECK
        // state "caches" the candidate awaiting COMMIT/CANCEL — cached is not applied — and TRY_NEW is documented as
        // "apply the HEALTH_CHECK-cached candidate live". So on entry the active table is the flashed (OLD) one; the
        // user must click NEW to preview the candidate. Pending FW confirmation; flip to 0 if FW auto-applies at HC.
        int _try_side = 1;
        // Side to restore _try_side to if the currently-pending TRY fails on the FW side. -1 = no TRY pending.
        // ImGui::RadioButton mutates _try_side inside the widget call, so on a failure the UI would keep asserting
        // the wrong side without this rollback. draw_health_check reads update_manager->done()/failed() to detect
        // settlement and restores or clears the pending state accordingly.
        int _pending_try_revert_to = -1;
    };

}
